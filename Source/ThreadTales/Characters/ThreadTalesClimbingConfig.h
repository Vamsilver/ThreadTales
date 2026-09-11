// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ThreadTalesClimbingConfig.generated.h"

class UAnimMontage;

/**
 * Data Asset holding all climbing montages, tuning parameters, and body calibrations for a character archetype.
 * Completely decouples character-specific animation physics from the world-sensing TraversalComponent.
 */
UCLASS(BlueprintType)
class THREADTALES_API UThreadTalesClimbingConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UThreadTalesClimbingConfig();

	// ==========================================
	// BASE CLIMBING & IDLE MONTAGES
	// ==========================================

	/** Looping idle hang montage when braced against wall */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Base")
	TObjectPtr<UAnimMontage> HangMontage;

	/** Looping idle hang montage when freehanging (feet dangling, no wall) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Base")
	TObjectPtr<UAnimMontage> BarHangMontage;

	/** Root motion mantle montage onto solid floor */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Base")
	TObjectPtr<UAnimMontage> ClimbOnTopMontage;

	/** Root motion mantle montage onto freehang bar/pipe */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Base")
	TObjectPtr<UAnimMontage> BarClimbOnTopMontage;

	/** Root motion climb down montage from standing into braced hang */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Base")
	TObjectPtr<UAnimMontage> ClimbDownMontage;

	/** Root motion climb down montage from standing into freehang */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Base")
	TObjectPtr<UAnimMontage> BarClimbDownMontage;

	/** Kick-off from wall / leap to opposing ledge montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Base")
	TObjectPtr<UAnimMontage> ClimbBackJumpMontage;

	/** Shimmy step left along continuous ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Base")
	TObjectPtr<UAnimMontage> ShimmyLeftMontage;

	/** Shimmy step right along continuous ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Base")
	TObjectPtr<UAnimMontage> ShimmyRightMontage;

	// ==========================================
	// SHORT DIRECTIONAL JUMP MONTAGES
	// ==========================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Short")
	TObjectPtr<UAnimMontage> ClimbHopUpMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Short")
	TObjectPtr<UAnimMontage> ClimbJumpUpRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Short")
	TObjectPtr<UAnimMontage> ClimbJumpRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Short")
	TObjectPtr<UAnimMontage> ClimbJumpDownRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Short")
	TObjectPtr<UAnimMontage> ClimbHopDownMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Short")
	TObjectPtr<UAnimMontage> ClimbJumpDownLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Short")
	TObjectPtr<UAnimMontage> ClimbJumpLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Short")
	TObjectPtr<UAnimMontage> ClimbJumpUpLeftMontage;

	// ==========================================
	// LONG DIRECTIONAL JUMP MONTAGES
	// ==========================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Long")
	TObjectPtr<UAnimMontage> ClimbLongHopUpMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Long")
	TObjectPtr<UAnimMontage> ClimbLongJumpUpRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Long")
	TObjectPtr<UAnimMontage> ClimbLongJumpRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Long")
	TObjectPtr<UAnimMontage> ClimbLongJumpDownRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Long")
	TObjectPtr<UAnimMontage> ClimbLongHopDownMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Long")
	TObjectPtr<UAnimMontage> ClimbLongJumpDownLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Long")
	TObjectPtr<UAnimMontage> ClimbLongJumpLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Long")
	TObjectPtr<UAnimMontage> ClimbLongJumpUpLeftMontage;

	// ==========================================
	// LEAP DIRECTIONAL JUMP MONTAGES
	// ==========================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Leap")
	TObjectPtr<UAnimMontage> LeapHopUpMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Leap")
	TObjectPtr<UAnimMontage> LeapJumpUpRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Leap")
	TObjectPtr<UAnimMontage> LeapJumpRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Leap")
	TObjectPtr<UAnimMontage> LeapJumpDownRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Leap")
	TObjectPtr<UAnimMontage> LeapHopDownMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Leap")
	TObjectPtr<UAnimMontage> LeapJumpDownLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Leap")
	TObjectPtr<UAnimMontage> LeapJumpLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Montages|Leap")
	TObjectPtr<UAnimMontage> LeapJumpUpLeftMontage;

	// ==========================================
	// PLACEMENT & BODY OFFSETS
	// ==========================================

	/** Distance to position actor capsule outward from the wall along wall normal while hanging */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Offsets")
	float HangWallOffset;

	/** Vertical downward offset from ledge top to character root/capsule location while hanging */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Offsets")
	float HangHeightOffset;

	// ==========================================
	// PLAYBACK RATE MULTIPLIERS
	// ==========================================

	/** Global playback rate multiplier for climbing montages and transitions (e.g. 1.0 = normal) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Playback", meta = (ClampMin = "0.5", ClampMax = "3.0"))
	float ClimbAnimPlayRateMultiplier;

	/** Playback rate multiplier for wall eject and back jump montages */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Playback", meta = (ClampMin = "0.5", ClampMax = "3.0"))
	float ClimbBackJumpPlayRate;

	// ==========================================
	// KINEMATICS & JUMP TIMINGS
	// ==========================================

	/** Total interpolation duration for short ledge-to-ledge transitions (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Kinematics")
	float ShortJumpDuration;

	/** Total interpolation duration for long ledge-to-ledge transitions (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Kinematics")
	float LongJumpDuration;

	/** Total interpolation duration for leap ledge-to-ledge transitions (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Kinematics")
	float LeapJumpDuration;

	/** Fraction of jump duration where flight phase ends and character settles onto ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Kinematics", meta = (ClampMin = "0.5", ClampMax = "0.95"))
	float JumpFlightEndRatio;

	/** Normalized time [0..1] within jump when parabolic arc reaches maximum apex height */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Kinematics", meta = (ClampMin = "0.1", ClampMax = "0.9"))
	float JumpArcPeakTime;

	/** Peak vertical parabolic arch height (cm) added during Short and Long jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Kinematics")
	float JumpArcHeight;

	/** Normalized time [0..1] within leap jump when parabolic arc reaches apex */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Kinematics", meta = (ClampMin = "0.1", ClampMax = "0.9"))
	float LeapJumpArcPeakTime;

	/** Peak vertical parabolic arch height (cm) added during Leap transitions */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Kinematics")
	float LeapJumpArcHeight;

	// ==========================================
	// DISTANCE BOUNDS (SHORT, LONG, LEAP)
	// ==========================================

	// Short Bounds
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Short")
	float MinHopUpHeight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Short")
	float MaxHopUpHeight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Short")
	float MinSideJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Short")
	float MaxSideJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Short")
	float MinDiagonalJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Short")
	float MaxDiagonalJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Short")
	float MinHopDownHeight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Short")
	float MaxHopDownHeight;

	// Long Bounds
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Long")
	float MinLongHopUpHeight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Long")
	float MaxLongHopUpHeight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Long")
	float MinLongSideJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Long")
	float MaxLongSideJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Long")
	float MinLongDiagonalJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Long")
	float MaxLongDiagonalJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Long")
	float MinLongHopDownHeight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Long")
	float MaxLongHopDownHeight;

	// Leap Bounds
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Leap")
	float MinLeapHopUpHeight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Leap")
	float MaxLeapHopUpHeight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Leap")
	float MinLeapSideJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Leap")
	float MaxLeapSideJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Leap")
	float MinLeapDiagonalJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Leap")
	float MaxLeapDiagonalJumpDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Leap")
	float MinLeapHopDownHeight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Bounds|Leap")
	float MaxLeapHopDownHeight;

	// ==========================================
	// SHIMMY (CONTINUOUS LEDGE TRAVERSAL)
	// ==========================================

	/** Horizontal step distance per shimmy input (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Shimmy")
	float ShimmyStepDistance;

	/** Cooldown between shimmy steps (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Shimmy")
	float ShimmyCooldownDuration;

	/** Depths into ledge surface to probe for wall continuity during shimmy (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Shimmy")
	TArray<float> ShimmyProbeDepths;

	/** Vertical search range up/down to locate ledge top during shimmy (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Shimmy")
	float ShimmyVerticalTraceRange;

	/** Maximum allowed Z difference between current and candidate ledge point during shimmy */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Shimmy")
	float ShimmyMaxZDifference;

	/** Input debounce duration for shimmy */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Shimmy")
	float ShimmyInputDebounceDuration;

	// ==========================================
	// CLIMB DOWN (ROOT MOTION & ALIGNMENT)
	// ==========================================

	/** Horizontal backward step distance from detected ledge edge to initiate climb down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|ClimbDown")
	float ClimbDownStepBackDistance;

	/** Maximum downward trace distance to verify wall face continuity for climb down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|ClimbDown")
	float ClimbDownDownwardTraceDepth;

	/** Horizontal displacement offset baked into the climb down root motion montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|ClimbDown")
	float ClimbDownRootMotionForwardOffset;

	/** Vertical displacement offset baked into the climb down root motion montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|ClimbDown")
	float ClimbDownRootMotionDownwardOffset;

	/** Minimum alignment dot between wall normal and probe direction for valid climb down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|ClimbDown")
	float ClimbDownMinApproachDot;

	// ==========================================
	// MANTLE / CLIMB UP STAND CLEARANCE
	// ==========================================

	/** Radius of capsule used to verify standing space when climbing up */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Mantle")
	float MantleStandCapsuleRadius;

	/** Half-height of capsule used to verify standing space when climbing up */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Mantle")
	float MantleStandCapsuleHalfHeight;

	/** Inward forward offset from ledge edge to target standing position */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Mantle")
	float MantleStandForwardOffset;

	/** Vertical upward offset above floor surface for standing capsule check */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Mantle")
	float MantleStandUpOffset;

	/** Inward offset from ledge to check if solid wall continues upward (rejecting mid-wall trims) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|Mantle")
	float MantleWallContinuityProbeDepth;

	// ==========================================
	// WALL EJECT & BACK JUMP
	// ==========================================

	/** Horizontal impulse speed away from wall when ejecting */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|WallEject")
	float WallEjectHorizontalSpeed;

	/** Vertical upward impulse speed when ejecting from wall */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|WallEject")
	float WallEjectVerticalSpeed;

	/** Half-angle of fan sector used to probe for opposing ledges behind character (degrees) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|WallEject")
	float WallEjectSectorHalfAngle;

	/** Number of rays in fan sector used to scan for opposing ledges */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|WallEject")
	int32 WallEjectSectorRayCount;

	/** Minimum height relative to current ledge for detecting opposing ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|WallEject")
	float WallEjectMinOpposingHeight;

	/** Maximum height relative to current ledge for detecting opposing ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|WallEject")
	float WallEjectMaxOpposingHeight;

	/** Dot product tolerance for opposing wall normal alignment */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|WallEject")
	float WallEjectNormalToleranceDot;

	/** Regrab cooldown duration applied after executing a wall eject */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|WallEject")
	float WallEjectRegrabCooldown;

	/** Maximum distance behind character to detect and leap to an opposing ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|WallEject")
	float BackJumpOpposingLedgeDistance;

	// ==========================================
	// SENSING & WORLD DETECTION
	// ==========================================

	/** Actor or Component tag required for ledges */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Detection")
	FName LedgeTag;

	/** If true, only surfaces with LedgeTag can be grabbed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Detection")
	bool bRequireLedgeTag;

	/** If true, character automatically grabs detected ledges when airborne */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Detection")
	bool bAutoGrabLedge;

	/** Forward trace distance from character center */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Detection")
	float ForwardTraceDistance;

	/** Vertical offset from character center for forward trace start */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Detection")
	float ForwardTraceZOffset;

	/** Height above forward hit to start downward trace for finding exact top edge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Detection")
	float LedgeDownTraceHeight;

	/** Depth below forward hit for downward trace end */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Detection")
	float LedgeDownTraceDepth;

	/** Minimum ledge height relative to character root for valid grab (reach of arms) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Detection")
	float MinGrabHeight;

	/** Maximum ledge height relative to character root for valid grab (reach of arms) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Detection")
	float MaxGrabHeight;

	/** Half-width distance from center to each hand for 3-point trace (shoulder width) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Detection")
	float HandSpacing;

	/** Maximum approach angle (in degrees) to wall normal for valid grab */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Detection")
	float MaxApproachAngle;

	/** Maximum vertical speed on upward jump to allow grab */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Detection")
	float MaxGrabVelocityZ;

	/** Multi-depth inward probe distances into wall top to detect narrow shelves vs thick walls (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Detection")
	TArray<float> LedgeProbeDepths;

	/** Maximum forward distance from character to wall at grab moment to prevent teleportation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Detection")
	float MaxGrabWallDistance;

	/** Maximum allowable height difference between left and right hand placements (sloped ledges) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Detection")
	float LedgeSlopeMaxZDifference;

	/** Inward distance from edge to probe surface under left and right hands */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Detection")
	float LedgeHandsProbeDepth;

	/** Vertical half-range for probing surface under left and right hands */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Detection")
	float LedgeHandsVerticalRange;

	// ==========================================
	// SENSING & VALIDATION
	// ==========================================

	/** Depth below ledge lip for validating solid vertical wall surface */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Validation")
	float LipBelowWallTraceDepth;

	/** Height above ledge lip for validating open air (flat wall and modular seam rejector) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Validation")
	float LipAboveAirTraceHeight;

	/** Horizontal 2D distance threshold to reject modular mesh seams and continuous flat walls */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Validation")
	float LipModularSeamRejectDistance;

	/** Sphere sweep radius checking clearance for player's torso and head when hanging */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Validation")
	float BodyClearanceRadius;

	/** Vertical upward height checked for body clearance */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Validation")
	float BodyClearanceHeight;

	// ==========================================
	// GENERAL TUNING
	// ==========================================

	/** Minimum time before character can regrab a ledge after dropping */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|General")
	float RegrabCooldownDuration;

	/** If true, character can grab and hang from narrow bars, rails, and edges with no wall underneath */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|General")
	bool bAllowFreehang;

	/** Search distance forward/down from feet to locate platform edge for climb down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|General")
	float ClimbDownTraceDistance;

	/** Input debounce duration after triggering a directional ledge jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tuning|General")
	float JumpInputDebounceDuration;

	// ==========================================
	// SENSING & FREEHANG
	// ==========================================

	/** Depth below ledge lip to check for solid wall support (feet/knees brace level) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Freehang")
	float FreehangWallCheckDepth;

	/** Forward trace distance in front of wall for freehang check */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Freehang")
	float FreehangWallTraceForward;

	/** Inward trace depth behind wall surface for freehang check */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|Freehang")
	float FreehangWallTraceBack;

	// ==========================================
	// SENSING & CLIMB DOWN PROBING
	// ==========================================

	/** Inward distance from wall face to probe for top surface during climb down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|ClimbDown")
	float ClimbDownFloorTraceForwardDist;

	/** Vertical half-range for locating ledge top surface during climb down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|ClimbDown")
	float ClimbDownFloorTraceZRange;

	/** Depth below top surface to pinpoint exact XY edge lip during climb down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|ClimbDown")
	float ClimbDownEdgeLipDepth;

	/** Horizontal probe range for pinpointing XY edge lip during climb down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|ClimbDown")
	float ClimbDownEdgeLipProbeRange;

	// ==========================================
	// SENSING & MULTI-LEDGE JUMP PROBING
	// ==========================================

	/** Inward penetration depths tested when scanning for adjacent directional ledges */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|MultiLedge")
	TArray<float> DirectionalProbeDepths;

	/** Vertical upward offset for downward trace in Hop Up jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|MultiLedge")
	float UpJumpTopTraceOffset;

	/** Vertical upward offset for downward trace in Hop Down jumps (Short/Long) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|MultiLedge")
	float DownJumpTopTraceOffset;

	/** Vertical upward offset for downward trace in Hop Down Leap jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|MultiLedge")
	float DownJumpLeapTopTraceOffset;

	/** Vertical upward offset for downward trace in Lateral (Left/Right) jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|MultiLedge")
	float LateralJumpTopTraceOffset;

	/** Vertical upward offset for downward trace in Diagonal Up jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|MultiLedge")
	float DiagonalUpJumpTopTraceOffset;

	/** Vertical upward offset for downward trace in Diagonal Down jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sensing|MultiLedge")
	float DiagonalDownJumpTopTraceOffset;
};
