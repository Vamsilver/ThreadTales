// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "ThreadTalesCharacter.generated.h"

class UThreadTalesAbilitySystemComponent;
class UThreadTalesAttributeSet;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class AThreadTalesCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AThreadTalesCharacter();

	// Implement IAbilitySystemInterface
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

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
};
