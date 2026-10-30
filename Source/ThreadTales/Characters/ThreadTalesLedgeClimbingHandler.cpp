// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesLedgeClimbingHandler.h"
#include "ThreadTales.h"
#include "Characters/ThreadTalesTraversalComponent.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesClimbingConfig.h"
#include "Characters/ThreadTalesLedgeDetector.h"
#include "Characters/ThreadTalesTraversalInputHelper.h"
#include "Characters/ThreadTalesClimbingKinematics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "MotionWarpingComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

FThreadTalesLedgeClimbingHandler::FThreadTalesLedgeClimbingHandler()
	: ClimbingSubState(EThreadTalesClimbingSubState::None)
	, bIsFreehang(false)
	, bSavedUseControllerDesiredRotation(true)
	, bSavedOrientRotationToMovement(false)
	, bSavedUseControllerRotationYaw(false)
	, CachedLedgeLocation(FVector::ZeroVector)
	, CachedWallNormal(FVector::ZeroVector)
	, CachedHangLocation(FVector::ZeroVector)
	, CachedHangRotation(FRotator::ZeroRotator)
	, TransitionStartLocation(FVector::ZeroVector)
	, TransitionTargetLocation(FVector::ZeroVector)
	, TransitionStartRotation(FRotator::ZeroRotator)
	, TransitionTargetRotation(FRotator::ZeroRotator)
	, PendingTargetLedgeLocation(FVector::ZeroVector)
	, PendingTargetWallNormal(FVector::ZeroVector)
	, TransitionDistanceType(EThreadTalesLedgeJumpDistanceType::Short)
	, TransitionTimer(0.0f)
	, TransitionDuration(0.8f)
	, CornerTransitionPivotLocation(FVector::ZeroVector)
	, ActiveCornerType(EThreadTalesCornerType::None)
	, RegrabCooldownTimer(0.0f)
	, ClimbingInputDebounceTimer(0.0f)
	, ShimmyCooldownTimer(0.0f)
	, ContinuousScanThrottleTimer(0.0f)
{
}

void FThreadTalesLedgeClimbingHandler::Initialize(const UThreadTalesClimbingConfig* Config, UAnimMontage* FallbackHangMontage)
{
	JumpConfigTable.Build(Config, FallbackHangMontage);
}

void FThreadTalesLedgeClimbingHandler::TickTimers(float DeltaTime)
{
	if (RegrabCooldownTimer > 0.0f)
	{
		RegrabCooldownTimer -= DeltaTime;
	}
	if (ClimbingInputDebounceTimer > 0.0f)
	{
		ClimbingInputDebounceTimer -= DeltaTime;
	}
	if (ShimmyCooldownTimer > 0.0f)
	{
		ShimmyCooldownTimer -= DeltaTime;
	}
	if (ContinuousScanThrottleTimer > 0.0f)
	{
		ContinuousScanThrottleTimer -= DeltaTime;
	}
}

bool FThreadTalesLedgeClimbingHandler::CanAttemptGrab(const UThreadTalesClimbingConfig* Config) const
{
	const bool bAuto = Config ? Config->Sensing.Detection.bAutoGrabLedge : true;
	return bAuto && (RegrabCooldownTimer <= 0.0f);
}



void FThreadTalesLedgeClimbingHandler::Tick(float DeltaTime, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!CharacterOwner || !OwnerComp)
	{
		return;
	}

	// Always unconditionally suppress CMC and Controller rotation for ALL climbing states (IdleHang, GrabCatch, MantleUp, Shimmy, Jumps, Corners, ClimbDown)
	if (OwnerComp->GetTraversalState() == EThreadTalesTraversalState::LedgeClimbing)
	{
		if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
		{
			MoveComp->bUseControllerDesiredRotation = false;
			MoveComp->bOrientRotationToMovement = false;
			MoveComp->RotationRate = FRotator::ZeroRotator;
			MoveComp->Velocity = FVector::ZeroVector;
		}
		CharacterOwner->bUseControllerRotationYaw = false;
	}

	// 1. If currently transitioning between ledges (Hop Up, Side Jump, Shimmy, CornerTransition), interpolate character
	if (OwnerComp->GetTraversalState() == EThreadTalesTraversalState::LedgeClimbing && IsKinematicTransitioning())
	{
		TickTransition(DeltaTime, OwnerComp, CharacterOwner);
		return;
	}

	// 2. While hanging stationary on ledge (IdleHang), hard-anchor and process input
	if (OwnerComp->GetTraversalState() == EThreadTalesTraversalState::LedgeClimbing &&
		ClimbingSubState == EThreadTalesClimbingSubState::IdleHang)
	{
		if (CharacterOwner->IsLocallyControlled())
		{
			APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
			ProcessClimbingMovement(DeltaTime, PC, OwnerComp, CharacterOwner);
		}

		if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
		{
			MoveComp->Velocity = FVector::ZeroVector;
			MoveComp->bUseControllerDesiredRotation = false;
			MoveComp->bOrientRotationToMovement = false;
		}

		CharacterOwner->SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation, false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}
	else if (OwnerComp->GetTraversalState() == EThreadTalesTraversalState::LedgeClimbing && ClimbingSubState == EThreadTalesClimbingSubState::ClimbDown)
	{
		if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
		{
			MoveComp->bUseControllerDesiredRotation = false;
			MoveComp->bOrientRotationToMovement = false;
		}
	}
}

