// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Combat/ThreadTalesLootTypes.h"
#include "ThreadTalesEnemyConfig.generated.h"

class UAnimMontage;
class AThreadTalesWeapon;
class UStateTree;

/**
 * Combat & Navigation ranges for Enemy AI
 */
USTRUCT(BlueprintType)
struct FThreadTalesEnemyCombatRanges
{
	GENERATED_BODY()

	/** Distance at which melee attacks can be executed (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranges", meta = (ClampMin = "50.0", ClampMax = "500.0", ToolTip = "Distance at which melee attack combo can trigger"))
	float MeleeAttackRange = 220.0f;

	/** Minimum distance to maintain while strafing/circling the player (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranges", meta = (ClampMin = "100.0", ClampMax = "800.0", ToolTip = "Minimum strafe circle distance"))
	float StrafeMinDistance = 240.0f;

	/** Maximum distance to maintain while strafing/circling the player (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranges", meta = (ClampMin = "150.0", ClampMax = "1200.0", ToolTip = "Maximum strafe circle distance before closing in"))
	float StrafeMaxDistance = 420.0f;

	/** Perception sight radius (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception", meta = (ClampMin = "300.0", ClampMax = "5000.0", ToolTip = "Vision detection range"))
	float SightRadius = 1600.0f;

	/** Perception lose sight radius (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception", meta = (ClampMin = "400.0", ClampMax = "6000.0", ToolTip = "Vision de-aggro range"))
	float LoseSightRadius = 2200.0f;

	/** Vision cone half-angle in degrees */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception", meta = (ClampMin = "10.0", ClampMax = "180.0", ToolTip = "Peripheral vision cone half-angle"))
	float PeripheralVisionAngleDegrees = 65.0f;
};

/**
 * Co-op Threat & Aggro parameters for 2P Co-op (HeroMale vs HeroFemale)
 */
USTRUCT(BlueprintType)
struct FThreadTalesEnemyThreatSettings
{
	GENERATED_BODY()

	/** Multiplier for damage dealt by a player to this enemy */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Threat", meta = (ClampMin = "0.1", ClampMax = "10.0", ToolTip = "Threat generated per point of damage received"))
	float DamageThreatMultiplier = 1.5f;

	/** Proximity threat weight applied based on target distance */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Threat", meta = (ClampMin = "100.0", ClampMax = "100000.0", ToolTip = "Proximity threat weight"))
	float DistanceThreatWeight = 2000.0f;

	/** Hysteresis factor to switch targets (e.g. 1.25 = new target must have 25% more threat to trigger switch) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Threat", meta = (ClampMin = "1.0", ClampMax = "2.5", ToolTip = "Target switch hysteresis ratio to avoid rapid target thrashing"))
	float TargetSwitchHysteresis = 1.25f;

	/** Rate at which threat decays per second */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Threat", meta = (ClampMin = "0.0", ClampMax = "100.0", ToolTip = "Threat decay rate per second"))
	float ThreatDecayRate = 5.0f;

	/** Threat added when sensed via Damage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Threat", meta = (ClampMin = "0.0", ClampMax = "500.0", ToolTip = "Threat added when damage stimulus is received"))
	float DamageSenseThreat = 50.0f;

	/** Threat added when sensed via Sight */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Threat", meta = (ClampMin = "0.0", ClampMax = "500.0", ToolTip = "Threat added when sight stimulus is received"))
	float SightSenseThreat = 15.0f;
};

/**
 * Attack timings and probability parameters
 */
USTRUCT(BlueprintType)
struct FThreadTalesEnemyCombatTiming
{
	GENERATED_BODY()

