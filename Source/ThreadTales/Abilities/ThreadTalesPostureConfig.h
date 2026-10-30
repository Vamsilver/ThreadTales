// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ThreadTalesPostureConfig.generated.h"

/**
 * Dedicated Data Asset defining all Sekiro-style Posture (Concentration),
 * Frontal Guard / Block mitigation, Perfect Parry (Deflection), and Stagger tuning parameters.
 */
UCLASS(BlueprintType, Const)
class THREADTALES_API UThreadTalesPostureConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ==========================================
	// 1. POSTURE POOL & SEKIRO CONCENTRATION REGEN
	// ==========================================

	/** Default maximum Posture / Poise pool capacity (100.0 = 0% to 100% tension scale) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Pool & Regen", meta = (ClampMin = "1.0"))
	float MaxPosture = 100.0f;

	/** Base passive Posture recovery rate (points per second) when not taking hits or blocking strikes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Pool & Regen", meta = (ClampMin = "0.0"))
	float PostureRegenPerSecond = 20.0f;

	/** Sekiro Concentration multiplier applied to PostureRegenPerSecond while holding Block (e.g. 2.0 = 2x faster cooling) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Pool & Regen", meta = (ClampMin = "0.1"))
	float BlockingPostureRegenMultiplier = 2.0f;

	/** Delay in seconds after taking a direct hit or blocking an attack before Posture begins cooling down */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Pool & Regen", meta = (ClampMin = "0.0"))
	float PostureRegenDelay = 1.10f;

	// ==========================================
	// 2. FRONTAL BLOCK & POSTURE BUILDUP
	// ==========================================

	/** Half-angle in degrees of the frontal defense cone (95.0 = 190 deg front hemisphere; attacks outside bypass guard) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Block & Guard", meta = (ClampMin = "15.0", ClampMax = "180.0"))
	float FrontalGuardHalfAngleDegrees = 95.0f;

	/** Fraction of incoming HP damage absorbed on a successful standard block (1.0 = 100% HP protection, 0 chip damage) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Block & Guard", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlockHpDamageAbsorption = 1.0f;

	/** Ratio of incoming PoiseDamage converted into Posture buildup when blocking a hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Block & Guard", meta = (ClampMin = "0.0"))
	float BlockPostureDamageRatio = 0.50f;

	/** Minimum Posture buildup added on every blocked attack */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Block & Guard", meta = (ClampMin = "0.0"))
	float BlockMinPostureCost = 15.0f;

	/** Multiplier applied to incoming PoiseDamage on unblocked direct hits */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Block & Guard", meta = (ClampMin = "0.0"))
	float UnblockedPostureDamageMultiplier = 1.0f;

	// ==========================================
	// 3. PERFECT PARRY / DEFLECTION
	// ==========================================

	/** Active Perfect Parry (Deflect) window duration in seconds upon pressing Block (RMB) (default: 0.20s, precision Sekiro timing) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Deflect & Parry", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float ParryWindowDuration = 0.20f;

	/** Amount of player's own Posture tension relieved (restored) on a successful Perfect Parry */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Deflect & Parry", meta = (ClampMin = "0.0"))
	float ParryPostureRelief = 15.0f;

	/** Hit-stop freeze duration (seconds) applied to the defender on a successful Perfect Parry */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Deflect & Parry", meta = (ClampMin = "0.0", ClampMax = "0.25"))
	float ParryDefenderHitStop = 0.07f;

	/** Hit-stop freeze duration (seconds) applied to the deflected attacker on a Perfect Parry */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Deflect & Parry", meta = (ClampMin = "0.0", ClampMax = "0.25"))
	float ParryAttackerHitStop = 0.08f;

	/** Amount of Posture/Poise damage dealt to the attacker upon a successful Perfect Parry deflect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Deflect & Parry", meta = (ClampMin = "0.0"))
	float ParryDeflectPostureDamage = 60.0f;

	// ==========================================
	// 4. GUARD PUSHBACK, HIT-STOP & STAGGER
	// ==========================================

	/** Remaining Poise threshold at or below which a blocked hit triggers heavy guard reaction and heavy pushback (e.g. <= 35 Poise = >= 65% Posture buildup) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Reactions & Stagger", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float HeavyGuardRemainingPoiseThreshold = 35.0f;

	/** Knockback impulse speed (cm/s) applied to defender on a normal blocked hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Reactions & Stagger", meta = (ClampMin = "0.0"))
	float NormalGuardPushbackForce = 140.0f;

	/** Knockback impulse speed (cm/s) applied to defender on a heavy blocked hit (when Posture tension is high) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Reactions & Stagger", meta = (ClampMin = "0.0"))
	float HeavyGuardPushbackForce = 280.0f;

	/** Hit-stop freeze duration (seconds) on a standard blocked hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Reactions & Stagger", meta = (ClampMin = "0.0", ClampMax = "0.25"))
	float BlockHitStopDuration = 0.05f;

	/** Hit-stop freeze duration (seconds) when Posture breaks (100% fill -> Stagger) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Reactions & Stagger", meta = (ClampMin = "0.0", ClampMax = "0.25"))
	float PostureBreakHitStopDuration = 0.08f;

	/** Duration in seconds of the extended Stun / Groggy state when an enemy's Posture breaks (0 Poise / 100% Posture) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Reactions & Stagger", meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float PostureBreakStunDuration = 3.75f;
};