bool FThreadTalesLedgeClimbingHandler::TryProcessClimbingJump(
	const APlayerController* PC,
	const FVector& IntentWorldDir,
	const FVector& WallRight,
	EThreadTalesLedgeJumpDirection JumpDir,
	bool bIsDirectJumpPress,
	UThreadTalesTraversalComponent* OwnerComp,
	AThreadTalesCharacter* CharacterOwner)
{
	TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][JUMP_ACTION] Space pressed! Evaluating Radial Scanner & Fallbacks..."));

	// 3D Radial Ledge Scanner: compute 3D world intent vector and probe volumetric cone
	if (IntentWorldDir.SizeSquared() > 0.05f)
	{
		FThreadTalesLedgeCandidate BestCandidate;
		if (FThreadTalesLedgeDetector::FindBestLedgeInCone(
				CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->IsDrawDebugTracesEnabled(),
				CachedLedgeLocation, CachedWallNormal,
				IntentWorldDir, JumpConfigTable, BestCandidate))
		{
			TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][RADIAL_WINNER] Found target ledge at %s (Normal: %s, Dir: %d, Distance: %d). Performing Jump."),
				*BestCandidate.LedgeLocation.ToString(), *BestCandidate.WallNormal.ToString(), (int32)BestCandidate.Direction, (int32)BestCandidate.DistanceType);
			ClimbingInputDebounceTimer = OwnerComp->GetJumpInputDebounceDuration();
			ContinuousScanThrottleTimer = 0.0f;
			bIsFreehang = BestCandidate.bIsFreehang;
			OwnerComp->SetIsFreehang(bIsFreehang);
			PerformLedgeJump(BestCandidate.Direction, BestCandidate.DistanceType, BestCandidate.LedgeLocation, BestCandidate.WallNormal, OwnerComp, CharacterOwner);
			return true;
		}
	}

	// Secondary Fallbacks when pressing Jump:
	// Priority 1: Down: Hop Down to lower tier ledge first; if none exists, check opposing ledge down; fallback to wall eject
	if (JumpDir == EThreadTalesLedgeJumpDirection::Down || IntentWorldDir.Z < -0.2f)
	{
		for (EThreadTalesLedgeJumpDistanceType DistType : { EThreadTalesLedgeJumpDistanceType::Short, EThreadTalesLedgeJumpDistanceType::Long, EThreadTalesLedgeJumpDistanceType::Leap })
		{
			FVector LowerLedgeLoc;
			FVector LowerWallNormal;
			if (FThreadTalesLedgeDetector::FindLedgeInDirection(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->IsDrawDebugTracesEnabled(), CachedLedgeLocation, CachedWallNormal, EThreadTalesLedgeJumpDirection::Down, DistType, JumpConfigTable, LowerLedgeLoc, LowerWallNormal))
			{
				TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][HOP_DOWN_TRIGGER] S+Space pressed. Found lower ledge at %s. Performing HopDown."), *LowerLedgeLoc.ToString());
				ClimbingInputDebounceTimer = OwnerComp->GetJumpInputDebounceDuration();
				PerformLedgeJump(EThreadTalesLedgeJumpDirection::Down, DistType, LowerLedgeLoc, LowerWallNormal, OwnerComp, CharacterOwner);
				return true;
		}
		}

		FVector OpposingLedgeLoc;
		FVector OpposingWallNormal;
		if (FThreadTalesLedgeDetector::FindOpposingLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->IsDrawDebugTracesEnabled(), CachedLedgeLocation, CachedWallNormal, OpposingLedgeLoc, OpposingWallNormal, EThreadTalesOpposingLedgeFilter::DownOnly))
		{
			TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][BACK_JUMP_DOWN_TRIGGER] S+Space pressed. Found opposing ledge below/level at %s. Performing Back Jump Down."), *OpposingLedgeLoc.ToString());
			ClimbingInputDebounceTimer = OwnerComp->GetJumpInputDebounceDuration();
			ExecuteOpposingLedgeJump(OpposingLedgeLoc, OpposingWallNormal, OwnerComp, CharacterOwner);
			return true;
		}

		TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][DOWN_FALLBACK] S+Space pressed, no lower ledge and no opposing ledge below. Falling back to Back Jump / Wall Eject. Executing Wall Eject into open air!"));
		ClimbingInputDebounceTimer = OwnerComp->GetJumpInputDebounceDuration();
		WallEject(OwnerComp, CharacterOwner);
		return true;
	}

	const bool bIsBackJump = FThreadTalesTraversalInputHelper::IsBackInputHeld(PC, CharacterOwner, &CachedWallNormal);

	// Priority 2: Back Jump: Player looking away from wall into open space (or holding backward intent)
	if (bIsBackJump)
	{
		TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][BACK_JUMP_TRIGGER] Back jump triggered (Looking away from wall). Invoking PerformBackJump."));
		ClimbingInputDebounceTimer = OwnerComp->GetJumpInputDebounceDuration();
		PerformBackJump(OwnerComp, CharacterOwner);
		return true;
	}

	// Priority 3: Up: Hop Up to upper ledge first; if none exists, Mantle Up onto platform; if none exists, Back Jump Up!
	if (JumpDir == EThreadTalesLedgeJumpDirection::Up || IntentWorldDir.Z > 0.2f || (bIsDirectJumpPress && IntentWorldDir.IsNearlyZero()))
	{
		for (EThreadTalesLedgeJumpDistanceType DistType : { EThreadTalesLedgeJumpDistanceType::Short, EThreadTalesLedgeJumpDistanceType::Long, EThreadTalesLedgeJumpDistanceType::Leap })
		{
			FVector UpperLedgeLoc;
			FVector UpperWallNormal;
			if (FThreadTalesLedgeDetector::FindLedgeInDirection(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->IsDrawDebugTracesEnabled(), CachedLedgeLocation, CachedWallNormal, EThreadTalesLedgeJumpDirection::Up, DistType, JumpConfigTable, UpperLedgeLoc, UpperWallNormal))
			{
				TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][HOP_UP_TRIGGER] Found upper ledge at %s. Performing HopUp."), *UpperLedgeLoc.ToString());
				ClimbingInputDebounceTimer = OwnerComp->GetJumpInputDebounceDuration();
				PerformLedgeJump(EThreadTalesLedgeJumpDirection::Up, DistType, UpperLedgeLoc, UpperWallNormal, OwnerComp, CharacterOwner);
				return true;
		}
		}

		FVector OutStandingLoc;
		if (FThreadTalesLedgeDetector::CanClimbUp(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->IsDrawDebugTracesEnabled(), CachedLedgeLocation, CachedWallNormal, OutStandingLoc))
		{
			TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][MANTLE_TRIGGER] CanClimbUp passed on Space press. Initiating ClimbUp!"));
			ClimbingInputDebounceTimer = OwnerComp->GetJumpInputDebounceDuration();
			ClimbUp(OwnerComp, CharacterOwner);
			return true;
		}

		FVector OpposingLedgeLoc;
		FVector OpposingWallNormal;
		if (FThreadTalesLedgeDetector::FindOpposingLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->IsDrawDebugTracesEnabled(), CachedLedgeLocation, CachedWallNormal, OpposingLedgeLoc, OpposingWallNormal, EThreadTalesOpposingLedgeFilter::UpOnly))
		{
			TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][BACK_JUMP_UP_TRIGGER] W+Space pressed. Upper path on wall blocked, but found opposing ledge above at %s. Performing Back Jump Up!"), *OpposingLedgeLoc.ToString());
			ClimbingInputDebounceTimer = OwnerComp->GetJumpInputDebounceDuration();
			ExecuteOpposingLedgeJump(OpposingLedgeLoc, OpposingWallNormal, OwnerComp, CharacterOwner);
			return true;
		}

		// Fallback for dead-end wall when pressing Jump (W+Space or neutral Space):
		// Upward path is blocked (no upper ledge, cannot mantle up, no opposing upper ledge).
		// Holding grip safely — Space must never drop or eject the character from ledge.
		TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][UP_BLOCKED] Upward path and opposing upper ledge blocked on Space press. Holding grip."));
		ClimbingInputDebounceTimer = OwnerComp->GetJumpInputDebounceDuration();
		return true;
	}

	// Lateral / Diagonal Jump fallback: Jump across gap along wall to the left/right
	if (JumpDir == EThreadTalesLedgeJumpDirection::Right || JumpDir == EThreadTalesLedgeJumpDirection::Left ||
		JumpDir == EThreadTalesLedgeJumpDirection::UpRight || JumpDir == EThreadTalesLedgeJumpDirection::UpLeft ||
		JumpDir == EThreadTalesLedgeJumpDirection::DownRight || JumpDir == EThreadTalesLedgeJumpDirection::DownLeft ||
		FMath::Abs(IntentWorldDir | WallRight) > 0.25f)
	{
		const EThreadTalesLedgeJumpDirection LateralDir = (JumpDir != EThreadTalesLedgeJumpDirection::None && JumpDir != EThreadTalesLedgeJumpDirection::Up && JumpDir != EThreadTalesLedgeJumpDirection::Down)
			? JumpDir
			: ((IntentWorldDir | WallRight) > 0.0f ? EThreadTalesLedgeJumpDirection::Right : EThreadTalesLedgeJumpDirection::Left);

		for (EThreadTalesLedgeJumpDistanceType DistType : { EThreadTalesLedgeJumpDistanceType::Short, EThreadTalesLedgeJumpDistanceType::Long, EThreadTalesLedgeJumpDistanceType::Leap })
		{
			FVector SideTargetLedgeLoc;
			FVector SideTargetWallNormal;
			if (FThreadTalesLedgeDetector::FindLedgeInDirection(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->IsDrawDebugTracesEnabled(), CachedLedgeLocation, CachedWallNormal, LateralDir, DistType, JumpConfigTable, SideTargetLedgeLoc, SideTargetWallNormal))
			{
				TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][SIDE_JUMP_TRIGGER] Space pressed. Found lateral ledge in direction %d at %s. Performing Jump."), (int32)LateralDir, *SideTargetLedgeLoc.ToString());
				ClimbingInputDebounceTimer = OwnerComp->GetJumpInputDebounceDuration();
				PerformLedgeJump(LateralDir, DistType, SideTargetLedgeLoc, SideTargetWallNormal, OwnerComp, CharacterOwner);
				return true;
			}
		}
	}

	return false;
}

