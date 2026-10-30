// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ThreadTalesAttributeSet.generated.h"

// Macro to automatically generate getter and setter functions for attributes
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

class AThreadTalesBaseCharacter;

/**
 * Base Attribute Set containing basic properties like Health, MaxHealth.
 */
UCLASS()
class THREADTALES_API UThreadTalesAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UThreadTalesAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	/** Applies direct incoming damage to this AttributeSet and notifies the owning character */
	void ReceiveDamage(float DamageAmount, AActor* SourceActor = nullptr);

	/** Resets current health back to MaxHealth */
	void ResetHealth();

	/** Resets current stamina back to MaxStamina */
	void ResetStamina();

	/** Resets current poise back to MaxPoise */
	void ResetPoise();

private:
	void HandleIncomingDamage(const struct FGameplayEffectModCallbackData& Data);
	void HandleIncomingPoiseDamage(const struct FGameplayEffectModCallbackData& Data);

	bool ProcessParryDeflect(AThreadTalesBaseCharacter* TTChar, AActor* EffectiveAttacker, AActor* TargetActor, AThreadTalesBaseCharacter* AttackerChar, bool bFacingAttacker);
	bool ProcessBlockMitigation(AThreadTalesBaseCharacter* TTChar, AActor* EffectiveAttacker, AThreadTalesBaseCharacter* AttackerChar, float LocalDamageDone, bool bFacingAttacker);

public:
	// Current Health
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Health", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UThreadTalesAttributeSet, Health)

	// Maximum Health
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Health", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UThreadTalesAttributeSet, MaxHealth)

	// Current Stamina
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Stamina", ReplicatedUsing = OnRep_Stamina)
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UThreadTalesAttributeSet, Stamina)

	// Maximum Stamina
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Stamina", ReplicatedUsing = OnRep_MaxStamina)
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS(UThreadTalesAttributeSet, MaxStamina)

	// Current Poise (Stagger resistance)
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Poise", ReplicatedUsing = OnRep_Poise)
	FGameplayAttributeData Poise;
	ATTRIBUTE_ACCESSORS(UThreadTalesAttributeSet, Poise)

	// Maximum Poise
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Poise", ReplicatedUsing = OnRep_MaxPoise)
	FGameplayAttributeData MaxPoise;
	ATTRIBUTE_ACCESSORS(UThreadTalesAttributeSet, MaxPoise)

	// Meta Attribute: Incoming Damage (Server-side only, modifies Health directly)
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Damage")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UThreadTalesAttributeSet, Damage)

	// Meta Attribute: Incoming Poise Damage (Server-side only, modifies Poise directly)
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Damage")
	FGameplayAttributeData PoiseDamage;
	ATTRIBUTE_ACCESSORS(UThreadTalesAttributeSet, PoiseDamage)

	// Functions for network replication
	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	virtual void OnRep_Stamina(const FGameplayAttributeData& OldStamina);

	UFUNCTION()
	virtual void OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina);

	UFUNCTION()
	virtual void OnRep_Poise(const FGameplayAttributeData& OldPoise);

	UFUNCTION()
	virtual void OnRep_MaxPoise(const FGameplayAttributeData& OldMaxPoise);

private:
	/** Monotonic timestamp of the last successful parry deflection (prevents duplicate hit reacts across multi-attribute damage specs) */
	double LastParryDeflectTimestamp = -1.0;
};
