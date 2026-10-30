// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesLedgeClimbingHandler.h"
#include "ThreadTales.h"
#include "Characters/ThreadTalesTraversalComponent.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesClimbingConfig.h"
#include "Characters/ThreadTalesLedgeDetector.h"
#include "Characters/ThreadTalesClimbingKinematics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "MotionWarpingComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

namespace
{
	void ComputeTransitionDurationAndPlayRate(
		UAnimMontage* MontageToPlay,
		float CustomDuration,
		bool bIsJump,
		bool bIsCorner,
		EThreadTalesLedgeJumpDistanceType DistanceType,
		const UThreadTalesClimbingConfig* Config,
		const UThreadTalesTraversalComponent* OwnerComp,
		float& OutPlayRate,
		float& OutDuration)
	{
		const bool bIsBackJumpUpMontage = MontageToPlay && Config &&
			(MontageToPlay == Config->Montages.Base.ClimbBackJumpUpMontage.Get() ||
			 MontageToPlay == Config->Montages.Base.ClimbBackJumpUpLeftMontage.Get() ||
			 (MontageToPlay->GetName().Contains(TEXT("back"), ESearchCase::IgnoreCase) && MontageToPlay->GetName().Contains(TEXT("up"), ESearchCase::IgnoreCase)) ||
			 MontageToPlay->GetName().Contains(TEXT("Behind_Up"), ESearchCase::IgnoreCase));

		const bool bIsBackJumpMontage = MontageToPlay && Config &&
			(bIsBackJumpUpMontage ||
			 MontageToPlay == Config->Montages.Base.ClimbBackJumpMontage.Get() ||
			 MontageToPlay == Config->Montages.Base.BarClimbBackJumpMontage.Get() ||
			 MontageToPlay->GetName().Contains(TEXT("back"), ESearchCase::IgnoreCase));

		if (bIsBackJumpMontage)
		{
			OutPlayRate = Config ? Config->Kinematics.Offsets.ClimbBackJumpPlayRate : 1.0f;
		}
		else if (bIsJump)
		{
			const float JumpMultiplier = Config ? Config->Kinematics.Offsets.JumpPlayRateMultiplier : 1.0f;
			OutPlayRate = FMath::Max(JumpMultiplier * (OwnerComp ? OwnerComp->GetClimbAnimPlayRateMultiplier() : 1.0f), 0.1f);
		}
		else if (bIsCorner)
		{
			OutPlayRate = 1.0f;
		}
		else
		{
			OutPlayRate = FMath::Max(OwnerComp ? OwnerComp->GetClimbAnimPlayRateMultiplier() : 1.0f, 0.1f);
		}

		if (CustomDuration > 0.0f)
		{
			OutDuration = CustomDuration;
			return;
		}

		const float RateScale = (MontageToPlay && MontageToPlay->RateScale > 0.0f) ? MontageToPlay->RateScale : 1.0f;
		const float EffectivePlayRate = FMath::Max(OutPlayRate * RateScale, 0.05f);

		if (bIsJump)
		{
			if (MontageToPlay)
			{
				const float BlendOutTime = MontageToPlay->BlendOut.GetBlendTime() > 0.0f ? MontageToPlay->BlendOut.GetBlendTime() : 0.05f;
				OutDuration = FMath::Max((MontageToPlay->GetPlayLength() - BlendOutTime) / EffectivePlayRate, 0.1f);
			}
			else
			{
				switch (DistanceType)
				{
				case EThreadTalesLedgeJumpDistanceType::Leap:
					OutDuration = (Config ? Config->Kinematics.Timings.LeapJumpDuration : 1.35f) / EffectivePlayRate;
					break;
				case EThreadTalesLedgeJumpDistanceType::Long:
					OutDuration = (Config ? Config->Kinematics.Timings.LongJumpDuration : 1.15f) / EffectivePlayRate;
					break;
				case EThreadTalesLedgeJumpDistanceType::Short:
				default:
					OutDuration = (Config ? Config->Kinematics.Timings.ShortJumpDuration : 0.95f) / EffectivePlayRate;
					break;
				}
			}
		}
		else if (MontageToPlay)
		{
			const float BlendOutTime = MontageToPlay->BlendOut.GetBlendTime();
			OutDuration = FMath::Max((MontageToPlay->GetPlayLength() - BlendOutTime) / EffectivePlayRate, 0.1f);
		}
		else
		{
			OutDuration = 0.8f / EffectivePlayRate;
		}
	}
}