bool FThreadTalesLedgeClimbingHandler::TryProcessClimbingDirectional(
	const APlayerController* PC,
	EThreadTalesLedgeJumpDirection JumpDir,
	UThreadTalesTraversalComponent* OwnerComp,
	AThreadTalesCharacter* CharacterOwner)
{
	// Non-jump inputs:
	// S held -> Traverse downwards along cliff/wall:
	// 1. Hop down to lower ledge if one exists below
	// 2. If no lower ledge exists below, stay hanging (dropping is strictly via Crouch key!)
	if (JumpDir == EThreadTalesLedgeJumpDirection::Down || FThreadTalesTraversalInputHelper::IsBackInputHeld(PC, CharacterOwner))
	{
		for (EThreadTalesLedgeJumpDistanceType DistType : { EThreadTalesLedgeJumpDistanceType::Short, EThreadTalesLedgeJumpDistanceType::Long, EThreadTalesLedgeJumpDistanceType::Leap })
		{
			FVector LowerLedgeLoc;
			FVector LowerWallNormal;
			if (FThreadTalesLedgeDetector::FindLedgeInDirection(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->IsDrawDebugTracesEnabled(), CachedLedgeLocation, CachedWallNormal, EThreadTalesLedgeJumpDirection::Down, DistType, JumpConfigTable, LowerLedgeLoc, LowerWallNormal))
			{
				TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][HOP_DOWN_TRIGGER] S held. Found lower ledge at %s. Performing HopDown."), *LowerLedgeLoc.ToString());
				ClimbingInputDebounceTimer = OwnerComp->GetJumpInputDebounceDuration();
				PerformLedgeJump(EThreadTalesLedgeJumpDirection::Down, DistType, LowerLedgeLoc, LowerWallNormal, OwnerComp, CharacterOwner);
				return true;
			}
		}

		TT_TRAVERSAL_LOG(OwnerComp, Verbose, TEXT("[TT_TRAVERSAL][DOWN_BLOCKED] S held, but no lower ledge found below. Holding grip. (Drop via Crouch)"));
	}

	// W held -> Traverse upwards along cliff/wall:
	// 1. Hop up to higher ledge if one exists above
	// 2. Mantle up onto top surface if no higher ledge and floor is standable
	if (JumpDir == EThreadTalesLedgeJumpDirection::Up)
	{
		for (EThreadTalesLedgeJumpDistanceType DistType : { EThreadTalesLedgeJumpDistanceType::Short, EThreadTalesLedgeJumpDistanceType::Long, EThreadTalesLedgeJumpDistanceType::Leap })
		{
			FVector UpperLedgeLoc;
			FVector UpperWallNormal;
			if (FThreadTalesLedgeDetector::FindLedgeInDirection(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->IsDrawDebugTracesEnabled(), CachedLedgeLocation, CachedWallNormal, EThreadTalesLedgeJumpDirection::Up, DistType, JumpConfigTable, UpperLedgeLoc, UpperWallNormal))
			{
				TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][HOP_UP_TRIGGER] W held. Found upper ledge at %s. Performing HopUp."), *UpperLedgeLoc.ToString());
				ClimbingInputDebounceTimer = OwnerComp->GetJumpInputDebounceDuration();
				PerformLedgeJump(EThreadTalesLedgeJumpDirection::Up, DistType, UpperLedgeLoc, UpperWallNormal, OwnerComp, CharacterOwner);
				return true;
			}
		}

		FVector OutStandingLoc;
		if (FThreadTalesLedgeDetector::CanClimbUp(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->IsDrawDebugTracesEnabled(), CachedLedgeLocation, CachedWallNormal, OutStandingLoc))
		{
			TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][MANTLE_TRIGGER] W held (at top). CanClimbUp passed! Initiating ClimbUp."));
			ClimbingInputDebounceTimer = OwnerComp->GetJumpInputDebounceDuration();
			ClimbUp(OwnerComp, CharacterOwner);
			return true;
		}
	}

	return false;
}

void FThreadTalesLedgeClimbingHandler::ProcessContinuousShimmy(
	float LateralSign,
	bool bIsDirectJumpPress,
	UThreadTalesTraversalComponent* OwnerComp,
	AThreadTalesCharacter* CharacterOwner)
{
	// Lateral Shimmy (holding A or D without Jump press)
	if (!bIsDirectJumpPress && LateralSign != 0.0f)
	{
		if (ClimbingInputDebounceTimer > 0.0f)
		{
			TT_TRAVERSAL_LOG(OwnerComp, Verbose, TEXT("[TT_TRAVERSAL][SHIMMY_WAIT] Debounce timer active: %.3f s remaining"), ClimbingInputDebounceTimer);
			return;
		}

		TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][SHIMMY_ATTEMPT] LatSign: %.2f | FromLoc: %s | Normal: %s"),
			LateralSign, *CachedLedgeLocation.ToString(), *CachedWallNormal.ToString());

		FVector SideLedgeLoc;
		FVector SideWallNormal;
		bool bIsContinuous = false;
		bool bSideIsFreehang = false;

		if (FThreadTalesLedgeDetector::FindSideLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->IsDrawDebugTracesEnabled(), CachedLedgeLocation, CachedWallNormal, LateralSign, false, SideLedgeLoc, SideWallNormal, bIsContinuous, &bSideIsFreehang))
		{
			TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][SHIMMY_FOUND] Side ledge at %s (Normal: %s, Continuous: %d, Freehang: %d). Calling StartShimmy."),
				*SideLedgeLoc.ToString(), *SideWallNormal.ToString(), bIsContinuous ? 1 : 0, bSideIsFreehang ? 1 : 0);
			ClimbingInputDebounceTimer = OwnerComp->GetShimmyInputDebounceDuration();
			StartShimmy(LateralSign, SideLedgeLoc, SideWallNormal, OwnerComp, CharacterOwner);
			return;
		}

		TT_TRAVERSAL_LOG(OwnerComp, Warning, TEXT("[TT_TRAVERSAL][SHIMMY_NOT_FOUND] FindSideLedge returned FALSE for LatSign=%.2f. Probing Corner..."), LateralSign);
		EThreadTalesCornerType CornerType = EThreadTalesCornerType::None;
		FVector CornerPivotLoc;
		FVector CornerLedgeLoc;
		FVector CornerWallNormal;
		if (FThreadTalesLedgeDetector::ProbeCornerLedge(
				CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->IsDrawDebugTracesEnabled(),
				CachedLedgeLocation, CachedWallNormal,
				LateralSign, CornerType, CornerPivotLoc, CornerLedgeLoc, CornerWallNormal))
		{
			TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][CORNER_FOUND] Found corner type %d at %s. Calling StartCornerTransition."),
				(int32)CornerType, *CornerLedgeLoc.ToString());
			ClimbingInputDebounceTimer = OwnerComp->GetShimmyInputDebounceDuration();
			StartCornerTransition(LateralSign, CornerType, CornerPivotLoc, CornerLedgeLoc, CornerWallNormal, OwnerComp, CharacterOwner);
			return;
		}

		TT_TRAVERSAL_LOG(OwnerComp, Warning, TEXT("[TT_TRAVERSAL][CORNER_NOT_FOUND] No corner found in direction %.2f."), LateralSign);
	}
	else if (LateralSign == 0.0f && ClimbingSubState == EThreadTalesClimbingSubState::Shimmying)
	{
		TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][SHIMMY_RELEASE] Lateral input released. Returning to IdleHang."));
		OwnerComp->SetShimmyDirectionSign(0.0f);
		OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::IdleHang);
		CrossfadeToHangMontage(OwnerComp, CharacterOwner);
		if (!CharacterOwner->HasAuthority())
		{
			OwnerComp->Server_UpdateShimmyContinuous(0.0f, CachedLedgeLocation, CachedWallNormal, bIsFreehang);
		}
		else if (CharacterOwner)
		{
			OwnerComp->Multicast_UpdateShimmyContinuous(0.0f, CachedLedgeLocation, CachedWallNormal, bIsFreehang);
		}
	}
}

