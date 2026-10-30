// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesLedgeClimbingHandler.h"
#include "ThreadTales.h"
#include "Characters/ThreadTalesTraversalComponent.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesClimbingConfig.h"
#include "Characters/ThreadTalesClimbingKinematics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"

namespace
{
	void EvaluateTelemetrySlice(
		int32 SlicePercent,
		float SliceNormalized,
		uint8 SliceBit,
		float KinematicProgress,
		float PhaseDelta,
		float HandProgress,
		float KinematicAlpha,
		float Threshold,
		const FString& MontageName,
		const FString& SubStateName,
		FThreadTalesPhaseTelemetry& PhaseTelemetry,
		UThreadTalesTraversalComponent* OwnerComp)
	{
		if (KinematicProgress >= SliceNormalized && !(PhaseTelemetry.EvaluatedSlices & SliceBit))
		{
			PhaseTelemetry.EvaluatedSlices |= SliceBit;

			if (PhaseDelta > Threshold)
			{
				TT_TRAVERSAL_LOG(OwnerComp, Warning,
					TEXT("[TT_TRAVERSAL][ANIM_LEAD] Slice %d%%: Hands lead kinematics! PhaseDelta=+%.3f (> +%.2f) | HandProgress=%.2f vs KinematicAlpha=%.2f | Montage: %s (Duration=%.3fs, SubState=%s)"),
					SlicePercent, PhaseDelta, Threshold, HandProgress, KinematicAlpha,
					*MontageName, PhaseTelemetry.TransitionDuration, *SubStateName);
			}
			else if (PhaseDelta < -Threshold)
			{
				TT_TRAVERSAL_LOG(OwnerComp, Warning,
					TEXT("[TT_TRAVERSAL][ANIM_LAG] Slice %d%%: Hands lag kinematics! PhaseDelta=%.3f (< -%.2f) | HandProgress=%.2f vs KinematicAlpha=%.2f | Montage: %s (Duration=%.3fs, SubState=%s)"),
					SlicePercent, PhaseDelta, -Threshold, HandProgress, KinematicAlpha,
					*MontageName, PhaseTelemetry.TransitionDuration, *SubStateName);
			}
			else
			{
				TT_TRAVERSAL_LOG(OwnerComp, Verbose,
					TEXT("[TT_TRAVERSAL][PHASE_SLICE] Slice %d%%: Synchronized | PhaseDelta=%+.3f | HandProgress=%.2f vs KinematicAlpha=%.2f | Montage: %s"),
					SlicePercent, PhaseDelta, HandProgress, KinematicAlpha, *MontageName);
			}
		}
	}
}

float FThreadTalesLedgeClimbingHandler::CalculateAnimProgress(const AThreadTalesCharacter* CharacterOwner, const UAnimMontage* Montage, float KinematicProgress) const
{
	if (!CharacterOwner || !Montage)
	{
		return 0.0f;
	}

	const USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();
	const UAnimInstance* AnimInst = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInst)
	{
		return 0.0f;
	}

	const float BlendOutTime = Montage->BlendOut.GetBlendTime() > 0.0f ? Montage->BlendOut.GetBlendTime() : 0.0f;
	const float EffectiveLength = FMath::Max(Montage->GetPlayLength() - BlendOutTime, 0.05f);

	if (AnimInst->Montage_IsActive(Montage))
	{
		const float CurrentPos = AnimInst->Montage_GetPosition(Montage);
		return FMath::Clamp(CurrentPos / EffectiveLength, 0.0f, 1.0f);
	}

	// If the montage is no longer active but kinematic progress is past halfway, the animation completed
	if (KinematicProgress >= 0.60f)
	{
		return 1.0f;
	}

	return 0.0f;
}

