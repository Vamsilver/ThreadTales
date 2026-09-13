// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesLedgeClimbingHandler.h"
#include "ThreadTales.h"
#include "Characters/ThreadTalesTraversalComponent.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesClimbingConfig.h"
#include "Characters/ThreadTalesLedgeDetector.h"
#include "Characters/ThreadTalesTraversalInputHelper.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"
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
	const bool bAuto = Config ? Config->bAutoGrabLedge : true;
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
		}
		CharacterOwner->bUseControllerRotationYaw = false;
	}

	// 1. If currently transitioning between ledges (Hop Up, Side Jump, Shimmy, CornerTransition), interpolate character
	if (OwnerComp->GetTraversalState() == EThreadTalesTraversalState::LedgeClimbing && IsTransitioning())
	{
		TransitionTimer += DeltaTime;
		float NormalizedTime = FMath::Clamp(TransitionTimer / FMath::Max(TransitionDuration, 0.01f), 0.0f, 1.0f);

		FVector NewLoc;
		FRotator NewRot;

		if (ClimbingSubState == EThreadTalesClimbingSubState::GrabCatch)
		{
			const float SnapDuration = FMath::Max(OwnerComp->ClimbingConfig ? OwnerComp->ClimbingConfig->GrabCatchDuration : 0.25f, 0.05f);
			const float SnapNormalizedTime = FMath::Clamp(TransitionTimer / SnapDuration, 0.0f, 1.0f);
			const float Alpha = FMath::InterpEaseOut(0.0f, 1.0f, SnapNormalizedTime, 2.5f);
			NewLoc = FMath::Lerp(TransitionStartLocation, TransitionTargetLocation, Alpha);
			NewRot = FQuat::Slerp(TransitionStartRotation.Quaternion(), TransitionTargetRotation.Quaternion(), Alpha).Rotator();

			// Once body has reached the wall anchor (SnapNormalizedTime >= 1.0), allow responsive early break if player presses input
			if (SnapNormalizedTime >= 1.0f && CharacterOwner->IsLocallyControlled())
			{
				if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
				{
					bool bIsSprint = false;
					const EThreadTalesLedgeJumpDirection JumpDir = FThreadTalesTraversalInputHelper::GetClimbingJumpDirection(PC, bIsSprint);
					const float LateralSign = FThreadTalesTraversalInputHelper::GetLateralInputSign(PC);
					if (JumpDir != EThreadTalesLedgeJumpDirection::None || LateralSign != 0.0f)
					{
						NormalizedTime = 1.0f;
					}
				}
			}
		}
		else if (ClimbingSubState == EThreadTalesClimbingSubState::CornerTransition)
		{
			// Two-segment curved arc: Start → CornerApex → Target (safely rounds corner without cutting geometry)
			FThreadTalesClimbingKinematics::CalculateCornerTransitionTransform(
				NormalizedTime,
				TransitionDuration,
				ActiveCornerType,
				OwnerComp->GetHangWallOffset(),
				OwnerComp->GetHangHeightOffset(),
				TransitionStartLocation,
				CornerTransitionPivotLocation,
				TransitionTargetLocation,
				TransitionStartRotation,
				TransitionTargetRotation,
				NewLoc,
				NewRot
			);
		}
		else
		{
			FThreadTalesClimbingKinematics::CalculateTransitionTransform(
				NormalizedTime,
				ClimbingSubState == EThreadTalesClimbingSubState::JumpTransition,
				TransitionDistanceType,
				OwnerComp->ClimbingConfig,
				TransitionStartLocation,
				TransitionTargetLocation,
				TransitionStartRotation,
				TransitionTargetRotation,
				NewLoc,
				NewRot
			);
		}

		CharacterOwner->SetActorLocationAndRotation(NewLoc, NewRot, false, nullptr, ETeleportType::TeleportPhysics);

		// Synchronously update camera rotation during corner transitions (local player only)
		if (ClimbingSubState == EThreadTalesClimbingSubState::CornerTransition && CharacterOwner->IsLocallyControlled())
		{
			if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
			{
				CameraHandler.Tick(DeltaTime, TransitionTimer, PC, OwnerComp->ClimbingConfig);
			}
		}

		if (NormalizedTime >= 1.0f)
		{
			const bool bWasGrabCatch = (ClimbingSubState == EThreadTalesClimbingSubState::GrabCatch);
			CameraHandler.Reset();
			OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::IdleHang);
			ActiveCornerType = EThreadTalesCornerType::None;
			CachedLedgeLocation = PendingTargetLedgeLocation;
			CachedWallNormal = PendingTargetWallNormal;
			CachedHangLocation = TransitionTargetLocation;
			CachedHangRotation = TransitionTargetRotation;

			CharacterOwner->SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation, false, nullptr, ETeleportType::TeleportPhysics);

			bool bChainedShimmy = false;
			if (!bWasGrabCatch && CharacterOwner->IsLocallyControlled())
			{
				APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
				const float NextDirSign = FThreadTalesTraversalInputHelper::GetLateralInputSign(PC);
				if (NextDirSign != 0.0f)
				{
					FVector NextLedgeLoc;
					FVector NextWallNormal;
					bool bNextContinuous = false;
					bool bNextIsFreehang = false;
					if (FThreadTalesLedgeDetector::FindSideLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->bDrawDebugTraces, CachedLedgeLocation, CachedWallNormal, NextDirSign, false, NextLedgeLoc, NextWallNormal, bNextContinuous, &bNextIsFreehang) && bNextContinuous)
					{
						bChainedShimmy = true;
						bIsFreehang = bNextIsFreehang;
						OwnerComp->SetIsFreehang(bIsFreehang);
						StartShimmy(NextDirSign, NextLedgeLoc, NextWallNormal, OwnerComp, CharacterOwner);
					}
				}
			}

			if (!bChainedShimmy)
			{
				ShimmyCooldownTimer = bWasGrabCatch ? 0.0f : 0.08f;
				ClimbingInputDebounceTimer = bWasGrabCatch ? 0.0f : 0.35f;
				CrossfadeToHangMontage(OwnerComp, CharacterOwner);
			}
		}
		return;
	}

	// 2. While hanging on ledge (IdleHang), hard-anchor and process input
	if (OwnerComp->GetTraversalState() == EThreadTalesTraversalState::LedgeClimbing && ClimbingSubState == EThreadTalesClimbingSubState::IdleHang)
	{
		// Safeguard: Ensure hang montage is active and seamlessly sustained while in IdleHang (prevents AnimGraph pose leaks)
		if (UAnimInstance* AnimInst = CharacterOwner->GetMesh() ? CharacterOwner->GetMesh()->GetAnimInstance() : nullptr)
		{
			UAnimMontage* CurrentMont = AnimInst->GetCurrentActiveMontage();
			UAnimMontage* ActiveHangMont = OwnerComp->GetActiveHangMontage(bIsFreehang);
			if (!CurrentMont || CurrentMont != ActiveHangMont || !AnimInst->Montage_IsPlaying(ActiveHangMont))
			{
				CrossfadeToHangMontage(OwnerComp, CharacterOwner);
			}
		}

		if (CharacterOwner->IsLocallyControlled())
		{
			APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
			ProcessInput(PC, OwnerComp, CharacterOwner);
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

void FThreadTalesLedgeClimbingHandler::ProcessInput(const APlayerController* PC, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (ClimbingInputDebounceTimer > 0.0f || !OwnerComp || OwnerComp->GetTraversalState() != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning() || !CharacterOwner || !PC)
	{
		return;
	}

	bool bIsSprint = false;
	const EThreadTalesLedgeJumpDirection JumpDir = FThreadTalesTraversalInputHelper::GetClimbingJumpDirection(PC, bIsSprint);
	if (JumpDir == EThreadTalesLedgeJumpDirection::None)
	{
		return;
	}

	const FVector Up = FVector::UpVector;
	const FVector WallNorm2D = CachedWallNormal.GetSafeNormal();
	const FVector WallRight = FVector::CrossProduct(WallNorm2D, Up).GetSafeNormal();

	// Lateral continuous movement priority (Shimmy & Corner Wrap when pressing strictly A or D without Sprint)
	if (!bIsSprint && (JumpDir == EThreadTalesLedgeJumpDirection::Right || JumpDir == EThreadTalesLedgeJumpDirection::Left))
	{
		const float SideSign = (JumpDir == EThreadTalesLedgeJumpDirection::Right) ? 1.0f : -1.0f;

		// 1. Regular continuous shimmy along the ledge (holding A or D)
		if (ShimmyCooldownTimer <= 0.0f)
		{
			FVector SideLedgeLoc;
			FVector SideWallNormal;
			bool bIsContinuous = false;
			bool bSideIsFreehang = false;
			if (FThreadTalesLedgeDetector::FindSideLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->bDrawDebugTraces, CachedLedgeLocation, CachedWallNormal, SideSign, false, SideLedgeLoc, SideWallNormal, bIsContinuous, &bSideIsFreehang) && bIsContinuous)
			{
				ClimbingInputDebounceTimer = OwnerComp->GetShimmyInputDebounceDuration();
				bIsFreehang = bSideIsFreehang;
				OwnerComp->SetIsFreehang(bIsFreehang);
				StartShimmy(SideSign, SideLedgeLoc, SideWallNormal, OwnerComp, CharacterOwner);
				return;
			}
		}

		// 2. Corner Wrapping fallback when continuous ledge ends at a wall corner
		if (ShimmyCooldownTimer <= 0.0f)
		{
			EThreadTalesCornerType CornerType = EThreadTalesCornerType::None;
			FVector CornerPivotLoc;
			FVector CornerLedgeLoc;
			FVector CornerWallNormal;
			if (FThreadTalesLedgeDetector::ProbeCornerLedge(
					CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->bDrawDebugTraces,
					CachedLedgeLocation, CachedWallNormal,
					SideSign, CornerType, CornerPivotLoc, CornerLedgeLoc, CornerWallNormal))
			{
				ClimbingInputDebounceTimer = OwnerComp->GetShimmyInputDebounceDuration();
				StartCornerTransition(SideSign, CornerType, CornerPivotLoc, CornerLedgeLoc, CornerWallNormal, OwnerComp, CharacterOwner);
				return;
			}
		}
	}

	// 3D Radial Ledge Scanner: compute 3D world intent vector and probe volumetric cone
	const bool bIsDirectJumpPress = FThreadTalesTraversalInputHelper::IsJumpPressed(PC);
	if (!bIsDirectJumpPress && ContinuousScanThrottleTimer > 0.0f)
	{
		return;
	}

	const FVector IntentWorldDir = FThreadTalesTraversalInputHelper::GetClimbingIntentWorldVector(PC, CachedWallNormal, WallRight, false);

	// Throttle continuous scanning to ~12.5 Hz (0.08s default) while holding stick/keys in IdleHang
	ContinuousScanThrottleTimer = OwnerComp->ClimbingConfig ? OwnerComp->ClimbingConfig->RadialScanContinuousThrottleInterval : 0.08f;

	FThreadTalesLedgeCandidate BestCandidate;
	if (FThreadTalesLedgeDetector::FindBestLedgeInCone(
			CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->bDrawDebugTraces,
			CachedLedgeLocation, CachedWallNormal,
			IntentWorldDir, JumpConfigTable, BestCandidate))
	{
		ClimbingInputDebounceTimer = OwnerComp->GetJumpInputDebounceDuration();
		ContinuousScanThrottleTimer = 0.0f;
		bIsFreehang = BestCandidate.bIsFreehang;
		OwnerComp->SetIsFreehang(bIsFreehang);
		PerformLedgeJump(BestCandidate.Direction, BestCandidate.DistanceType, BestCandidate.LedgeLocation, BestCandidate.WallNormal, OwnerComp, CharacterOwner);
		return;
	}

	// Secondary Fallbacks:
	// A. If Up / Space was pressed, test classic FindLedgeInDirection for upper ledge, then CanClimbUp
	if (JumpDir == EThreadTalesLedgeJumpDirection::Up)
	{
		for (EThreadTalesLedgeJumpDistanceType DistType : { EThreadTalesLedgeJumpDistanceType::Short, EThreadTalesLedgeJumpDistanceType::Long, EThreadTalesLedgeJumpDistanceType::Leap })
		{
			FVector TargetLedgeLoc;
			FVector TargetWallNormal;
			if (FThreadTalesLedgeDetector::FindLedgeInDirection(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->bDrawDebugTraces, CachedLedgeLocation, CachedWallNormal, EThreadTalesLedgeJumpDirection::Up, DistType, JumpConfigTable, TargetLedgeLoc, TargetWallNormal))
			{
				ClimbingInputDebounceTimer = OwnerComp->GetJumpInputDebounceDuration();
				PerformLedgeJump(EThreadTalesLedgeJumpDirection::Up, DistType, TargetLedgeLoc, TargetWallNormal, OwnerComp, CharacterOwner);
				return;
			}
		}

		FVector TargetStandingLoc;
		if (FThreadTalesLedgeDetector::CanClimbUp(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->bDrawDebugTraces, CachedLedgeLocation, CachedWallNormal, TargetStandingLoc))
		{
			ClimbingInputDebounceTimer = OwnerComp->GetJumpInputDebounceDuration();
			ClimbUp(OwnerComp, CharacterOwner);
			return;
		}
	}

	// B. If Down was pressed, test classic FindLedgeInDirection for lower ledge, then BackJump
	if (JumpDir == EThreadTalesLedgeJumpDirection::Down)
	{
		for (EThreadTalesLedgeJumpDistanceType DistType : { EThreadTalesLedgeJumpDistanceType::Short, EThreadTalesLedgeJumpDistanceType::Long, EThreadTalesLedgeJumpDistanceType::Leap })
		{
			FVector TargetLedgeLoc;
			FVector TargetWallNormal;
			if (FThreadTalesLedgeDetector::FindLedgeInDirection(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->bDrawDebugTraces, CachedLedgeLocation, CachedWallNormal, EThreadTalesLedgeJumpDirection::Down, DistType, JumpConfigTable, TargetLedgeLoc, TargetWallNormal))
			{
				ClimbingInputDebounceTimer = OwnerComp->GetJumpInputDebounceDuration();
				PerformLedgeJump(EThreadTalesLedgeJumpDirection::Down, DistType, TargetLedgeLoc, TargetWallNormal, OwnerComp, CharacterOwner);
				return;
			}
		}

		if (bIsSprint)
		{
			ClimbingInputDebounceTimer = OwnerComp->GetJumpInputDebounceDuration();
			PerformBackJump(OwnerComp, CharacterOwner);
			return;
		}
	}

	// C. If Diagonals or Lateral jump across gaps
	if (JumpDir != EThreadTalesLedgeJumpDirection::None && JumpDir != EThreadTalesLedgeJumpDirection::Up && JumpDir != EThreadTalesLedgeJumpDirection::Down)
	{
		const TArray<EThreadTalesLedgeJumpDistanceType> Priorities = FThreadTalesJumpConfigTable::GetDistancePriorities(bIsSprint);
		for (EThreadTalesLedgeJumpDistanceType DistType : Priorities)
		{
			FVector TargetLedgeLoc;
			FVector TargetWallNormal;
			if (FThreadTalesLedgeDetector::FindLedgeInDirection(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->bDrawDebugTraces, CachedLedgeLocation, CachedWallNormal, JumpDir, DistType, JumpConfigTable, TargetLedgeLoc, TargetWallNormal))
			{
				ClimbingInputDebounceTimer = OwnerComp->GetJumpInputDebounceDuration();
				PerformLedgeJump(JumpDir, DistType, TargetLedgeLoc, TargetWallNormal, OwnerComp, CharacterOwner);
				return;
			}
		}
	}

	ClimbingInputDebounceTimer = OwnerComp->GetJumpInputDebounceDuration();
}

void FThreadTalesLedgeClimbingHandler::HandleJumpInput(const APlayerController* PC, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!OwnerComp || OwnerComp->GetTraversalState() != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning() || !CharacterOwner || !PC)
	{
		return;
	}

	if (FThreadTalesTraversalInputHelper::IsBackInputHeld(PC))
	{
		ClimbingInputDebounceTimer = 0.5f;
		PerformBackJump(OwnerComp, CharacterOwner);
		return;
	}

	// Reset debounce and throttle so immediate Space press is responsive and processes directional jump or mantle
	ClimbingInputDebounceTimer = 0.0f;
	ContinuousScanThrottleTimer = 0.0f;
	ProcessInput(PC, OwnerComp, CharacterOwner);
}

