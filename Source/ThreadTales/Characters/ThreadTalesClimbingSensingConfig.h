// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/ThreadTalesClimbingRadialScannerConfig.h"
#include "ThreadTalesClimbingSensingConfig.generated.h"

/**
 * World raycasting, sphere sweeping, and ledge hold detection parameters.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingDetectionConfig
{
	GENERATED_BODY()

	FThreadTalesClimbingDetectionConfig()
		: LedgeTag(FName(TEXT("Ledge")))
		, bRequireLedgeTag(false)
		, bAutoGrabLedge(true)
		, ForwardTraceDistance(100.0f)
		, ForwardTraceZOffset(45.0f)
		, LedgeDownTraceHeight(30.0f)
		, LedgeDownTraceDepth(45.0f)
		, MinGrabHeight(20.0f)
		, MaxGrabHeight(78.0f)
		, HandSpacing(22.0f)
		, MaxApproachAngle(65.0f)
		, MaxGrabVelocityZ(750.0f)
		, LedgeProbeDepths({ 2.5f, 5.0f, 10.0f, 18.0f })
		, MaxGrabWallDistance(85.0f)
		, LedgeSlopeMaxZDifference(25.0f)
		, LedgeHandsProbeDepth(4.0f)
		, LedgeHandsVerticalRange(15.0f)
	{
	}

	/** Actor or Component tag required for ledges */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Detection", meta = (ToolTip = "Actor or Component tag required for ledges."))
	FName LedgeTag;

	/** If true, only surfaces with LedgeTag can be grabbed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Detection", meta = (ToolTip = "If true, only surfaces with LedgeTag can be grabbed."))
	bool bRequireLedgeTag;

	/** If true, character automatically grabs detected ledges when airborne */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Detection", meta = (ToolTip = "If true, character automatically grabs detected ledges when airborne."))
	bool bAutoGrabLedge;

	/** Forward trace distance from character center */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Detection", meta = (ClampMin = "20.0", ToolTip = "Forward trace distance from character center (cm)."))
	float ForwardTraceDistance;

	/** Vertical offset from character center for forward trace start */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Detection", meta = (ToolTip = "Vertical offset from character center for forward trace start (cm)."))
	float ForwardTraceZOffset;

	/** Height above forward hit to start downward trace for finding exact top edge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Detection", meta = (ClampMin = "5.0", ToolTip = "Height above forward hit to start downward trace (cm)."))
	float LedgeDownTraceHeight;

	/** Depth below forward hit for downward trace end */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Detection", meta = (ClampMin = "5.0", ToolTip = "Depth below forward hit for downward trace end (cm)."))
	float LedgeDownTraceDepth;

	/** Minimum ledge height relative to character root for valid grab (reach of arms) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Detection", meta = (ClampMin = "0.0", ToolTip = "Minimum ledge height relative to character root for valid grab (cm)."))
	float MinGrabHeight;

	/** Maximum ledge height relative to character root for valid grab (reach of arms) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Detection", meta = (ClampMin = "0.0", ToolTip = "Maximum ledge height relative to character root for valid grab (cm)."))
	float MaxGrabHeight;

	/** Half-width distance from center to each hand for 3-point trace (shoulder width) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Detection", meta = (ClampMin = "5.0", ToolTip = "Half-width distance from center to each hand (cm)."))
	float HandSpacing;

	/** Maximum approach angle (in degrees) to wall normal for valid grab */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Detection", meta = (ClampMin = "10.0", ClampMax = "90.0", ToolTip = "Maximum approach angle to wall normal (degrees)."))
	float MaxApproachAngle;

	/** Maximum vertical speed on upward jump to allow grab */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Detection", meta = (ClampMin = "100.0", ToolTip = "Maximum vertical speed on upward jump to allow grab (cm/s)."))
	float MaxGrabVelocityZ;

	/** Multi-depth inward probe distances into wall top to detect narrow shelves vs thick walls (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Detection", meta = (ToolTip = "Multi-depth inward probe distances into wall top (cm)."))
	TArray<float> LedgeProbeDepths;

	/** Maximum forward distance from character to wall at grab moment to prevent teleportation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Detection", meta = (ClampMin = "10.0", ToolTip = "Maximum forward distance from character to wall at grab moment (cm)."))
	float MaxGrabWallDistance;

	/** Maximum allowable height difference between left and right hand placements (sloped ledges) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Detection", meta = (ClampMin = "0.0", ToolTip = "Maximum allowable height difference between hands (cm)."))
	float LedgeSlopeMaxZDifference;

	/** Inward distance from edge to probe surface under left and right hands */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Detection", meta = (ClampMin = "1.0", ToolTip = "Inward distance from edge to probe surface under hands (cm)."))
	float LedgeHandsProbeDepth;

	/** Vertical half-range for probing surface under left and right hands */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Detection", meta = (ClampMin = "2.0", ToolTip = "Vertical half-range for probing surface under hands (cm)."))
	float LedgeHandsVerticalRange;
};

