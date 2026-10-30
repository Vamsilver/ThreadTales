// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ThreadTalesEnemyWindupTask.h"
#include "Characters/ThreadTalesBaseCharacter.h"
#include "Characters/ThreadTalesEnemyCharacter.h"
#include "Characters/ThreadTalesCharacter.h"
#include "AI/ThreadTalesAIController.h"
#include "AI/ThreadTalesEnemyConfig.h"
#include "StateTreeExecutionContext.h"

FStateTreeTask_EnemyWindup::FStateTreeTask_EnemyWindup()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreeTask_EnemyWindup::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = Cast<AThreadTalesBaseCharacter>(Context.GetOwner());
	if (!Enemy)
	{
		return EStateTreeRunStatus::Failed;
	}

	AThreadTalesAIController* AICon = Cast<AThreadTalesAIController>(Enemy->GetController());
	if (AICon)
	{
		AICon->StopMovement();
		if (AThreadTalesCharacter* Target = AICon->GetCombatTarget())
		{
			AICon->SetFocus(Target, EAIFocusPriority::Gameplay);
		}
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime = WindupDuration > 0.0f ? WindupDuration : 0.65f;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EnemyWindup::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
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
		AICon->ClearFocus(EAIFocusPriority::Gameplay);
		return EStateTreeRunStatus::Failed;
	}

	const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(Enemy);
	const FThreadTalesEnemyDifficultySettings& Difficulty = EnemyChar ? EnemyChar->GetDifficultySettings() : FThreadTalesEnemyDifficultySettings();

	const float TotalDuration = WindupDuration > 0.0f ? WindupDuration : 0.65f;
	const float Elapsed = TotalDuration - InstanceData.RemainingTime;
	const float ProgressRatio = TotalDuration > 0.0f ? (Elapsed / TotalDuration) : 0.0f;

	// Track target smoothly until commitment threshold is reached
	if (ProgressRatio <= Difficulty.TrackingCommitRatio)
	{
		const FVector ToTarget = (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
		if (!ToTarget.IsNearlyZero())
		{
			const FRotator CurrentRot = Enemy->GetActorRotation();
			const FRotator TargetRot = FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f);
			Enemy->SetActorRotation(FMath::RInterpConstantTo(CurrentRot, TargetRot, DeltaTime, Difficulty.TrackingTurnRate));
		}
	}

	const float MeleeAttackRange = (EnemyChar && EnemyChar->GetEnemyConfig())
		? EnemyChar->GetEnemyConfig()->Ranges.MeleeAttackRange
		: 220.0f;
	const float MaxWindupDist = MeleeAttackRange * 1.8f;

	const float DistSq = FVector::DistSquared2D(Enemy->GetActorLocation(), Target->GetActorLocation());
	if (DistSq > FMath::Square(MaxWindupDist))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.RemainingTime <= 0.0f)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EnemyWindup::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
}