void FThreadTalesLedgeClimbingHandler::StartLedgeClimb(const FVector& LedgeLocation, const FVector& WallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!CharacterOwner || !OwnerComp)
	{
		return;
	}

	bIsFreehang = FThreadTalesLedgeDetector::CheckIsFreehangLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->bDrawDebugTraces, LedgeLocation, WallNormal);
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
		CharacterOwner->bUseControllerRotationYaw = false;
	}

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
		const float BlendOut = GrabMontage->BlendOut.GetBlendTime() > 0.0f ? GrabMontage->BlendOut.GetBlendTime() : 0.20f;
		const float ConfigGrabRate = OwnerComp->ClimbingConfig
			? (bIsFreehang ? OwnerComp->ClimbingConfig->BarGrabPlayRate : OwnerComp->ClimbingConfig->GrabPlayRate)
			: (bIsFreehang ? 2.3f : 1.4f);
		const float GrabPlayRate = ConfigGrabRate * OwnerComp->GetClimbAnimPlayRateMultiplier();
		const float EffectivePlayRate = GrabPlayRate * (GrabMontage->RateScale > 0.0f ? GrabMontage->RateScale : 1.0f);
		TransitionDuration = FMath::Max((GrabMontage->GetPlayLength() - BlendOut) / EffectivePlayRate, 0.15f);
		CharacterOwner->PlayAnimMontage(GrabMontage, GrabPlayRate);
	}
	else
	{
		const UThreadTalesClimbingConfig* Config = OwnerComp->ClimbingConfig;
		TransitionDuration = Config ? Config->GrabCatchDuration : 0.20f;
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
	if (!OwnerComp || OwnerComp->GetTraversalState() != EThreadTalesTraversalState::LedgeClimbing || !CharacterOwner)
	{
		return;
	}

	CameraHandler.Reset();
	StopAllClimbingMontages(CharacterOwner);
	if (ClimbingSubState == EThreadTalesClimbingSubState::ClimbDown)
	{
		SetClimbDownCollision(false, CharacterOwner);
	}
	OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::None);

	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
		MoveComp->bOrientRotationToMovement = bSavedOrientRotationToMovement;
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

