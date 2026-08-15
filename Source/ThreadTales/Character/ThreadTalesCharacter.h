// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "ThreadTalesCharacter.generated.h"

class UThreadTalesAbilitySystemComponent;
class UThreadTalesAttributeSet;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChangedDelegate, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCharacterDeathDelegate, AActor*, DeadActor, AActor*, KillerActor);

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
	UPROPERTY()
	const UThreadTalesAttributeSet* AttributeSet;

	// Equipment Manager
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	class UThreadTalesEquipmentComponent* EquipmentComponent;

	/** Whether the character is currently alive */
	UPROPERTY(ReplicatedUsing = OnRep_IsAlive, BlueprintReadOnly, Category = "ThreadTales|Health")
	bool bIsAlive;

	UFUNCTION()
	virtual void OnRep_IsAlive();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// Helper function to trigger abilities (e.g. from Input Actions)
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Abilities")
	void ActivateAbilityByTag(FGameplayTag ContainerTag);

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
};