/**
 * Lip geometric validation, modular seam rejection, and torso clearance verification.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingValidationConfig
{
	GENERATED_BODY()

	FThreadTalesClimbingValidationConfig()
		: LipBelowWallTraceDepth(6.0f)
		, LipAboveAirTraceHeight(6.0f)
		, LipModularSeamRejectDistance(3.5f)
		, BodyClearanceRadius(20.0f)
		, BodyClearanceHeight(40.0f)
	{
	}

	/** Depth below ledge lip for validating solid vertical wall surface */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Validation", meta = (ClampMin = "1.0", ToolTip = "Depth below ledge lip for validating vertical wall surface (cm)."))
	float LipBelowWallTraceDepth;

	/** Height above ledge lip for validating open air (flat wall and modular seam rejector) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Validation", meta = (ClampMin = "1.0", ToolTip = "Height above ledge lip for validating open air (cm)."))
	float LipAboveAirTraceHeight;

	/** Minimum horizontal shelf depth to reject modular mesh seams while accepting shallow wall rungs */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Validation", meta = (ClampMin = "0.5", ToolTip = "Minimum shelf depth to reject modular seams (cm)."))
	float LipModularSeamRejectDistance;

	/** Sphere sweep radius checking clearance for player's torso and head when hanging */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Validation", meta = (ClampMin = "5.0", ToolTip = "Sphere sweep radius checking body clearance (cm)."))
	float BodyClearanceRadius;

	/** Vertical upward height checked for body clearance */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Validation", meta = (ClampMin = "10.0", ToolTip = "Vertical height checked for body clearance (cm)."))
	float BodyClearanceHeight;
};

/**
 * Freehang vs braced wall support detection parameters.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingFreehangSensingConfig
{
	GENERATED_BODY()

	FThreadTalesClimbingFreehangSensingConfig()
		: FreehangWallCheckDepth(70.0f)
		, FreehangWallTraceForward(12.0f)
		, FreehangWallTraceBack(5.0f)
	{
	}

	/** Depth below ledge lip to check for solid wall support (feet/knees brace level) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Freehang", meta = (ClampMin = "10.0", ToolTip = "Depth below ledge lip to check for wall support (cm)."))
	float FreehangWallCheckDepth;

	/** Forward trace distance in front of wall for freehang check */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Freehang", meta = (ClampMin = "0.0", ToolTip = "Forward trace distance for freehang check (cm)."))
	float FreehangWallTraceForward;

	/** Inward trace depth behind wall surface for freehang check */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Freehang", meta = (ClampMin = "0.0", ToolTip = "Inward trace depth behind wall surface (cm)."))
	float FreehangWallTraceBack;
};