	/** Minimum cooldown between attack attempts (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.5", ClampMax = "10.0", ToolTip = "Cooldown between attack attempts"))
	float MinAttackCooldown = 1.2f;

	/** Maximum cooldown between attack attempts (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.5", ClampMax = "10.0", ToolTip = "Cooldown between attack attempts"))
	float MaxAttackCooldown = 2.2f;

	/** Strafe circle duration before deciding next action (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "1.0", ClampMax = "10.0", ToolTip = "Duration of strafing behavior before attack or reposition"))
	float StrafeDuration = 2.8f;

	/** Probability of blocking an incoming frontal attack (0.0 - 1.0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Chance to raise guard against player attack"))
	float BlockProbability = 0.35f;

	/** Probability of attempting a parry if blocking is active (0.0 - 1.0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Chance to deflect incoming strike"))
	float ParryProbability = 0.20f;
};

/**
 * Difficulty & Precision parameters modulating AI responsiveness, tracking, and evasion against player actions
 */
USTRUCT(BlueprintType)
struct FThreadTalesEnemyDifficultySettings
{
	GENERATED_BODY()

	/** General difficulty multiplier (0.5 = Novice, 1.0 = Adept/Normal, 1.5+ = Master) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.1", ClampMax = "3.0", ToolTip = "Overall difficulty scaling multiplier"))
	float DifficultyMultiplier = 1.0f;

	/** Turn tracking speed towards player during windup and attacks (degrees per second, higher = more precise tracking) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "60.0", ClampMax = "1200.0", ToolTip = "Tracking rotation speed towards target while preparing strikes (degrees/second)"))
	float TrackingTurnRate = 360.0f;

	/** Fraction of windup time during which tracking is active before the strike commits (0.0 to 1.0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Ratio of windup during which AI tracks the player before committing to strike angle"))
	float TrackingCommitRatio = 0.70f;

	/** Chance to execute an evasive backstep after an attack in neutral conditions (default low: 0.20 = 20%) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Base probability of backstep after attack sequence; evasion is not routine behavior"))
	float PostAttackEvadeChance = 0.20f;

	/** Chance to evade when player is within threatening close range (< CloseThreatDistance) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Tactical evade probability when player is in close threat range"))
	float ReactiveEvadeChance = 0.65f;

	/** Proximity threshold (cm) triggering tactical evasion check */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "50.0", ClampMax = "500.0", ToolTip = "Distance considered dangerous close-range threat"))
	float CloseThreatDistance = 140.0f;

	/** Probability of following up with a combo attack strike */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Chance to chain first attack strike into second attack strike"))
	float ComboChainChance = 0.60f;
};

/**
 * Primary Data Asset configuring AI behavior, ranges, and timings for ThreadTales enemies.
 */

/**
 * Flight and aerial combat parameters
 */
USTRUCT(BlueprintType)
struct FThreadTalesEnemyFlightSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "50.0", ClampMax = "1000.0", ToolTip = "Target hover altitude above ground (cm)"))
	float HoverAltitude = 260.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "0.5", ClampMax = "5.0", ToolTip = "Takeoff duration (seconds)"))
	float TakeoffDuration = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "60.0", ClampMax = "1440.0", ToolTip = "Ground turn rate prior to takeoff (deg/s)"))
	float GroundTurnRate = 720.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "60.0", ClampMax = "1440.0", ToolTip = "In-air turn rate (deg/s)"))
	float AirTurnRate = 540.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "0.5", ClampMax = "10.0", ToolTip = "Hover duration before picking dive (seconds)"))
	float HoverDuration = 1.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "300.0", ClampMax = "3000.0", ToolTip = "Linear dive speed towards target (cm/s)"))
	float DiveSpeed = 1300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "20.0", ClampMax = "300.0", ToolTip = "Ground line trace check distance (cm)"))
	float DiveGroundCheckDistance = 75.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "0.0", ClampMax = "100.0", ToolTip = "Hover wobble sinusoidal amplitude (cm)"))
	float HoverWobbleAmplitude = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "0.1", ClampMax = "10.0", ToolTip = "Hover wobble sinusoidal frequency (Hz)"))
	float HoverWobbleFrequency = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "1.0", ClampMax = "15.0", ToolTip = "Maximum flight duration before forced attack"))
	float AirFlightMaxDuration = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "100.0", ClampMax = "1500.0", ToolTip = "Horizontal range to initiate aerial dive (cm)"))
	float DiveInitiationRange = 300.0f;
};