void FThreadTalesLedgeClimbingHandler::UpdatePhaseTelemetry(
	float KinematicProgress,
	UThreadTalesTraversalComponent* OwnerComp,
	AThreadTalesCharacter* CharacterOwner)
{
	if (!PhaseTelemetry.IsTracking() || !CharacterOwner || !OwnerComp)
	{
		return;
	}

	const UAnimMontage* Montage = PhaseTelemetry.ActiveMontage.Get();
	USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();
	if (!Mesh)
	{
		return;
	}

	// 1. Physical hand tracking in actor-relative space (RTS_Actor) to eliminate self-referential capsule motion
	const bool bHasLeftSocket = Mesh->DoesSocketExist(TEXT("hand_l"));
	const bool bHasRightSocket = Mesh->DoesSocketExist(TEXT("hand_r"));
	const FVector LeftHand = bHasLeftSocket ? Mesh->GetSocketTransform(TEXT("hand_l"), RTS_Actor).GetLocation() : FVector::ZeroVector;
	const FVector RightHand = bHasRightSocket ? Mesh->GetSocketTransform(TEXT("hand_r"), RTS_Actor).GetLocation() : FVector::ZeroVector;
	const FVector CurrentHandLoc = (LeftHand + RightHand) * 0.5f;

	if (!PhaseTelemetry.bHandsInitialized)
	{
		PhaseTelemetry.InitialHandLocation = CurrentHandLoc;
		PhaseTelemetry.TargetHandLocation = CurrentHandLoc;
		PhaseTelemetry.InitialLeftHandLocation = LeftHand;
		PhaseTelemetry.InitialRightHandLocation = RightHand;
		PhaseTelemetry.bHandsInitialized = true;
	}

	// 2. Measure actual physical progress of hands / animation
	const float AnimProgress = CalculateAnimProgress(CharacterOwner, Montage, KinematicProgress);
	const float LeftHandDisp = FVector::Dist(LeftHand, PhaseTelemetry.InitialLeftHandLocation);
	const float RightHandDisp = FVector::Dist(RightHand, PhaseTelemetry.InitialRightHandLocation);
	const float HandDisplacement = (PhaseTelemetry.DistanceType == EThreadTalesLedgeJumpDistanceType::Short)
		? FMath::Max(LeftHandDisp, RightHandDisp)
		: FVector::Dist(CurrentHandLoc, PhaseTelemetry.InitialHandLocation);

	float TargetDisplacement = 45.0f;
	if (PhaseTelemetry.DistanceType == EThreadTalesLedgeJumpDistanceType::Leap)
	{
		TargetDisplacement = 185.0f;
	}
	else if (PhaseTelemetry.DistanceType == EThreadTalesLedgeJumpDistanceType::Long)
	{
		TargetDisplacement = 90.0f;
	}

	float HandProgress = AnimProgress;
	if (PhaseTelemetry.SubState == EThreadTalesClimbingSubState::JumpTransition)
	{
		const float FlightRange = FMath::Max(PhaseTelemetry.FlightEnd - PhaseTelemetry.AnticipationEnd, 0.01f);
		if (AnimProgress <= PhaseTelemetry.AnticipationEnd)
		{
			HandProgress = 0.0f;
		}
		else if (AnimProgress >= PhaseTelemetry.FlightEnd)
		{
			HandProgress = 1.0f;
		}
		else
		{
			HandProgress = FMath::Clamp((AnimProgress - PhaseTelemetry.AnticipationEnd) / FlightRange, 0.0f, 1.0f);
		}

		if (PhaseTelemetry.ActualHandDetachNormalizedTime < 0.0f && HandDisplacement >= 8.0f)
		{
			PhaseTelemetry.ActualHandDetachNormalizedTime = AnimProgress;
		}

		if (PhaseTelemetry.ActualHandCatchNormalizedTime < 0.0f && AnimProgress >= PhaseTelemetry.FlightEnd)
		{
			PhaseTelemetry.ActualHandCatchNormalizedTime = AnimProgress;
		}
	}

	// 3. Expected kinematic alpha
	float KinematicAlpha = 0.0f;
	if (PhaseTelemetry.SubState == EThreadTalesClimbingSubState::JumpTransition)
	{
		KinematicAlpha = FThreadTalesClimbingKinematics::CalculateFlightAlpha(
			KinematicProgress,
			PhaseTelemetry.DistanceType,
			OwnerComp->GetClimbingConfig(),
			PhaseTelemetry.TransitionDuration,
			PhaseTelemetry.bIsBackJump,
			PhaseTelemetry.bIsBackJumpUp);
	}
	else
	{
		KinematicAlpha = FMath::SmoothStep(0.0f, 1.0f, KinematicProgress);
	}

	// 4. Honest PhaseDelta
	const float PhaseDelta = HandProgress - KinematicAlpha;
	PhaseTelemetry.SampleCount++;
	PhaseTelemetry.SumPhaseDelta += PhaseDelta;
	if (PhaseDelta > PhaseTelemetry.MaxLead)
	{
		PhaseTelemetry.MaxLead = PhaseDelta;
	}
	if (PhaseDelta < -PhaseTelemetry.MaxLag)
	{
		PhaseTelemetry.MaxLag = -PhaseDelta;
	}

	const float Threshold = OwnerComp->GetPhaseDesyncThreshold();
	const FString SubStateName = StaticEnum<EThreadTalesClimbingSubState>()->GetNameStringByValue(static_cast<int64>(PhaseTelemetry.SubState));
	const FString MontageName = Montage ? Montage->GetName() : TEXT("None");

	EvaluateTelemetrySlice(25, 0.25f, 0x01, KinematicProgress, PhaseDelta, HandProgress, KinematicAlpha, Threshold, MontageName, SubStateName, PhaseTelemetry, OwnerComp);
	EvaluateTelemetrySlice(50, 0.50f, 0x02, KinematicProgress, PhaseDelta, HandProgress, KinematicAlpha, Threshold, MontageName, SubStateName, PhaseTelemetry, OwnerComp);
	EvaluateTelemetrySlice(75, 0.75f, 0x04, KinematicProgress, PhaseDelta, HandProgress, KinematicAlpha, Threshold, MontageName, SubStateName, PhaseTelemetry, OwnerComp);
	EvaluateTelemetrySlice(100, 1.00f, 0x08, KinematicProgress, PhaseDelta, HandProgress, KinematicAlpha, Threshold, MontageName, SubStateName, PhaseTelemetry, OwnerComp);
}

