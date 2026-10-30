// Copyright ThreadTales. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreadTalesLootTypes.generated.h"

class AActor;

/**
 * Single loot drop entry (pickup class and drop quantity)
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesLootDropEntry
{
	GENERATED_BODY()

	/** Soft class reference to the pickup actor (e.g. BP_Leave, BP_ThreadEssence) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ToolTip = "Pickup actor class for currency drop"))
	TSoftClassPtr<AActor> PickupClass;

	/** Minimum number of pickups to drop */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0", ClampMax = "50", ToolTip = "Minimum number of pickups to drop"))
	int32 MinDropCount = 1;

	/** Maximum number of pickups to drop */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0", ClampMax = "50", ToolTip = "Maximum number of pickups to drop"))
	int32 MaxDropCount = 3;

	/** Probability of this drop entry occurring (0.0 = never, 1.0 = guaranteed) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Probability of drop"))
	float DropChance = 1.0f;
};

/**
 * Death loot drop & physical fountain settings
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesEnemyLootSettings
{
	GENERATED_BODY()

	/** List of loot drop entries (what resources and quantities drop from this enemy) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ToolTip = "Array of loot drop configurations"))
	TArray<FThreadTalesLootDropEntry> DropEntries;

	/** Vertical offset above the enemy's origin for spawning pickups (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0", ClampMax = "200.0", ToolTip = "Spawn height offset above enemy base"))
	float SpawnZOffset = 60.0f;

	/** Minimum launch speed for the ejection fountain (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "50.0", ClampMax = "2000.0", ToolTip = "Minimum ejection velocity speed"))
	float MinLaunchSpeed = 350.0f;

	/** Maximum launch speed for the ejection fountain (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "50.0", ClampMax = "2000.0", ToolTip = "Maximum ejection velocity speed"))
	float MaxLaunchSpeed = 650.0f;

	/** Half-angle cone of the upward fountain dispersion (degrees) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "5.0", ClampMax = "85.0", ToolTip = "Half-angle cone of the upward ejection fountain"))
	float LaunchConeHalfAngle = 45.0f;
};
