// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ThreadTalesEnemyFaceTargetTask.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Characters/ThreadTalesBaseCharacter.h"
#include "Characters/ThreadTalesEnemyCharacter.h"
#include "Characters/ThreadTalesCharacter.h"
#include "AI/ThreadTalesAIController.h"
#include "AI/ThreadTalesEnemyConfig.h"
#include "StateTreeExecutionContext.h"

FStateTreeTask_EnemyFaceTarget::FStateTreeTask_EnemyFaceTarget()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreeTask_EnemyFaceTarget::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = Cast<AThreadTalesBaseCharacter>(Context.GetOwner());
	if (!Enemy)
	{
		return EStateTreeRunStatus::Failed;
	}

	AThreadTalesAIController* AICon = Cast<AThreadTalesAIController>(Enemy->GetController());
	if (!AICon)
	{
		return EStateTreeRunStatus::Failed;
	}

	AICon->StopMovement();

	// Ground combat invariant: enforce MOVE_Walking and kill linear velocity during in-place reorientation
	if (UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement())
	{
		if (MoveComp->MovementMode != MOVE_Walking)
		{
			MoveComp->SetMovementMode(MOVE_Walking);
		}
		MoveComp->Velocity = FVector::ZeroVector;
	}

	AThreadTalesCharacter* Target = AICon->GetCombatTarget();
	if (!Target || !Target->IsAlive())
	{
		Target = AICon->EvaluateBestTarget();
	}
	if (!Target || !Target->IsAlive())
	{
		return EStateTreeRunStatus::Failed;
	}

	AICon->SetFocus(Target, EAIFocusPriority::Gameplay);

	const FVector ToTarget = (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
	if (ToTarget.IsNearlyZero())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	const FRotator CurrentRot = Enemy->GetActorRotation();
	const FRotator TargetRot = FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f);
	const float DeltaYaw = FMath::Abs(FRotator::NormalizeAxis(TargetRot.Yaw - CurrentRot.Yaw));

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const float EffectivePrecision = PrecisionDegrees > 0.0f ? PrecisionDegrees : InstanceData.PrecisionDegrees;
	if (DeltaYaw <= EffectivePrecision)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.RemainingTime = MaxTurnDuration > 0.0f ? MaxTurnDuration : InstanceData.MaxTurnDuration;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EnemyFaceTarget::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime -= DeltaTime;

	AThreadTalesBaseCharacter* Enemy = Cast<AThreadTalesBaseCharacter>(Context.GetOwner());
	if (!Enemy)
	{
		return EStateTreeRunStatus::Failed;
	}

	AThreadTalesAIController* AICon = Cast<AThreadTalesAIController>(Enemy->GetController());
	if (!AICon)
	{
		return EStateTreeRunStatus::Failed;
	}

	AThreadTalesCharacter* Target = AICon->GetCombatTarget();
	if (!Target || !Target->IsAlive())
	{
		return EStateTreeRunStatus::Failed;
	}

	const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(Enemy);
	const FThreadTalesEnemyDifficultySettings& Difficulty = EnemyChar ? EnemyChar->GetDifficultySettings() : FThreadTalesEnemyDifficultySettings();

	const FVector ToTarget = (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
	if (!ToTarget.IsNearlyZero())
	{
		const FRotator CurrentRot = Enemy->GetActorRotation();
		const FRotator TargetRot = FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f);
		const float ConfigTurnRate = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->LeapSettings.SteeringTurnRate : 540.0f;
		const float BaseTurnRate = (Difficulty.TrackingTurnRate > 0.0f ? Difficulty.TrackingTurnRate : 360.0f);
		const float TurnRate = FMath::Max(BaseTurnRate, ConfigTurnRate);
		Enemy->SetActorRotation(FMath::RInterpConstantTo(CurrentRot, TargetRot, DeltaTime, TurnRate));

		const float DeltaYaw = FMath::Abs(FRotator::NormalizeAxis(TargetRot.Yaw - Enemy->GetActorRotation().Yaw));
		const float EffectivePrecision = PrecisionDegrees > 0.0f ? PrecisionDegrees : InstanceData.PrecisionDegrees;
		if (DeltaYaw <= EffectivePrecision)
		{
			return EStateTreeRunStatus::Succeeded;
		}
	}

	if (InstanceData.RemainingTime <= 0.0f)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EnemyFaceTarget::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = Cast<AThreadTalesBaseCharacter>(Context.GetOwner());
	if (Enemy)
	{
		if (AThreadTalesAIController* AICon = Cast<AThreadTalesAIController>(Enemy->GetController()))
		{
			AICon->ClearFocus(EAIFocusPriority::Gameplay);
		}
	}
}
