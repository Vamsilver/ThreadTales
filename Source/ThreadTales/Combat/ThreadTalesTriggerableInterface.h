// Copyright ThreadTales. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ThreadTalesTriggerableInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UThreadTalesTriggerableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface implemented by interactive world actors (doors, gates, elevators, traps)
 * that can be triggered or toggled by switches, levers, or buttons.
 */
class THREADTALES_API IThreadTalesTriggerableInterface
{
	GENERATED_BODY()

public:
	/**
	 * Executes activation/toggle logic on this actor.
	 * @param InstigatorActor The actor (player or trigger mechanism) initiating the trigger.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ThreadTales|Interaction|Trigger")
	void ExecuteTrigger(AActor* InstigatorActor);

	/**
	 * Executes deactivation/reset/reverse toggle logic on this actor (e.g. closing a door when lever is switched off).
	 * If not implemented or overridden in Blueprints, default implementation falls back to ExecuteTrigger.
	 * @param InstigatorActor The actor (player or trigger mechanism) initiating the reset.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ThreadTales|Interaction|Trigger")
	void ExecuteResetTrigger(AActor* InstigatorActor);

	/**
	 * Explicitly sets the triggered/active state on this actor (true = active/opened, false = inactive/closed).
	 * @param bActive The target state.
	 * @param InstigatorActor The actor initiating the state change.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ThreadTales|Interaction|Trigger")
	void ExecuteSetTriggerState(bool bActive, AActor* InstigatorActor);
};
