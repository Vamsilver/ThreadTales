// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/ThreadTalesGameplayAbility.h"
#include "ThreadTalesGA_BowDraw.generated.h"

class AThreadTalesWeapon_Bow;
class UAnimMontage;
class USoundBase;

/**
 * Hold-activated Bow Draw & Aim Gameplay Ability for Archer Archetype.
 * On activation (RMB hold):
 * - Activates OTS aim camera and 8-way strafe Motion Matching via Character->SetAimInputDown(true).
 * - Grants TAG_State_Combat_Aiming.
 * - Charges bowstring draw tension (0.0 -> 1.0) over DrawDuration.
 * - Updates AThreadTalesWeapon_Bow::SetDrawRatio dynamically.
 * - Consumes initial stamina and continuous hold stamina drain at full tension.
 * On input release (RMB released):
 * - Restores camera, clears aim mode via SetAimInputDown(false).
 * - Resets bowstring draw tension to 0.0.
 * - Ends ability cleanly.
 */
UCLASS()
class THREADTALES_API UThreadTalesGA_BowDraw : public UThreadTalesGameplayAbility
{
	GENERATED_BODY()

public:
	UThreadTalesGA_BowDraw();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Resets current draw tension ratio back to 0.0 for rapid follow-up shots without dropping aim stance */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Bow|Combat", meta = (ToolTip = "Resets draw tension to 0.0 without leaving aim stance"))
	void ResetDrawTension();

	/** Cancels aim stance and ends the ability from an external caller (e.g. GA_BowRelease) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Bow|Combat", meta = (ToolTip = "Cancels aim and ends the draw ability"))
	void CancelAimAndEnd();

	/** Resumes playing the aim ready montage after a release recoil ends while still aiming */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Bow|Combat", meta = (ToolTip = "Resumes aim ready stance after firing recoil"))
	void ResumeAimReadyMontage();

	/** Stops the active aim ready montage when transitioning into string draw on LMB */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Bow|Combat", meta = (ToolTip = "Stops aim ready montage to begin string draw"))
	void StopAimReadyMontage();

	/** Returns current normalized bowstring draw tension ratio (0.0 to 1.0) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Bow|Combat", meta = (ToolTip = "Returns normalized draw tension ratio (0.0 = relaxed, 1.0 = fully drawn)"))
	float GetCurrentDrawRatio() const { return CurrentDrawRatio; }

	/** Returns whether the bow has reached full draw tension (1.0) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Bow|Combat", meta = (ToolTip = "True if bowstring is at maximum tension"))
	bool IsFullyDrawn() const { return CurrentDrawRatio >= 1.0f; }

	/** Time in seconds required to reach full bowstring tension (1.0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Balancing", meta = (ClampMin = "0.1", ToolTip = "Time in seconds to reach full draw tension (1.0)"))
	float DrawDuration = 0.65f;

	/** Initial stamina cost deducted when initiating bow draw */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Balancing", meta = (ClampMin = "0.0", ToolTip = "Stamina consumed upon initiating bow draw"))
	float InitialStaminaCost = 10.0f;

	/** Continuous stamina drained per second while holding bow at maximum tension (1.0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Balancing", meta = (ClampMin = "0.0", ToolTip = "Continuous stamina drain per second while holding full tension"))
	float HoldStaminaDrainRate = 8.0f;

	/** Whether aiming with RMB automatically begins charging bowstring draw tension (default: false, drawing is on LMB) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Balancing", meta = (ToolTip = "Whether aiming automatically charges bowstring draw tension"))
	bool bAutoDrawTension = false;

	/** Duration in seconds of the close-quarters bow parry deflection window (default: 0.20s, precision Sekiro timing) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Parry", meta = (ClampMin = "0.05", ClampMax = "1.0", ToolTip = "Duration of the close-quarters perfect parry window upon RMB press (0.20s)"))
	float ParryWindowDuration = 0.20f;

	/** Duration in seconds of the point-blank parry counter-shot window upon successful deflection (default: 1.25s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Parry", meta = (ClampMin = "0.2", ClampMax = "3.0", ToolTip = "Duration of the parry counter-shot window granted on successful deflection (1.25s)"))
	float ParryCounterWindowDuration = 1.25f;

	/** Maximum distance in cm to detect an incoming melee attack threat when pressing RMB */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Parry", meta = (ClampMin = "50.0", ClampMax = "600.0", ToolTip = "Threat radius in cm for close-quarters bow parry deflection"))
	float BowParryThreatRadiusCm = 280.0f;

