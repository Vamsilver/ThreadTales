// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "ThreadTalesEnemyWindupTask.generated.h"

class AThreadTalesEnemyCharacter;
class AThreadTalesAIController;
class AThreadTalesCharacter;

/**
 * Instance data for Enemy Windup/Telegraph Task
 */
USTRUCT()
struct THREADTALES_API FStateTreeTask_EnemyWindupInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Combat")
	float WindupDuration = 0.65f;

	float RemainingTime = 0.0f;
};

/**
 * StateTree Task orienting toward target and holding combat telegraph posture before attack strike
 */
USTRUCT(meta = (DisplayName = "Enemy Windup Telegraph"))
struct THREADTALES_API FStateTreeTask_EnemyWindup : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_EnemyWindupInstanceData;

	FStateTreeTask_EnemyWindup();

	UPROPERTY(EditAnywhere, Category = "Combat")
	float WindupDuration = 0.65f;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
