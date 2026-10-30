// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "ThreadTalesStateTreeTasks.generated.h"

class AThreadTalesDummy;

UENUM(BlueprintType)
enum class ECombatDummyPhaseAction : uint8
{
	IdleWait UMETA(DisplayName = "Idle (Range & Cooldown Wait)"),
	Windup UMETA(DisplayName = "Start Windup"),
	OpenParryWindow UMETA(DisplayName = "Open Parry Window"),
	ExecuteStrike UMETA(DisplayName = "Execute Strike"),
	EndStrike UMETA(DisplayName = "End Strike"),
	FinishRecovery UMETA(DisplayName = "Finish Recovery"),
	StartParryStagger UMETA(DisplayName = "Start Parry Stagger"),
	FinishParryRecovery UMETA(DisplayName = "Finish Parry Recovery"),
	StartPostureBreakStun UMETA(DisplayName = "Start Posture Break Stun")
};

USTRUCT()
struct THREADTALES_API FStateTreeTask_DummyCombatActionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Action")
	float Duration = 0.0f;

	float RemainingTime = 0.0f;
};

/**
 * Native StateTree Task driving modular combat actions on AThreadTalesDummy.
 */
USTRUCT(meta = (DisplayName = "Dummy Combat Action"))
struct THREADTALES_API FStateTreeTask_DummyCombatAction : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_DummyCombatActionInstanceData;

	FStateTreeTask_DummyCombatAction();

	UPROPERTY(EditAnywhere, Category = "Action")
	ECombatDummyPhaseAction PhaseAction = ECombatDummyPhaseAction::Windup;

	UPROPERTY(EditAnywhere, Category = "Action")
	bool bUseDummyConfiguredDuration = true;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

UCLASS()
class THREADTALES_API UThreadTalesStateTreeHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|AI")
	static bool BuildAndCompileDummyStateTree(UStateTree* StateTreeAsset);

	UFUNCTION(BlueprintCallable, Category = "ThreadTales|AI")
	static bool BuildAndCompileEnemyStateTree(UStateTree* StateTreeAsset);

	UFUNCTION(BlueprintCallable, Category = "ThreadTales|AI")
	static bool BuildAndCompileLadybugStateTree(UStateTree* StateTreeAsset);

	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Animation")
	static bool BuildKnockdownPostureBreakMontage();
#endif
};
