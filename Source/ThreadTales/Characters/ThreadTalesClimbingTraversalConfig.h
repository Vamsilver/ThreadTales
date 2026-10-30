// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreadTalesClimbingTraversalConfig.generated.h"

/**
 * Continuous and discrete shimmy traversal tuning parameters along continuous ledges.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingShimmyConfig
{
	GENERATED_BODY()

	FThreadTalesClimbingShimmyConfig()
		: ShimmyStepDistance(67.0f)
		, ShimmyContinuousSpeed(110.0f)
		, ShimmyCooldownDuration(0.55f)
		, ShimmyProbeDepths({ 3.0f, 7.0f, 12.0f })
		, ShimmyVerticalTraceRange(20.0f)
		, ShimmyMaxZDifference(20.0f)
		, ShimmyInputDebounceDuration(0.15f)
	{
	}

	/** Horizontal step distance per shimmy input (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|Shimmy", meta = (ClampMin = "10.0", ToolTip = "Horizontal step distance per shimmy input (cm)."))
	float ShimmyStepDistance;

	/** Continuous lateral movement speed along ledge (cm/s) when driven by ABP BlendSpace */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|Shimmy", meta = (ClampMin = "30.0", ClampMax = "300.0", ToolTip = "Continuous lateral movement speed along ledge (cm/s)."))
	float ShimmyContinuousSpeed;

	/** Cooldown between shimmy steps (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|Shimmy", meta = (ClampMin = "0.0", ToolTip = "Cooldown between shimmy steps (seconds)."))
	float ShimmyCooldownDuration;

	/** Depths into ledge surface to probe for wall continuity during shimmy (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|Shimmy", meta = (ToolTip = "Depths into ledge surface to probe for wall continuity (cm)."))
	TArray<float> ShimmyProbeDepths;

	/** Vertical search range up/down to locate ledge top during shimmy (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|Shimmy", meta = (ClampMin = "5.0", ToolTip = "Vertical search range up/down to locate ledge top during shimmy (cm)."))
	float ShimmyVerticalTraceRange;

	/** Maximum allowed Z difference between current and candidate ledge point during shimmy */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|Shimmy", meta = (ClampMin = "0.0", ToolTip = "Maximum allowed Z difference for shimmy continuity (cm)."))
	float ShimmyMaxZDifference;

	/** Input debounce duration for shimmy */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|Shimmy", meta = (ClampMin = "0.0", ToolTip = "Input debounce duration for shimmy (seconds)."))
	float ShimmyInputDebounceDuration;
};