void FThreadTalesLedgeClimbingHandler::Internal_StartTransition(UAnimMontage* MontageToPlay, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner, float CustomDuration, EThreadTalesLedgeJumpDistanceType DistanceType)
{
	if (!CharacterOwner || !OwnerComp)
	{
		return;
	}

	CameraHandler.Reset();
	TransitionStartLocation = CachedHangLocation.IsNearlyZero() ? CharacterOwner->GetActorLocation() : CachedHangLocation;
	TransitionStartRotation = CachedHangRotation.IsNearlyZero() ? CharacterOwner->GetActorRotation() : CachedHangRotation;

	const FRotator TargetFaceWallRot = (-TargetWallNormal).Rotation();
	TransitionTargetRotation = FRotator(0.0f, TargetFaceWallRot.Yaw, 0.0f);
	TransitionTargetLocation = TargetLedgeLoc + (TargetWallNormal * OwnerComp->GetHangWallOffset()) - FVector(0.0f, 0.0f, OwnerComp->GetHangHeightOffset());

	PendingTargetLedgeLocation = TargetLedgeLoc;
	PendingTargetWallNormal = TargetWallNormal;

	bIsFreehang = FThreadTalesLedgeDetector::CheckIsFreehangLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->bDrawDebugTraces, TargetLedgeLoc, TargetWallNormal);
	OwnerComp->SetIsFreehang(bIsFreehang);

	const bool bIsCorner = (ClimbingSubState == EThreadTalesClimbingSubState::CornerTransition);
	const bool bIsJump = (!bIsCorner && MontageToPlay != OwnerComp->GetActiveShimmyMontage(true, bIsFreehang) && MontageToPlay != OwnerComp->GetActiveShimmyMontage(false, bIsFreehang));
	if (!bIsCorner)
	{
		OwnerComp->SetClimbingSubState(bIsJump ? EThreadTalesClimbingSubState::JumpTransition : EThreadTalesClimbingSubState::Shimmying);
	}
	TransitionDistanceType = DistanceType;
	TransitionTimer = 0.0f;

	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->SetMovementMode(MOVE_Flying);
		MoveComp->bUseControllerDesiredRotation = false;
		MoveComp->bOrientRotationToMovement = false;
	}
	CharacterOwner->bUseControllerRotationYaw = false;
	CharacterOwner->SetActorLocationAndRotation(TransitionStartLocation, TransitionStartRotation, false, nullptr, ETeleportType::TeleportPhysics);

	const UThreadTalesClimbingConfig* Config = OwnerComp->ClimbingConfig;
	float FinalPlayRate = FMath::Max(OwnerComp->GetClimbAnimPlayRateMultiplier(), 0.1f);
	if (MontageToPlay && Config && (MontageToPlay == Config->ClimbBackJumpMontage || MontageToPlay == Config->BarClimbBackJumpMontage))
	{
		FinalPlayRate = Config->ClimbBackJumpPlayRate;
	}
	else if (bIsCorner)
	{
		FinalPlayRate = 1.0f;
	}

	float Duration = CustomDuration;
	if (Duration <= 0.0f)
	{
		if (bIsJump)
		{
			switch (DistanceType)
			{
			case EThreadTalesLedgeJumpDistanceType::Leap:
				Duration = Config ? Config->LeapJumpDuration : 1.00f;
				FinalPlayRate = (MontageToPlay && Config && (MontageToPlay == Config->ClimbBackJumpMontage || MontageToPlay == Config->BarClimbBackJumpMontage))
					? Config->ClimbBackJumpPlayRate
					: 1.0f;
				break;
			case EThreadTalesLedgeJumpDistanceType::Long:
				Duration = Config ? Config->LongJumpDuration : 1.00f;
				FinalPlayRate = 1.0f;
				break;
			case EThreadTalesLedgeJumpDistanceType::Short:
			default:
				Duration = Config ? Config->ShortJumpDuration : 0.95f;
				FinalPlayRate = 1.0f;
				break;
			}

			// Synchronize montage playback speed with the jump duration so that the montage reaches its
			// blend-out trigger point at the exact instant the kinematic transition completes and CrossfadeToHangMontage is called!
			// This prevents ANY gap or leak of the underlying AnimGraph pose.
			if (MontageToPlay && Duration > 0.0f && FinalPlayRate == 1.0f)
			{
				const float BlendOutTime = MontageToPlay->BlendOut.GetBlendTime() > 0.0f ? MontageToPlay->BlendOut.GetBlendTime() : 0.20f;
				const float ActiveDuration = FMath::Max(MontageToPlay->GetPlayLength() - BlendOutTime, 0.1f);
				FinalPlayRate = ActiveDuration / Duration;
			}
		}
		else if (MontageToPlay)
		{
			// For Shimmy and Corner: Crossfade to Hang montage right as the active montage begins blending out,
			// preventing the AnimGraph's underlying locomotion/falling pose from leaking through!
			const float BlendOutTime = MontageToPlay->BlendOut.GetBlendTime();
			Duration = FMath::Max(MontageToPlay->GetPlayLength() - BlendOutTime, 0.1f);
		}
		else
		{
			Duration = 0.8f;
		}
	}

	TransitionDuration = bIsJump ? Duration : (Duration / FinalPlayRate);

	// Start synchronized camera tracking for corner transition (local player only)
	if (bIsCorner && CharacterOwner->IsLocallyControlled())
	{
		if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
		{
			CameraHandler.StartCornerTurn(
				PC,
				TransitionStartRotation.Yaw,
				TransitionTargetRotation.Yaw,
				TransitionDuration,
				OwnerComp->ClimbingConfig
			);
		}
	}

	if (MontageToPlay)
	{
		CharacterOwner->PlayAnimMontage(MontageToPlay, FinalPlayRate);
	}
}

