// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThreadTalesWeapon.h"
#include "ThreadTalesEquipmentComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStanceChanged, bool, bInCombat);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponEquippedDelegate, AThreadTalesWeapon*, NewWeapon);

/**
 * Component responsible for managing equipped items, weapon drawing/sheathing, and combat stance transitions.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THREADTALES_API UThreadTalesEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UThreadTalesEquipmentComponent();

	/** Equips a specific weapon class. Replicated to server if called on client. */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Equipment")
	void EquipWeapon(TSubclassOf<AThreadTalesWeapon> WeaponClass);

	UFUNCTION(Server, Reliable)
	void ServerEquipWeapon(TSubclassOf<AThreadTalesWeapon> WeaponClass);

	/** Unequips and destroys current weapon */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Equipment")
	void UnequipWeapon();

	UFUNCTION(Server, Reliable)
	void ServerUnequipWeapon();

	/** Toggles between Sheathed (peaceful) and Drawn (combat) state */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Combat")
	void ToggleCombatStance();

	UFUNCTION(Server, Reliable)
	void ServerToggleCombatStance();

	/** Draws the weapon from sheath into hand */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Combat")
	void DrawWeapon();

	UFUNCTION(Server, Reliable)
	void ServerDrawWeapon();

	/** Returns the weapon from hand back to sheath */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Combat")
	void SheathWeapon();

	UFUNCTION(Server, Reliable)
	void ServerSheathWeapon();

	/** Triggers weapon primary attack or queues the next combo strike if already attacking */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Combat")
	void PerformPrimaryAttack();

	/** Called when the attack button is released (e.g. to release slingshot charge) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Combat")
	void ReleasePrimaryAttack();

	/** Triggers 8-directional Evade / Dodge */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Combat")
	void PerformEvade();

	/** Returns true if character currently has a weapon drawn and ready for combat */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Combat")
	bool IsInCombatStance() const;

	/** Returns current weapon combat state */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Combat")
	EWeaponCombatState GetCurrentWeaponCombatState() const;

	/** Returns the currently equipped weapon */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Equipment")
	AThreadTalesWeapon* GetCurrentWeapon() const { return CurrentWeapon; }

	/** Event triggered when entering or exiting combat stance */
	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Combat")
	FOnCombatStanceChanged OnCombatStanceChanged;

	/** Event triggered when a weapon is equipped */
	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Equipment")
	FOnWeaponEquippedDelegate OnWeaponEquipped;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The weapon currently held by the character */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon, Transient)
	AThreadTalesWeapon* CurrentWeapon;

	UFUNCTION()
	void OnRep_CurrentWeapon(AThreadTalesWeapon* OldWeapon);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetMovementInputBlocked(bool bBlocked);

	UFUNCTION()
	void OnEquipMontageEnded(UAnimMontage* Montage, bool bInterrupted);
};
