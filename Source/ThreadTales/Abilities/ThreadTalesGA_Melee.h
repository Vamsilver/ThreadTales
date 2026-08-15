// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreadTalesGameplayAbility.h"
#include "ThreadTalesGA_Melee.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;

/**
 * Professional Section-based Combo Gameplay Ability for Melee Weapons.
 * Plays the weapon's ComboAttackMontage and branches across sections (Combo1..Combo4) via Combo Windows.
 */
UCLASS()
class THREADTALES_API UThreadTalesGA_Melee : public UThreadTalesGameplayAbility
{
	GENERATED_BODY()
	
public:
	UThreadTalesGA_Melee();

	/** Names of the sections in the combo montage to play in sequence */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Combo")
	TArray<FName> ComboSectionNames;

	/** Fallback Montage to play if weapon doesn't have one assigned */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Combo")
	UAnimMontage* FallbackAttackMontage;

	/** Maximum time in seconds before window opens that an attack click is still remembered (Input Buffering) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Combo", meta = (ClampMin = 0, ClampMax = 1))
	float InputBufferTolerance = 0.4f;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

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

	/** Time of the last attack button click */
	float LastAttackInputTime;

	/** Pointer to the currently playing montage */
	UPROPERTY()
	UAnimMontage* CurrentPlayingMontage;

	UPROPERTY()
	UAbilityTask_PlayMontageAndWait* MontageTask;

	UFUNCTION()
	void OnMontageCompleted();
	
	UFUNCTION()
	void OnMontageInterrupted();
	
	UFUNCTION()
	void OnMontageCancelled();

	void JumpToNextComboSection();
};
