// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "ThreadTalesEnemyFaceTargetTask.generated.h"

class AThreadTalesEnemyCharacter;
class AThreadTalesAIController;
class AThreadTalesCharacter;

/**
 * Instance data for Enemy Face Target Task
 */
USTRUCT()
struct THREADTALES_API FStateTreeTask_EnemyFaceTargetInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Combat")
	float PrecisionDegrees = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float MaxTurnDuration = 0.45f;

	float RemainingTime = 0.0f;
};

/**
 * StateTree Task causing enemy to smoothly orient toward combat target before telegraph/attack
 */
USTRUCT(meta = (DisplayName = "Enemy Face Target"))
struct THREADTALES_API FStateTreeTask_EnemyFaceTarget : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_EnemyFaceTargetInstanceData;

	FStateTreeTask_EnemyFaceTarget();

	UPROPERTY(EditAnywhere, Category = "Combat")
	float PrecisionDegrees = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float MaxTurnDuration = 0.45f;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