void FThreadTalesLedgeClimbingHandler::HopUp(const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!OwnerComp || OwnerComp->GetTraversalState() != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning())
	{
		return;
	}

	OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::JumpTransition);

	bIsFreehang = FThreadTalesLedgeDetector::CheckIsFreehangLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->bDrawDebugTraces, TargetLedgeLoc, TargetWallNormal);
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

	bIsFreehang = FThreadTalesLedgeDetector::CheckIsFreehangLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->bDrawDebugTraces, TargetLedgeLoc, TargetWallNormal);
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

	bIsFreehang = FThreadTalesLedgeDetector::CheckIsFreehangLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->bDrawDebugTraces, TargetLedgeLoc, TargetWallNormal);
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

void FThreadTalesLedgeClimbingHandler::StartShimmy(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!OwnerComp || OwnerComp->GetTraversalState() != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning())
	{
		return;
	}

	OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::Shimmying);

	bIsFreehang = FThreadTalesLedgeDetector::CheckIsFreehangLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->bDrawDebugTraces, TargetLedgeLoc, TargetWallNormal);
	OwnerComp->SetIsFreehang(bIsFreehang);

	UAnimMontage* Montage = OwnerComp->GetActiveShimmyMontage(DirectionSign < 0.0f, bIsFreehang);
	Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal, OwnerComp, CharacterOwner);

	if (CharacterOwner && !CharacterOwner->HasAuthority())
	{
		OwnerComp->Server_StartShimmy(DirectionSign, TargetLedgeLoc, TargetWallNormal);
	}
	else if (CharacterOwner)
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
	if (MontageToPlay)
	{
		const float Duration = CharacterOwner->PlayAnimMontage(MontageToPlay, OwnerComp->GetClimbAnimPlayRateMultiplier());
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
			OnClimbMontageEnded(MontageToPlay, false, OwnerComp, CharacterOwner);
		}
	}
	else
	{
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

	if (!FThreadTalesLedgeDetector::FindEdgeForClimbDown(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->bDrawDebugTraces, LedgeLocation, WallNormal, bFoundFreehang))
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
		OwnerComp->bDrawDebugTraces,
		CachedLedgeLocation,
		CachedWallNormal,
		BestOpposingLedgeLoc,
		BestOpposingWallNormal
	);

	if (bFoundOpposingLedge)
	{
		UE_LOG(LogThreadTales, Log, TEXT("[Climbing] PerformBackJump: Jumping to opposing ledge at %s (Normal: %s)"),
			*BestOpposingLedgeLoc.ToString(), *BestOpposingWallNormal.ToString());

		UAnimMontage* LeapMont = OwnerComp->GetActiveBackJumpMontage(bIsFreehang);
		if (!LeapMont)
		{
			LeapMont = OwnerComp->GetJumpMontage(EThreadTalesLedgeJumpDirection::Down, EThreadTalesLedgeJumpDistanceType::Leap, bIsFreehang);
		}

		OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::JumpTransition);
		StopAllClimbingMontages(CharacterOwner);
		Internal_StartTransition(LeapMont, BestOpposingLedgeLoc, BestOpposingWallNormal, OwnerComp, CharacterOwner, -1.0f, EThreadTalesLedgeJumpDistanceType::Leap);

		if (!CharacterOwner->HasAuthority())
		{
			OwnerComp->Server_LedgeJump(EThreadTalesLedgeJumpDirection::Down, EThreadTalesLedgeJumpDistanceType::Leap, BestOpposingLedgeLoc, BestOpposingWallNormal);
		}
		else
		{
			OwnerComp->Multicast_LedgeJump(EThreadTalesLedgeJumpDirection::Down, EThreadTalesLedgeJumpDistanceType::Leap, BestOpposingLedgeLoc, BestOpposingWallNormal);
		}
		return;
	}

	// 2. If no opposing ledge is found, perform a standard Wall Eject (kick-off)
	UE_LOG(LogThreadTales, Log, TEXT("[Climbing] PerformBackJump: No opposing ledge found, executing Wall Eject"));
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
		MoveComp->SetMovementMode(MOVE_Falling);
		MoveComp->Velocity = FThreadTalesClimbingKinematics::CalculateWallEjectVelocity(CachedWallNormal, OwnerComp->ClimbingConfig);
	}
	CharacterOwner->bUseControllerRotationYaw = bSavedUseControllerRotationYaw;

	OwnerComp->SetTraversalState(EThreadTalesTraversalState::None);
	RegrabCooldownTimer = OwnerComp->GetWallEjectRegrabCooldown();
}

