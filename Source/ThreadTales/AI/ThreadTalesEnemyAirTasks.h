// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "ThreadTalesEnemyAirTasks.generated.h"

class AThreadTalesEnemyCharacter;
class AThreadTalesAIController;
class AThreadTalesCharacter;
class UAnimMontage;

/**
 * Instance data for Ladybug Takeoff Task
 */
USTRUCT()
struct THREADTALES_API FStateTreeTask_LadybugTakeoffInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Flight")
	float HoverAltitude = 260.0f;

	UPROPERTY(EditAnywhere, Category = "Flight")
	float MaxTakeoffDuration = 1.2f;

	UPROPERTY(EditAnywhere, Category = "Flight")
	float GroundTurnRate = 720.0f;

	float RemainingTime = 0.0f;
	float TargetAltitudeZ = 0.0f;
	float GroundZ = 0.0f;
};

/**
 * StateTree Task transitioning Ladybug into air (MOVE_Flying) and lifting to hover altitude
 */
USTRUCT(meta = (DisplayName = "Ladybug Takeoff"))
struct THREADTALES_API FStateTreeTask_LadybugTakeoff : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_LadybugTakeoffInstanceData;

	FStateTreeTask_LadybugTakeoff();

	UPROPERTY(EditAnywhere, Category = "Flight")
	float HoverAltitude = 260.0f;

	UPROPERTY(EditAnywhere, Category = "Flight")
	float MaxTakeoffDuration = 1.2f;

	UPROPERTY(EditAnywhere, Category = "Flight")
	float GroundTurnRate = 720.0f;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/**
 * Instance data for Ladybug Air Hover Task
 */
USTRUCT()
struct THREADTALES_API FStateTreeTask_LadybugAirHoverInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Flight")
	float HoverDuration = 1.6f;

	float RemainingTime = 0.0f;
	float BaseAltitudeZ = 0.0f;
	float HoverElapsedTime = 0.0f;
};

/**
 * StateTree Task maintaining altitude and tracking player while preparing aerial strike
 */
USTRUCT(meta = (DisplayName = "Ladybug Air Hover"))
struct THREADTALES_API FStateTreeTask_LadybugAirHover : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_LadybugAirHoverInstanceData;

	FStateTreeTask_LadybugAirHover();

	UPROPERTY(EditAnywhere, Category = "Flight")
	float HoverDuration = 1.6f;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/**
 * Instance data for Ladybug Dive Attack Task
 */
USTRUCT()
struct THREADTALES_API FStateTreeTask_LadybugDiveAttackInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Combat")
	float DiveSpeed = 1300.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float MaxDiveDuration = 1.2f;

	float RemainingTime = 0.0f;
	FVector DiveVelocity = FVector::ZeroVector;
	bool bHasLanded = false;
};

/**
 * StateTree Task executing high-speed aerial dive strike and returning to MOVE_Walking upon impact
 */
USTRUCT(meta = (DisplayName = "Ladybug Dive Attack"))
struct THREADTALES_API FStateTreeTask_LadybugDiveAttack : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_LadybugDiveAttackInstanceData;

	FStateTreeTask_LadybugDiveAttack();

	UPROPERTY(EditAnywhere, Category = "Combat")
	float DiveSpeed = 1300.0f;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