/**
 * Root motion and alignment parameters for lowering from standing into a ledge hang.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingClimbDownConfig
{
	GENERATED_BODY()

	FThreadTalesClimbingClimbDownConfig()
		: ClimbDownStepBackDistance(30.0f)
		, ClimbDownDownwardTraceDepth(120.0f)
		, ClimbDownRootMotionForwardOffset(72.2f)
		, ClimbDownRootMotionDownwardOffset(171.32f)
		, ClimbDownMinApproachDot(0.25f)
	{
	}

	/** Horizontal backward step distance from detected ledge edge to initiate climb down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|ClimbDown", meta = (ClampMin = "0.0", ToolTip = "Horizontal backward step distance to initiate climb down (cm)."))
	float ClimbDownStepBackDistance;

	/** Maximum downward trace distance to verify wall face continuity for climb down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|ClimbDown", meta = (ClampMin = "10.0", ToolTip = "Maximum downward trace distance for climb down (cm)."))
	float ClimbDownDownwardTraceDepth;

	/** Horizontal displacement offset baked into the climb down root motion montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|ClimbDown", meta = (ToolTip = "Horizontal displacement offset baked into root motion (cm)."))
	float ClimbDownRootMotionForwardOffset;

	/** Vertical displacement offset baked into the climb down root motion montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|ClimbDown", meta = (ToolTip = "Vertical displacement offset baked into root motion (cm)."))
	float ClimbDownRootMotionDownwardOffset;

	/** Minimum alignment dot between wall normal and probe direction for valid climb down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|ClimbDown", meta = (ClampMin = "-1.0", ClampMax = "1.0", ToolTip = "Minimum approach dot product for climb down."))
	float ClimbDownMinApproachDot;
};

/**
 * Standing clearance capsule checks for mantling onto solid floors.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingMantleConfig
{
	GENERATED_BODY()

	FThreadTalesClimbingMantleConfig()
		: MantleStandCapsuleRadius(31.0f)
		, MantleStandCapsuleHalfHeight(82.0f)
		, MantleStandForwardOffset(40.0f)
		, MantleStandUpOffset(5.0f)
		, MantleWallContinuityProbeDepth(15.0f)
	{
	}

	/** Radius of capsule used to verify standing space when climbing up */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|Mantle", meta = (ClampMin = "10.0", ToolTip = "Standing clearance capsule radius (cm)."))
	float MantleStandCapsuleRadius;

	/** Half-height of capsule used to verify standing space when climbing up */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|Mantle", meta = (ClampMin = "20.0", ToolTip = "Standing clearance capsule half-height (cm)."))
	float MantleStandCapsuleHalfHeight;

	/** Inward forward offset from ledge edge to target standing position */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|Mantle", meta = (ClampMin = "0.0", ToolTip = "Forward offset to target standing position (cm)."))
	float MantleStandForwardOffset;

	/** Vertical upward offset above floor surface for standing capsule check */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|Mantle", meta = (ClampMin = "0.0", ToolTip = "Vertical upward offset for standing check (cm)."))
	float MantleStandUpOffset;

	/** Inward offset from ledge to check if solid wall continues upward (rejecting mid-wall trims) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|Mantle", meta = (ClampMin = "1.0", ToolTip = "Wall continuity probe depth for mantle rejection (cm)."))
	float MantleWallContinuityProbeDepth;
};

/**
 * Wall kick-off impulse, sector raycasting, and opposing ledge detection tuning.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingWallEjectConfig
{
	GENERATED_BODY()

	FThreadTalesClimbingWallEjectConfig()
		: WallEjectHorizontalSpeed(450.0f)
		, WallEjectVerticalSpeed(350.0f)
		, WallEjectSectorHalfAngle(45.0f)
		, WallEjectSectorRayCount(7)
		, WallEjectMinOpposingHeight(-100.0f)
		, WallEjectMaxOpposingHeight(150.0f)
		, WallEjectNormalToleranceDot(-0.7f)
		, WallEjectRegrabCooldown(0.55f)
		, BackJumpOpposingLedgeDistance(450.0f)
	{
	}

	/** Horizontal impulse speed away from wall when ejecting */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|WallEject", meta = (ClampMin = "0.0", ToolTip = "Horizontal launch speed for wall eject (cm/s)."))
	float WallEjectHorizontalSpeed;

	/** Vertical upward impulse speed when ejecting from wall */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|WallEject", meta = (ClampMin = "0.0", ToolTip = "Vertical launch speed for wall eject (cm/s)."))
	float WallEjectVerticalSpeed;

	/** Half-angle of fan sector used to probe for opposing ledges behind character (degrees) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|WallEject", meta = (ClampMin = "10.0", ClampMax = "90.0", ToolTip = "Fan sector half-angle for opposing ledge search (degrees)."))
	float WallEjectSectorHalfAngle;

	/** Number of rays in fan sector used to scan for opposing ledges */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|WallEject", meta = (ClampMin = "1", ClampMax = "25", ToolTip = "Ray count in opposing ledge scan sector."))
	int32 WallEjectSectorRayCount;

	/** Minimum height relative to current ledge for detecting opposing ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|WallEject", meta = (ToolTip = "Minimum height for opposing ledge detection (cm)."))
	float WallEjectMinOpposingHeight;

	/** Maximum height relative to current ledge for detecting opposing ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|WallEject", meta = (ToolTip = "Maximum height for opposing ledge detection (cm)."))
	float WallEjectMaxOpposingHeight;

	/** Dot product tolerance for opposing wall normal alignment */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|WallEject", meta = (ClampMin = "-1.0", ClampMax = "0.0", ToolTip = "Normal tolerance dot product for opposing wall alignment."))
	float WallEjectNormalToleranceDot;

	/** Regrab cooldown duration applied after executing a wall eject */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|WallEject", meta = (ClampMin = "0.1", ToolTip = "Cooldown before allowing regrab after wall eject (seconds)."))
	float WallEjectRegrabCooldown;

	/** Maximum distance behind character to detect and leap to an opposing ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|WallEject", meta = (ClampMin = "50.0", ToolTip = "Maximum distance behind character to leap to opposing ledge (cm)."))
	float BackJumpOpposingLedgeDistance;
};

/**
 * General climbing mechanics and camera rotation parameters.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingGeneralConfig
{
	GENERATED_BODY()

	FThreadTalesClimbingGeneralConfig()
		: RegrabCooldownDuration(1.5f)
		, bAllowFreehang(true)
		, ClimbDownTraceDistance(70.0f)
		, JumpInputDebounceDuration(0.35f)
	{
	}

	/** Minimum time before character can regrab a ledge after dropping */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|General", meta = (ClampMin = "0.1", ToolTip = "Ledge regrab cooldown after dropping (seconds)."))
	float RegrabCooldownDuration;

	/** If true, character can grab and hang from narrow bars, rails, and edges with no wall underneath */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|General", meta = (ToolTip = "If true, allows freehanging on narrow bars and pipes."))
	bool bAllowFreehang;

	/** Search distance forward/down from feet to locate platform edge for climb down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|General", meta = (ClampMin = "10.0", ToolTip = "Search distance to locate platform edge for climb down (cm)."))
	float ClimbDownTraceDistance;

	/** Input debounce duration after triggering a directional ledge jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|General", meta = (ClampMin = "0.0", ToolTip = "Jump input debounce duration (seconds)."))
	float JumpInputDebounceDuration;
};

/**
 * Camera follow and corner transition turn tuning.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingCameraConfig
{
	GENERATED_BODY()

	FThreadTalesClimbingCameraConfig()
		: bEnableCornerCameraTurn(false)
		, CornerCameraTurnInterpSpeed(5.0f)
	{
	}

	/** Whether the camera control rotation should smoothly turn to face the new wall after a corner transition */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|Camera", meta = (ToolTip = "If true, smoothly rotates camera around corners."))
	bool bEnableCornerCameraTurn;

	/** Interpolation speed for smooth camera rotation follow after rounding a corner */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal|Camera", meta = (ClampMin = "1.0", ClampMax = "20.0", EditCondition = "bEnableCornerCameraTurn", ToolTip = "Camera corner turn interpolation speed."))
	float CornerCameraTurnInterpSpeed;
};

/**
 * Composite traversal configuration bundling shimmy, climb down, mantle, wall eject, and camera parameters.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingTraversalConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal", meta = (ToolTip = "Shimmy lateral movement tuning."))
	FThreadTalesClimbingShimmyConfig Shimmy;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal", meta = (ToolTip = "Climb down lowering tuning."))
	FThreadTalesClimbingClimbDownConfig ClimbDown;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal", meta = (ToolTip = "Mantle and stand clearance verification tuning."))
	FThreadTalesClimbingMantleConfig Mantle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal", meta = (ToolTip = "Wall eject and back jump tuning."))
	FThreadTalesClimbingWallEjectConfig WallEject;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal", meta = (ToolTip = "General climbing cooldowns and tolerances."))
	FThreadTalesClimbingGeneralConfig General;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Traversal", meta = (ToolTip = "Camera orientation tuning."))
	FThreadTalesClimbingCameraConfig Camera;
};
