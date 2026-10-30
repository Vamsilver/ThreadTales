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

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

	/** Called when attack input (LMB) is pressed while evading */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Evade")
	void NotifyAttackInput();

	/** Attempts to cancel evade recovery and execute a seamless Dodge Attack */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Evade")
	bool TryExecuteDodgeAttack();

	/** Gameplay Effect to delay stamina regeneration after evade completes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Stamina")
	TSubclassOf<class UGameplayEffect> StaminaDelayEffectClass;

	/** Calculates the 8-way evade direction based on WASD input vector relative to camera rotation */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Evade")
	EEvadeDirection CalculateEvadeDirection(float& OutAngle) const;

	/** Calculates the early recovery cancel time in seconds for a specific montage and direction */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Evade")
	float GetCancelTimeForMontage(const UAnimMontage* Montage, EEvadeDirection Direction) const;

	/** 8-directional Montages */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Montages")
	TObjectPtr<UAnimMontage> EvadeForwardMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Montages")
	TObjectPtr<UAnimMontage> EvadeForwardRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Montages")
	TObjectPtr<UAnimMontage> EvadeRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Montages")
	TObjectPtr<UAnimMontage> EvadeBackwardRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Montages")
	TObjectPtr<UAnimMontage> EvadeBackwardMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Montages")
	TObjectPtr<UAnimMontage> EvadeBackwardLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Montages")
	TObjectPtr<UAnimMontage> EvadeLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Montages")
	TObjectPtr<UAnimMontage> EvadeForwardLeftMontage;

	/** Fallback single dodge montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Montages")
	TObjectPtr<UAnimMontage> DefaultDodgeMontage;

	/** Exit speed when cancelling evade into sprint (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Locomotion")
	float ExitSprintSpeed = 650.0f;

	/** Exit speed when cancelling evade into normal run (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Locomotion")
	float ExitRunSpeed = 450.0f;

	/** Blend out duration when stopping evade montage for early locomotion cancel */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Locomotion")
	float EarlyCancelBlendOutTime = 0.18f;

	/** Maximum angular delta in degrees for direct rotation alignment during forward cancel */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Locomotion")
	float DirectAlignmentYawThreshold = 25.0f;

	/** Early cancel percentage for Forward roll (0.0 to 1.0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Timing", meta = (ClampMin = 0.1, ClampMax = 1.0))
	float ForwardCancelPercentage = 0.50f;

	/** Early cancel percentage for Forward-Diagonal rolls (0.0 to 1.0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Timing", meta = (ClampMin = 0.1, ClampMax = 1.0))
	float DiagonalForwardCancelPercentage = 0.58f;

	/** Early cancel percentage for Lateral (Left/Right) rolls (0.0 to 1.0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Timing", meta = (ClampMin = 0.1, ClampMax = 1.0))
	float LateralCancelPercentage = 0.62f;

	/** Early cancel percentage for Backward and Backward-Diagonal rolls (0.0 to 1.0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Timing", meta = (ClampMin = 0.1, ClampMax = 1.0))
	float BackwardCancelPercentage = 0.75f;

	/** Default percentage of the montage length to check for early cancel (0.0 to 1.0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Timing")
	float DefaultEarlyCancelPercentage = 0.50f;

	/** Duration of active invulnerability frames (I-Frames) from start of evade in seconds.
	 * If <= 0.0f, character is invulnerable for the entire evade duration. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Timing")
	float IFrameDuration = 0.45f;

	/** Minimum time in seconds after evade start before recovery cancel window opens (defaults to IFrameDuration) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Timing")
	float EarlyCancelWindowStartTime = 0.45f;

	/** Interval in seconds to continuously poll movement input during recovery window */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Timing")
	float RecoveryCheckInterval = 0.04f;

	/** Maximum time in seconds before recovery window that an attack click is still remembered (Input Buffering) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Timing", meta = (ClampMin = 0, ClampMax = 1))
	float AttackBufferTolerance = 0.40f;

	/** If false, evade will not buffer or trigger dodge attacks (can be toggled in BP) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Evade|Combat")
	bool bEnableDodgeAttack = true;

	/** Optional specific cancel percentage for specific montages. Overrides the default if the montage is added here. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|Timing")
	TMap<UAnimMontage*, float> MontageCancelPercentages;

	/** True if character currently has active I-Frames during this evade */
	UPROPERTY(BlueprintReadOnly, Category = "ThreadTales|Evade|State")
	bool bInvulnerableActive = false;

	/** If true, the player pressed attack during the evade and wants to transition into Dodge Attack */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|State")
	bool bQueueDodgeAttack = false;

	/** Time of the last attack button click during this evade */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Evade|State")
	float LastAttackInputTime = -100.0f;

protected:
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	FTimerHandle RecoveryTimerHandle;
	float ActiveCancelTime = 0.45f;

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMontageCancelled();

	UFUNCTION()
	void OnCheckInputTimerFinished();

	UFUNCTION()
	void OnIFrameTimerFinished();

	UFUNCTION()
	void PollRecoveryInput();

	bool TryEarlyCancelToLocomotion();

	bool CanExecuteDodgeAttack(const ACharacter* Character, const UAbilitySystemComponent* ASC) const;
	FVector ComputeDodgeAttackDirection(const ACharacter* Character) const;
	FVector ComputeMovementIntent(const ACharacter* Character) const;

	UAnimMontage* GetMontageForDirection(EEvadeDirection Direction) const;
};
