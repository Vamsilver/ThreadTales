// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/ThreadTalesWeapon.h"
#include "ThreadTalesWeapon_Sword.generated.h"

class UBoxComponent;

/**
 * Sword weapon that supports combo attacks and parry.
 */
UCLASS()
class THREADTALES_API AThreadTalesWeapon_Sword : public AThreadTalesWeapon
{
	GENERATED_BODY()
	
public:
	AThreadTalesWeapon_Sword();

protected:
	virtual void BeginPlay() override;

	// Hitbox for detecting overlaps during swings
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UBoxComponent> Hitbox;

	UFUNCTION()
	void OnHitboxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

public:
	// Enables or disables the hitbox (called from Animation Notifies)
	virtual void SetHitboxEnabled(bool bEnabled) override;

	virtual void Tick(float DeltaTime) override;

	/** Toggle real-time visualization of weapon hitbox and impact traces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug")
	bool bShowDebugTrace;

	/** Returns true if debug visualization is enabled either via CVar (tt.Combat.DebugTrace) or property */
	bool ShouldDrawDebug() const;

	/** Base damage dealt by sword strikes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat")
	float BaseDamage;

	/** Base poise damage dealt by sword strikes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat")
	float BasePoiseDamage;

	/** GameplayEffect class used to apply damage and poise damage via GAS */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	TSubclassOf<class UGameplayEffect> DamageEffectClass;

private:
	/** Tracks actors already hit during the current attack swing */
	TSet<TWeakObjectPtr<AActor>> HitActorsThisSwing;

	/** True while attack swing hitbox is active */
	bool bHitboxActive;
};
