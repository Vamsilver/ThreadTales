// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/ThreadTalesClimbingJumpBounds.h"
#include "ThreadTalesClimbingKinematicsConfig.generated.h"

/**
 * Spatial body positioning offsets and animation playback rate multipliers for climbing.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingKinematicsOffsets
{
	GENERATED_BODY()

	FThreadTalesClimbingKinematicsOffsets()
		: HangWallOffset(24.0f)
		, HangHeightOffset(70.0f)
		, ClimbAnimPlayRateMultiplier(1.0f)
		, JumpPlayRateMultiplier(1.0f)
		, ClimbBackJumpPlayRate(1.0f)
		, GrabPlayRate(1.4f)
		, BarGrabPlayRate(2.3f)
	{
	}

	/** Distance to position actor capsule outward from the wall along wall normal while hanging */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Offsets", meta = (ClampMin = "0.0", ToolTip = "Distance to position actor capsule outward from wall normal (cm)."))
	float HangWallOffset;

	/** Vertical downward offset from ledge top to character root/capsule location while hanging */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Offsets", meta = (ClampMin = "0.0", ToolTip = "Vertical downward offset from ledge top to character root (cm)."))
	float HangHeightOffset;

	/** Global playback rate multiplier for climbing montages and transitions */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Playback", meta = (ClampMin = "0.5", ClampMax = "3.0", ToolTip = "Global playback rate multiplier for climbing montages."))
	float ClimbAnimPlayRateMultiplier;

	/** Playback rate multiplier for climbing ledge jumps (Hop Up/Down, Side Jumps) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Playback", meta = (ClampMin = "0.5", ClampMax = "3.0", ToolTip = "Playback rate multiplier for climbing ledge jumps."))
	float JumpPlayRateMultiplier;

	/** Playback rate multiplier for wall eject and back jump montages */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Playback", meta = (ClampMin = "0.5", ClampMax = "3.0", ToolTip = "Playback rate multiplier for wall eject and back jump montages."))
	float ClimbBackJumpPlayRate;

	/** Playback rate multiplier for grabbing a braced ledge from air */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Playback", meta = (ClampMin = "0.5", ClampMax = "4.0", ToolTip = "Playback rate multiplier for grabbing a braced ledge from air."))
	float GrabPlayRate;

	/** Playback rate multiplier for grabbing a freehang bar/pipe from air */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Playback", meta = (ClampMin = "0.5", ClampMax = "4.0", ToolTip = "Playback rate multiplier for grabbing a freehang bar from air."))
	float BarGrabPlayRate;
};

