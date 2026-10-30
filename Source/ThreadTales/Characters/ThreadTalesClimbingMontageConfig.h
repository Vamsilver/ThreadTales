// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/ThreadTalesClimbingMontageBase.h"
#include "Characters/ThreadTalesClimbingMontageJumps.h"
#include "ThreadTalesClimbingMontageConfig.generated.h"

/**
 * Composite configuration bundling all character archetype climbing animation montages.
 * Decouples 70 directional, corner, and base montages into modular sub-structures.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingMontagesConfig
{
	GENERATED_BODY()

	/** Base idle hang, mantle, climb down, and shimmy montages */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages", meta = (ToolTip = "Base idle hang, mantle, climb down, and shimmy montages."))
	FThreadTalesClimbingMontagesBase Base;

	/** Outer and inner corner traversal montages */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages", meta = (ToolTip = "Outer and inner corner traversal montages."))
	FThreadTalesClimbingMontagesCorner Corner;

	/** Short ledge-to-ledge directional jump montages */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages", meta = (ToolTip = "Short ledge-to-ledge directional jump montages."))
	FThreadTalesClimbingMontagesShortJump ShortJump;

	/** Long ledge-to-ledge directional jump montages */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages", meta = (ToolTip = "Long ledge-to-ledge directional jump montages."))
	FThreadTalesClimbingMontagesLongJump LongJump;

	/** Leap ledge-to-ledge directional jump montages */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages", meta = (ToolTip = "Leap ledge-to-ledge directional jump montages."))
	FThreadTalesClimbingMontagesLeapJump LeapJump;
};