/**
 * Ledge top probing offsets and directional multi-ledge jump scanning parameters.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingProbingConfig
{
	GENERATED_BODY()

	FThreadTalesClimbingProbingConfig()
		: ClimbDownFloorTraceForwardDist(8.0f)
		, ClimbDownFloorTraceZRange(35.0f)
		, ClimbDownEdgeLipDepth(8.0f)
		, ClimbDownEdgeLipProbeRange(25.0f)
		, DirectionalProbeDepths({ 2.5f, 5.0f, 10.0f, 18.0f })
		, UpJumpTopTraceOffset(55.0f)
		, DownJumpTopTraceOffset(22.0f)
		, DownJumpLeapTopTraceOffset(28.0f)
		, LateralJumpTopTraceOffset(28.0f)
		, DiagonalUpJumpTopTraceOffset(50.0f)
		, DiagonalDownJumpTopTraceOffset(25.0f)
	{
	}

	/** Inward distance from wall face to probe for top surface during climb down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Probing", meta = (ToolTip = "Inward distance to probe for top surface during climb down (cm)."))
	float ClimbDownFloorTraceForwardDist;

	/** Vertical half-range for locating ledge top surface during climb down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Probing", meta = (ToolTip = "Vertical half-range for locating top surface (cm)."))
	float ClimbDownFloorTraceZRange;

	/** Depth below top surface to pinpoint exact XY edge lip during climb down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Probing", meta = (ToolTip = "Depth below top surface to pinpoint edge lip (cm)."))
	float ClimbDownEdgeLipDepth;

	/** Horizontal probe range for pinpointing XY edge lip during climb down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Probing", meta = (ToolTip = "Horizontal probe range for pinpointing edge lip (cm)."))
	float ClimbDownEdgeLipProbeRange;

	/** Inward penetration depths tested when scanning for adjacent directional ledges */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Probing", meta = (ToolTip = "Inward penetration depths for directional scans (cm)."))
	TArray<float> DirectionalProbeDepths;

	/** Vertical upward offset for downward trace in Hop Up jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Probing", meta = (ToolTip = "Vertical upward offset for Hop Up jumps (cm)."))
	float UpJumpTopTraceOffset;

	/** Vertical upward offset for downward trace in Hop Down jumps (Short/Long) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Probing", meta = (ToolTip = "Vertical upward offset for Hop Down jumps (cm)."))
	float DownJumpTopTraceOffset;

	/** Vertical upward offset for downward trace in Hop Down Leap jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Probing", meta = (ToolTip = "Vertical upward offset for Hop Down Leap jumps (cm)."))
	float DownJumpLeapTopTraceOffset;

	/** Vertical upward offset for downward trace in Lateral (Left/Right) jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Probing", meta = (ToolTip = "Vertical upward offset for Lateral jumps (cm)."))
	float LateralJumpTopTraceOffset;

	/** Vertical upward offset for downward trace in Diagonal Up jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Probing", meta = (ToolTip = "Vertical upward offset for Diagonal Up jumps (cm)."))
	float DiagonalUpJumpTopTraceOffset;

	/** Vertical upward offset for downward trace in Diagonal Down jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Probing", meta = (ToolTip = "Vertical upward offset for Diagonal Down jumps (cm)."))
	float DiagonalDownJumpTopTraceOffset;
};

/**
 * Corner detection raycasts and angle acceptance thresholds.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingCornerSensingConfig
{
	GENERATED_BODY()

	FThreadTalesClimbingCornerSensingConfig()
		: CornerMaxAngleDegrees(120.0f)
		, CornerProbeDistance(80.0f)
		, CornerInwardProbeDepth(30.0f)
	{
	}

	/** Maximum angle (degrees) between current and target wall normals to accept as a wrappable corner */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Corner", meta = (ClampMin = "60.0", ClampMax = "170.0", ToolTip = "Maximum angle between normals to accept as corner (degrees)."))
	float CornerMaxAngleDegrees;

	/** Distance along wall tangent to shoot the corner pivot detection ray (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Corner", meta = (ClampMin = "20.0", ToolTip = "Distance along wall tangent to shoot corner pivot ray (cm)."))
	float CornerProbeDistance;

	/** How far past the detected corner pivot point to probe for the new ledge face (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing|Corner", meta = (ClampMin = "5.0", ToolTip = "How far past corner pivot point to probe for new face (cm)."))
	float CornerInwardProbeDepth;
};

/**
 * Composite sensing configuration bundling detection, validation, freehang, probing, corner, and radial scanner.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingSensingConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing", meta = (ToolTip = "Ledge detection and reach parameters."))
	FThreadTalesClimbingDetectionConfig Detection;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing", meta = (ToolTip = "Lip validation and clearance verification."))
	FThreadTalesClimbingValidationConfig Validation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing", meta = (ToolTip = "Freehang vs braced wall detection."))
	FThreadTalesClimbingFreehangSensingConfig Freehang;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing", meta = (ToolTip = "Multi-ledge directional probing offsets."))
	FThreadTalesClimbingProbingConfig Probing;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing", meta = (ToolTip = "Corner traversal raycasting."))
	FThreadTalesClimbingCornerSensingConfig Corner;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Sensing", meta = (ToolTip = "3D volumetric radial intent scanner."))
	FThreadTalesClimbingRadialScannerConfig RadialScanner;
};