void FThreadTalesLedgeClimbingHandler::Internal_StartTransition(
	UAnimMontage* MontageToPlay,
	const FVector& TargetLedgeLoc,
	const FVector& TargetWallNormal,
	UThreadTalesTraversalComponent* OwnerComp,
	AThreadTalesCharacter* CharacterOwner,
	float CustomDuration,
	EThreadTalesLedgeJumpDistanceType DistanceType)
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

	bIsFreehang = FThreadTalesLedgeDetector::CheckIsFreehangLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->DebugConfig.bDrawDebugTraces, TargetLedgeLoc, TargetWallNormal);
	OwnerComp->SetIsFreehang(bIsFreehang);

	const bool bIsCorner = (ClimbingSubState == EThreadTalesClimbingSubState::CornerTransition);
	const bool bIsShimmy = (ClimbingSubState == EThreadTalesClimbingSubState::Shimmying);
	const bool bIsJump = !bIsCorner && !bIsShimmy;
	if (!bIsCorner && !bIsShimmy)
	{
		OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::JumpTransition);
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

	const UThreadTalesClimbingConfig* Config = OwnerComp->ClimbingConfig;
	float FinalPlayRate = 1.0f;
	float Duration = 0.8f;
	ComputeTransitionDurationAndPlayRate(MontageToPlay, CustomDuration, bIsJump, bIsCorner, DistanceType, Config, OwnerComp, FinalPlayRate, Duration);
	TransitionDuration = Duration;

	const bool bTargetIsElevated = (TransitionTargetLocation.Z > TransitionStartLocation.Z + 25.0f);
	const bool bIsBackJumpUpMontage = MontageToPlay && Config && bTargetIsElevated &&
		(MontageToPlay == Config->Montages.Base.ClimbBackJumpUpMontage.Get() ||
		 MontageToPlay == Config->Montages.Base.ClimbBackJumpUpLeftMontage.Get() ||
		 (MontageToPlay->GetName().Contains(TEXT("back"), ESearchCase::IgnoreCase) && MontageToPlay->GetName().Contains(TEXT("up"), ESearchCase::IgnoreCase)) ||
		 MontageToPlay->GetName().Contains(TEXT("Behind_Up"), ESearchCase::IgnoreCase));

	const bool bIsBackJumpMontage = MontageToPlay && Config &&
		(bIsBackJumpUpMontage ||
		 MontageToPlay == Config->Montages.Base.ClimbBackJumpMontage.Get() ||
		 MontageToPlay == Config->Montages.Base.BarClimbBackJumpMontage.Get() ||
		 MontageToPlay->GetName().Contains(TEXT("back"), ESearchCase::IgnoreCase));
	bTransitionIsBackJump = bIsBackJumpMontage;
	bTransitionIsBackJumpUp = bIsBackJumpUpMontage;

	constexpr float BackJumpHoldVerticalOffset = 28.0f;
	const FVector HoldLocation = TransitionStartLocation - FVector(0.0f, 0.0f, BackJumpHoldVerticalOffset);
	const bool bIsAnyBackJump = (bIsBackJumpMontage || bIsBackJumpUpMontage);
	const FVector InitialLoc = bIsAnyBackJump ? HoldLocation : TransitionStartLocation;
	CharacterOwner->SetActorLocationAndRotation(InitialLoc, TransitionStartRotation, false, nullptr, ETeleportType::TeleportPhysics);

	if (bIsCorner && CharacterOwner->IsLocallyControlled() && Config && Config->Traversal.Camera.bEnableCornerCameraTurn)
	{
		if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
		{
			CameraHandler.StartCornerTurn(PC, TransitionStartRotation.Yaw, TransitionTargetRotation.Yaw, TransitionDuration, Config);
		}
	}

	if (MontageToPlay)
	{
		if (bIsAnyBackJump && CharacterOwner->GetMotionWarpingComponent())
		{
			const FTransform WarpTransform(TransitionTargetRotation, TransitionTargetLocation);
			CharacterOwner->GetMotionWarpingComponent()->AddOrUpdateWarpTargetFromTransform(FName(TEXT("BackJumpUpTarget")), WarpTransform);

			const FTransform HoldTransform(TransitionStartRotation, HoldLocation);
			CharacterOwner->GetMotionWarpingComponent()->AddOrUpdateWarpTargetFromTransform(FName(TEXT("BackJumpUpHoldTarget")), HoldTransform);

			TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][WARP_TARGET] Registered BackJumpUpTarget at Loc=%s, HoldTarget at Loc=%s (Offset=%.1f)"),
				*TransitionTargetLocation.ToString(), *HoldLocation.ToString(), BackJumpHoldVerticalOffset);
		}
		else if (CharacterOwner->GetMotionWarpingComponent())
		{
			CharacterOwner->GetMotionWarpingComponent()->RemoveWarpTarget(FName(TEXT("BackJumpUpTarget")));
			CharacterOwner->GetMotionWarpingComponent()->RemoveWarpTarget(FName(TEXT("BackJumpUpHoldTarget")));
		}

		const float PlayRes = CharacterOwner->PlayAnimMontage(MontageToPlay, FinalPlayRate);
		float AnticipationEnd = 0.0f;
		float FlightEnd = 1.0f;
		if (bIsJump)
		{
			FThreadTalesJumpConfigTable::GetKinematicPhases(DistanceType, Config, AnticipationEnd, FlightEnd, TransitionDuration, bTransitionIsBackJump, bTransitionIsBackJumpUp);
		}
		PhaseTelemetry.Start(MontageToPlay, TransitionDuration, FinalPlayRate, ClimbingSubState, DistanceType, AnticipationEnd, FlightEnd, bTransitionIsBackJump, bTransitionIsBackJumpUp);
		TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][MONTAGE_PLAY] Montage '%s' PlayRate=%.2f, PlayAnimMontage returned %.3f s (TransitionDuration=%.3f s)"),
			*MontageToPlay->GetName(), FinalPlayRate, PlayRes, TransitionDuration);
		if (PlayRes <= 0.0f)
		{
			TT_TRAVERSAL_LOG(OwnerComp, Error, TEXT("[TT_TRAVERSAL][MONTAGE_FAIL] PlayAnimMontage returned 0.0s for '%s'! Mesh: %s, AnimInst: %s"),
				*MontageToPlay->GetName(),
				CharacterOwner->GetMesh() ? *CharacterOwner->GetMesh()->GetName() : TEXT("NoMesh"),
				(CharacterOwner->GetMesh() && CharacterOwner->GetMesh()->GetAnimInstance()) ? *CharacterOwner->GetMesh()->GetAnimInstance()->GetName() : TEXT("NoAnimInst"));
		}
	}
	else
	{
		PhaseTelemetry.Reset();
		TT_TRAVERSAL_LOG(OwnerComp, Warning, TEXT("[TT_TRAVERSAL][TRANSITION_NO_MONTAGE] Transition started with NULL montage! SubState: %d, Target: %s"),
			(int32)ClimbingSubState, *TransitionTargetLocation.ToString());
	}
}

