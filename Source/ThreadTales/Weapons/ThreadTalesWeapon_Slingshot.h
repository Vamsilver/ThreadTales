// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreadTalesWeapon.h"
#include "ThreadTalesWeapon_Slingshot.generated.h"

class UArrowComponent;
class AThreadTalesProjectile;
class UAnimMontage;
class USoundBase;

/**
 * Slingshot weapon for ranged attacks with charge-up and projectile firing.
 */
UCLASS()
class THREADTALES_API AThreadTalesWeapon_Slingshot : public AThreadTalesWeapon
{
	GENERATED_BODY()
	
public:
	AThreadTalesWeapon_Slingshot();

	/** Spawns and fires a projectile from the slingshot */
	UFUNCTION(BlueprintCallable, Category = "Slingshot")
	AThreadTalesProjectile* FireProjectile(FVector SpawnLocation, FRotator AimRotation, float ChargeAlpha);

	/** Returns projectile class */
	TSubclassOf<AThreadTalesProjectile> GetProjectileClass() const { return ProjectileClass; }

	/** Charge timings */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slingshot|Charge")
	float MinChargeTime;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slingshot|Charge")
	float MaxChargeTime;

	/** Damage scaling */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slingshot|Damage")
	float MinDamage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slingshot|Damage")
	float MaxDamage;

	/** Montages for Aim and Fire */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slingshot|Animation")
	UAnimMontage* AimMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slingshot|Animation")
	UAnimMontage* FireMontage;

	/** Audio effects */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slingshot|Audio")
	USoundBase* PullSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slingshot|Audio")
	USoundBase* ReleaseSound;

	/** Projectile class to spawn */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slingshot|Projectile")
	TSubclassOf<AThreadTalesProjectile> ProjectileClass;

	/** Arrow indicating muzzle / projectile release point */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slingshot|Components")
	UArrowComponent* ProjectileSpawnPoint;

protected:
	virtual void BeginPlay() override;

	// Max ammo capacity
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Slingshot|Ammo")
	int32 MaxAmmo;

	// Current ammo count
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Slingshot|Ammo")
	int32 CurrentAmmo;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