/**
 * Trajectory durations, flight curve exponents, and parabolic arc heights for climbing jumps.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingKinematicsTimings
{
	GENERATED_BODY()

	FThreadTalesClimbingKinematicsTimings()
		: GrabCatchDuration(0.20f)
		, ShortJumpDuration(0.95f)
		, LongJumpDuration(1.00f)
		, LeapJumpDuration(1.00f)
		, JumpFlightEndRatio(0.66f)
		, ShortHandDetachTime(0.20f)
		, LongHandDetachTime(0.20f)
		, LeapHandDetachTime(0.20f)
		, ShortFlightDuration(0.80f)
		, LongFlightDuration(0.78f)
		, LeapFlightDuration(1.10f)
		, JumpArcPeakTime(0.50f)
		, ShortJumpArcHeight(5.0f)
		, JumpArcHeight(10.0f)
		, LeapJumpArcPeakTime(0.45f)
		, LeapJumpArcHeight(20.0f)
		, ShortJumpFlightExponent(1.0f)
		, bShortJumpUseEaseOut(false)
		, JumpFlightExponent(1.15f)
		, LeapJumpFlightExponent(1.20f)
	{
	}

	/** Total interpolation duration for smooth kinematic catch into wall anchor when grabbing from air (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Timings", meta = (ClampMin = "0.05", ClampMax = "0.6", ToolTip = "Catch interpolation duration (seconds)."))
	float GrabCatchDuration;

	/** Total interpolation duration for short ledge-to-ledge transitions (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Timings", meta = (ClampMin = "0.1", ToolTip = "Total duration for short jump transitions (seconds)."))
	float ShortJumpDuration;

	/** Total interpolation duration for long ledge-to-ledge transitions (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Timings", meta = (ClampMin = "0.1", ToolTip = "Total duration for long jump transitions (seconds)."))
	float LongJumpDuration;

	/** Total interpolation duration for leap ledge-to-ledge transitions (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Timings", meta = (ClampMin = "0.1", ToolTip = "Total duration for leap transitions (seconds)."))
	float LeapJumpDuration;

	/** Fraction of jump duration where flight phase ends and character settles onto ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Timings", meta = (ClampMin = "0.5", ClampMax = "0.95", ToolTip = "Fraction of duration where flight ends (0.5..0.95)."))
	float JumpFlightEndRatio;

	/** Delay from jump start until character hands detach from initial ledge for short jumps (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Timings", meta = (ClampMin = "0.05", ClampMax = "0.5", ToolTip = "Short jump hand detach delay (seconds)."))
	float ShortHandDetachTime;

	/** Delay from jump start until character hands detach from initial ledge for long jumps (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Timings", meta = (ClampMin = "0.05", ClampMax = "0.5", ToolTip = "Long jump hand detach delay (seconds)."))
	float LongHandDetachTime;

	/** Delay from jump start until character hands detach from initial ledge for leap transitions (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Timings", meta = (ClampMin = "0.05", ClampMax = "0.5", ToolTip = "Leap transition hand detach delay (seconds)."))
	float LeapHandDetachTime;

	/** Real duration of physical capsule flight for short jumps before settling onto ledge (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Timings", meta = (ClampMin = "0.15", ClampMax = "2.0", ToolTip = "Short jump physical flight duration (seconds)."))
	float ShortFlightDuration;

	/** Real duration of physical capsule flight for long jumps before settling onto ledge (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Timings", meta = (ClampMin = "0.15", ClampMax = "2.0", ToolTip = "Long jump physical flight duration (seconds)."))
	float LongFlightDuration;

	/** Real duration of physical capsule flight for leap transitions before settling onto ledge (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Timings", meta = (ClampMin = "0.15", ClampMax = "2.0", ToolTip = "Leap physical flight duration (seconds)."))
	float LeapFlightDuration;

	/** Normalized time [0..1] within jump when parabolic arc reaches maximum apex height */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Timings", meta = (ClampMin = "0.1", ClampMax = "0.9", ToolTip = "Normalized apex time for short/long jumps (0..1)."))
	float JumpArcPeakTime;

	/** Peak outward parabolic arc height (cm) added during Short jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Timings", meta = (ClampMin = "0.0", ClampMax = "30.0", ToolTip = "Peak outward parabolic arc height for short jumps (cm)."))
	float ShortJumpArcHeight;

	/** Peak outward parabolic arc height (cm) added during Long jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Timings", meta = (ClampMin = "0.0", ClampMax = "50.0", ToolTip = "Peak outward parabolic arc height for long jumps (cm)."))
	float JumpArcHeight;

	/** Normalized time [0..1] within leap jump when parabolic arc reaches apex */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Timings", meta = (ClampMin = "0.1", ClampMax = "0.9", ToolTip = "Normalized apex time for leap jumps (0..1)."))
	float LeapJumpArcPeakTime;

	/** Peak vertical parabolic arch height (cm) added during Leap transitions */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Timings", meta = (ClampMin = "0.0", ToolTip = "Peak vertical parabolic arch height for leap transitions (cm)."))
	float LeapJumpArcHeight;

	/** Flight curve interpolation exponent for Short jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Timings", meta = (ClampMin = "1.0", ClampMax = "3.0", ToolTip = "Short jump flight interpolation exponent."))
	float ShortJumpFlightExponent;

	/** If true, uses EaseOut interpolation for Short jumps; if false, uses EaseInOut */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Timings", meta = (ToolTip = "If true, uses EaseOut interpolation for Short jumps."))
	bool bShortJumpUseEaseOut;

	/** Flight curve interpolation exponent for Long jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Timings", meta = (ClampMin = "1.0", ClampMax = "3.0", ToolTip = "Long jump flight interpolation exponent."))
	float JumpFlightExponent;

	/** Flight curve interpolation exponent for Leap jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics|Timings", meta = (ClampMin = "1.0", ClampMax = "3.0", ToolTip = "Leap jump flight interpolation exponent."))
	float LeapJumpFlightExponent;
};

/**
 * Composite climbing kinematics configuration bundling offsets, timings, and jump bounds.
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingKinematicsConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics", meta = (ToolTip = "Body positioning offsets and playback rates."))
	FThreadTalesClimbingKinematicsOffsets Offsets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics", meta = (ToolTip = "Jump trajectory timings, flight durations, and arc parameters."))
	FThreadTalesClimbingKinematicsTimings Timings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Kinematics", meta = (ToolTip = "Spatial reach and distance bounds for Short, Long, and Leap jumps."))
	FThreadTalesClimbingJumpBounds Bounds;
};
