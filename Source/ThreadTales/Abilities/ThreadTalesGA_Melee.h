// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreadTalesGameplayAbility.h"
#include "Abilities/ThreadTalesComboAbilityInterface.h"
#include "ThreadTalesGA_Melee.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;

/**
 * Professional Section-based Combo Gameplay Ability for Melee Weapons.
 * Plays the weapon's ComboAttackMontage and branches across sections (Combo1..Combo4) via Combo Windows.
 */
UCLASS()
class THREADTALES_API UThreadTalesGA_Melee : public UThreadTalesGameplayAbility, public IThreadTalesComboAbilityInterface
{
	GENERATED_BODY()
	
public:
	UThreadTalesGA_Melee();

	/** Names of the sections in the combo montage to play in sequence */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Combo")
	TArray<FName> ComboSectionNames;

	/** Array of individual combo strike montages (Strike1, Strike2, Strike3, Strike4) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Combo")
	TArray<UAnimMontage*> ComboStrikeMontages;

	/** Montage played when executing a dodge attack (Anim_ARPGSamurai_Attack_Dodge) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Combo")
	TObjectPtr<UAnimMontage> DodgeAttackMontage;

	/** Play rate for dodge attack montage (default: 1.5x for snappy counter-attack feel) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Combo", meta = (ClampMin = 0.5, ClampMax = 2.5))
	float DodgeAttackPlayRate = 1.5f;

	/** Montage played when executing a sprint attack (Anim_ARPGSamurai_Attack_Sprint) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Combo")
	TObjectPtr<UAnimMontage> SprintAttackMontage;

	/** Fallback single Montage to play if weapon doesn't have one assigned */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Combo")
	TObjectPtr<UAnimMontage> FallbackAttackMontage;

	/** Maximum time in seconds before window opens that an attack click is still remembered (Input Buffering) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Combo", meta = (ClampMin = 0, ClampMax = 1))
	float InputBufferTolerance = 0.4f;

	/** Gameplay Effect to delay stamina regeneration after attack completes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Combo|Stamina")
	TSubclassOf<class UGameplayEffect> StaminaDelayEffectClass;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

	bool HasValidComboStrikeAtIndex(int32 Index) const;
	void PlayComboStrike(int32 Index);
	void PlayDodgeAttack();
	void PlaySprintAttack();

	/** Called when attack button is pressed while this ability is running */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Combo")
	void NotifyComboInput();

	/** Called from AnimNotifyState_ComboWindow when the window opens */
	void NotifyComboWindowBegin();

	/** Called from AnimNotifyState_ComboWindow when the window ends (branches to next combo if input received) */
	void NotifyComboWindowEnd();

	/** Returns current combo strike index (0 = Combo1, 1 = Combo2, etc.) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Combo")
	int32 GetCurrentComboIndex() const { return CurrentComboIndex; }

protected:
	/** Current section index in ComboSectionNames */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Combo")
	int32 CurrentComboIndex;

	/** If true, the combo window is currently open and accepting input */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Combo")
	bool bComboWindowOpen;

	/** If true, the player pressed attack during the combo window */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Combo")
	bool bQueueNextCombo;

	/** True if currently playing a dodge attack */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Combo")
	bool bIsDodgeAttack;

	/** True if currently playing a sprint attack */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Combo")
	bool bIsSprintAttack;

	/** Pointer to the currently playing montage */
	UPROPERTY()
	TObjectPtr<UAnimMontage> CurrentPlayingMontage;

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UFUNCTION()
	void OnMontageCompleted();
	
	UFUNCTION()
	void OnMontageInterrupted();
	
	UFUNCTION()
	void OnMontageCancelled();

	void JumpToNextComboSection();

	/** Aligns character yaw to camera/movement direction at the start of each strike */
	void AlignCharacterToCameraDirection();

private:
	/** Time of the last attack button click */
	float LastAttackInputTime = 0.0f;
};