void FThreadTalesLedgeClimbingHandler::TickTransition(float DeltaTime, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	TransitionTimer += DeltaTime;
	const float NormalizedTime = FMath::Clamp(TransitionTimer / FMath::Max(TransitionDuration, 0.01f), 0.0f, 1.0f);

	UpdatePhaseTelemetry(NormalizedTime, OwnerComp, CharacterOwner);

	FVector NewLoc;
	FRotator NewRot;

	if (ClimbingSubState == EThreadTalesClimbingSubState::GrabCatch)
	{
		const float SnapDuration = FMath::Max(TransitionDuration, 0.05f);
		const float SnapNormalizedTime = FMath::Clamp(TransitionTimer / SnapDuration, 0.0f, 1.0f);
		const float Alpha = FMath::InterpEaseOut(0.0f, 1.0f, SnapNormalizedTime, 2.5f);
		NewLoc = FMath::Lerp(TransitionStartLocation, TransitionTargetLocation, Alpha);
		NewRot = FQuat::Slerp(TransitionStartRotation.Quaternion(), TransitionTargetRotation.Quaternion(), Alpha).Rotator();
	}
	else if (ClimbingSubState == EThreadTalesClimbingSubState::CornerTransition)
	{
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
			NewRot,
			TransitionDuration,
			bTransitionIsBackJump,
			bTransitionIsBackJumpUp
		);
	}

	if (!(bTransitionIsBackJump || bTransitionIsBackJumpUp))
	{
		CharacterOwner->SetActorLocationAndRotation(NewLoc, NewRot, false, nullptr, ETeleportType::TeleportPhysics);
	}
	else
	{
		// Native Motion Warping and Root Motion drive capsule transform during Back Jump flight.
		// If root motion is unexpectedly inactive (e.g. montage failed or interrupted), fallback to kinematic positioning.
		if (!CharacterOwner->IsPlayingRootMotion())
		{
			CharacterOwner->SetActorLocationAndRotation(NewLoc, NewRot, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	if (ClimbingSubState == EThreadTalesClimbingSubState::CornerTransition && CharacterOwner->IsLocallyControlled())
	{
		if (OwnerComp->ClimbingConfig && OwnerComp->ClimbingConfig->Traversal.Camera.bEnableCornerCameraTurn)
		{
			if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
			{
				CameraHandler.Tick(DeltaTime, TransitionTimer, PC, OwnerComp->ClimbingConfig);
			}
		}
	}

	if (NormalizedTime >= 1.0f)
	{
		const bool bWasGrabCatch = (ClimbingSubState == EThreadTalesClimbingSubState::GrabCatch);
		const bool bWasShimmy = (ClimbingSubState == EThreadTalesClimbingSubState::Shimmying);
		CameraHandler.Reset();
		LogPhaseSummary(OwnerComp, CharacterOwner, false);
		OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::IdleHang);
		ActiveCornerType = EThreadTalesCornerType::None;
		CachedLedgeLocation = PendingTargetLedgeLocation;
		CachedWallNormal = PendingTargetWallNormal;
		CachedHangLocation = TransitionTargetLocation;
		CachedHangRotation = TransitionTargetRotation;

		CharacterOwner->SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation, false, nullptr, ETeleportType::TeleportPhysics);

		if (CharacterOwner->GetMotionWarpingComponent())
		{
			CharacterOwner->GetMotionWarpingComponent()->RemoveWarpTarget(FName(TEXT("BackJumpUpTarget")));
			CharacterOwner->GetMotionWarpingComponent()->RemoveWarpTarget(FName(TEXT("BackJumpUpHoldTarget")));
		}

		TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][TRANSITION_DONE] Succeeded! Settled at %s (Normal: %s). New HangLoc: %s"),
			*CachedLedgeLocation.ToString(), *CachedWallNormal.ToString(), *CachedHangLocation.ToString());

		ShimmyCooldownTimer = (bWasGrabCatch || bWasShimmy) ? 0.0f : 0.08f;
		ClimbingInputDebounceTimer = (bWasGrabCatch || bWasShimmy) ? 0.02f : 0.25f;
		CrossfadeToHangMontage(OwnerComp, CharacterOwner);
	}
}