void FThreadTalesLedgeClimbingHandler::CrossfadeToHangMontage(UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!CharacterOwner || !OwnerComp)
	{
		return;
	}

	// Re-verify freehang state at the settled anchor location to ensure accurate montage selection
	bIsFreehang = FThreadTalesLedgeDetector::CheckIsFreehangLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->bDrawDebugTraces, CachedLedgeLocation, CachedWallNormal);
	OwnerComp->SetIsFreehang(bIsFreehang);

	UAnimMontage* ActiveHangMont = OwnerComp->GetActiveHangMontage(bIsFreehang);
	if (ActiveHangMont)
	{
		if (UAnimInstance* AnimInst = CharacterOwner->GetMesh() ? CharacterOwner->GetMesh()->GetAnimInstance() : nullptr)
		{
			if (AnimInst->GetCurrentActiveMontage() == ActiveHangMont && AnimInst->Montage_IsPlaying(ActiveHangMont))
			{
				return;
			}
		}

		CharacterOwner->PlayAnimMontage(ActiveHangMont);
		if (UAnimInstance* AnimInst = CharacterOwner->GetMesh() ? CharacterOwner->GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInst->Montage_SetNextSection(FName("Default"), FName("Default"), ActiveHangMont);
		}
	}
}

void FThreadTalesLedgeClimbingHandler::StopAllClimbingMontages(AThreadTalesCharacter* CharacterOwner)
{
	if (!CharacterOwner)
	{
		return;
	}

	if (UAnimInstance* AnimInst = CharacterOwner->GetMesh() ? CharacterOwner->GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInst->StopAllMontages(0.15f);
	}
}

