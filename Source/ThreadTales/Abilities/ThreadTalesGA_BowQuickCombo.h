// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/ThreadTalesGameplayAbility.h"
#include "Abilities/ThreadTalesComboAbilityInterface.h"
#include "ThreadTalesGA_BowQuickCombo.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class AThreadTalesWeapon_Bow;
class AThreadTalesProjectile_Arrow;
class UThreadTalesBowConfig;

/**
 * 4-Stage Acrobatic Hipfire Combo & Directional Dodge-to-Shot Ability for the Bow Hunter archetype.
 * Operates when LMB is pressed outside Aim Mode (AM_Bow_Combo_01..04) or during Evade recovery (AM_Bow_DodgeAttack_F/B/L/R).
 * Features TargetLock auto-facing and rapid ballistic arrow launches.
 */
UCLASS()
class THREADTALES_API UThreadTalesGA_BowQuickCombo : public UThreadTalesGameplayAbility, public IThreadTalesComboAbilityInterface
{
	GENERATED_BODY()

public:
	UThreadTalesGA_BowQuickCombo();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

	/** Called by UAnimNotifyState_ComboWindow when combo cancel window opens */
	virtual void NotifyComboWindowBegin() override;

	/** Called by UAnimNotifyState_ComboWindow when combo cancel window closes */
	virtual void NotifyComboWindowEnd() override;

	/** Called when attack input is received while ability is active */
	virtual void NotifyComboInput() override;

	/** Called by UAnimNotifyState_EarlyLocomotionCancel when movement input cancels recovery */
	virtual bool TryCancelToLocomotion() override;

	/** Spawns the quick-shot projectile for the current combo step or dodge shot (callable by timer or anim notify) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Bow|Combat")
	void NotifyQuickShotFire();

	/** Returns current combo step index (0 = Combo 1, 1 = Combo 2, etc., -1 if dodge shot) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Bow|Combat")
	int32 GetCurrentComboIndex() const { return CurrentComboIndex; }

	/** Returns whether currently executing a dodge counter shot */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Bow|Combat")
	bool IsDodgeShot() const { return bIsDodgeShot; }

	/** Returns whether currently executing a point-blank parry counter shot */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Bow|Combat")
	bool IsParryCounterShot() const { return bIsParryCounterShot; }

	/** Arrow projectile asset to spawn */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Combat", meta = (ToolTip = "Projectile arrow asset spawned on quick shot release."))
	TSubclassOf<AThreadTalesProjectile_Arrow> ArrowClass;

	/** Sound effect played upon quick release of an arrow */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Cosmetics", meta = (ToolTip = "Sound cue played upon rapid arrow launch."))
	TObjectPtr<USoundBase> QuickShotSound;

	/** Active montage task */
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	/** Pointer to currently playing montage */
	UPROPERTY()
	TObjectPtr<UAnimMontage> CurrentPlayingMontage;

	/** Cached pointer to equipped bow */
	UPROPERTY()
	TWeakObjectPtr<AThreadTalesWeapon_Bow> CachedBowWeapon;

	/** Forward offset in cm from arrow spawn point to prevent avatar self-collision */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Ballistics", meta = (ClampMin = "0.0", ClampMax = "150.0", ToolTip = "Forward offset in cm from spawn socket to clear capsule bounds."))
	float ArrowSpawnForwardOffset = 45.0f;

	/** Fallback fire delay in seconds from montage start if no anim notify triggers */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|QuickCombo", meta = (ClampMin = "0.05", ClampMax = "1.50", ToolTip = "Fallback delay in seconds from montage start when arrow fires."))
	float DefaultFireDelaySeconds = 0.28f;

	/** Base health damage dealt by quick hipfire combo shots */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Damage", meta = (ClampMin = "1.0", ClampMax = "200.0", ToolTip = "Base health damage applied per quick hipfire arrow."))
	float BaseComboDamage = 22.0f;

	/** Base poise/posture damage dealt by quick hipfire combo shots */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Damage", meta = (ClampMin = "1.0", ClampMax = "200.0", ToolTip = "Base posture/poise damage applied per quick hipfire arrow."))
	float BaseComboPoiseDamage = 12.0f;

	/** Multiplier applied to poise damage for Dodge-to-Shot counter attacks */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Damage", meta = (ClampMin = "1.0", ClampMax = "5.0", ToolTip = "Poise damage multiplier applied to dodge counter shots."))
	float DodgeShotPoiseMultiplier = 2.2f;

