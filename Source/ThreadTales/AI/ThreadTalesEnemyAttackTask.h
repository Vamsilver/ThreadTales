// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "ThreadTalesEnemyAttackTask.generated.h"

class AThreadTalesEnemyCharacter;
class AThreadTalesAIController;
class AThreadTalesCharacter;
class UAnimMontage;

/**
 * Instance data for Enemy Melee Attack Combo Task
 */
USTRUCT()
struct THREADTALES_API FStateTreeTask_EnemyMeleeComboInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Combat")
	float MaxComboTimeout = 3.0f;

	float RemainingTime = 0.0f;
	float CurrentMontageDuration = 0.0f;
	int32 CurrentStrikeIndex = 0;
};

/**
 * StateTree Task triggering and driving enemy melee combo attacks with dynamic chaining
 */
USTRUCT(meta = (DisplayName = "Enemy Melee Combo"))
struct THREADTALES_API FStateTreeTask_EnemyMeleeCombo : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_EnemyMeleeComboInstanceData;

	FStateTreeTask_EnemyMeleeCombo();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