/**
 * Leap attack, dynamic steering, and combo chaining parameters
 */
USTRUCT(BlueprintType)
struct FThreadTalesEnemyLeapAttackSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leap", meta = (ClampMin = "10.0", ClampMax = "300.0", ToolTip = "Desired distance to target on leap landing (cm)"))
	float DesiredMeleeDistance = 65.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leap", meta = (ClampMin = "50.0", ClampMax = "500.0", ToolTip = "Nominal root motion leap distance of strike 1 (cm)"))
	float NominalLeapDistance = 145.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leap", meta = (ClampMin = "0.1", ClampMax = "1.0", ToolTip = "Minimum root motion translation scale"))
	float MinLeapTranslationScale = 0.85f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leap", meta = (ClampMin = "1.0", ClampMax = "3.0", ToolTip = "Maximum root motion translation scale"))
	float MaxLeapTranslationScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leap", meta = (ClampMin = "60.0", ClampMax = "1440.0", ToolTip = "Dynamic attack steering turn rate (deg/s)"))
	float SteeringTurnRate = 540.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leap", meta = (ClampMin = "0.0", ClampMax = "2.0", ToolTip = "Time window from strike start with active steering (seconds)"))
	float SteeringWindow = 0.40f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leap", meta = (ClampMin = "10.0", ClampMax = "180.0", ToolTip = "Maximum forward yaw angle for steering tracking (deg)"))
	float SteeringMaxYawAngle = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leap", meta = (ClampMin = "0.05", ClampMax = "1.0", ToolTip = "Early crossfade lead-time between combo strikes (seconds)"))
	float CrossfadeLeadTime = 0.20f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leap", meta = (ClampMin = "1.0", ClampMax = "3.0", ToolTip = "MeleeAttackRange multiplier for chaining into next strike"))
	float ComboChainRangeMultiplier = 1.5f;
};

/**
 * Natural limb/body attack hitbox parameters
 */
USTRUCT(BlueprintType)
struct FThreadTalesEnemyHitboxSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hitbox", meta = (ClampMin = "10.0", ClampMax = "300.0", ToolTip = "Natural attack sphere radius (cm)"))
	float SphereRadius = 65.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hitbox", meta = (ToolTip = "Relative offset of natural attack sphere (cm)"))
	FVector RelativeOffset = FVector(75.0f, 0.0f, 0.0f);
};