void FThreadTalesLedgeClimbingHandler::ProcessClimbingMovement(float DeltaTime, const APlayerController* PC, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!OwnerComp || OwnerComp->GetTraversalState() != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning() || !CharacterOwner || !PC)
	{
		return;
	}

	bool bIsSprint = false;
	const EThreadTalesLedgeJumpDirection JumpDir = FThreadTalesTraversalInputHelper::GetClimbingJumpDirection(PC, bIsSprint, CharacterOwner);
	const bool bIsDirectJumpPress = FThreadTalesTraversalInputHelper::IsJumpPressed(PC, CharacterOwner);
	const float LateralSign = FThreadTalesTraversalInputHelper::GetLateralInputSign(PC, CharacterOwner);

	if (OwnerComp)
	{
		OwnerComp->ResetClimbingMovementInput();
	}

	if (GEngine && CharacterOwner && CharacterOwner->IsLocallyControlled())
	{
		GEngine->AddOnScreenDebugMessage(101, 0.0f, FColor::Yellow,
			FString::Printf(TEXT("[Traversal] State: %d | SubState: %d | Freehang: %d | LatSign: %.2f | Space: %d"),
				(int32)OwnerComp->GetTraversalState(),
				(int32)ClimbingSubState,
				bIsFreehang ? 1 : 0,
				LateralSign,
				bIsDirectJumpPress ? 1 : 0));
	}

	const FVector Up = FVector::UpVector;
	const FVector WallNorm2D = CachedWallNormal.GetSafeNormal();
	const FVector WallRight = FVector::CrossProduct(WallNorm2D, Up).GetSafeNormal();

	if (OwnerComp && OwnerComp->ShouldLogTraversal())
	{
		static float LastLoggedLatSign = 999.0f;
		static bool LastLoggedJump = false;
		static EThreadTalesLedgeJumpDirection LastLoggedDir = EThreadTalesLedgeJumpDirection::None;
		if (LateralSign != LastLoggedLatSign || bIsDirectJumpPress != LastLoggedJump || JumpDir != LastLoggedDir)
		{
			LastLoggedLatSign = LateralSign;
			LastLoggedJump = bIsDirectJumpPress;
			LastLoggedDir = JumpDir;
			UE_LOG(LogThreadTalesTraversal, Log,
				TEXT("[TT_TRAVERSAL][INTENT] Character '%s' | SubState: %d | Freehang: %d | LatSign: %.2f | Space: %d | Sprint: %d | JumpDir: %d | Debounce: %.3f | LedgeLoc: %s"),
				*CharacterOwner->GetName(), (int32)ClimbingSubState, bIsFreehang ? 1 : 0, LateralSign, bIsDirectJumpPress ? 1 : 0, bIsSprint ? 1 : 0, (int32)JumpDir, ClimbingInputDebounceTimer, *CachedLedgeLocation.ToString());
		}
	}

	const bool bIsBackJumpIntent = FThreadTalesTraversalInputHelper::IsBackInputHeld(PC, CharacterOwner, &CachedWallNormal);
	const FVector IntentWorldDir = FThreadTalesTraversalInputHelper::GetClimbingIntentWorldVector(PC, CachedWallNormal, WallRight, bIsBackJumpIntent, CharacterOwner);

	// 1. Evaluate Jump Actions (Space pressed)
	if (bIsDirectJumpPress && ClimbingInputDebounceTimer <= 0.0f)
	{
		if (TryProcessClimbingJump(PC, IntentWorldDir, WallRight, JumpDir, bIsDirectJumpPress, OwnerComp, CharacterOwner))
		{
			return;
		}
	}
	// 2. Evaluate Non-Jump Directional Actions (W/S held without Space)
	else if (!bIsDirectJumpPress && ClimbingInputDebounceTimer <= 0.0f)
	{
		if (TryProcessClimbingDirectional(PC, JumpDir, OwnerComp, CharacterOwner))
		{
			return;
		}
	}

	// 3. Evaluate Lateral Continuous Shimmy
	ProcessContinuousShimmy(LateralSign, bIsDirectJumpPress, OwnerComp, CharacterOwner);
}

void FThreadTalesLedgeClimbingHandler::HandleJumpInput(const APlayerController* PC, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!OwnerComp || OwnerComp->GetTraversalState() != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning() || !CharacterOwner || !PC)
	{
		return;
	}

	// Reset debounce and throttle so immediate Space press is responsive and processes directional jump or mantle
	ClimbingInputDebounceTimer = 0.0f;
	ContinuousScanThrottleTimer = 0.0f;
	ProcessClimbingMovement(0.0f, PC, OwnerComp, CharacterOwner);
}