void FThreadTalesLedgeClimbingHandler::SetClimbDownCollision(bool bEnable, AThreadTalesCharacter* CharacterOwner)
{
	if (!CharacterOwner)
	{
		return;
	}

	if (UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent())
	{
		const ECollisionResponse Response = bEnable ? ECR_Ignore : ECR_Block;
		Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, Response);
		Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, Response);
	}
}

void FThreadTalesLedgeClimbingHandler::OnClimbMontageEnded(UAnimMontage* Montage, bool bInterrupted, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!CharacterOwner || !OwnerComp)
	{
		return;
	}

	if (ClimbingSubState == EThreadTalesClimbingSubState::MantleUp)
	{
		OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::None);
		bIsFreehang = false;
		OwnerComp->SetIsFreehang(false);
		OwnerComp->SetTraversalState(EThreadTalesTraversalState::None);
		RegrabCooldownTimer = OwnerComp->GetRegrabCooldownDuration();
	}

	if (OwnerComp->GetTraversalState() == EThreadTalesTraversalState::None && ClimbingSubState != EThreadTalesClimbingSubState::ClimbDown)
	{
		if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
		{
			if (MoveComp->MovementMode != MOVE_Walking)
			{
				MoveComp->Velocity = FVector::ZeroVector;
				MoveComp->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
				MoveComp->bOrientRotationToMovement = bSavedOrientRotationToMovement;
				CharacterOwner->bUseControllerRotationYaw = bSavedUseControllerRotationYaw;
				MoveComp->SetMovementMode(MOVE_Walking);
			}
		}
	}
}