void FThreadTalesLedgeClimbingHandler::LogPhaseSummary(
	UThreadTalesTraversalComponent* OwnerComp,
	AThreadTalesCharacter* CharacterOwner,
	bool bInterrupted)
{
	if (!PhaseTelemetry.IsTracking() || !OwnerComp)
	{
		PhaseTelemetry.Reset();
		return;
	}

	const float Threshold = OwnerComp->GetPhaseDesyncThreshold();
	const float AvgDelta = PhaseTelemetry.SampleCount > 0 ? (PhaseTelemetry.SumPhaseDelta / PhaseTelemetry.SampleCount) : 0.0f;
	const bool bDesyncDetected = (PhaseTelemetry.MaxLead > Threshold) || (PhaseTelemetry.MaxLag > Threshold);
	const FString SubStateName = StaticEnum<EThreadTalesClimbingSubState>()->GetNameStringByValue(static_cast<int64>(PhaseTelemetry.SubState));
	const FString MontageName = PhaseTelemetry.ActiveMontage.IsValid() ? PhaseTelemetry.ActiveMontage->GetName() : TEXT("None");
	const FString DistanceName = StaticEnum<EThreadTalesLedgeJumpDistanceType>()->GetNameStringByValue(static_cast<int64>(PhaseTelemetry.DistanceType));

	const float DetachError = (PhaseTelemetry.ActualHandDetachNormalizedTime >= 0.0f) ? (PhaseTelemetry.ActualHandDetachNormalizedTime - PhaseTelemetry.AnticipationEnd) : 0.0f;
	const float CatchError = (PhaseTelemetry.ActualHandCatchNormalizedTime >= 0.0f) ? (PhaseTelemetry.ActualHandCatchNormalizedTime - PhaseTelemetry.FlightEnd) : 0.0f;

	if (PhaseTelemetry.SubState == EThreadTalesClimbingSubState::JumpTransition)
	{
		TT_TRAVERSAL_LOG(OwnerComp, Log,
			TEXT("[TT_TRAVERSAL][PHASE_SUMMARY] %s Jump '%s' (Dist: %s, Dur: %.3fs, PlayRate: %.2f) | Status: %s | MaxLead=%+.3f, MaxLag=-%.3f, AvgDelta=%+.3f | Detach: Act=%.2f vs Cfg=%.2f (Err=%+.2f) | Catch: Act=%.2f vs Cfg=%.2f (Err=%+.2f) | Samples: %d"),
			bInterrupted ? TEXT("INTERRUPTED") : TEXT("Completed"),
			*MontageName,
			*DistanceName,
			PhaseTelemetry.TransitionDuration,
			PhaseTelemetry.PlayRate,
			bDesyncDetected ? TEXT("DESYNC_DETECTED") : TEXT("SYNCHRONIZED"),
			PhaseTelemetry.MaxLead,
			PhaseTelemetry.MaxLag,
			AvgDelta,
			PhaseTelemetry.ActualHandDetachNormalizedTime,
			PhaseTelemetry.AnticipationEnd,
			DetachError,
			PhaseTelemetry.ActualHandCatchNormalizedTime,
			PhaseTelemetry.FlightEnd,
			CatchError,
			PhaseTelemetry.SampleCount);
	}
	else
	{
		TT_TRAVERSAL_LOG(OwnerComp, Log,
			TEXT("[TT_TRAVERSAL][PHASE_SUMMARY] %s '%s' (%s, Dur: %.3fs, PlayRate: %.2f) | Status: %s | MaxLead=%+.3f, MaxLag=-%.3f, AvgDelta=%+.3f | Samples: %d"),
			bInterrupted ? TEXT("INTERRUPTED") : TEXT("Completed"),
			*MontageName,
			*SubStateName,
			PhaseTelemetry.TransitionDuration,
			PhaseTelemetry.PlayRate,
			bDesyncDetected ? TEXT("DESYNC_DETECTED") : TEXT("SYNCHRONIZED"),
			PhaseTelemetry.MaxLead,
			PhaseTelemetry.MaxLag,
			AvgDelta,
			PhaseTelemetry.SampleCount);
	}

	PhaseTelemetry.Reset();
}
