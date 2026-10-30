// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "AI/ThreadTalesEnemyAttackTask.h"
#include "ThreadTalesEnemyStateTreeTasks.generated.h"

class AThreadTalesEnemyCharacter;
class AThreadTalesAIController;
class AThreadTalesCharacter;
class UAnimMontage;

/**
 * Instance data for Enemy Chase Task
 */
USTRUCT()
struct THREADTALES_API FStateTreeTask_EnemyChaseTargetInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Navigation")
	float AcceptanceRadius = 180.0f;
};

/**
 * StateTree Task moving enemy to current combat target using NavMesh
 */
USTRUCT(meta = (DisplayName = "Enemy Chase Target"))
struct THREADTALES_API FStateTreeTask_EnemyChaseTarget : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_EnemyChaseTargetInstanceData;

	FStateTreeTask_EnemyChaseTarget();

	UPROPERTY(EditAnywhere, Category = "Navigation")
	float AcceptanceRadius = 180.0f;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/**
 * Instance data for Enemy Strafe Task
 */
USTRUCT()
struct THREADTALES_API FStateTreeTask_EnemyStrafeAroundTargetInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Strafe")
	float StrafeDuration = 1.4f;

	float RemainingTime = 0.0f;
	int32 StrafeDirection = 1;
};

/**
 * StateTree Task causing enemy to strafe/circle around target while maintaining combat stance
 */
USTRUCT(meta = (DisplayName = "Enemy Strafe Around Target"))
struct THREADTALES_API FStateTreeTask_EnemyStrafeAroundTarget : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_EnemyStrafeAroundTargetInstanceData;

	FStateTreeTask_EnemyStrafeAroundTarget();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};


/**
 * Instance data for Enemy Stagger Task (Parry reaction)
 */
USTRUCT()
struct THREADTALES_API FStateTreeTask_EnemyStaggerInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Reaction")
	float StaggerDuration = 0.85f;

	float RemainingTime = 0.0f;
};

/**
 * StateTree Task causing enemy to stagger upon receiving parry event
 */
USTRUCT(meta = (DisplayName = "Enemy Parry Stagger"))
struct THREADTALES_API FStateTreeTask_EnemyStagger : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_EnemyStaggerInstanceData;

	FStateTreeTask_EnemyStagger();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/**
 * Instance data for Enemy Posture Break Task (Knockdown/Stun)
 */
USTRUCT()
struct THREADTALES_API FStateTreeTask_EnemyPostureBreakInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Reaction")
	float StunDuration = 3.5f;

	float RemainingTime = 0.0f;
};

/**
 * StateTree Task causing enemy to fall/collapse in posture break knockdown
 */
USTRUCT(meta = (DisplayName = "Enemy Posture Break Stun"))
struct THREADTALES_API FStateTreeTask_EnemyPostureBreak : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_EnemyPostureBreakInstanceData;

	FStateTreeTask_EnemyPostureBreak();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/**
 * Instance data for Enemy Disengage Backstep Task
 */
USTRUCT()
struct THREADTALES_API FStateTreeTask_EnemyBackstepInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Disengage")
	float RecoveryDuration = 1.2f;

	float RemainingTime = 0.0f;
	float TotalDuration = 0.0f;
	FVector DisengageDirection = FVector::ZeroVector;
	bool bIsLadybug = false;
};

/**
 * StateTree Task performing backstep evade after combo attack to open recovery window
 */
USTRUCT(meta = (DisplayName = "Enemy Disengage Backstep"))
struct THREADTALES_API FStateTreeTask_EnemyBackstep : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_EnemyBackstepInstanceData;

	FStateTreeTask_EnemyBackstep();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/**
 * Instance data for Enemy Patrol / Wander Task
 */
USTRUCT()
struct THREADTALES_API FStateTreeTask_EnemyPatrolInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Patrol")
	float PatrolRadius = 900.0f;

	UPROPERTY(EditAnywhere, Category = "Patrol")
	float WaitDuration = 3.0f;

	float RemainingWaitTime = 0.0f;
	bool bIsWaitingAtPoint = false;
	FVector CurrentGoalLocation = FVector::ZeroVector;
};

/**
 * StateTree Task for idle area investigation / patrolling random points on NavMesh
 */
USTRUCT(meta = (DisplayName = "Enemy Patrol / Wander"))
struct THREADTALES_API FStateTreeTask_EnemyPatrol : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_EnemyPatrolInstanceData;

	FStateTreeTask_EnemyPatrol();

	UPROPERTY(EditAnywhere, Category = "Patrol")
	float PatrolRadius = 900.0f;

	UPROPERTY(EditAnywhere, Category = "Patrol")
	float PatrolWalkSpeed = 200.0f;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

// ================================================================================
// LADYBUG-SPECIFIC TASKS (Phase 1: Ground Combat)
// ================================================================================

/**
 * Instance data for Ladybug Circle Strafe Task
 */
USTRUCT()
struct THREADTALES_API FStateTreeTask_LadybugCircleStrafeInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Strafe")
	float StrafeDuration = 1.2f;

	UPROPERTY(EditAnywhere, Category = "Strafe")
	float OrbitRadius = 280.0f;

	UPROPERTY(EditAnywhere, Category = "Strafe")
	float TangentOffset = 150.0f;

	UPROPERTY(EditAnywhere, Category = "Strafe")
	float DirectionFlipInterval = 0.6f;

	float RemainingTime = 0.0f;
	float TimeSinceLastFlip = 0.0f;
	int32 StrafeDirection = 1;
};

/**
 * Ladybug continuous orbital strafe around target with periodic direction changes
 */
USTRUCT(meta = (DisplayName = "Ladybug Circle Strafe"))
struct THREADTALES_API FStateTreeTask_LadybugCircleStrafe : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_LadybugCircleStrafeInstanceData;

	FStateTreeTask_LadybugCircleStrafe();

	UPROPERTY(EditAnywhere, Category = "Strafe")
	float StrafeWalkSpeed = 400.0f;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/**
 * Instance data for Ladybug Roll Disengage Task
 */
USTRUCT()
struct THREADTALES_API FStateTreeTask_LadybugRollDisengageInstanceData
{
	GENERATED_BODY()

	float RemainingTime = 0.0f;
	bool bHasRotated = false;
};

/**
 * Ladybug forward roll disengage with 180° turn (no backstep animation available)
 */
USTRUCT(meta = (DisplayName = "Ladybug Roll Disengage"))
struct THREADTALES_API FStateTreeTask_LadybugRollDisengage : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_LadybugRollDisengageInstanceData;

	FStateTreeTask_LadybugRollDisengage();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

