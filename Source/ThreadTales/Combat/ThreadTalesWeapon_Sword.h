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
	UBoxComponent* Hitbox;

	UFUNCTION()
	void OnHitboxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

public:
	// Enables or disables the hitbox (called from Animation Notifies)
	virtual void SetHitboxEnabled(bool bEnabled) override;

	/** Base damage dealt by sword strikes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat")
	float BaseDamage;

private:
	/** Tracks actors already hit during the current attack swing */
	TSet<TWeakObjectPtr<AActor>> HitActorsThisSwing;
};
