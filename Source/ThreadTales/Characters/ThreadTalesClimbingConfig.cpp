// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesClimbingConfig.h"

UThreadTalesClimbingConfig::UThreadTalesClimbingConfig()
{
	// Placement & Body Offsets
	HangWallOffset = 24.0f;
	HangHeightOffset = 70.0f;

	// Playback Rate Multipliers
	ClimbAnimPlayRateMultiplier = 1.0f;
	ClimbBackJumpPlayRate = 1.35f;

	// Kinematics & Jump Timings
	GrabCatchDuration = 0.20f;
	ShortJumpDuration = 0.95f;
	LongJumpDuration = 1.00f;
	LeapJumpDuration = 1.00f;
	JumpFlightEndRatio = 0.85f;
	JumpArcPeakTime = 0.50f;
	JumpArcHeight = 12.0f;
	LeapJumpArcPeakTime = 0.45f;
	LeapJumpArcHeight = 35.0f;

	// Short Bounds
	MinHopUpHeight = 20.0f;
	MaxHopUpHeight = 58.0f;
	MinSideJumpDistance = 25.0f;
	MaxSideJumpDistance = 58.0f;
	MinDiagonalJumpDistance = 25.0f;
	MaxDiagonalJumpDistance = 60.0f;
	MinHopDownHeight = 20.0f;
	MaxHopDownHeight = 58.0f;

	// Long Bounds
	MinLongHopUpHeight = 50.0f;
	MaxLongHopUpHeight = 110.0f;
	MinLongSideJumpDistance = 50.0f;
	MaxLongSideJumpDistance = 110.0f;
	MinLongDiagonalJumpDistance = 50.0f;
	MaxLongDiagonalJumpDistance = 110.0f;
	MinLongHopDownHeight = 50.0f;
	MaxLongHopDownHeight = 110.0f;

	// Leap Bounds
	MinLeapHopUpHeight = 85.0f;
	MaxLeapHopUpHeight = 200.0f;
	MinLeapSideJumpDistance = 85.0f;
	MaxLeapSideJumpDistance = 200.0f;
	MinLeapDiagonalJumpDistance = 85.0f;
	MaxLeapDiagonalJumpDistance = 200.0f;
	MinLeapHopDownHeight = 85.0f;
	MaxLeapHopDownHeight = 200.0f;

	// Shimmy
	ShimmyStepDistance = 67.0f;
	ShimmyCooldownDuration = 0.55f;
	ShimmyProbeDepths = { 3.0f, 7.0f, 12.0f };
	ShimmyVerticalTraceRange = 20.0f;
	ShimmyMaxZDifference = 20.0f;
	ShimmyInputDebounceDuration = 0.15f;

	// Climb Down
	ClimbDownStepBackDistance = 30.0f;
	ClimbDownDownwardTraceDepth = 120.0f;
	ClimbDownRootMotionForwardOffset = 72.2f;
	ClimbDownRootMotionDownwardOffset = 171.32f;
	ClimbDownMinApproachDot = 0.25f;

	// Mantle / Climb Up Stand Verification
	MantleStandCapsuleRadius = 31.0f;
	MantleStandCapsuleHalfHeight = 82.0f;
	MantleStandForwardOffset = 40.0f;
	MantleStandUpOffset = 5.0f;
	MantleWallContinuityProbeDepth = 15.0f;

	// Wall Eject & Back Jump
	WallEjectHorizontalSpeed = 450.0f;
	WallEjectVerticalSpeed = 350.0f;
	WallEjectSectorHalfAngle = 45.0f;
	WallEjectSectorRayCount = 7;
	WallEjectMinOpposingHeight = -100.0f;
	WallEjectMaxOpposingHeight = 150.0f;
	WallEjectNormalToleranceDot = -0.7f;
	WallEjectRegrabCooldown = 0.55f;
	BackJumpOpposingLedgeDistance = 450.0f;

	// Sensing & World Detection
	LedgeTag = FName(TEXT("Ledge"));
	bRequireLedgeTag = false;
	bAutoGrabLedge = true;
	ForwardTraceDistance = 100.0f;
	ForwardTraceZOffset = 45.0f;
	LedgeDownTraceHeight = 30.0f;
	LedgeDownTraceDepth = 45.0f;
	MinGrabHeight = 20.0f;
	MaxGrabHeight = 78.0f;
	HandSpacing = 22.0f;
	MaxApproachAngle = 65.0f;
	MaxGrabVelocityZ = 750.0f;
	LedgeProbeDepths = { 2.5f, 5.0f, 10.0f, 18.0f };
	MaxGrabWallDistance = 85.0f;
	LedgeSlopeMaxZDifference = 25.0f;
	LedgeHandsProbeDepth = 4.0f;
	LedgeHandsVerticalRange = 15.0f;

	// Sensing & Validation
	LipBelowWallTraceDepth = 6.0f;
	LipAboveAirTraceHeight = 6.0f;
	LipModularSeamRejectDistance = 3.5f;
	BodyClearanceRadius = 20.0f;
	BodyClearanceHeight = 40.0f;

	// General Tuning
	RegrabCooldownDuration = 1.5f;
	bAllowFreehang = true;
	ClimbDownTraceDistance = 70.0f;
	JumpInputDebounceDuration = 0.35f;

	// Sensing & Freehang
	FreehangWallCheckDepth = 70.0f;
	FreehangWallTraceForward = 35.0f;
	FreehangWallTraceBack = 25.0f;

	// Sensing & Climb Down Probing
	ClimbDownFloorTraceForwardDist = 8.0f;
	ClimbDownFloorTraceZRange = 35.0f;
	ClimbDownEdgeLipDepth = 8.0f;
	ClimbDownEdgeLipProbeRange = 25.0f;

	// Sensing & Multi-Ledge Jump Probing
	DirectionalProbeDepths = { 2.5f, 5.0f, 10.0f, 18.0f };
	UpJumpTopTraceOffset = 55.0f;
	DownJumpTopTraceOffset = 22.0f;
	DownJumpLeapTopTraceOffset = 28.0f;
	LateralJumpTopTraceOffset = 28.0f;
	DiagonalUpJumpTopTraceOffset = 50.0f;
	DiagonalDownJumpTopTraceOffset = 25.0f;

	// Sensing & Corner Traversal
	// CornerMaxAngleDegrees: 120° allows tight 90° corners + slight chamfers with tolerance
	CornerMaxAngleDegrees = 120.0f;
	// CornerProbeDistance: 80cm along tangent — enough to exit the current wall geometry
	CornerProbeDistance = 80.0f;
	// CornerInwardProbeDepth: 30cm past the pivot to reliably sample the new wall face
	CornerInwardProbeDepth = 30.0f;

	// Camera Tuning
	bEnableCornerCameraTurn = true;
	CornerCameraTurnInterpSpeed = 5.0f;

	// 3D Radial Ledge Scanner
	RadialScanConeAngleH = 45.0f;
	RadialScanConeAngleV = 35.0f;
	RadialScanRaysH = 5;
	RadialScanRaysV = 3;
	RadialScanMaxDistance = 220.0f;
	RadialScanAngleWeight = 0.65f;
	RadialScanDistanceWeight = 0.35f;
	RadialScanMaxValidations = 3;
	RadialScanContinuousThrottleInterval = 0.08f;
}
