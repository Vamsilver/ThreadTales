// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "Combat/ThreadTalesTargetableInterface.h"
#include "ThreadTalesBaseCharacter.generated.h"

class UThreadTalesAbilitySystemComponent;
class UThreadTalesAttributeSet;
class UThreadTalesEquipmentComponent;
class UThreadTalesStaminaComponent;
class UThreadTalesCombatReactionHandler;
class UThreadTalesCombatLocomotionHandler;
class UAIPerceptionStimuliSourceComponent;
class UThreadTalesPostureConfig;
class UAnimMontage;
class AThreadTalesWeapon;
class UWidgetComponent;
class UUserWidget;
class UGameplayAbility;
class UGameplayEffect;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChangedDelegate, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaminaChangedDelegate, float, CurrentStamina, float, MaxStamina);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPoiseChangedDelegate, float, CurrentPoise, float, MaxPoise);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStaggeredDelegate, AActor*, InstigatorActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHitReactDelegate, AActor*, InstigatorActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGuardHitDelegate, AActor*, AttackerActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnParriedDelegate, AActor*, ParryingActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSuccessfulParryDelegate, AActor*, AttackingActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCharacterDeathDelegate, AActor*, DeadActor, AActor*, KillerActor);

/**
 * Base character class in ThreadTales.
 * Provides unified GAS abilities, AttributeSet (Health, Stamina, Poise),
 * Souls-like combat reactions, posture breaking, equipment, stance overlays,
 * and AI perception stimuli for both Player and Enemy characters.
 */
UCLASS(Abstract, config=Game)
class THREADTALES_API AThreadTalesBaseCharacter : public ACharacter, public IAbilitySystemInterface, public IThreadTalesTargetableInterface
{
	GENERATED_BODY()

public:
	AThreadTalesBaseCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Implement IAbilitySystemInterface
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// Combat & Attribute Delegates
	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Attributes")
	FOnHealthChangedDelegate OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Attributes")
	FOnStaminaChangedDelegate OnStaminaChanged;

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Attributes")
	FOnPoiseChangedDelegate OnPoiseChanged;

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Combat")
	FOnStaggeredDelegate OnStaggered;

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Combat")
	FOnHitReactDelegate OnHitReact;

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Combat")
	FOnGuardHitDelegate OnGuardHit;

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Combat")
	FOnParriedDelegate OnParried;

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Combat")
	FOnSuccessfulParryDelegate OnSuccessfulParry;

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Health")
	FOnCharacterDeathDelegate OnCharacterDeath;

	// Attribute Getters
	float GetHealth() const;
	float GetMaxHealth() const;
	float GetStamina() const;
	float GetMaxStamina() const;
	float GetPoise() const;
	float GetMaxPoise() const;

	virtual bool IsStaggered() const;
	void ApplyHitStop(float Duration = 0.06f);
	void ResumeFromHitStop();
	bool IsAlive() const { return bIsAlive; }

	// Attribute and Reaction Handlers
	virtual void HandleHealthChanged(float NewHealth, float MaxHealthVal);
	virtual void HandleStaminaChanged(float NewStamina, float MaxStaminaVal);
	virtual void HandlePoiseChanged(float NewPoise, float MaxPoiseVal);
	virtual void HandleStaggered(AActor* AttackerActor);
	virtual void HandleHitReact(AActor* AttackerActor);
	virtual void HandleGuardHit(AActor* AttackerActor);
	virtual float GetPostureBreakStunDuration() const;
	virtual float GetParryRecoilDuration() const;
	virtual void HandleParried(AActor* ParryingActor);
	virtual void HandlePostureBreak(AActor* ParryingActor, float StunDuration = -1.0f);
	virtual void FinishPostureBreak();
	virtual void FinishParriedStagger();
	virtual void HandleSuccessfulParry(AActor* AttackingActor);
	void OpenParryCounterWindow(float Duration = 1.25f);
	void CloseParryCounterWindow();

	virtual bool CanDie() const { return true; }
	virtual void Die(AActor* KillerActor);
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	// Combat Stances & State
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Combat")
	void PerformEvade();

	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Combat")
	void ToggleCombatStance();

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Combat")
	bool IsInCombatStance() const;

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Combat")
	bool IsEvading() const;

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Combat")
	bool IsBlocking() const;

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Movement")
	bool IsGrounded() const;

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Movement")
	bool IsAirborne() const;

	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Combat")
	virtual bool IsParrying() const;

	bool IsFacingAttacker(const AActor* AttackerActor, float MaxHalfAngleDegrees = 95.0f) const;
	bool IsSprinting() const;
	void StopSprinting();
	virtual bool CanAimOrBlock() const;

	// Montages
	virtual UAnimMontage* GetStaggerMontage() const;
	virtual UAnimMontage* GetPostureBreakMontage() const;
	virtual UAnimMontage* GetHitReactMontage() const;
	virtual UAnimMontage* GetGuardHitMontage() const;
	virtual UAnimMontage* GetGuardHitHeavyMontage() const;
	virtual UAnimMontage* GetParryMontage() const;

