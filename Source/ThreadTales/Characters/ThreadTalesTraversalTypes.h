// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreadTalesTraversalTypes.generated.h"

class UAnimMontage;

/**
 * Traversal states supported by the Traversal Component.
 * Designed to be modular and extended with Ladder and Rope mechanics in future milestones.
 */
UENUM(BlueprintType)
enum class EThreadTalesTraversalState : uint8
{
	None            UMETA(DisplayName = "None"),
	LedgeClimbing   UMETA(DisplayName = "Ledge Climbing"),
	Ladder          UMETA(DisplayName = "Ladder"),
	Rope            UMETA(DisplayName = "Rope")
};

/**
 * 8 directions for jumping between ledges while climbing.
 */
UENUM(BlueprintType)
enum class EThreadTalesLedgeJumpDirection : uint8
{
	None        UMETA(DisplayName = "None"),
	Up          UMETA(DisplayName = "Up"),
	UpRight     UMETA(DisplayName = "Up-Right"),
	Right       UMETA(DisplayName = "Right"),
	DownRight   UMETA(DisplayName = "Down-Right"),
	Down        UMETA(DisplayName = "Down"),
	DownLeft    UMETA(DisplayName = "Down-Left"),
	Left        UMETA(DisplayName = "Left"),
	UpLeft      UMETA(DisplayName = "Up-Left")
};

/**
 * Jump distance categories for climbing traversal.
 */
UENUM(BlueprintType)
enum class EThreadTalesLedgeJumpDistanceType : uint8
{
	Short       UMETA(DisplayName = "Short"),
	Long        UMETA(DisplayName = "Long"),
	Leap        UMETA(DisplayName = "Leap")
};

/**
 * Sub-states within LedgeClimbing (single source of truth replacing disparate boolean flags).
 */
UENUM(BlueprintType)
enum class EThreadTalesClimbingSubState : uint8
{
	None            UMETA(DisplayName = "None"),
	IdleHang        UMETA(DisplayName = "Idle Hang"),
	Shimmying       UMETA(DisplayName = "Shimmying"),
	JumpTransition  UMETA(DisplayName = "Jump Transition"),
	MantleUp        UMETA(DisplayName = "Mantle Up"),
	ClimbDown       UMETA(DisplayName = "Climb Down")
};

/**
 * Configuration entry for a directional ledge jump at a specific distance tier.
 */
USTRUCT(BlueprintType)
struct FThreadTalesLedgeJumpConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge")
	float MinDistance = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge")
	float MaxDistance = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge")
	float NominalDistance = 48.1f;
};
