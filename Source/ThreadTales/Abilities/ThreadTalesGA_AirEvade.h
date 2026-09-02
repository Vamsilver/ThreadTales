// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreadTalesGameplayAbility.h"
#include "Abilities/ThreadTalesGA_Evade.h" // For EEvadeDirection
#include "ThreadTalesGA_AirEvade.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_ApplyRootMotionConstantForce;

/**
 * Air Evade / Air Dash Gameplay Ability.
 * Uses physical force/impulse instead of root motion animation.
 */
UCLASS()
class THREADTALES_API UThreadTalesGA_AirEvade : public UThreadTalesGameplayAbility
{
	GENERATED_BODY()

public:
	UThreadTalesGA_AirEvade();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Strength of the air dash */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|AirEvade")
	float DashForce = 1500.f;

	/** How long the dash lasts in the air (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|AirEvade")
	float DashDuration = 0.3f;

	/** Montages for 8 directions (In-Place, NO Root Motion) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|AirEvade|Montages")
	UAnimMontage* EvadeForwardMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|AirEvade|Montages")
	UAnimMontage* EvadeForwardRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|AirEvade|Montages")
	UAnimMontage* EvadeRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|AirEvade|Montages")
	UAnimMontage* EvadeBackwardRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|AirEvade|Montages")
	UAnimMontage* EvadeBackwardMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|AirEvade|Montages")
	UAnimMontage* EvadeBackwardLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|AirEvade|Montages")
	UAnimMontage* EvadeLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|AirEvade|Montages")
	UAnimMontage* EvadeForwardLeftMontage;

	/** Fallback single dodge montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|AirEvade|Montages")
	UAnimMontage* DefaultDodgeMontage;

protected:
	UPROPERTY()
	UAbilityTask_PlayMontageAndWait* MontageTask;

	UPROPERTY()
	UAbilityTask_ApplyRootMotionConstantForce* DashTask;

	UFUNCTION()
	void OnDashFinished();

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMontageCancelled();

	UAnimMontage* GetMontageForDirection(EEvadeDirection Direction) const;
	EEvadeDirection CalculateEvadeDirection(float& OutAngle) const;
};
