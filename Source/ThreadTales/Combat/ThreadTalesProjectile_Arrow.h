// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/ThreadTalesProjectile.h"
#include "ThreadTalesProjectile_Arrow.generated.h"

class UNiagaraSystem;

/**
 * Ballistic arrow projectile for Bow Hunter archetype.
 * Scales initial velocity, damage, and poise damage from bow draw tension.
 * Embeds into hit targets or environment surfaces without bouncing.
 */
UCLASS()
class THREADTALES_API AThreadTalesProjectile_Arrow : public AThreadTalesProjectile
{
	GENERATED_BODY()

public:
	AThreadTalesProjectile_Arrow();

	/** Initializes speed, gravity, and combat damage/poise values according to bow draw ratio (0.0 .. 1.0) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Projectile")
	void InitializeArrow(float DrawRatio, float InBaseDamage, float InPoiseDamage, AActor* InShooter, const FVector& InLaunchDirection = FVector::ZeroVector);

	/** Returns whether arrow has already struck a surface or character */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Projectile")
	bool HasStruck() const { return bHasStruck; }

	/** Poise damage dealt to target on impact (contributes to enemy Posture Break) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Combat")
	float PoiseDamage;

	/** Optional Niagara VFX spawned on impact */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Effects")
	TObjectPtr<UNiagaraSystem> ImpactNiagara;

	/** Minimum ballistic flight speed at low draw tension (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Physics", meta = (ClampMin = "500.0", ToolTip = "Minimum arrow speed in cm/s at low draw tension"))
	float MinFlightSpeed = 2500.0f;

	/** Maximum ballistic flight speed at full draw tension (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Physics", meta = (ClampMin = "1000.0", ToolTip = "Maximum arrow speed in cm/s at full draw tension"))
	float MaxFlightSpeed = 7500.0f;

	/** Ballistic gravity scale at minimum draw tension (higher drop for short arcs) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Physics", meta = (ClampMin = "0.0", ToolTip = "Gravity scale at low draw tension"))
	float MinDrawGravityScale = 0.30f;

	/** Ballistic gravity scale at maximum draw tension (flat pinpoint trajectory) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Physics", meta = (ClampMin = "0.0", ToolTip = "Gravity scale at full draw tension"))
	float MaxDrawGravityScale = 0.06f;

	/** Maximum number of stuck/embedded arrows preserved in the world before the oldest is recycled */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Physics", meta = (ClampMin = "1", ToolTip = "Maximum number of stuck arrows before FIFO recycling"))
	int32 MaxEmbeddedArrows = 10;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Called when the arrow hits something */
	virtual void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;

	/** Embedded penetration depth into the hit surface */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Physics", meta = (AllowPrivateAccess = "true"))
	float PenetrationDepth;

	/** Whether the arrow has already struck something and embedded */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|State", meta = (AllowPrivateAccess = "true"))
	bool bHasStruck;
};
