// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ThreadTalesGameplayEffects.generated.h"

/**
 * Instant Stamina Cost for Melee Attacks (-15 Stamina)
 */
UCLASS()
class THREADTALES_API UThreadTalesGE_CostMelee : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UThreadTalesGE_CostMelee();
};

/**
 * Instant Stamina Cost for Evade / Dodge (-20 Stamina)
 */
UCLASS()
class THREADTALES_API UThreadTalesGE_CostEvade : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UThreadTalesGE_CostEvade();
};

/**
 * Periodic Stamina Regeneration (+2.5 Stamina every 0.1s = 25/s)
 * Inhibited when attacking, evading, or when stamina regen is blocked by delay.
 */
UCLASS()
class THREADTALES_API UThreadTalesGE_StaminaRegen : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UThreadTalesGE_StaminaRegen();
};

/**
 * 1.0s Duration effect that grants TAG_State_Combat_StaminaRegenBlocked to delay regen after action.
 */
UCLASS()
class THREADTALES_API UThreadTalesGE_StaminaDelay : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UThreadTalesGE_StaminaDelay();
};

/**
 * 1.6s Exhaustion Duration effect that grants TAG_State_Combat_StaminaRegenBlocked when Stamina reaches 0.0f.
 */
UCLASS()
class THREADTALES_API UThreadTalesGE_StaminaExhaustedDelay : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UThreadTalesGE_StaminaExhaustedDelay();
};

/**
 * Instant Damage and Poise Damage effect applied by weapon hits.
 * Magnitudes are specified via SetByCaller: TAG_SetByCaller_Damage & TAG_SetByCaller_PoiseDamage.
 */
UCLASS()
class THREADTALES_API UThreadTalesGE_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UThreadTalesGE_Damage();
};

/**
 * Duration effect (~1.2s) that grants TAG_State_Combat_Staggered when Poise breaks.
 */
UCLASS()
class THREADTALES_API UThreadTalesGE_Stagger : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UThreadTalesGE_Stagger();
};

/**
 * Duration effect (~0.4s) that grants TAG_State_Combat_HitReact on regular hits.
 */
UCLASS()
class THREADTALES_API UThreadTalesGE_HitReact : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UThreadTalesGE_HitReact();
};

/**
 * Periodic Poise Regeneration (+5.0 Poise every 0.2s = 25/s)
 * Inhibited during Stagger, HitReact, or active combat states.
 */
UCLASS()
class THREADTALES_API UThreadTalesGE_PoiseRegen : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UThreadTalesGE_PoiseRegen();
};
