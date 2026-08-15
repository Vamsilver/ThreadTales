// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreadTalesWeapon.h"
#include "ThreadTalesWeapon_Slingshot.generated.h"

class UArrowComponent;

/**
 * Slingshot weapon for ranged attacks.
 */
UCLASS()
class THREADTALES_API AThreadTalesWeapon_Slingshot : public AThreadTalesWeapon
{
	GENERATED_BODY()
	
public:
	AThreadTalesWeapon_Slingshot();

protected:
	virtual void BeginPlay() override;

	// Indicates where the projectile should spawn and fly towards
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UArrowComponent* ProjectileSpawnPoint;

	// Max ammo capacity
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Ammo")
	int32 MaxAmmo;

	// Current ammo count
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Combat|Ammo")
	int32 CurrentAmmo;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
