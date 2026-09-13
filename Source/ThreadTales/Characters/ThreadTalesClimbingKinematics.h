// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/ThreadTalesTraversalTypes.h"

class UAnimMontage;
class UThreadTalesClimbingConfig;

/**
 * Fast lookup table for directional jumps: [Direction 0..8][DistanceType 0..2]
 * Resolves fallback hierarchies and caches bounds.
 */
struct FThreadTalesJumpConfigTable
{
	FThreadTalesLedgeJumpConfig Table[9][3];

	/** Compiles individual montage/distance properties into JumpConfigTable with fallback hierarchy */
	void Build(const UThreadTalesClimbingConfig* Config, UAnimMontage* FallbackHangMontage = nullptr);

	const FThreadTalesLedgeJumpConfig& GetConfig(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const;
	bool GetDistanceBounds(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, float& OutMinDist, float& OutMaxDist) const;
	bool IsDistanceValid(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, float Distance) const;
	float GetNominalDistance(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const;
	UAnimMontage* GetJumpMontage(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const;

	static TArray<EThreadTalesLedgeJumpDistanceType> GetDistancePriorities(bool bIsSprint);
	static void GetKinematicPhases(EThreadTalesLedgeJumpDistanceType DistanceType, const UThreadTalesClimbingConfig* Config, float& OutAnticipationEnd, float& OutFlightEnd);

private:
	void SetConfig(EThreadTalesLedgeJumpDirection Dir, EThreadTalesLedgeJumpDistanceType DistType, UAnimMontage* Montage, float MinDist, float MaxDist, float NominalDist);
};

/**
 * Pure kinematic math utility for climbing transitions, parabolic arcs, pre-alignment, and impulses.
 */
class FThreadTalesClimbingKinematics
{
public:
	/** Calculates interpolated actor location and rotation during a ledge transition */
	static void CalculateTransitionTransform(
		float NormalizedTime,
		bool bIsJumpTransition,
		EThreadTalesLedgeJumpDistanceType DistanceType,
		const UThreadTalesClimbingConfig* Config,
		const FVector& StartLocation,
		const FVector& TargetLocation,
		const FRotator& StartRotation,
		const FRotator& TargetRotation,
		FVector& OutLocation,
		FRotator& OutRotation
	);

	/** Computes kick-off launch impulse velocity for wall ejection */
	static FVector CalculateWallEjectVelocity(
		const FVector& WallNormal,
		const UThreadTalesClimbingConfig* Config
	);

	/** Calculates actor spawn / start transform before playing root motion climb down montage */
	static void CalculateClimbDownPreAlignment(
		const FVector& LedgeLoc,
		const FVector& WallNormal2D,
		const UThreadTalesClimbingConfig* Config,
		FVector& OutStartLoc,
		FRotator& OutStartRot
	);

	/**
	 * Calculates capsule transform for 90-degree corner transition around a wall/beam.
	 * Decouples the horizontal XY curved arc (which bows outward around the corner vertex)
	 * from vertical Z height, and synchronizes the active turning motion with the montage's
	 * active phase (~0.55s), keeping the capsule planted at target for settling.
	 *
	 * @param NormalizedTime   Overall transition progress [0..1]
	 * @param CornerType       Outer (convex) or Inner (concave) corner
	 * @param HangWallOffset   Horizontal wall offset for capsule hang
	 * @param HangHeightOffset Vertical hang offset below ledge
	 * @param StartLoc         Hang location on the starting ledge
	 * @param CornerPivotLoc   World position of the corner vertex at ledge height
	 * @param TargetLoc        Target hang location on the new ledge
	 * @param StartRot         Character rotation at start (facing old wall)
	 * @param TargetRot        Character rotation at target (facing new wall)
	 * @param OutLoc           Interpolated world location
	 * @param OutRot           Interpolated world rotation
	 */
	static void CalculateCornerTransitionTransform(
		float NormalizedTime,
		float TransitionDuration,
		EThreadTalesCornerType CornerType,
		float HangWallOffset,
		float HangHeightOffset,
		const FVector& StartLoc,
		const FVector& CornerPivotLoc,
		const FVector& TargetLoc,
		const FRotator& StartRot,
		const FRotator& TargetRot,
		FVector& OutLoc,
		FRotator& OutRot
	);
};