void FThreadTalesLedgeClimbingHandler::StartLedgeClimb(const FVector& LedgeLocation, const FVector& WallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!CharacterOwner || !OwnerComp)
	{
		return;
	}

	bIsFreehang = FThreadTalesLedgeDetector::CheckIsFreehangLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->IsDrawDebugTracesEnabled(), LedgeLocation, WallNormal);
	OwnerComp->SetIsFreehang(bIsFreehang);
	CachedLedgeLocation = LedgeLocation;
	CachedWallNormal = WallNormal;

	const FRotator FaceWallRotation = (-WallNormal).Rotation();
	CachedHangRotation = FRotator(0.0f, FaceWallRotation.Yaw, 0.0f);
	CachedHangLocation = LedgeLocation + (WallNormal * OwnerComp->GetHangWallOffset()) - FVector(0.0f, 0.0f, OwnerComp->GetHangHeightOffset());

	OwnerComp->SetTraversalState(EThreadTalesTraversalState::LedgeClimbing);

	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		bSavedUseControllerDesiredRotation = MoveComp->bUseControllerDesiredRotation;
		bSavedOrientRotationToMovement = MoveComp->bOrientRotationToMovement;
		bSavedUseControllerRotationYaw = CharacterOwner->bUseControllerRotationYaw;

		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->SetMovementMode(MOVE_Flying);
		MoveComp->bUseControllerDesiredRotation = false;
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->RotationRate = FRotator::ZeroRotator;
		CharacterOwner->bUseControllerRotationYaw = false;
	}

	CharacterOwner->SetStrafeInputDown(false);
	CharacterOwner->SetAimInputDown(false);

	TransitionStartLocation = CharacterOwner->GetActorLocation();
	TransitionStartRotation = CharacterOwner->GetActorRotation();
	TransitionTargetLocation = CachedHangLocation;
	TransitionTargetRotation = CachedHangRotation;
	PendingTargetLedgeLocation = LedgeLocation;
	PendingTargetWallNormal = WallNormal;
	TransitionTimer = 0.0f;

	UAnimMontage* GrabMontage = OwnerComp->GetActiveGrabMontage(bIsFreehang);
	if (GrabMontage)
	{
		const float BlendOut = GrabMontage->BlendOut.GetBlendTime() > 0.0f ? GrabMontage->BlendOut.GetBlendTime() : 0.05f;
		const float BaseGrabPlayRate = bIsFreehang ? OwnerComp->GetBarGrabPlayRate() : OwnerComp->GetGrabPlayRate();
		const float GrabPlayRate = BaseGrabPlayRate * OwnerComp->GetClimbAnimPlayRateMultiplier();
		const float EffectivePlayRate = GrabPlayRate * (GrabMontage->RateScale > 0.0f ? GrabMontage->RateScale : 1.0f);
		TransitionDuration = FMath::Max((GrabMontage->GetPlayLength() - BlendOut) / EffectivePlayRate, 0.15f);
		CharacterOwner->PlayAnimMontage(GrabMontage, GrabPlayRate);
		PhaseTelemetry.Start(GrabMontage, TransitionDuration, GrabPlayRate, EThreadTalesClimbingSubState::GrabCatch);
	}
	else
	{
		PhaseTelemetry.Reset();
		const UThreadTalesClimbingConfig* Config = OwnerComp->ClimbingConfig;
		TransitionDuration = Config ? Config->Kinematics.Timings.GrabCatchDuration : 0.20f;
		CrossfadeToHangMontage(OwnerComp, CharacterOwner);
	}

	OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::GrabCatch);

	if (CharacterOwner->IsLocallyControlled())
	{
		if (!CharacterOwner->HasAuthority())
		{
			OwnerComp->Server_StartLedgeClimb(LedgeLocation, WallNormal);
		}
		else
		{
			OwnerComp->Multicast_StartLedgeClimb(LedgeLocation, WallNormal);
		}
	}
}

void FThreadTalesLedgeClimbingHandler::ReleaseLedge(UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!OwnerComp || OwnerComp->GetTraversalState() != EThreadTalesTraversalState::LedgeClimbing || !CharacterOwner || IsTransitioning())
	{
		return;
	}

	CameraHandler.Reset();
	LogPhaseSummary(OwnerComp, CharacterOwner, true);
	StopAllClimbingMontages(CharacterOwner);
	if (ClimbingSubState == EThreadTalesClimbingSubState::ClimbDown)
	{
		SetClimbDownCollision(false, CharacterOwner);
	}
	OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::None);
	bIsFreehang = false;
	OwnerComp->SetIsFreehang(false);

	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
		MoveComp->bOrientRotationToMovement = bSavedOrientRotationToMovement;
		MoveComp->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
		MoveComp->SetMovementMode(MOVE_Falling);
	}
	CharacterOwner->bUseControllerRotationYaw = bSavedUseControllerRotationYaw;

	OwnerComp->SetTraversalState(EThreadTalesTraversalState::None);
	RegrabCooldownTimer = OwnerComp->GetRegrabCooldownDuration();

	if (!CharacterOwner->HasAuthority())
	{
		OwnerComp->Server_ReleaseLedge();
	}
	else
	{
		OwnerComp->Multicast_ReleaseLedge();
	}
}


void FThreadTalesLedgeClimbingHandler::HopUp(const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!OwnerComp || OwnerComp->GetTraversalState() != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning())
	{
		return;
	}

	OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::JumpTransition);

	bIsFreehang = FThreadTalesLedgeDetector::CheckIsFreehangLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->IsDrawDebugTracesEnabled(), TargetLedgeLoc, TargetWallNormal);
	OwnerComp->SetIsFreehang(bIsFreehang);

	UAnimMontage* Montage = OwnerComp->GetJumpMontage(EThreadTalesLedgeJumpDirection::Up, EThreadTalesLedgeJumpDistanceType::Short, bIsFreehang);
	Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal, OwnerComp, CharacterOwner, -1.0f, EThreadTalesLedgeJumpDistanceType::Short);

	if (CharacterOwner && !CharacterOwner->HasAuthority())
	{
		OwnerComp->Server_HopUp(TargetLedgeLoc, TargetWallNormal);
	}
	else if (CharacterOwner)
	{
		OwnerComp->Multicast_HopUp(TargetLedgeLoc, TargetWallNormal);
	}
}

void FThreadTalesLedgeClimbingHandler::SideJump(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!OwnerComp || OwnerComp->GetTraversalState() != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning())
	{
		return;
	}

	OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::JumpTransition);

	bIsFreehang = FThreadTalesLedgeDetector::CheckIsFreehangLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->IsDrawDebugTracesEnabled(), TargetLedgeLoc, TargetWallNormal);
	OwnerComp->SetIsFreehang(bIsFreehang);

	const EThreadTalesLedgeJumpDirection Dir = (DirectionSign < 0.0f) ? EThreadTalesLedgeJumpDirection::Left : EThreadTalesLedgeJumpDirection::Right;
	UAnimMontage* Montage = OwnerComp->GetJumpMontage(Dir, EThreadTalesLedgeJumpDistanceType::Short, bIsFreehang);
	Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal, OwnerComp, CharacterOwner, -1.0f, EThreadTalesLedgeJumpDistanceType::Short);

	if (CharacterOwner && !CharacterOwner->HasAuthority())
	{
		OwnerComp->Server_SideJump(DirectionSign, TargetLedgeLoc, TargetWallNormal);
	}
	else if (CharacterOwner)
	{
		OwnerComp->Multicast_SideJump(DirectionSign, TargetLedgeLoc, TargetWallNormal);
	}
}

