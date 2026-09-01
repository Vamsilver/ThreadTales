// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "ThreadTalesCharacter.generated.h"

class UThreadTalesAbilitySystemComponent;
class UThreadTalesAttributeSet;
class UThreadTalesInputConfig;
class UNiagaraSystem;
class USoundBase;
class UAnimMontage;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChangedDelegate, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCharacterDeathDelegate, AActor*, DeadActor, AActor*, KillerActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDoubleJumpDelegate, AThreadTalesCharacter*, Character);

UCLASS(config=Game)
class THREADTALES_API AThreadTalesCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AThreadTalesCharacter();

	// Implement IAbilitySystemInterface
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** Called when health changes */
	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Health")
	FOnHealthChangedDelegate OnHealthChanged;

	/** Called when character dies */
	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Health")
	FOnCharacterDeathDelegate OnCharacterDeath;

	/** Returns current health */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Health")
	float GetHealth() const;

	/** Returns maximum health */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Health")
	float GetMaxHealth() const;

	/** Returns true if character is alive */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Health")
	bool IsAlive() const { return bIsAlive; }

	/** Handles health change notifications */
	virtual void HandleHealthChanged(float NewHealth, float MaxHealthVal);

	/** Handles character death */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Health")
	virtual void Die(AActor* KillerActor);

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

protected:
	virtual void BeginPlay() override;

	// Server-side initialization
	virtual void PossessedBy(AController* NewController) override;

	// Client-side initialization
	virtual void OnRep_Controller() override;

	// Ability System Component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	UThreadTalesAbilitySystemComponent* AbilitySystemComponent;

	// Attribute Set
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	UThreadTalesAttributeSet* AttributeSet;

	// Equipment Manager
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	class UThreadTalesEquipmentComponent* EquipmentComponent;

	/** Whether the character is currently alive */
	UPROPERTY(ReplicatedUsing = OnRep_IsAlive, BlueprintReadOnly, Category = "ThreadTales|Health")
	bool bIsAlive;

	UFUNCTION()
	virtual void OnRep_IsAlive();

	/** Input configuration data asset mapping Input Actions to Gameplay Tags */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Input")
	TObjectPtr<const UThreadTalesInputConfig> InputConfig;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Handlers for ability inputs mapped via Input Tags */
	void Input_AbilityInputTagPressed(FGameplayTag InputTag);
	void Input_AbilityInputTagReleased(FGameplayTag InputTag);

	/** Handles for dynamic ability input bindings */
	TArray<uint32> AbilityBindHandles;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// Helper function to trigger abilities (e.g. from Input Actions)
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Abilities")
	void ActivateAbilityByTag(FGameplayTag ContainerTag);

	/** Triggers 8-directional Evade / Dodge */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Combat")
	void PerformEvade();

	/** Toggles weapon combat stance (Draw / Sheath) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Combat")
	void ToggleCombatStance();

	/** Returns true if character is currently evading / rolling */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Combat")
	bool IsEvading() const;

	/** Returns true if character currently has weapon drawn in combat stance */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Combat")
	bool IsInCombatStance() const;

	/** Default Gameplay Abilities granted to this character (configurable in Blueprints) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Abilities")
	TArray<TSubclassOf<class UGameplayAbility>> DefaultAbilities;

	/** Evade ability class granted by default */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Abilities")
	TSubclassOf<class UGameplayAbility> EvadeAbilityClass;

	/** Default weapon class equipped on spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Equipment")
	TSubclassOf<class AThreadTalesWeapon> DefaultWeaponClass;

	/** Returns equipment component */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Equipment")
	class UThreadTalesEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	/** Example variable to prove C++ connection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Stats")
	float MaxHealth;

	/** Debug reference for Male hero class (BP_HeroMale) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Debug")
	TSubclassOf<AThreadTalesCharacter> DebugMaleClass;

	/** Debug reference for Female hero class (BP_HeroFemale) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Debug")
	TSubclassOf<AThreadTalesCharacter> DebugFemaleClass;

	/** Switches current character to the specified class */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Debug")
	void DebugSwitchCharacter(TSubclassOf<AThreadTalesCharacter> NewCharacterClass);

	/** Switch to Male character (can also be called via ~ console: DebugSwitchToMale) */
	UFUNCTION(Exec, BlueprintCallable, Category = "ThreadTales|Debug")
	void DebugSwitchToMale();

	/** Switch to Female character (can also be called via ~ console: DebugSwitchToFemale) */
	UFUNCTION(Exec, BlueprintCallable, Category = "ThreadTales|Debug")
	void DebugSwitchToFemale();

	/** Toggles between Male and Female characters (can also be called via ~ console: DebugToggleGender) */
	UFUNCTION(Exec, BlueprintCallable, Category = "ThreadTales|Debug")
	void DebugToggleGender();

	// ==========================================
	// MOVEMENT & DOUBLE JUMP
	// ==========================================

	/** Maximum allowed consecutive jumps (1 = single jump, 2 = double jump, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "ThreadTales|Movement", meta = (ClampMin = 1, ClampMax = 5))
	int32 MaxJumpCount;

	/** Vertical velocity (jump force) for the double jump. Default is 500.0f. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement")
	float DoubleJumpZVelocity;

	/** Optional AnimMontage to play when performing a double jump (fallback / default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement|Montages")
	TObjectPtr<UAnimMontage> DoubleJumpMontage;

	/** Neutral / In-Place double jump montage (when no WASD input) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement|Montages")
	TObjectPtr<UAnimMontage> DoubleJumpNeutralMontage;

	/** Forward double jump montage (W) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement|Montages")
	TObjectPtr<UAnimMontage> DoubleJumpForwardMontage;

	/** Backward double jump montage (S) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement|Montages")
	TObjectPtr<UAnimMontage> DoubleJumpBackwardMontage;

	/** Left double jump montage (A) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement|Montages")
	TObjectPtr<UAnimMontage> DoubleJumpLeftMontage;

	/** Right double jump montage (D) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement|Montages")
	TObjectPtr<UAnimMontage> DoubleJumpRightMontage;

	/** Optional Sound to play on double jump */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement")
	TObjectPtr<USoundBase> DoubleJumpSound;

	/** Optional VFX/Niagara effect to spawn on double jump */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement")
	TObjectPtr<UNiagaraSystem> DoubleJumpEffect;

	/** Event broadcasted when double jump is performed */
	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Movement")
	FOnDoubleJumpDelegate OnDoubleJumped;

	/** Blueprint event called when double jump occurs */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|Movement")
	void BP_OnDoubleJump();

	/** Called when a double jump is performed */
	virtual void OnDoubleJump();

	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;

	/** Returns true if character is in mid-air performing a multi/double jump */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Movement")
	bool IsDoubleJumping() const;

	/** Flag tracking if in-air double jump has occurred during current airborne period */
	UPROPERTY(Transient)
	bool bHasDoubleJumpedInAir;

	/** Overridden from ACharacter to trigger double jump directly when jumping in air */
	virtual void Jump() override;
	virtual void Landed(const FHitResult& Hit) override;

	/** Overridden from ACharacter to ensure air jumps in GASP trigger OnJumped reliably */
	virtual void CheckJumpInput(float DeltaTime) override;

	/** Overridden from ACharacter to detect double jumps */
	virtual void OnJumped_Implementation() override;

	/** Sets the max jump count dynamically (e.g. power-ups) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Movement")
	void SetMaxJumpCount(int32 NewMaxJumpCount);

	/** Explicitly performs double jump */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Movement")
	void PerformDoubleJump();
};
