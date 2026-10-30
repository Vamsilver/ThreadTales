// Copyright ThreadTales. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreadTalesSpawnerTypes.generated.h"

class AThreadTalesEnemyCharacter;
class UThreadTalesEnemyConfig;

/**
 * Geometric shape of the spawn area.
 */
UENUM(BlueprintType)
enum class ESpawnAreaShape : uint8
{
	Radius UMETA(DisplayName = "Radius / Circle"),
	Box UMETA(DisplayName = "Box / Bounds"),
	CustomPoints UMETA(DisplayName = "Custom Spawn Points")
};

/**
 * Release behavior for enemy waves.
 */
UENUM(BlueprintType)
enum class ESpawnWaveMode : uint8
{
	Burst UMETA(DisplayName = "Burst (Simultaneous)"),
	Staggered UMETA(DisplayName = "Staggered (Interval)"),
	Reinforcement UMETA(DisplayName = "Reinforcement (Maintain Limit)")
};

/**
 * Condition required to trigger the spawner.
 */
UENUM(BlueprintType)
enum class ETriggerActivationMode : uint8
{
	OverlapAnyPlayer UMETA(DisplayName = "Overlap Any Player (P1 or P2)"),
	OverlapBothPlayers UMETA(DisplayName = "Overlap Both Players (P1 and P2)"),
	ExternalOnly UMETA(DisplayName = "External Signal Only (Switch/Script)")
};

/**
 * Definition of an enemy entry inside a wave pool.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesSpawnEntry
{
	GENERATED_BODY()

	/** Class of enemy to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner|Enemy", meta = (ToolTip = "Enemy character blueprint or archetype to spawn."))
	TSubclassOf<AThreadTalesEnemyCharacter> EnemyClass;

	/** Optional DataAsset configuration override for attributes, difficulty, and montages */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner|Enemy", meta = (ToolTip = "Optional EnemyConfig DataAsset override."))
	TObjectPtr<UThreadTalesEnemyConfig> ConfigOverride = nullptr;

	/** Total count of this enemy type to spawn in the wave */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner|Enemy", meta = (ClampMin = "1", ClampMax = "100", ToolTip = "Number of enemies of this type to spawn."))
	int32 Count = 1;

	/** Selection weight when spawning random composition */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner|Enemy", meta = (ClampMin = "0.1", ClampMax = "10.0", ToolTip = "Random selection weight."))
	float Weight = 1.0f;
};

// Spawner Event Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemySpawnerWaveStartedDelegate, AActor*, SpawnerActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemySpawnedDelegate, AActor*, SpawnerActor, AThreadTalesEnemyCharacter*, SpawnedEnemy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEnemyDefeatedDelegate, AActor*, SpawnerActor, AThreadTalesEnemyCharacter*, DefeatedEnemy, int32, RemainingCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemySpawnerWaveClearedDelegate, AActor*, SpawnerActor);

