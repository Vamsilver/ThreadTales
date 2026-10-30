// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThreadTalesStaminaComponent.generated.h"

class AThreadTalesBaseCharacter;
class AThreadTalesCharacter;
class UThreadTalesStaminaConfig;
class UThreadTalesPostureConfig;
class UThreadTalesAbilitySystemComponent;
class UThreadTalesAttributeSet;

/**
 * Component responsible for Souls-like stamina consumption, exhaustion penalties,
 * continuous sprint drain, and Sekiro-style posture/concentration recovery delays.
 */
UCLASS(ClassGroup = (ThreadTales), meta = (BlueprintSpawnableComponent))
class THREADTALES_API UThreadTalesStaminaComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UThreadTalesStaminaComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Returns the stamina configuration data asset */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Stamina")
	const UThreadTalesStaminaConfig* GetStaminaConfig() const { return StaminaConfig; }

	/** Returns the Sekiro posture & defense configuration data asset */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Posture")
	const UThreadTalesPostureConfig* GetPostureConfig() const { return PostureConfig; }

	/** Souls-like Any-Remainder rule: returns true if character has > 0.0f Stamina remaining */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Stamina")
	bool CanSpendStamina() const;

	/**
	 * Attempts to consume Stamina using the Souls-like Any-Remainder > 0 rule.
	 * Succeeds as long as CurrentStamina > 0.0f (clamping to 0.0f and triggering ExhaustedRegenDelay if depleted).
	 */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Stamina")
	bool TryConsumeStamina(float CostAmount, bool bApplyRegenDelay = true);

	/** Applies stamina regeneration delay (NormalRegenDelay if !bExhausted, ExhaustedRegenDelay if bExhausted) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Stamina")
	void ApplyStaminaRegenDelay(bool bExhausted = false);

	/** Applies posture/poise regeneration delay after taking hit or block posture damage */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Poise")
	void ApplyPoiseRegenDelay(float CustomDelay = -1.0f);

	/** Flashes the HUD Stamina bar to indicate an action was blocked due to 0 Stamina */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Stamina")
	void NotifyInsufficientStamina();

	/** Clears CBP_SandboxCharacter WantsToSprint and bIsSprintInputDown when stamina is exhausted */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Stamina")
	void ForceClearGaspalsSprint();

	// Stamina Cost Accessors (resolved from StaminaConfig with C++ fallbacks)
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Stamina")
	float GetLightAttackStaminaCost() const;

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Stamina")
	float GetDodgeAttackStaminaCost() const;

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Stamina")
	float GetSprintAttackStaminaCost() const;

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Stamina")
	float GetGroundEvadeStaminaCost() const;

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Stamina")
	float GetAirEvadeStaminaCost() const;

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Stamina")
	float GetJumpStaminaCost() const;

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Stamina")
	float GetDoubleJumpStaminaCost() const;

	/** Initializes default data assets and initial attribute values */
	void InitializeDefaultConfigs();

protected:
	/** Data asset defining all Souls-like Stamina costs, continuous drain rates, regeneration rate, and delays */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Stamina", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const UThreadTalesStaminaConfig> StaminaConfig;

	/** Dedicated Data asset defining all Sekiro-style Posture (Concentration), Block mitigation, Deflect, and Stagger parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Posture", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const UThreadTalesPostureConfig> PostureConfig;

private:
	AThreadTalesBaseCharacter* GetCharacterOwner() const;
	UThreadTalesAbilitySystemComponent* GetOwnerASC() const;
	const UThreadTalesAttributeSet* GetOwnerAttributeSet() const;
	UThreadTalesAttributeSet* GetOwnerAttributeSetMutable() const;

	void UpdateSprintStaminaDrain(float DeltaSeconds, bool bSprintKeyHeld);
	void UpdateStaminaRegenDA(float DeltaSeconds);
	void UpdatePostureRecovery(float DeltaSeconds);

	void HandleSprintExhaustion();

	/** Timer handle governing data-driven stamina regeneration delay */
	FTimerHandle StaminaRegenDelayTimerHandle;

	/** Removes TAG_State_Combat_StaminaRegenBlocked when recovery delay expires */
	void OnStaminaRegenDelayExpired();

	/** Timer handle governing Sekiro-style posture/poise regeneration delay */
	FTimerHandle PoiseRegenDelayTimerHandle;

	/** True when posture regeneration is paused after taking hit or block posture damage */
	bool bPoiseRegenBlockedByDelay = false;

	/** Clears bPoiseRegenBlockedByDelay when posture recovery delay expires */
	void OnPoiseRegenDelayExpired();

	/** True when character was actively sprinting and draining stamina on the previous frame */
	bool bWasActivelySprinting = false;

	/** True when sprinting drained stamina to 0.0f; prevents immediate sprint re-entry until Shift is released or stamina recovers */
	bool bSprintLockedByExhaustion = false;
};