	/** Multiplier applied to poise damage for point-blank Parry Counter shots */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Damage", meta = (ClampMin = "1.0", ClampMax = "6.0", ToolTip = "Poise damage multiplier applied to parry counter shots."))
	float ParryCounterPoiseMultiplier = 3.5f;

	/** Multiplier applied to health damage for point-blank Parry Counter shots */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Damage", meta = (ClampMin = "1.0", ClampMax = "5.0", ToolTip = "Health damage multiplier applied to parry counter shots."))
	float ParryCounterDamageMultiplier = 1.75f;

	/** Play rate multiplier for quick combo shots (default: 1.0x for natural Root Motion kinematic timing) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bow|Combat", meta = (ClampMin = "0.5", ClampMax = "2.5", ToolTip = "Speed multiplier for hipfire combo montages."))
	float ComboAttackPlayRate = 1.0f;

	/** Play rate multiplier for dodge counter shot (default: 1.0x for natural kinematic timing) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bow|Combat", meta = (ClampMin = "0.5", ClampMax = "2.5", ToolTip = "Speed multiplier for dodge shot montages."))
	float DodgeAttackPlayRate = 1.0f;

	/** Angular yaw offset in degrees to compensate baked animation torso angle so bow aims directly at camera crosshair */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Combat", meta = (ClampMin = "-90.0", ClampMax = "90.0", ToolTip = "Yaw offset in degrees to align UpperBody bow aim with camera."))
	float ComboTorsoCompensationYaw = 22.0f;

	/** Current combo index (0..3) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bow|State")
	int32 CurrentComboIndex = 0;

	/** Flag indicating if the player buffered the next combo attack */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bow|State")
	bool bQueueNextCombo = false;

	/** Whether the combo cancel window is currently open (allows immediate branching to next shot) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bow|State")
	bool bComboWindowOpen = false;

	/** Flag indicating if currently in a dodge counter shot */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bow|State")
	bool bIsDodgeShot = false;

	/** Flag indicating if currently executing a point-blank parry counter shot */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bow|State")
	bool bIsParryCounterShot = false;

	/** Whether the arrow for the current step has already spawned */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bow|State")
	bool bHasFiredCurrentShot = false;

	/** Flag indicating whether the recovery window allows early cancel to locomotion on movement input */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bow|State")
	bool bCanEarlyLocomotionCancel = false;

protected:

	void PlayComboStep(int32 StepIndex);
	void PlayDodgeShot(int32 DirectionIndex);
	void PlayParryCounterShot(int32 DirectionIndex);
	void StartMontageTask(UAnimMontage* MontageToPlay, float PlayRate);
	void SpawnQuickArrowProjectile(const FVector& LaunchDirection, const FVector& MuzzleOrigin, float Damage, float PoiseDamage, float Speed, float GravityScale);
	void AlignFacingToTargetOrCamera();
	int32 DetermineDodgeDirectionIndex(const ACharacter* Character) const;
	int32 DetermineParryCounterDirectionIndex(const ACharacter* Character) const;
	void SetAvatarStrafeInput(bool bStrafing) const;

	AThreadTalesWeapon_Bow* GetEquippedBow() const;
	const UThreadTalesBowConfig* GetBowConfig() const;

	UFUNCTION()
	void HandleEarlyRecoveryTimer();

	UFUNCTION()
	void CheckEarlyLocomotionInput();

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageBlendOut();

	UFUNCTION()
	void OnMontageSafetyTimeout();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMontageCancelled();

private:
	/** Timer handle for scheduling the arrow fire */
	FTimerHandle FireTimerHandle;

	/** Fallback safety timer handle ensuring montage task never hangs */
	FTimerHandle SafetyTimerHandle;

	/** Timer handle for opening the buffered combo chaining window */
	FTimerHandle ComboWindowOpenTimerHandle;

	/** Timer handle for closing the buffered combo chaining window */
	FTimerHandle ComboWindowCloseTimerHandle;

	/** Timer handle for unlocking early locomotion cancel after arrow release */
	FTimerHandle EarlyRecoveryTimerHandle;

	/** Configures firing, combo chaining window, and early recovery timers */
	void ArmAttackTimers(float FireDelay, float EarlyRecDelay, float EffectivePlayRate, int32 StepIndex);
};
