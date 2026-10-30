// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreadTalesGameplayAbility.h"
#include "ThreadTalesGA_Block.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitInputRelease;
class UAbilityTask_WaitDelay;

/**
 * Hold-activated Block (Guard) Gameplay Ability.
 * On activation: enters GASPALS aim/strafe mode, opens 0.25s parry window (TAG_State_Combat_Parrying),
 * grants TAG_State_Combat_Blocking.
 * On RMB release: clears aim mode, ends ability.
 * Damage absorption, parry deflection, and Guard Break are handled in UThreadTalesAttributeSet::PostGameplayEffectExecute.
 */
UCLASS()
class THREADTALES_API UThreadTalesGA_Block : public UThreadTalesGameplayAbility
{
	GENERATED_BODY()

public:
	UThreadTalesGA_Block();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

	/** Duration of the perfect parry window starting on block activation (default: 0.20s, precision Sekiro timing) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Block")
	float ParryWindowDuration = 0.20f;

	/** Guard stance montage (Guard_Start -> Guard loop). Assigned per-weapon in BP. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Block|Montages")
	TObjectPtr<UAnimMontage> GuardMontage;

protected:
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> ParryDelayTask;

	bool bParryActive = false;

	UFUNCTION()
	void OnParryWindowExpired();

	UFUNCTION()
	void OnGuardMontageCompleted();

	UFUNCTION()
	void OnGuardMontageInterrupted();

	UFUNCTION()
	void OnGuardMontageCancelled();

private:
	/** Sets or clears GASPALS aim input on the avatar character */
	void SetAvatarAimInput(bool bAiming) const;
};