void FThreadTalesLedgeClimbingHandler::PerformLedgeJump(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!OwnerComp || OwnerComp->GetTraversalState() != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning())
	{
		return;
	}

	OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::JumpTransition);

	bIsFreehang = FThreadTalesLedgeDetector::CheckIsFreehangLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->IsDrawDebugTracesEnabled(), TargetLedgeLoc, TargetWallNormal);
	OwnerComp->SetIsFreehang(bIsFreehang);

	UAnimMontage* Montage = OwnerComp->GetJumpMontage(Direction, DistanceType, bIsFreehang);
	Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal, OwnerComp, CharacterOwner, -1.0f, DistanceType);

	if (CharacterOwner && !CharacterOwner->HasAuthority())
	{
		OwnerComp->Server_LedgeJump(Direction, DistanceType, TargetLedgeLoc, TargetWallNormal);
	}
	else if (CharacterOwner)
	{
		OwnerComp->Multicast_LedgeJump(Direction, DistanceType, TargetLedgeLoc, TargetWallNormal);
	}
}

void FThreadTalesLedgeClimbingHandler::UpdateContinuousShimmyTransform(const FVector& NewLedgeLoc, const FVector& NewWallNormal, bool bInFreehang, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!OwnerComp || !CharacterOwner)
	{
		return;
	}

	CachedLedgeLocation = NewLedgeLoc;
	CachedWallNormal = NewWallNormal;
	bIsFreehang = bInFreehang;
	OwnerComp->SetIsFreehang(bIsFreehang);

	const FRotator TargetFaceWallRot = (-CachedWallNormal).Rotation();
	CachedHangRotation = FRotator(0.0f, TargetFaceWallRot.Yaw, 0.0f);
	CachedHangLocation = CachedLedgeLocation + (CachedWallNormal * OwnerComp->GetHangWallOffset()) - FVector(0.0f, 0.0f, OwnerComp->GetHangHeightOffset());

	CharacterOwner->SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation, false, nullptr, ETeleportType::TeleportPhysics);
}

void FThreadTalesLedgeClimbingHandler::StartShimmy(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!OwnerComp || OwnerComp->GetTraversalState() != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning() || !CharacterOwner)
	{
		return;
	}

	OwnerComp->SetShimmyDirectionSign(DirectionSign);
	OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::Shimmying);

	bIsFreehang = FThreadTalesLedgeDetector::CheckIsFreehangLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->IsDrawDebugTracesEnabled(), TargetLedgeLoc, TargetWallNormal);
	OwnerComp->SetIsFreehang(bIsFreehang);

	UAnimMontage* Montage = OwnerComp->GetActiveShimmyMontage(DirectionSign < 0.0f, bIsFreehang);
	TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][SHIMMY_EXECUTE] DirSign=%.2f | Freehang=%d | Montage=%s | TargetLoc=%s"),
		DirectionSign, bIsFreehang ? 1 : 0, Montage ? *Montage->GetName() : TEXT("NULL!"), *TargetLedgeLoc.ToString());

	Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal, OwnerComp, CharacterOwner);

	if (!CharacterOwner->HasAuthority())
	{
		OwnerComp->Server_StartShimmy(DirectionSign, TargetLedgeLoc, TargetWallNormal);
	}
	else
	{
		OwnerComp->Multicast_StartShimmy(DirectionSign, TargetLedgeLoc, TargetWallNormal);
	}
}

void FThreadTalesLedgeClimbingHandler::ClimbUp(UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!OwnerComp || OwnerComp->GetTraversalState() != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning() || !CharacterOwner)
	{
		return;
	}

	OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::MantleUp);

	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->bUseControllerDesiredRotation = false;
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->SetMovementMode(MOVE_Flying);
	}
	CharacterOwner->bUseControllerRotationYaw = false;
	CharacterOwner->SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation, false, nullptr, ETeleportType::TeleportPhysics);

	UAnimMontage* MontageToPlay = OwnerComp->GetActiveClimbOnTopMontage(bIsFreehang);
	TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][MANTLE_EXECUTE] Freehang=%d | Montage=%s | CachedHangLoc=%s"),
		bIsFreehang ? 1 : 0, MontageToPlay ? *MontageToPlay->GetName() : TEXT("NULL!"), *CachedHangLocation.ToString());

	if (MontageToPlay)
	{
		const float Duration = CharacterOwner->PlayAnimMontage(MontageToPlay, OwnerComp->GetClimbAnimPlayRateMultiplier());
		TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][MANTLE_PLAY] PlayAnimMontage returned %.3f s"), Duration);
		if (Duration > 0.0f)
		{
			if (UAnimInstance* AnimInst = CharacterOwner->GetMesh() ? CharacterOwner->GetMesh()->GetAnimInstance() : nullptr)
			{
				FOnMontageBlendingOutStarted BlendingOutDelegate;
				BlendingOutDelegate.BindUObject(OwnerComp, &UThreadTalesTraversalComponent::OnClimbMontageBlendingOut);
				AnimInst->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontageToPlay);

				FOnMontageEnded EndDelegate;
				EndDelegate.BindUObject(OwnerComp, &UThreadTalesTraversalComponent::OnClimbMontageEnded);
				AnimInst->Montage_SetEndDelegate(EndDelegate, MontageToPlay);
			}
		}
		else
		{
			TT_TRAVERSAL_LOG(OwnerComp, Error, TEXT("[TT_TRAVERSAL][MANTLE_FAIL] ClimbOnTop PlayAnimMontage returned 0.0s!"));
			OnClimbMontageEnded(MontageToPlay, false, OwnerComp, CharacterOwner);
		}
	}
	else
	{
		TT_TRAVERSAL_LOG(OwnerComp, Error, TEXT("[TT_TRAVERSAL][MANTLE_FAIL] ClimbOnTop Montage is NULL!"));
		OnClimbMontageEnded(nullptr, false, OwnerComp, CharacterOwner);
	}

	if (!CharacterOwner->HasAuthority())
	{
		OwnerComp->Server_ClimbUp();
	}
	else
	{
		OwnerComp->Multicast_PlayClimbMontage(MontageToPlay);
	}
}

