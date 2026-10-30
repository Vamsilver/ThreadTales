// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ThreadTalesStaminaConfig.generated.h"

/**
 * Data Asset defining all Souls-like Stamina economy parameters:
 * discrete action costs, continuous drain rates, regeneration rate, and recovery delays.
 */
UCLASS(BlueprintType, Const)
class THREADTALES_API UThreadTalesStaminaConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ==========================================
	// MELEE ATTACK COSTS
	// ==========================================

	/** Stamina cost per standard combo strike (Strike 1..N) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Combat", meta = (ClampMin = "0.0"))
	float LightAttackCost = 10.0f;

	/** Stamina cost for counter-slash out of Evade / Dodge (AM_Attack_Dodge) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Combat", meta = (ClampMin = "0.0"))
	float DodgeAttackCost = 10.0f;

	/** Stamina cost for running slash out of Sprint (AM_Attack_Sprint) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Combat", meta = (ClampMin = "0.0"))
	float SprintAttackCost = 12.0f;

	// ==========================================
	// EVASION & JUMP COSTS
	// ==========================================

	/** Stamina cost for 8-directional ground evade / dodge roll (GA_Evade) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Movement", meta = (ClampMin = "0.0"))
	float GroundEvadeCost = 14.0f;

	/** Stamina cost for mid-air evade / air dash (GA_AirEvade) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Movement", meta = (ClampMin = "0.0"))
	float AirEvadeCost = 14.0f;

	/** Stamina cost for standard ground jump (Jump) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Movement", meta = (ClampMin = "0.0"))
	float JumpCost = 8.0f;

	/** Stamina cost for mid-air double jump (GA_DoubleJump) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Movement", meta = (ClampMin = "0.0"))
	float DoubleJumpCost = 8.0f;

	/** Continuous stamina drain per second while sprinting and moving above sprint speed threshold */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Movement", meta = (ClampMin = "0.0"))
	float SprintDrainPerSecond = 8.0f;

	/** Minimum horizontal speed (cm/s) required while sprinting to drain stamina */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Movement", meta = (ClampMin = "50.0"))
	float SprintDrainMinSpeed = 300.0f;

	// ==========================================
	// DEFENSE, BLOCKING & SEKIRO POSTURE
	// ==========================================

	/** Minimum stamina deducted when blocking an incoming hit (0.0 in Sekiro Posture mode where block drains Posture instead) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Defense", meta = (ClampMin = "0.0"))
	float BlockHitMinCost = 0.0f;

	/** Ratio of incoming raw damage converted into stamina drain on block (0.0 in Sekiro Posture mode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Defense", meta = (ClampMin = "0.0"))
	float BlockHitDamageRatio = 0.0f;

	/** Whether holding Block (TAG_State_Combat_Blocking) pauses stamina regeneration (false in Sekiro mode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Defense")
	bool bPauseRegenWhileBlocking = false;

	/** Ratio of incoming PoiseDamage applied to Posture/Poise when blocking a hit (e.g. 0.50 = 15 Posture buildup on a 30 PoiseDamage hit) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Sekiro", meta = (ClampMin = "0.0"))
	float BlockPoiseDamageRatio = 0.50f;

	/** Minimum Posture/Poise buildup when blocking an incoming hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Sekiro", meta = (ClampMin = "0.0"))
	float BlockMinPoiseCost = 15.0f;

	/** Amount of own Posture/Poise recovered (tension relieved) upon executing a Perfect Parry / Deflect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Sekiro", meta = (ClampMin = "0.0"))
	float ParryPoiseRelief = 15.0f;

	/** Base passive Posture/Poise recovery per second when not taking hits */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Sekiro", meta = (ClampMin = "0.0"))
	float PoiseRegenPerSecond = 20.0f;

	/** Sekiro concentration multiplier applied to PoiseRegenPerSecond while holding Block (e.g. 2.0x faster Posture recovery) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Sekiro", meta = (ClampMin = "1.0"))
	float BlockingPoiseRegenMultiplier = 2.0f;

	/** Delay in seconds after taking a hit or blocking before Posture/Poise begins recovering */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture|Sekiro", meta = (ClampMin = "0.0"))
	float PoiseRegenDelay = 1.10f;

	// ==========================================
	// REGENERATION & RECOVERY DELAYS
	// ==========================================

	/** Passive stamina regeneration per second when not inhibited by combat actions or regen delay */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Regeneration", meta = (ClampMin = "0.0"))
	float StaminaRegenPerSecond = 35.0f;

	/** Recovery delay (seconds) before stamina regeneration resumes after spending stamina (when Stamina > 0 remains) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Regeneration", meta = (ClampMin = "0.0"))
	float NormalRegenDelay = 0.75f;

	/** Exhaustion penalty delay (seconds) before stamina regeneration resumes when Stamina is drained to 0.0 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Regeneration", meta = (ClampMin = "0.0"))
	float ExhaustedRegenDelay = 1.25f;
};
