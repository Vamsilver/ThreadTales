// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ThreadTalesProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/**
 * Physical projectile (e.g. Slingshot rock) with ballistic physics, bounce, and damage on impact.
 */
UCLASS()
class THREADTALES_API AThreadTalesProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	AThreadTalesProjectile();

	/** Initializes projectile speed, gravity, and damage according to slingshot charge alpha (0.0 .. 1.0) */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void InitializeProjectile(float ChargeAlpha, float InBaseDamage);

	/** Sphere collision component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> CollisionComp;

	/** Visual mesh of the projectile (e.g. rock) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> ProjectileMesh;

	/** Projectile movement component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** Base damage dealt on impact */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile|Combat")
	float BaseDamage;

	/** GameplayEffect class used to apply damage via GAS */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Combat")
	TSubclassOf<class UGameplayEffect> DamageEffectClass;

	/** Sound played when the projectile hits an object */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Effects")
	TObjectPtr<USoundBase> ImpactSound;

	/** Particle effect spawned on impact */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Effects")
	TObjectPtr<UParticleSystem> ImpactEmitter;

protected:
	virtual void BeginPlay() override;

	/** Called when the projectile hits something */
	UFUNCTION()
	virtual void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Max bounces before projectile stops */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Physics", meta = (AllowPrivateAccess = "true"))
	int32 MaxBounces = 2;

	int32 CurrentBounceCount = 0;
	bool bHasDealtDamage = false;
};
