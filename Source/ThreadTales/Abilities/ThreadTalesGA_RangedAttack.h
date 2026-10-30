// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreadTalesGameplayAbility.h"
#include "ThreadTalesGA_RangedAttack.generated.h"

class AThreadTalesWeapon_Slingshot;
class UAbilityTask_WaitInputRelease;

/**
 * Gameplay Ability for Ranged Weapon Attacks (Slingshot Charge & Release).
 * Uses standard GAS WaitInputRelease task to handle charge and release mechanics.
 */
UCLASS()
class THREADTALES_API UThreadTalesGA_RangedAttack : public UThreadTalesGameplayAbility
{
	GENERATED_BODY()
	
public:
	UThreadTalesGA_RangedAttack();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Releases charge and fires the projectile */
	UFUNCTION(BlueprintCallable, Category = "Abilities|Ranged")
	void ReleaseAndFire();

	/** Gets current charge progress (0.0 .. 1.0) for UI */
	UFUNCTION(BlueprintPure, Category = "Abilities|Ranged")
	float GetCurrentChargeRatio() const;

protected:
	UPROPERTY(Transient)
	TObjectPtr<AThreadTalesWeapon_Slingshot> ActiveSlingshot;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputRelease> WaitInputReleaseTask;

	UFUNCTION()
	void OnInputReleasedCallback(float TimeHeld);

	float ChargeStartTime;
	bool bIsCharging;
};