bool FThreadTalesLedgeClimbingHandler::TryClimbDown(UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!OwnerComp || OwnerComp->GetTraversalState() != EThreadTalesTraversalState::None || !CharacterOwner)
	{
		return false;
	}

	UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
	if (!MoveComp || !MoveComp->IsWalking())
	{
		return false;
	}

	FVector LedgeLocation;
	FVector WallNormal;
	bool bFoundFreehang = false;

	if (!FThreadTalesLedgeDetector::FindEdgeForClimbDown(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->IsDrawDebugTracesEnabled(), LedgeLocation, WallNormal, bFoundFreehang))
	{
		return false;
	}

	bIsFreehang = bFoundFreehang;
	OwnerComp->SetIsFreehang(bIsFreehang);
	OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::ClimbDown);
	CachedLedgeLocation = LedgeLocation;
	CachedWallNormal = WallNormal;

	const FRotator FaceWallRotation = (-WallNormal).Rotation();
	CachedHangRotation = FRotator(0.0f, FaceWallRotation.Yaw, 0.0f);
	CachedHangLocation = LedgeLocation + (WallNormal * OwnerComp->GetHangWallOffset()) - FVector(0.0f, 0.0f, OwnerComp->GetHangHeightOffset());

	OwnerComp->SetTraversalState(EThreadTalesTraversalState::LedgeClimbing);
	bSavedUseControllerDesiredRotation = MoveComp->bUseControllerDesiredRotation;
	bSavedOrientRotationToMovement = MoveComp->bOrientRotationToMovement;
	bSavedUseControllerRotationYaw = CharacterOwner->bUseControllerRotationYaw;
	MoveComp->bUseControllerDesiredRotation = false;
	MoveComp->bOrientRotationToMovement = false;
	CharacterOwner->bUseControllerRotationYaw = false;
	MoveComp->Velocity = FVector::ZeroVector;
	MoveComp->SetMovementMode(MOVE_Flying);

	CharacterOwner->SetStrafeInputDown(false);
	CharacterOwner->SetAimInputDown(false);

	SetClimbDownCollision(true, CharacterOwner);

	FVector ClimbDownStartLocation;
	FRotator ClimbDownStartRotation;
	FThreadTalesClimbingKinematics::CalculateClimbDownPreAlignment(LedgeLocation, WallNormal, OwnerComp->ClimbingConfig, ClimbDownStartLocation, ClimbDownStartRotation);
	CharacterOwner->SetActorLocationAndRotation(ClimbDownStartLocation, ClimbDownStartRotation, false, nullptr, ETeleportType::TeleportPhysics);

	UAnimMontage* MontageToPlay = OwnerComp->GetActiveClimbDownMontage(bIsFreehang);
	if (MontageToPlay)
	{
		const float Duration = CharacterOwner->PlayAnimMontage(MontageToPlay, OwnerComp->GetClimbAnimPlayRateMultiplier());
		if (Duration > 0.0f)
		{
			if (UAnimInstance* AnimInst = CharacterOwner->GetMesh() ? CharacterOwner->GetMesh()->GetAnimInstance() : nullptr)
			{
				FOnMontageBlendingOutStarted BlendingOutDelegate;
				BlendingOutDelegate.BindUObject(OwnerComp, &UThreadTalesTraversalComponent::OnClimbDownMontageBlendingOut);
				AnimInst->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontageToPlay);

				FOnMontageEnded EndDelegate;
				EndDelegate.BindUObject(OwnerComp, &UThreadTalesTraversalComponent::OnClimbDownMontageEnded);
				AnimInst->Montage_SetEndDelegate(EndDelegate, MontageToPlay);
			}
		}
		else
		{
			OnClimbDownMontageEnded(MontageToPlay, false, OwnerComp, CharacterOwner);
		}
	}

	if (!CharacterOwner->HasAuthority())
	{
		OwnerComp->Server_ClimbDown(LedgeLocation, WallNormal, bIsFreehang);
	}
	else
	{
		OwnerComp->Multicast_ClimbDown(LedgeLocation, WallNormal, bIsFreehang);
	}

	return true;
}

void FThreadTalesLedgeClimbingHandler::ExecuteOpposingLedgeJump(
	const FVector& TargetLedgeLoc,
	const FVector& TargetWallNormal,
	UThreadTalesTraversalComponent* OwnerComp,
	AThreadTalesCharacter* CharacterOwner)
{
	if (!OwnerComp || OwnerComp->GetTraversalState() != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning() || !CharacterOwner)
	{
		return;
	}

	UE_LOG(LogThreadTales, Log, TEXT("[Climbing] ExecuteOpposingLedgeJump: Jumping to opposing ledge at %s (Normal: %s)"),
		*TargetLedgeLoc.ToString(), *TargetWallNormal.ToString());

	const FVector CurrentLoc = CachedHangLocation.IsNearlyZero() ? CharacterOwner->GetActorLocation() : CachedHangLocation;
	const float DistToOpposing = FVector::Dist(CurrentLoc, TargetLedgeLoc);
	EThreadTalesLedgeJumpDistanceType OpposingDistType = EThreadTalesLedgeJumpDistanceType::Leap;
	if (DistToOpposing < 220.0f)
	{
		OpposingDistType = EThreadTalesLedgeJumpDistanceType::Short;
	}
	else if (DistToOpposing < 340.0f)
	{
		OpposingDistType = EThreadTalesLedgeJumpDistanceType::Long;
	}

	const float HeightDelta = TargetLedgeLoc.Z - CachedLedgeLocation.Z;
	const bool bIsBackJumpUp = (HeightDelta > 50.0f);

	UAnimMontage* LeapMont = nullptr;
	if (bIsBackJumpUp)
	{
		const FVector ActorRight = CharacterOwner ? CharacterOwner->GetActorRightVector() : FVector::CrossProduct(CachedWallNormal.GetSafeNormal2D(), FVector::UpVector).GetSafeNormal();
		const float LateralOffset = FVector::DotProduct(TargetLedgeLoc - CachedLedgeLocation, ActorRight);
		LeapMont = OwnerComp->GetActiveBackJumpUpMontage(bIsFreehang, LateralOffset);
		if (GEngine)
		{
			const bool bIsLeft = (LeapMont && LeapMont->GetName().Contains(TEXT("_L")));
			GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor::Cyan,
				FString::Printf(TEXT("[Parkour] Back Jump Up: %s (Lateral: %.1f cm)"),
					bIsLeft ? TEXT("LEFT (Mirror)") : TEXT("RIGHT (Original)"), LateralOffset));
		}
	}
	else
	{
		LeapMont = OwnerComp->GetActiveBackJumpMontage(bIsFreehang);
	}
	if (!LeapMont)
	{
		LeapMont = OwnerComp->GetJumpMontage(EThreadTalesLedgeJumpDirection::Down, OpposingDistType, bIsFreehang);
	}

	OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::JumpTransition);
	StopAllClimbingMontages(CharacterOwner);

	if (bIsBackJumpUp && CharacterOwner->GetMotionWarpingComponent())
	{
		const FRotator TargetFaceWallRot = (-TargetWallNormal).Rotation();
		const FRotator TargetRot(0.0f, TargetFaceWallRot.Yaw, 0.0f);
		const FVector TargetLoc = TargetLedgeLoc + (TargetWallNormal * OwnerComp->GetHangWallOffset()) - FVector(0.0f, 0.0f, OwnerComp->GetHangHeightOffset());
		CharacterOwner->GetMotionWarpingComponent()->AddOrUpdateWarpTargetFromTransform(FName(TEXT("BackJumpUpTarget")), FTransform(TargetRot, TargetLoc));
	}

	Internal_StartTransition(LeapMont, TargetLedgeLoc, TargetWallNormal, OwnerComp, CharacterOwner, -1.0f, OpposingDistType);

	if (!CharacterOwner->HasAuthority())
	{
		OwnerComp->Server_LedgeJump(EThreadTalesLedgeJumpDirection::Down, OpposingDistType, TargetLedgeLoc, TargetWallNormal);
	}
	else
	{
		OwnerComp->Multicast_LedgeJump(EThreadTalesLedgeJumpDirection::Down, OpposingDistType, TargetLedgeLoc, TargetWallNormal);
	}
}