void FThreadTalesLedgeClimbingHandler::CrossfadeToHangMontage(UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!CharacterOwner || !OwnerComp)
	{
		return;
	}

	bIsFreehang = FThreadTalesLedgeDetector::CheckIsFreehangLedge(CharacterOwner, OwnerComp->ClimbingConfig, OwnerComp->DebugConfig.bDrawDebugTraces, CachedLedgeLocation, CachedWallNormal);
	OwnerComp->SetIsFreehang(bIsFreehang);
	OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::IdleHang);

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
	PhaseTelemetry.Reset();

	if (!CharacterOwner)
	{
		return;
	}

	if (CharacterOwner->GetMotionWarpingComponent())
	{
		CharacterOwner->GetMotionWarpingComponent()->RemoveWarpTarget(FName(TEXT("BackJumpUpTarget")));
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

	TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][MONTAGE_ENDED] Montage: %s | Interrupted: %d | SubState: %d"),
		Montage ? *Montage->GetName() : TEXT("None"), bInterrupted ? 1 : 0, (int32)ClimbingSubState);

	if (ClimbingSubState == EThreadTalesClimbingSubState::MantleUp)
	{
		CharacterOwner->ResetJumpState();
		CharacterOwner->StopJumping();
		CharacterOwner->bPressedJump = false;
		CharacterOwner->SetMantleExitDebounce(0.40f);

		OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::None);
		bIsFreehang = false;
		OwnerComp->SetIsFreehang(false);
		OwnerComp->SetTraversalState(EThreadTalesTraversalState::None);
		RegrabCooldownTimer = OwnerComp->GetRegrabCooldownDuration();

		if (!CharacterOwner->HasAuthority())
		{
			OwnerComp->Server_CompleteClimbUp();
		}
		else
		{
			OwnerComp->Multicast_CompleteClimbUp();
		}
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
				MoveComp->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
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

	TT_TRAVERSAL_LOG(OwnerComp, Log, TEXT("[TT_TRAVERSAL][MONTAGE_BLENDOUT] Montage: %s | Interrupted: %d | SubState: %d | Loc: %s"),
		Montage ? *Montage->GetName() : TEXT("None"), bInterrupted ? 1 : 0, (int32)ClimbingSubState, *CharacterOwner->GetActorLocation().ToString());

	if (ClimbingSubState == EThreadTalesClimbingSubState::MantleUp)
	{
		CharacterOwner->ResetJumpState();
		CharacterOwner->StopJumping();
		CharacterOwner->bPressedJump = false;
		CharacterOwner->SetMantleExitDebounce(0.40f);

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

		OwnerComp->SetClimbingSubState(EThreadTalesClimbingSubState::None);
		bIsFreehang = false;
		OwnerComp->SetIsFreehang(false);
		OwnerComp->SetTraversalState(EThreadTalesTraversalState::None);
		RegrabCooldownTimer = OwnerComp->GetRegrabCooldownDuration();

		if (!CharacterOwner->HasAuthority())
		{
			OwnerComp->Server_CompleteClimbUp();
		}
		else
		{
			OwnerComp->Multicast_CompleteClimbUp();
		}
	}
}

