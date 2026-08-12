// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThreadTalesEquipmentComponent.generated.h"

class AThreadTalesWeapon;

/**
 * Component responsible for managing equipped items/weapons.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THREADTALES_API UThreadTalesEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UThreadTalesEquipmentComponent();

protected:
	virtual void BeginPlay() override;

public:	
	// Equips a specific weapon. If called on client, forwards to server.
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void EquipWeapon(TSubclassOf<AThreadTalesWeapon> WeaponClass);

	UFUNCTION(Server, Reliable)
	void ServerEquipWeapon(TSubclassOf<AThreadTalesWeapon> WeaponClass);

	// Unequips current weapon
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void UnequipWeapon();

	UFUNCTION(Server, Reliable)
	void ServerUnequipWeapon();

	// Returns the currently equipped weapon
	UFUNCTION(BlueprintPure, Category = "Equipment")
	AThreadTalesWeapon* GetCurrentWeapon() const { return CurrentWeapon; }

protected:
	// The weapon currently held by the character
	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon, Transient)
	AThreadTalesWeapon* CurrentWeapon;

	UFUNCTION()
	void OnRep_CurrentWeapon(AThreadTalesWeapon* OldWeapon);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
