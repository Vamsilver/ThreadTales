// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPtr.h"
#include "ThreadTalesBowConfig.generated.h"

class UAnimMontage;

/**
 * Data-Driven configuration for the Archer / Bow Hunter combat system (BP_HeroFemale).
 * Defines 3-tier power charge parameters (LV1/LV2/LV3), UpperBody draw/release montages,
 * 4-hit hipfire quick-shot combo, directional Dodge-to-Shot, and close-quarters Bow Parry.
 */
UCLASS(BlueprintType, Const)
class THREADTALES_API UThreadTalesBowConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ==========================================
	// 1. 3-TIER CHARGE THRESHOLDS & TIMINGS
	// ==========================================

	/** Minimum draw ratio (0.0 to 1.0) required to fire an arrow; releasing below this cancels cleanly */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Charge", meta = (ClampMin = "0.01", ClampMax = "0.50", ToolTip = "Minimum draw ratio (0.0 to 1.0 ratio) required to fire an arrow; releasing below this cancels cleanly."))
	float MinReleaseThreshold = 0.10f;

	/** Draw time in seconds required to reach LV2 (Full Draw) tier */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Charge", meta = (ClampMin = "0.15", ClampMax = "2.00", ToolTip = "Hold duration in seconds required to transition from LV1 Quick Shot to LV2 Full Draw."))
	float LV2ChargeTimeSeconds = 0.45f;

	/** Draw time in seconds required to reach LV3 (Overcharged Piercing Shot) tier */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Charge", meta = (ClampMin = "0.30", ClampMax = "3.00", ToolTip = "Hold duration in seconds required to transition from LV2 Full Draw to LV3 Overcharged Piercing Shot."))
	float LV3ChargeTimeSeconds = 0.90f;

	// ==========================================
	// 2. BALLISTICS & DAMAGE PER CHARGE TIER
	// ==========================================

	/** Arrow launch speed in cm/s for LV1 Quick Shot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Ballistics", meta = (ClampMin = "500.0", ClampMax = "15000.0", ToolTip = "Initial projectile speed in cm/s for LV1 Quick Shot."))
	float LV1ArrowSpeed = 3500.0f;

	/** Arrow launch speed in cm/s for LV2 Full Draw */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Ballistics", meta = (ClampMin = "500.0", ClampMax = "15000.0", ToolTip = "Initial projectile speed in cm/s for LV2 Full Draw."))
	float LV2ArrowSpeed = 5200.0f;

	/** Arrow launch speed in cm/s for LV3 Overcharged Piercing Shot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Ballistics", meta = (ClampMin = "500.0", ClampMax = "15000.0", ToolTip = "Initial projectile speed in cm/s for LV3 Overcharged Piercing Shot."))
	float LV3ArrowSpeed = 7500.0f;

	/** Gravity scale multiplier for LV1 Quick Shot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Ballistics", meta = (ClampMin = "0.0", ClampMax = "2.0", ToolTip = "Projectile gravity scale multiplier (ratio) for LV1 Quick Shot."))
	float LV1GravityScale = 0.25f;

	/** Gravity scale multiplier for LV2 Full Draw */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Ballistics", meta = (ClampMin = "0.0", ClampMax = "2.0", ToolTip = "Projectile gravity scale multiplier (ratio) for LV2 Full Draw."))
	float LV2GravityScale = 0.15f;

	/** Gravity scale multiplier for LV3 Overcharged Piercing Shot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Ballistics", meta = (ClampMin = "0.0", ClampMax = "2.0", ToolTip = "Projectile gravity scale multiplier (ratio) for LV3 Overcharged Piercing Shot."))
	float LV3GravityScale = 0.06f;

	/** Health damage in points for LV1 Quick Shot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Damage", meta = (ClampMin = "1.0", ClampMax = "500.0", ToolTip = "Health damage in points dealt on hit by LV1 Quick Shot."))
	float LV1HealthDamage = 25.0f;

	/** Health damage in points for LV2 Full Draw */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Damage", meta = (ClampMin = "1.0", ClampMax = "500.0", ToolTip = "Health damage in points dealt on hit by LV2 Full Draw."))
	float LV2HealthDamage = 45.0f;

	/** Health damage in points for LV3 Overcharged Piercing Shot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Damage", meta = (ClampMin = "1.0", ClampMax = "500.0", ToolTip = "Health damage in points dealt on hit by LV3 Overcharged Piercing Shot."))
	float LV3HealthDamage = 75.0f;

	/** Poise / Posture damage in points for LV1 Quick Shot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Damage", meta = (ClampMin = "0.0", ClampMax = "500.0", ToolTip = "Posture / Poise damage in points dealt on hit by LV1 Quick Shot."))
	float LV1PoiseDamage = 15.0f;

	/** Poise / Posture damage in points for LV2 Full Draw */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Damage", meta = (ClampMin = "0.0", ClampMax = "500.0", ToolTip = "Posture / Poise damage in points dealt on hit by LV2 Full Draw."))
	float LV2PoiseDamage = 30.0f;

	/** Poise / Posture damage in points for LV3 Overcharged Piercing Shot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Damage", meta = (ClampMin = "0.0", ClampMax = "500.0", ToolTip = "Posture / Poise damage in points dealt on hit by LV3 Overcharged Piercing Shot."))
	float LV3PoiseDamage = 55.0f;

	// ==========================================
	// 3. UPPER-BODY AIMED DRAW & RELEASE MONTAGES
	// ==========================================

	/** UpperBody ready aim stance montage (AM_Bow_Aim_Ready) played on RMB hold */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Animation", meta = (ToolTip = "UpperBody ready aim stance montage (AM_Bow_Aim_Ready) played on RMB hold."))
	TSoftObjectPtr<UAnimMontage> AimReadyMontage;

	/** Playback rate for the AimReady arrow nocking montage (0.42 = ~0.62s realistic quiver draw) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Animation", meta = (ClampMin = "0.1", ClampMax = "2.0", ToolTip = "Playback rate for the AimReady arrow nocking montage."))
	float AimReadyPlayRate = 0.42f;

	/** Delay in seconds from AimReady start when arrow appears in fingers */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Animation", meta = (ClampMin = "0.05", ClampMax = "1.0", ToolTip = "Delay in seconds from AimReady start when cosmetic nocked arrow appears in fingers."))
	float NockArrowDelaySeconds = 0.28f;

	/** UpperBody montage playing Start (0.42s) -> looping Loop while LMB is held (Default fallback) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Animation", meta = (ToolTip = "UpperBody montage (AM_Bow_Draw_Hold) with Start (0.42s) and looping Loop sections."))
	TSoftObjectPtr<UAnimMontage> DrawHoldMontage;

	/** Draw hold montage for LV1 tier (AS_Skill_04_Charge_LV1 or AM_Bow_Draw_Hold) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Animation", meta = (ToolTip = "Draw hold montage for LV1 tier."))
	TSoftObjectPtr<UAnimMontage> DrawHoldMontageLV1;

	/** Draw hold montage for LV2 tier (AS_Skill_04_Charge_LV2 or deeper hold) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Animation", meta = (ToolTip = "Draw hold montage for LV2 tier."))
	TSoftObjectPtr<UAnimMontage> DrawHoldMontageLV2;

	/** Draw hold montage for LV3 tier (AS_Skill_04_Charge_LV3 or heavy overcharge hold) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Animation", meta = (ToolTip = "Draw hold montage for LV3 tier."))
	TSoftObjectPtr<UAnimMontage> DrawHoldMontageLV3;

	/** UpperBody release recoil montage for LV1 Quick Shot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Animation", meta = (ToolTip = "UpperBody release recoil montage (AM_Bow_Release_LV1) for LV1 Quick Shot."))
	TSoftObjectPtr<UAnimMontage> ReleaseMontageLV1;

	/** UpperBody release recoil montage for LV2 Full Draw */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Animation", meta = (ToolTip = "UpperBody release recoil montage (AM_Bow_Release_LV2) for LV2 Full Draw."))
	TSoftObjectPtr<UAnimMontage> ReleaseMontageLV2;

	/** UpperBody release recoil montage for LV3 Overcharged Piercing Shot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Animation", meta = (ToolTip = "UpperBody release recoil montage (AM_Bow_Release_LV3) for LV3 Overcharged Piercing Shot."))
	TSoftObjectPtr<UAnimMontage> ReleaseMontageLV3;

	// ==========================================
	// 4. HIPFIRE QUICK-SHOT COMBO & DODGE-SHOT
	// ==========================================

	/** 4-stage hipfire quick-shot combo montages (LMB without RMB) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|QuickCombo", meta = (ToolTip = "Ordered 4-stage hipfire quick-shot combo montages (AM_Bow_Combo_01..04)."))
	TArray<TSoftObjectPtr<UAnimMontage>> QuickShotComboMontages;

	/** 4-directional Dodge-to-Shot montages: Index 0=Forward, 1=Backward, 2=Left, 3=Right */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|QuickCombo", meta = (ToolTip = "4-directional Dodge-to-Shot counter montages (0=F, 1=B, 2=L, 3=R)."))
	TArray<TSoftObjectPtr<UAnimMontage>> DodgeShotMontages;

	/** Normalized fire delay timestamp in seconds within Quick-Shot combo montages */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|QuickCombo", meta = (ClampMin = "0.05", ClampMax = "1.50", ToolTip = "Delay in seconds from montage start when the quick-shot arrow projectile is spawned."))
	float QuickShotFireDelaySeconds = 0.28f;

	// ==========================================
	// 5. CLOSE-QUARTERS BOW PARRY & COUNTER-SHOT
	// ==========================================

	/** Close-quarters Bow Parry deflection montages (Left / Right) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Parry", meta = (ToolTip = "Close-quarters bow parry deflection montages (AM_Bow_Parry_L / AM_Bow_Parry_R)."))
	TArray<TSoftObjectPtr<UAnimMontage>> BowParryMontages;

	/** Point-blank Parry Counter-Shot montages (Left / Right) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Parry", meta = (ToolTip = "Point-blank parry counter-shot montages (AM_Bow_ParryCounter_L / AM_Bow_ParryCounter_R)."))
	TArray<TSoftObjectPtr<UAnimMontage>> BowParryCounterMontages;

	/** Maximum distance in cm to an attacking enemy within which pressing RMB triggers a Bow Parry deflect window */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Parry", meta = (ClampMin = "50.0", ClampMax = "600.0", ToolTip = "Maximum distance in cm to an attacking enemy for triggering a close-quarters bow parry."))
	float BowParryThreatRadiusCm = 280.0f;

	/** Evaluates the active charge tier (1 = LV1, 2 = LV2, 3 = LV3) for a given hold duration in seconds */
	UFUNCTION(BlueprintPure, Category = "Bow|Charge")
	int32 EvaluateChargeTier(float HoldDurationSeconds) const;

	/** Returns the projectile speed in cm/s for the specified charge tier (1..3) */
	UFUNCTION(BlueprintPure, Category = "Bow|Ballistics")
	float GetSpeedForTier(int32 ChargeTier) const;

	/** Returns the projectile gravity scale for the specified charge tier (1..3) */
	UFUNCTION(BlueprintPure, Category = "Bow|Ballistics")
	float GetGravityForTier(int32 ChargeTier) const;

	/** Returns the health damage in points for the specified charge tier (1..3) */
	UFUNCTION(BlueprintPure, Category = "Bow|Damage")
	float GetHealthDamageForTier(int32 ChargeTier) const;

	/** Returns the poise/posture damage in points for the specified charge tier (1..3) */
	UFUNCTION(BlueprintPure, Category = "Bow|Damage")
	float GetPoiseDamageForTier(int32 ChargeTier) const;

	/** Loads and returns the UpperBody release recoil montage for the specified charge tier (1..3) */
	UFUNCTION(BlueprintPure, Category = "Bow|Animation")
	UAnimMontage* GetReleaseMontageForTier(int32 ChargeTier) const;

	/** Loads and returns the UpperBody ready aim stance montage */
	UFUNCTION(BlueprintPure, Category = "Bow|Animation")
	UAnimMontage* GetAimReadyMontage() const;

	/** Returns the playback rate for the AimReady arrow nocking montage */
	UFUNCTION(BlueprintPure, Category = "Bow|Animation")
	float GetAimReadyPlayRate() const { return AimReadyPlayRate; }

	/** Returns the delay in seconds before nocking the cosmetic arrow */
	UFUNCTION(BlueprintPure, Category = "Bow|Animation")
	float GetNockArrowDelaySeconds() const { return NockArrowDelaySeconds; }

	/** Loads and returns the draw hold montage for the specified charge tier (1..3) */
	UFUNCTION(BlueprintPure, Category = "Bow|Animation")
	UAnimMontage* GetDrawHoldMontageForTier(int32 ChargeTier) const;

	/** Loads and returns the UpperBody draw hold montage (AM_Bow_Draw_Hold) */
	UFUNCTION(BlueprintPure, Category = "Bow|Animation")
	UAnimMontage* GetDrawHoldMontage() const;

	/** Loads and returns the close-quarters bow parry deflection montage (0 = Left, 1 = Right) */
	UFUNCTION(BlueprintPure, Category = "Bow|Parry")
	UAnimMontage* GetParryMontage(int32 DirectionIndex = 0) const;

	/** Loads and returns the point-blank parry counter-shot montage (0 = Left, 1 = Right) */
	UFUNCTION(BlueprintPure, Category = "Bow|Parry")
	UAnimMontage* GetParryCounterMontage(int32 DirectionIndex = 0) const;
};