void FThreadTalesLedgeClimbingHandler::OnClimbMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!CharacterOwner || !OwnerComp)
	{
		return;
	}

	if (ClimbingSubState == EThreadTalesClimbingSubState::MantleUp)
	{
		if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
		{
			UWorld* World = CharacterOwner->GetWorld();
			UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
			if (World && Capsule)
			{
				const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
				const FVector CapsuleLoc = CharacterOwner->GetActorLocation();

				FHitResult FloorHit;
				FCollisionQueryParams FloorParams;
				FloorParams.AddIgnoredActor(CharacterOwner);

				const FVector FloorTraceStart = FVector(CapsuleLoc.X, CapsuleLoc.Y, CapsuleLoc.Z + 20.0f);
				const FVector FloorTraceEnd = FVector(CapsuleLoc.X, CapsuleLoc.Y, CapsuleLoc.Z - HalfHeight - 60.0f);

				if (World->LineTraceSingleByChannel(FloorHit, FloorTraceStart, FloorTraceEnd, ECC_Visibility, FloorParams))
				{
					const FVector TargetFloorLoc = FVector(CapsuleLoc.X, CapsuleLoc.Y, FloorHit.ImpactPoint.Z + HalfHeight + 1.0f);
					CharacterOwner->SetActorLocation(TargetFloorLoc, false, nullptr, ETeleportType::TeleportPhysics);
				}
				else
				{
					const float Radius = Capsule->GetScaledCapsuleRadius();
					const FVector FallbackXY = CachedLedgeLocation - (CachedWallNormal * (Radius + 15.0f));
					const FVector FallbackLoc = FVector(FallbackXY.X, FallbackXY.Y, CachedLedgeLocation.Z + HalfHeight + 1.0f);
					CharacterOwner->SetActorLocation(FallbackLoc, false, nullptr, ETeleportType::TeleportPhysics);
				}
			}

			MoveComp->Velocity = FVector::ZeroVector;
			MoveComp->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
			MoveComp->bOrientRotationToMovement = bSavedOrientRotationToMovement;
			CharacterOwner->bUseControllerRotationYaw = bSavedUseControllerRotationYaw;
			MoveComp->SetMovementMode(MOVE_Walking);
		}
	}
}