void FThreadTalesLedgeClimbingHandler::PerformBackJump(UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!OwnerComp || OwnerComp->GetTraversalState() != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning() || !CharacterOwner)
	{
		return;
	}

	UE_LOG(LogThreadTales, Log, TEXT("[Climbing] PerformBackJump: Initiating opposing ledge search for %s"), *CharacterOwner->GetName());

	FVector BestOpposingLedgeLoc = FVector::ZeroVector;
	FVector BestOpposingWallNormal = FVector::ZeroVector;

	const bool bFoundOpposingLedge = FThreadTalesLedgeDetector::FindOpposingLedge(
		CharacterOwner,
		OwnerComp->ClimbingConfig,
		OwnerComp->IsDrawDebugTracesEnabled(),
		CachedLedgeLocation,
		CachedWallNormal,
		BestOpposingLedgeLoc,
		BestOpposingWallNormal,
		EThreadTalesOpposingLedgeFilter::Any
	);

	if (bFoundOpposingLedge)
	{
		ExecuteOpposingLedgeJump(BestOpposingLedgeLoc, BestOpposingWallNormal, OwnerComp, CharacterOwner);
		return;
	}

	// 2. If no opposing ledge is found behind, execute Wall Eject into open air with backwards impulse
	UE_LOG(LogThreadTales, Log, TEXT("[Climbing] PerformBackJump: No opposing ledge found behind %s. Executing Wall Eject into open air!"), *CharacterOwner->GetName());
	WallEject(OwnerComp, CharacterOwner);
}

void FThreadTalesLedgeClimbingHandler::WallEject(UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!OwnerComp || OwnerComp->GetTraversalState() != EThreadTalesTraversalState::LedgeClimbing || !CharacterOwner)
	{
		return;
	}

	CameraHandler.Reset();
	StopAllClimbingMontages(CharacterOwner);
	OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::None);
	bIsFreehang = false;
	OwnerComp->SetIsFreehang(false);

	if (!CharacterOwner->HasAuthority())
	{
		OwnerComp->Server_WallEject();
	}
	else
	{
		OwnerComp->Multicast_WallEject();
	}

	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
		MoveComp->bOrientRotationToMovement = bSavedOrientRotationToMovement;
		MoveComp->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
		MoveComp->SetMovementMode(MOVE_Falling);
		MoveComp->Velocity = FThreadTalesClimbingKinematics::CalculateWallEjectVelocity(CachedWallNormal, OwnerComp->ClimbingConfig);
	}
	CharacterOwner->bUseControllerRotationYaw = bSavedUseControllerRotationYaw;

	// Rotate character immediately to face away from the wall
	const FRotator EjectRotation = FRotator(0.0f, CachedWallNormal.Rotation().Yaw, 0.0f);
	CharacterOwner->SetActorRotation(EjectRotation);

	// Count Wall Eject as initial airborne jump so double jump remains available as the second jump
	CharacterOwner->JumpCurrentCount = 1;

	OwnerComp->SetTraversalState(EThreadTalesTraversalState::None);
	RegrabCooldownTimer = OwnerComp->GetWallEjectRegrabCooldown();
}


const FThreadTalesLedgeJumpConfig& FThreadTalesLedgeClimbingHandler::GetJumpConfig(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const
{
	return JumpConfigTable.GetConfig(Direction, DistanceType);
}

bool FThreadTalesLedgeClimbingHandler::GetJumpDistanceBounds(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, float& OutMinDist, float& OutMaxDist) const
{
	return JumpConfigTable.GetDistanceBounds(Direction, DistanceType, OutMinDist, OutMaxDist);
}

bool FThreadTalesLedgeClimbingHandler::IsLedgeDistanceValid(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, float Distance) const
{
	return JumpConfigTable.IsDistanceValid(Direction, DistanceType, Distance);
}

float FThreadTalesLedgeClimbingHandler::GetNominalJumpDistance(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const
{
	return JumpConfigTable.GetNominalDistance(Direction, DistanceType);
}

UAnimMontage* FThreadTalesLedgeClimbingHandler::GetJumpMontage(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const
{
	return JumpConfigTable.GetJumpMontage(Direction, DistanceType);
}

UAnimMontage* FThreadTalesLedgeClimbingHandler::GetCornerMontage(float DirectionSign, EThreadTalesCornerType CornerType, UThreadTalesTraversalComponent* OwnerComp) const
{
	if (!OwnerComp)
	{
		return nullptr;
	}

	return OwnerComp->GetActiveCornerMontage(DirectionSign < 0.0f, CornerType, bIsFreehang);
}

void FThreadTalesLedgeClimbingHandler::StartCornerTransition(
	float DirectionSign,
	EThreadTalesCornerType CornerType,
	const FVector& CornerPivotLoc,
	const FVector& TargetLedgeLoc,
	const FVector& TargetWallNormal,
	UThreadTalesTraversalComponent* OwnerComp,
	AThreadTalesCharacter* CharacterOwner)
{
	if (!OwnerComp || !CharacterOwner)
	{
		return;
	}

	// Guard: only allowed from IdleHang, not mid-transition
	if (OwnerComp->GetTraversalState() != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning())
	{
		return;
	}

	// Cache corner data before entering transition
	CornerTransitionPivotLocation = CornerPivotLoc;
	ActiveCornerType              = CornerType;
	OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::CornerTransition);

	bIsFreehang = FThreadTalesLedgeDetector::CheckIsFreehangLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->IsDrawDebugTracesEnabled(), TargetLedgeLoc, TargetWallNormal);
	OwnerComp->SetIsFreehang(bIsFreehang);

	// Select montage (dedicated Corner montage, or fallback to Shimmy montage)
	UAnimMontage* CornerMontage = GetCornerMontage(DirectionSign, CornerType, OwnerComp);

	// Corner transition uses the same duration as a Shimmy step (ShortJump duration)
	// Internal_StartTransition with CustomDuration=-1 uses ShimmyCooldownDuration internally via DistanceType::Short
	Internal_StartTransition(
		CornerMontage,
		TargetLedgeLoc,
		TargetWallNormal,
		OwnerComp,
		CharacterOwner,
		-1.0f,
		EThreadTalesLedgeJumpDistanceType::Short
	);

	if (OwnerComp->IsDrawDebugTracesEnabled() && CharacterOwner->GetWorld())
	{
		UWorld* World = CharacterOwner->GetWorld();
		DrawDebugSphere(World, CornerPivotLoc, 8.0f, 12, FColor::Red, false, 2.0f);
		DrawDebugSphere(World, TargetLedgeLoc, 10.0f, 12, FColor::Green, false, 2.0f);
		DrawDebugDirectionalArrow(World, CornerPivotLoc, TargetLedgeLoc, 15.0f, FColor::Cyan, false, 2.0f, 0, 2.0f);
		DrawDebugString(World, CornerPivotLoc + FVector(0, 0, 20.0f),
			CornerType == EThreadTalesCornerType::Outer ? TEXT("OUTER CORNER") : TEXT("INNER CORNER"),
			nullptr, FColor::Cyan, 2.0f, true);
	}

	UE_LOG(LogThreadTales, Log, TEXT("[Traversal] StartCornerTransition: %s corner, DirectionSign=%.0f, Pivot=%s, Target=%s"),
		CornerType == EThreadTalesCornerType::Outer ? TEXT("Outer") : TEXT("Inner"),
		DirectionSign, *CornerPivotLoc.ToString(), *TargetLedgeLoc.ToString());

	// Network: local client sends to server, server multicasts to all
	if (!CharacterOwner->HasAuthority())
	{
		OwnerComp->Server_StartCornerTransition(DirectionSign, CornerType, CornerPivotLoc, TargetLedgeLoc, TargetWallNormal);
	}
	else
	{
		OwnerComp->Multicast_StartCornerTransition(DirectionSign, CornerType, CornerPivotLoc, TargetLedgeLoc, TargetWallNormal);
	}
}
