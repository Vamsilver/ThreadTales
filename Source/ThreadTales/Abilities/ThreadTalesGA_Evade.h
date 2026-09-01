// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreadTalesGameplayAbility.h"
#include "ThreadTalesGA_Evade.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;

UENUM(BlueprintType)
enum class EEvadeDirection : uint8
{
	Forward,
	ForwardRight,
	Right,
	BackwardRight,
	Backward,
	BackwardLeft,
	Left,
	ForwardLeft
};

/**
 * 8-Directional Evade / Dodge Gameplay Ability.
 * Calculates player movement input relative to camera view and plays the corresponding 8-way montage.
 */
UCLASS()
class THREADTALES_API UThreadTalesGA_Evade : public UThreadTalesGameplayAbility
{
	GENERATED_BODY()

public:
	UThreadTalesGA_Evade();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** Calculates the 8-way evade direction based on WASD input vector relative to camera rotation */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Evade")
	EEvadeDirection CalculateEvadeDirection(float& OutAngle) const;

	/** 8-directional Montages */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Montages")
	UAnimMontage* EvadeForwardMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Montages")
	UAnimMontage* EvadeForwardRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Montages")
	UAnimMontage* EvadeRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Montages")
	UAnimMontage* EvadeBackwardRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Montages")
	UAnimMontage* EvadeBackwardMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Montages")
	UAnimMontage* EvadeBackwardLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Montages")
	UAnimMontage* EvadeLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Montages")
	UAnimMontage* EvadeForwardLeftMontage;

	/** Fallback single dodge montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Montages")
	UAnimMontage* DefaultDodgeMontage;

	/** Default percentage of the montage length to check for early cancel (0.0 to 1.0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Timing")
	float DefaultEarlyCancelPercentage = 0.75f;

	/** Optional specific cancel percentage for specific montages. Overrides the default if the montage is added here. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Timing")
	TMap<UAnimMontage*, float> MontageCancelPercentages;

protected:
	UPROPERTY()
	UAbilityTask_PlayMontageAndWait* MontageTask;

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMontageCancelled();

	UFUNCTION()
	void OnCheckInputTimerFinished();

	UAnimMontage* GetMontageForDirection(EEvadeDirection Direction) const;
};