UCLASS(BlueprintType, Const)
class THREADTALES_API UThreadTalesEnemyConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UThreadTalesEnemyConfig();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("ThreadTalesEnemyConfig"), GetFName());
	}

	/** Localized/display name for this enemy archetype shown on overhead HUD and target indicators */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|UI", meta = (ToolTip = "Отображаемое имя архетипа противника (например, 'Жук-скарабей', 'Самурай')"))
	FText EnemyDisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Ranges")
	FThreadTalesEnemyCombatRanges Ranges;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Threat")
	FThreadTalesEnemyThreatSettings ThreatSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Timing")
	FThreadTalesEnemyCombatTiming Timing;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Difficulty")
	FThreadTalesEnemyDifficultySettings DifficultySettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Flight")
	FThreadTalesEnemyFlightSettings FlightSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Leap")
	FThreadTalesEnemyLeapAttackSettings LeapSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Hitbox")
	FThreadTalesEnemyHitboxSettings HitboxSettings;

	/** If true, AI StateTree tasks override CharacterMovement MaxWalkSpeed using values below. If false, CharacterMovement MaxWalkSpeed is strictly preserved and never modified by StateTree. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Locomotion")
	bool bOverrideWalkSpeed = true;

	/** Maximum walk speed during chase/combat engagement (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Locomotion", meta = (ClampMin = "50.0", ClampMax = "2000.0", ToolTip = "Walk speed applied to CharacterMovement during Chase target phase", EditCondition = "bOverrideWalkSpeed"))
	float ChaseWalkSpeed = 600.0f;

	/** Maximum walk speed during wandering / patrol phase (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Locomotion", meta = (ClampMin = "50.0", ClampMax = "2000.0", ToolTip = "Walk speed applied to CharacterMovement during Patrol / Wander phase", EditCondition = "bOverrideWalkSpeed"))
	float PatrolWalkSpeed = 200.0f;

	/** Maximum walk speed during combat strafe / circling phase (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Locomotion", meta = (ClampMin = "50.0", ClampMax = "2000.0", ToolTip = "Walk speed applied to CharacterMovement during Strafe / Circling phase", EditCondition = "bOverrideWalkSpeed"))
	float StrafeWalkSpeed = 300.0f;

	/** Base attack damage applied by this enemy archetype */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat", meta = (ClampMin = "1.0", ClampMax = "500.0", ToolTip = "Base strike damage applied by enemy melee attacks"))
	float AttackDamage = 25.0f;

	/** Default weapon class equipped by this enemy archetype */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat")
	TSoftClassPtr<AThreadTalesWeapon> DefaultWeaponClass;

	/** Attack combo montages executed in sequence */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat|Montages")
	TArray<TSoftObjectPtr<UAnimMontage>> AttackComboMontages;

	/** Heavy / Thrust unblockable attack montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat|Montages")
	TSoftObjectPtr<UAnimMontage> HeavyAttackMontage;

	/** Montage to play when this enemy receives light hit reaction */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat|Montages")
	TSoftObjectPtr<UAnimMontage> HitReactMontage;

	/** Montage to play when this enemy's attack is parried (stagger) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat|Montages")
	TSoftObjectPtr<UAnimMontage> StaggerMontage;

	/** Montage to play when this enemy's posture is broken (knockdown/stun) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat|Montages")
	TSoftObjectPtr<UAnimMontage> PostureBreakMontage;

	/** Montage to play when recovering from posture break (get-up) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat|Montages")
	TSoftObjectPtr<UAnimMontage> RecoverMontage;

	/** Optional airborne hit reaction montage (fallback to HitReactMontage if null) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat|Montages|Airborne")
	TSoftObjectPtr<UAnimMontage> AirHitReactMontage;

	/** Optional airborne parry stagger montage (fallback to StaggerMontage if null) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat|Montages|Airborne")
	TSoftObjectPtr<UAnimMontage> AirStaggerMontage;

	/** Optional airborne posture break montage (fallback to PostureBreakMontage if null) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat|Montages|Airborne")
	TSoftObjectPtr<UAnimMontage> AirPostureBreakMontage;

	/** Tactical evade / backstep montage for disengaging */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat|Montages")
	TSoftObjectPtr<UAnimMontage> EvadeMontage;

	/** Takeoff into air montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat|Montages")
	TSoftObjectPtr<UAnimMontage> TakeoffMontage;

	/** Air hover / flight loop montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat|Montages")
	TSoftObjectPtr<UAnimMontage> AirHoverMontage;

	/** Dive / ground slam attack montage from air */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat|Montages")
	TSoftObjectPtr<UAnimMontage> AirDiveAttackMontage;

	/** Primary StateTree asset assigned to this enemy archetype */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|StateTree")
	TSoftObjectPtr<class UStateTree> StateTreeAsset;

	/** Loot drop settings for physical currency/resource fountain on death */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Loot")
	FThreadTalesEnemyLootSettings LootSettings;

	/** Helper to resolve or load a drop entry's pickup actor class synchronously */
	UFUNCTION(BlueprintPure, Category = "AI|Loot")
	static TSubclassOf<AActor> ResolveLootPickupClass(const FThreadTalesLootDropEntry& DropEntry)
	{
		return DropEntry.PickupClass.IsNull() ? nullptr : TSubclassOf<AActor>(DropEntry.PickupClass.LoadSynchronous());
	}
};
