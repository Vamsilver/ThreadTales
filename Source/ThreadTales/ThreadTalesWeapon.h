// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayAbilitySpec.h"
#include "ThreadTalesWeapon.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;

UCLASS(Abstract)
class THREADTALES_API AThreadTalesWeapon : public AActor
{
	GENERATED_BODY()
	
public:	
	AThreadTalesWeapon();

	// Called when this weapon is equipped by an actor
	virtual void Equip(AActor* NewOwner);

	// Called when this weapon is unequipped
	virtual void Unequip();

protected:
	virtual void BeginPlay() override;

	// The mesh of the weapon
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* WeaponMesh;

	// The abilities granted when this weapon is equipped
	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;

	// Handles to the granted abilities so we can remove them later
	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;

	// Cache of the owner's ASC
	UPROPERTY()
	UAbilitySystemComponent* OwnerASC;
};