void FThreadTalesLedgeClimbingHandler::OnClimbDownMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner)
{
	if (!CharacterOwner || !OwnerComp)
	{
		return;
	}

	if (bInterrupted)
	{
		if (ClimbingSubState == EThreadTalesClimbingSubState::ClimbDown)
		{
			SetClimbDownCollision(false, CharacterOwner);
		}
		return;
	}

	if (ClimbingSubState == EThreadTalesClimbingSubState::ClimbDown)
	{
		SetClimbDownCollision(false, CharacterOwner);
		CharacterOwner->SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation, false, nullptr, ETeleportType::TeleportPhysics);

		if (CharacterOwner->GetMotionWarpingComponent())
		{
			CharacterOwner->GetMotionWarpingComponent()->RemoveWarpTarget(FName(TEXT("BackJumpUpTarget")));
			CharacterOwner->GetMotionWarpingComponent()->RemoveWarpTarget(FName(TEXT("BackJumpUpHoldTarget")));
		}
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

	if (bInterrupted)
	{
		if (ClimbingSubState == EThreadTalesClimbingSubState::ClimbDown)
		{
			SetClimbDownCollision(false, CharacterOwner);
		}
		return;
	}

	if (ClimbingSubState == EThreadTalesClimbingSubState::ClimbDown || ClimbingSubState == EThreadTalesClimbingSubState::IdleHang)
	{
		SetClimbDownCollision(false, CharacterOwner);
		CharacterOwner->SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation, false, nullptr, ETeleportType::TeleportPhysics);

		if (CharacterOwner->GetMotionWarpingComponent())
		{
			CharacterOwner->GetMotionWarpingComponent()->RemoveWarpTarget(FName(TEXT("BackJumpUpTarget")));
			CharacterOwner->GetMotionWarpingComponent()->RemoveWarpTarget(FName(TEXT("BackJumpUpHoldTarget")));
		}
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