	/** Returns whether the close-quarters bow parry deflection window is currently open */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Bow|Parry")
	bool IsParryActive() const { return bParryActive; }

	/** Returns whether the point-blank parry counter-shot window is currently active */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Bow|Parry")
	bool IsParryCounterWindowActive() const { return bParryCounterWindowActive; }

protected:
	/** Timer callback updating draw tension and hold stamina drain */
	void OnDrawTick();

	/** Resolves the equipped bow weapon from avatar actor */
	AThreadTalesWeapon_Bow* GetEquippedBow() const;

	/** Sets or clears GASPALS aim input on avatar character */
	void SetAvatarAimInput(bool bAiming) const;

	/** Sets or clears GASPALS strafe input on avatar character */
	void SetAvatarStrafeInput(bool bStrafing) const;

	/** Detects whether an enemy within threat radius is executing a melee strike */
	bool DetectIncomingMeleeThreat(AActor*& OutThreatActor) const;

	/** Evaluates whether a candidate actor poses an immediate melee threat */
	bool IsActorMeleeThreat(AActor* Candidate, const class AThreadTalesCharacter* TTChar) const;

	/** Determines directional parry index (0 = Left, 1 = Right) based on relative position of threat */
	int32 DetermineParryDirectionIndex(const AActor* ThreatActor) const;

	/** Called when the 0.20s close-quarters bow parry window expires */
	UFUNCTION()
	void OnParryWindowExpired();

	/** Initiates close-quarters parry window and plays appropriate parry deflect or aim montage */
	void InitiateParryWindowAndMontage(AThreadTalesCharacter* TTChar, AThreadTalesWeapon_Bow* Bow);

	/** Bound to AThreadTalesCharacter::OnSuccessfulParry delegate when parry deflect lands */
	UFUNCTION()
	void HandleSuccessfulParry(AActor* AttackingActor);

	/** Normalized draw tension (0.0 .. 1.0) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|State")
	float CurrentDrawRatio = 0.0f;

	/** Elapsed time since current draw started */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|State")
	float DrawElapsed = 0.0f;

	/** Whether the 0.20s bow parry deflect window is currently open */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|State")
	bool bParryActive = false;

	/** Whether the 1.0s parry counter window is currently active */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|State")
	bool bParryCounterWindowActive = false;

	/** Direction index of last parry (0 = Left, 1 = Right) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|State")
	int32 LastParryDirectionIndex = 0;

	/** Optional upper body draw montage (if not using pure GASPALS Bow overlay) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Montages", meta = (ToolTip = "Optional upper body draw montage"))
	UAnimMontage* DrawMontage = nullptr;

	/** Optional sound played when beginning to pull the bowstring */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Audio", meta = (ToolTip = "Sound played when bowstring draw begins"))
	USoundBase* DrawStartSound = nullptr;

private:
	/** Timer handle driving tension progression */
	FTimerHandle DrawTimerHandle;

	/** Timer handle for the 0.20s parry deflect window */
	FTimerHandle ParryWindowTimerHandle;

	/** Timer handle for resuming AimReady montage after parry deflect animation completes */
	FTimerHandle ResumeAimTimerHandle;

	/** Timer handle for revealing nocked cosmetic arrow on bow */
	FTimerHandle NockArrowTimerHandle;

	/** Cached reference to equipped bow weapon */
	TWeakObjectPtr<AThreadTalesWeapon_Bow> CachedBowWeapon;

	/** Currently active aim ready montage on avatar */
	TWeakObjectPtr<UAnimMontage> ActiveAimMontage;
};
