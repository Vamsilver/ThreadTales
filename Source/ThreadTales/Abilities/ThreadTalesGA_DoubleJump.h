// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreadTalesGameplayAbility.h"
#include "ThreadTalesGA_DoubleJump.generated.h"

class UAnimMontage;
class USoundBase;
class UNiagaraSystem;
class UAbilityTask_PlayMontageAndWait;

/**
 * Double Jump Gameplay Ability.
 * Gives vertical air impulse, spawns optional VFX/Sound, and plays an in-air AnimMontage.
 */
UCLASS()
class THREADTALES_API UThreadTalesGA_DoubleJump : public UThreadTalesGameplayAbility
{
	GENERATED_BODY()

public:
	UThreadTalesGA_DoubleJump();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** Vertical velocity (impulse force) for the double jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|DoubleJump")
	float DoubleJumpZVelocity;

	/** AnimMontage to play on double jump (e.g. AM_Running_Forward_Flip) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|DoubleJump")
	TObjectPtr<UAnimMontage> DoubleJumpMontage;

	/** Optional Sound to play on double jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|DoubleJump")
	TObjectPtr<USoundBase> DoubleJumpSound;

	/** Optional VFX / Niagara system to spawn under character feet on double jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|DoubleJump")
	TObjectPtr<UNiagaraSystem> DoubleJumpEffect;

protected:
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMontageCancelled();
};