	// Airborne Montages (fallback to Grounded if null)
	virtual UAnimMontage* GetAirStaggerMontage() const;
	virtual UAnimMontage* GetAirPostureBreakMontage() const;
	virtual UAnimMontage* GetAirHitReactMontage() const;

	// Effective Montages (resolves Airborne vs Grounded with fallback)
	virtual UAnimMontage* GetEffectiveStaggerMontage() const;
	virtual UAnimMontage* GetEffectivePostureBreakMontage() const;
	virtual UAnimMontage* GetEffectiveHitReactMontage() const;
	virtual UAnimMontage* GetRecoverMontage() const;

	// Equipment Accessors
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Equipment")
	TSubclassOf<AThreadTalesWeapon> DefaultWeaponClass;

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Equipment")
	UThreadTalesEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	// Overlay System Integration
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Overlay")
	void SetOverlayPose(uint8 NewOverlayPose);

	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Overlay")
	void SetOverlayPoseByName(FName PoseName);

	uint8 GetOverlayPose() const;
	void SetAimInputDown(bool bAiming);
	void ServerSetAimInputDown_Implementation(bool bAiming);
	void SetStrafeInputDown(bool bStrafing);
	void ServerSetStrafeInputDown_Implementation(bool bStrafing);

	// Abilities & Passives
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Abilities")
	TArray<TSubclassOf<UGameplayEffect>> DefaultPassiveEffects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Abilities")
	TSubclassOf<UGameplayAbility> EvadeAbilityClass;

	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Abilities")
	void ActivateAbilityByTag(FGameplayTag ContainerTag);

	// Component Accessors
	const UThreadTalesPostureConfig* GetPostureConfig() const;
	UThreadTalesAttributeSet* GetAttributeSet() const { return AttributeSet; }
	UThreadTalesStaminaComponent* GetStaminaComponent() const { return StaminaComponent; }
	UThreadTalesCombatReactionHandler* GetCombatReactionHandler() const { return CombatReactionHandler; }
	UThreadTalesCombatLocomotionHandler* GetCombatLocomotionHandler() const { return CombatLocomotionHandler; }
	UAIPerceptionStimuliSourceComponent* GetStimuliSourceComponent() const { return StimuliSourceComponent; }

	bool CanSpendStamina() const;
	bool TryConsumeStamina(float CostAmount, bool bApplyRegenDelay = true);
	void ApplyStaminaRegenDelay(bool bExhausted = false);
	void NotifyInsufficientStamina();
	float GetJumpStaminaCost() const;

	/** Returns the target lock reticle widget component attached to this character */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	UWidgetComponent* GetTargetLockWidgetComponent() const { return TargetLockWidgetComponent; }

	/** Returns the configured target lock reticle widget class */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	TSubclassOf<UUserWidget> GetTargetLockWidgetClass() const { return TargetLockWidgetClass; }

	// IThreadTalesTargetableInterface
	virtual bool CanBeTargeted_Implementation(const AActor* TargetingActor) const override;
	virtual FVector GetTargetLockLocation_Implementation(const AActor* TargetingActor) const override;
	virtual void OnTargetLocked_Implementation(AActor* TargetingActor) override;
	virtual void OnTargetUnlocked_Implementation(AActor* TargetingActor) override;

protected:
	void UpdateMovementModeGameplayTags(EMovementMode NewMovementMode);
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UThreadTalesAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UThreadTalesAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UThreadTalesEquipmentComponent> EquipmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UThreadTalesStaminaComponent> StaminaComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UThreadTalesCombatReactionHandler> CombatReactionHandler;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UThreadTalesCombatLocomotionHandler> CombatLocomotionHandler;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIPerceptionStimuliSourceComponent> StimuliSourceComponent;

	/** Screen-Space Widget Component displaying target lock reticle directly on the torso/socket */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> TargetLockWidgetComponent;

	/** Widget Blueprint class to display upon target lock (assigned in Blueprint Details, zero hardcoded paths) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> TargetLockWidgetClass;

	/** Minimum scale of the reticle at far distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|UI", meta = (AllowPrivateAccess = "true"))
	float TargetLockMinScale;

	/** Actor currently locking onto this character */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CurrentTargetingActor;

	/** Updates dynamic distance scaling for the target lock reticle */
	void UpdateTargetLockWidgetScaling();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Attributes", meta = (AllowPrivateAccess = "true"))
	float MaxHealth;

	UPROPERTY(ReplicatedUsing = OnRep_IsAlive, BlueprintReadOnly, Category = "ThreadTales|Health", meta = (AllowPrivateAccess = "true"))
	bool bIsAlive;

	UFUNCTION()
	virtual void OnRep_IsAlive();

	void InitializeDefaultAbilitiesAndPassives();
};