void FThreadTalesLedgeClimbingHandler::OnClimbDownMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!CharacterOwner || !OwnerComp)
	{
		return;
	}

	if (ClimbingSubState == EThreadTalesClimbingSubState::ClimbDown)
	{
		SetClimbDownCollision(false, CharacterOwner);
		CharacterOwner->SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation, false, nullptr, ETeleportType::TeleportPhysics);
		OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::IdleHang);

		if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
		{
			MoveComp->Velocity = FVector::ZeroVector;
			MoveComp->SetMovementMode(MOVE_Flying);
			MoveComp->bUseControllerDesiredRotation = false;
			MoveComp->bOrientRotationToMovement = false;
		}

		CrossfadeToHangMontage(OwnerComp, CharacterOwner);
	}
}

void FThreadTalesLedgeClimbingHandler::OnClimbDownMontageEnded(UAnimMontage* Montage, bool bInterrupted, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!CharacterOwner || !OwnerComp)
	{
		return;
	}

	if (ClimbingSubState == EThreadTalesClimbingSubState::ClimbDown || ClimbingSubState == EThreadTalesClimbingSubState::IdleHang)
	{
		SetClimbDownCollision(false, CharacterOwner);
		CharacterOwner->SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation, false, nullptr, ETeleportType::TeleportPhysics);
		OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::IdleHang);

		if (OwnerComp->GetTraversalState() == EThreadTalesTraversalState::LedgeClimbing)
		{
			if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
			{
				MoveComp->Velocity = FVector::ZeroVector;
				MoveComp->SetMovementMode(MOVE_Flying);
				MoveComp->bUseControllerDesiredRotation = false;
				MoveComp->bOrientRotationToMovement = false;
			}
		}

		UE_LOG(LogThreadTales, Warning, TEXT("[Traversal] ClimbDown Montage Ended: Smoothly settled into hang state (Freehang: %d)."), bIsFreehang);
	}
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

	bIsFreehang = FThreadTalesLedgeDetector::CheckIsFreehangLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->bDrawDebugTraces, TargetLedgeLoc, TargetWallNormal);
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
