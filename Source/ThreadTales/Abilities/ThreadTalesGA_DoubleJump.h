// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreadTalesGameplayAbility.h"
#include "ThreadTalesGA_DoubleJump.generated.h"

class UAnimMontage;
class USoundBase;
class UNiagaraSystem;
class UAbilityTask_PlayMontageAndWait;

UENUM(BlueprintType)
enum class EDoubleJumpDirection : uint8
{
	Neutral     UMETA(DisplayName = "Neutral / In-Place"),
	Forward     UMETA(DisplayName = "Forward"),
	Backward    UMETA(DisplayName = "Backward"),
	Left        UMETA(DisplayName = "Left"),
	Right       UMETA(DisplayName = "Right")
};

/**
 * Directional Double Jump Gameplay Ability.
 * Gives vertical air impulse, spawns optional VFX/Sound, calculates direction relative to camera,
 * and plays the corresponding in-air AnimMontage (Forward flip, Backflip, Left/Right spin, or Neutral).
 */
UCLASS()
class THREADTALES_API UThreadTalesGA_DoubleJump : public UThreadTalesGameplayAbility
{
	GENERATED_BODY()

public:
	UThreadTalesGA_DoubleJump();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** Calculates double jump direction based on player input relative to camera view */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|DoubleJump")
	EDoubleJumpDirection CalculateDoubleJumpDirection(float& OutAngle) const;

	/** Helper to select montage based on direction */
	UAnimMontage* GetMontageForDirection(EDoubleJumpDirection Direction) const;

	/** Vertical velocity (impulse force) for the double jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|DoubleJump")
	float DoubleJumpZVelocity;

	/** Horizontal push strength for directional double jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|DoubleJump")
	float DoubleJumpHorizontalVelocity = 800.0f;

	/** Neutral / In-Place double jump montage (when no WASD input) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|DoubleJump|Montages")
	TObjectPtr<UAnimMontage> DoubleJumpNeutralMontage;

	/** Forward double jump montage (W) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|DoubleJump|Montages")
	TObjectPtr<UAnimMontage> DoubleJumpForwardMontage;

	/** Backward double jump montage (S) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|DoubleJump|Montages")
	TObjectPtr<UAnimMontage> DoubleJumpBackwardMontage;

	/** Left double jump montage (A) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|DoubleJump|Montages")
	TObjectPtr<UAnimMontage> DoubleJumpLeftMontage;

	/** Right double jump montage (D) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|DoubleJump|Montages")
	TObjectPtr<UAnimMontage> DoubleJumpRightMontage;

	/** Optional Sound to play on double jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|DoubleJump")
	TObjectPtr<USoundBase> DoubleJumpSound;

	/** Optional VFX / Niagara system to spawn under character feet on double jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|DoubleJump")
	TObjectPtr<UNiagaraSystem> DoubleJumpEffect;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	bool bSavedOrientRotationToMovement;
	bool bSavedUseControllerDesiredRotation;

	FTimerHandle FailsafeTimerHandle;

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMontageCancelled();
};

