// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreadTalesClimbingRadialScannerConfig.generated.h"

/**
 * 3D volumetric radial cone scanner configuration for dynamic ledge intent resolution.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingRadialScannerConfig
{
	GENERATED_BODY()

	FThreadTalesClimbingRadialScannerConfig()
		: RadialScanConeAngleH(45.0f)
		, RadialScanConeAngleV(35.0f)
		, RadialScanRaysH(5)
		, RadialScanRaysV(3)
		, RadialScanMaxDistance(220.0f)
		, RadialScanAngleWeight(0.65f)
		, RadialScanDistanceWeight(0.35f)
		, RadialScanMaxValidations(3)
		, RadialScanContinuousThrottleInterval(0.08f)
	{
	}

	/** Half-angle in degrees of the horizontal search cone around player's intent vector */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|RadialScanner", meta = (ClampMin = "15.0", ClampMax = "90.0", ToolTip = "Horizontal cone half-angle (degrees)."))
	float RadialScanConeAngleH;

	/** Half-angle in degrees of the vertical search cone around player's intent vector */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|RadialScanner", meta = (ClampMin = "15.0", ClampMax = "75.0", ToolTip = "Vertical cone half-angle (degrees)."))
	float RadialScanConeAngleV;

	/** Number of radial ray steps along horizontal axis of the scan cone */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|RadialScanner", meta = (ClampMin = "3", ClampMax = "15", ToolTip = "Horizontal ray steps in scan cone."))
	int32 RadialScanRaysH;

	/** Number of radial ray steps along vertical axis of the scan cone */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|RadialScanner", meta = (ClampMin = "1", ClampMax = "9", ToolTip = "Vertical ray steps in scan cone."))
	int32 RadialScanRaysV;

	/** Maximum search distance (cm) for 3D volumetric ledge detection */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|RadialScanner", meta = (ClampMin = "50.0", ClampMax = "400.0", ToolTip = "Maximum search distance for 3D ledge detection (cm)."))
	float RadialScanMaxDistance;

	/** Weight of angular alignment in candidate scoring heuristic (0.0 .. 1.0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|RadialScanner", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Weight of angular alignment in candidate scoring heuristic (0.0..1.0)."))
	float RadialScanAngleWeight;

	/** Weight of proximity (shorter distance) in candidate scoring heuristic (0.0 .. 1.0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|RadialScanner", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Weight of proximity in candidate scoring heuristic (0.0..1.0)."))
	float RadialScanDistanceWeight;

	/** Maximum number of top candidates to run expensive body & hand clearance checks on (Coarse-to-Fine LOD) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|RadialScanner", meta = (ClampMin = "1", ClampMax = "10", ToolTip = "Maximum candidate count for detailed clearance validation."))
	int32 RadialScanMaxValidations;

	/** Throttle interval in seconds for continuous intent scanning while holding stick in IdleHang (~12.5 Hz) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|RadialScanner", meta = (ClampMin = "0.01", ClampMax = "0.5", ToolTip = "Scan throttle interval in seconds (seconds)."))
	float RadialScanContinuousThrottleInterval;
};
