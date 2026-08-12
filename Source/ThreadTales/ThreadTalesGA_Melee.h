// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreadTalesGameplayAbility.h"
#include "ThreadTalesGA_Melee.generated.h"

/**
 * Base ability for Melee Attacks (Sword)
 */
UCLASS()
class THREADTALES_API UThreadTalesGA_Melee : public UThreadTalesGameplayAbility
{
	GENERATED_BODY()
	
public:
	UThreadTalesGA_Melee();

	// The animation montage to play
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	class UAnimMontage* AttackMontage;

	// Overridden to activate the ability
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UFUNCTION()
	void OnMontageCompleted();
	
	UFUNCTION()
	void OnMontageInterrupted();
	
	UFUNCTION()
	void OnMontageCancelled();
};
