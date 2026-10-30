// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreadTalesClimbingJumpBounds.generated.h"

/**
 * Short ledge jump spatial distance and height bounds.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingShortBounds
{
	GENERATED_BODY()

	FThreadTalesClimbingShortBounds()
		: MinHopUpHeight(20.0f)
		, MaxHopUpHeight(58.0f)
		, MinSideJumpDistance(25.0f)
		, MaxSideJumpDistance(58.0f)
		, MinDiagonalJumpDistance(25.0f)
		, MaxDiagonalJumpDistance(60.0f)
		, MinHopDownHeight(20.0f)
		, MaxHopDownHeight(58.0f)
	{
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Short", meta = (ClampMin = "0.0", ToolTip = "Minimum vertical height for short hop up."))
	float MinHopUpHeight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Short", meta = (ClampMin = "0.0", ToolTip = "Maximum vertical height for short hop up."))
	float MaxHopUpHeight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Short", meta = (ClampMin = "0.0", ToolTip = "Minimum lateral distance for short side jump."))
	float MinSideJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Short", meta = (ClampMin = "0.0", ToolTip = "Maximum lateral distance for short side jump."))
	float MaxSideJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Short", meta = (ClampMin = "0.0", ToolTip = "Minimum diagonal distance for short jump."))
	float MinDiagonalJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Short", meta = (ClampMin = "0.0", ToolTip = "Maximum diagonal distance for short jump."))
	float MaxDiagonalJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Short", meta = (ClampMin = "0.0", ToolTip = "Minimum vertical depth for short hop down."))
	float MinHopDownHeight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Short", meta = (ClampMin = "0.0", ToolTip = "Maximum vertical depth for short hop down."))
	float MaxHopDownHeight;
};

/**
 * Long ledge jump spatial distance and height bounds.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingLongBounds
{
	GENERATED_BODY()

	FThreadTalesClimbingLongBounds()
		: MinLongHopUpHeight(50.0f)
		, MaxLongHopUpHeight(110.0f)
		, MinLongSideJumpDistance(50.0f)
		, MaxLongSideJumpDistance(110.0f)
		, MinLongDiagonalJumpDistance(50.0f)
		, MaxLongDiagonalJumpDistance(110.0f)
		, MinLongHopDownHeight(50.0f)
		, MaxLongHopDownHeight(110.0f)
	{
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Long", meta = (ClampMin = "0.0", ToolTip = "Minimum vertical height for long hop up."))
	float MinLongHopUpHeight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Long", meta = (ClampMin = "0.0", ToolTip = "Maximum vertical height for long hop up."))
	float MaxLongHopUpHeight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Long", meta = (ClampMin = "0.0", ToolTip = "Minimum lateral distance for long side jump."))
	float MinLongSideJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Long", meta = (ClampMin = "0.0", ToolTip = "Maximum lateral distance for long side jump."))
	float MaxLongSideJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Long", meta = (ClampMin = "0.0", ToolTip = "Minimum diagonal distance for long jump."))
	float MinLongDiagonalJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Long", meta = (ClampMin = "0.0", ToolTip = "Maximum diagonal distance for long jump."))
	float MaxLongDiagonalJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Long", meta = (ClampMin = "0.0", ToolTip = "Minimum vertical depth for long hop down."))
	float MinLongHopDownHeight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Long", meta = (ClampMin = "0.0", ToolTip = "Maximum vertical depth for long hop down."))
	float MaxLongHopDownHeight;
};

/**
 * Leap ledge jump spatial distance and height bounds.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingLeapBounds
{
	GENERATED_BODY()

	FThreadTalesClimbingLeapBounds()
		: MinLeapHopUpHeight(85.0f)
		, MaxLeapHopUpHeight(200.0f)
		, MinLeapSideJumpDistance(85.0f)
		, MaxLeapSideJumpDistance(200.0f)
		, MinLeapDiagonalJumpDistance(85.0f)
		, MaxLeapDiagonalJumpDistance(200.0f)
		, MinLeapHopDownHeight(85.0f)
		, MaxLeapHopDownHeight(200.0f)
	{
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Leap", meta = (ClampMin = "0.0", ToolTip = "Minimum vertical height for leap hop up."))
	float MinLeapHopUpHeight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Leap", meta = (ClampMin = "0.0", ToolTip = "Maximum vertical height for leap hop up."))
	float MaxLeapHopUpHeight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Leap", meta = (ClampMin = "0.0", ToolTip = "Minimum lateral distance for leap side jump."))
	float MinLeapSideJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Leap", meta = (ClampMin = "0.0", ToolTip = "Maximum lateral distance for leap side jump."))
	float MaxLeapSideJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Leap", meta = (ClampMin = "0.0", ToolTip = "Minimum diagonal distance for leap jump."))
	float MinLeapDiagonalJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Leap", meta = (ClampMin = "0.0", ToolTip = "Maximum diagonal distance for leap jump."))
	float MaxLeapDiagonalJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Leap", meta = (ClampMin = "0.0", ToolTip = "Minimum vertical depth for leap hop down."))
	float MinLeapHopDownHeight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds|Leap", meta = (ClampMin = "0.0", ToolTip = "Maximum vertical depth for leap hop down."))
	float MaxLeapHopDownHeight;
};

/**
 * Composite jump bounds structure covering all distance tiers.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingJumpBounds
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds", meta = (ToolTip = "Short jump bounds."))
	FThreadTalesClimbingShortBounds Short;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds", meta = (ToolTip = "Long jump bounds."))
	FThreadTalesClimbingLongBounds Long;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Bounds", meta = (ToolTip = "Leap jump bounds."))
	FThreadTalesClimbingLeapBounds Leap;
};
