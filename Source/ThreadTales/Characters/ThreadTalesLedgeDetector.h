// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Characters/ThreadTalesTraversalTypes.h"

class ACharacter;
class UThreadTalesClimbingConfig;
struct FThreadTalesJumpConfigTable;

/**
 * Dedicated spatial sensing & world collision analysis module for climbing traversal.
 * Handles all forward wall tracing, downward lip finding, hand placement, freehang checking,
 * 8-directional multi-ledge probing, climb-down cliff detection, and mantle standing space validation.
 */
class FThreadTalesLedgeDetector
{
public:
	/** Attempts to detect a grab-able ledge in front of the character */
	static bool DetectLedge(
		const ACharacter* Character,
		const UThreadTalesClimbingConfig* Config,
		bool bDrawDebugTraces,
		FVector& OutLedgeLocation,
		FVector& OutWallNormal
	);

	/** Multi-level raycast sweep against the forward surface */
	static bool TraceForwardWall(
		const ACharacter* Character,
		const UThreadTalesClimbingConfig* Config,
		bool bDrawDebugTraces,
		FHitResult& OutForwardHit,
		FVector& OutWallNormal2D
	);

	/** Downward raycast from above the forward wall hit to locate exact top surface */
	static bool ProbeLedgeTopSurface(
		const ACharacter* Character,
		const UThreadTalesClimbingConfig* Config,
		bool bDrawDebugTraces,
		const FHitResult& ForwardHit,
		const FVector& WallNormal2D,
		FHitResult& OutTopHit
	);

	/** Validates that a detected candidate ledge edge has a true physical lip and sufficient body clearance */
	static bool ValidateLedgeLipAndClearance(
		const ACharacter* Character,
		const UThreadTalesClimbingConfig* Config,
		bool bDrawDebugTraces,
		const FVector& EdgeLoc,
		const FVector& WallNormal2D
	);

	/** Verifies that both left and right hands can rest on a stable, non-sloped ledge surface */
	static bool ValidateLedgeHandsPlacement(
		const ACharacter* Character,
		const UThreadTalesClimbingConfig* Config,
		bool bDrawDebugTraces,
		const FVector& TrueLedgeEdge,
		const FVector& WallNormal2D
	);

	/** Probes below a candidate ledge lip to test if solid vertical wall exists (false = freehang bar/edge) */
	static bool CheckIsFreehangLedge(
		const ACharacter* Character,
		const UThreadTalesClimbingConfig* Config,
		bool bDrawDebugTraces,
		const FVector& EdgeLoc,
		const FVector& WallNormal2D
	);

	/** Probes for a higher ledge directly above the current one */
	static bool FindUpperLedge(
		const ACharacter* Character,
		const UThreadTalesClimbingConfig* Config,
		bool bDrawDebugTraces,
		const FVector& CurrentLedgeLoc,
		const FVector& CurrentWallNormal,
		FVector& OutLedgeLoc,
		FVector& OutWallNormal
	);

	/** Probes for adjacent ledge along wall tangent (-1 = Left, +1 = Right). If bForJump is true, probes for jump distance */
	static bool FindSideLedge(
		const ACharacter* Character,
		const UThreadTalesClimbingConfig* Config,
		bool bDrawDebugTraces,
		const FVector& CurrentLedgeLoc,
		const FVector& CurrentWallNormal,
		float DirectionSign,
		bool bForJump,
		FVector& OutLedgeLoc,
		FVector& OutWallNormal,
		bool& bOutIsContinuous
	);

	/** Probes for a target ledge in one of the 8 directions with Short, Long, or Leap distance parameter */
	static bool FindLedgeInDirection(
		const ACharacter* Character,
		const UThreadTalesClimbingConfig* Config,
		bool bDrawDebugTraces,
		const FVector& CurrentLedgeLoc,
		const FVector& CurrentWallNormal,
		EThreadTalesLedgeJumpDirection Direction,
		EThreadTalesLedgeJumpDistanceType DistanceType,
		const FThreadTalesJumpConfigTable& JumpTable,
		FVector& OutLedgeLoc,
		FVector& OutWallNormal
	);

	/** Searches downward in front/back of character standing at a platform edge to locate climbable drop-off */
	static bool FindEdgeForClimbDown(
		const ACharacter* Character,
		const UThreadTalesClimbingConfig* Config,
		bool bDrawDebugTraces,
		FVector& OutLedgeLoc,
		FVector& OutWallNormal,
		bool& bOutIsFreehang
	);

	/** Traces down at specified distance in front of feet to detect floor drop-off / cliff edge */
	static bool TraceClimbDownFloorCliff(
		const ACharacter* Character,
		bool bDrawDebugTraces,
		const FVector& ProbeDir,
		float Dist,
		FHitResult& OutFloorHit
	);

	/** Traces inward horizontally below floor level to pinpoint exact top lip and outer wall normal */
	static bool PinpointClimbDownLedgeLip(
		const ACharacter* Character,
		const UThreadTalesClimbingConfig* Config,
		bool bDrawDebugTraces,
		const FHitResult& WallHit,
		const FVector& WallNormal2D,
		float FeetZ,
		FVector& OutEdgeLoc,
		FVector& OutRefinedWallNormal
	);

	/** Checks whether character can mantle on top of the current ledge (no wall above, solid floor, standing space) */
	static bool CanClimbUp(
		const ACharacter* Character,
		const UThreadTalesClimbingConfig* Config,
		bool bDrawDebugTraces,
		const FVector& CurrentLedgeLoc,
		const FVector& CurrentWallNormal,
		FVector& OutStandingLoc
	);

	/** Searches behind the character's back for an opposing wall/ledge to jump onto */
	static bool FindOpposingLedge(
		const ACharacter* Character,
		const UThreadTalesClimbingConfig* Config,
		bool bDrawDebugTraces,
		const FVector& CurrentLedgeLoc,
		const FVector& CurrentWallNormal,
		FVector& OutLedgeLoc,
		FVector& OutWallNormal
	);

	/**
	 * Probes around a wall corner (outer or inner) when FindSideLedge finds no continuous ledge.
	 *
	 * Algorithm (3 phases):
	 *   Phase 1 — Corner pivot detection: line trace along wall tangent (DirectionSign * CornerProbeDistance).
	 *             No hit  → Outer corner, pivot = trace end point.
	 *             Wall hit → Inner corner, pivot = impact point, new normal = opposite of hit normal.
	 *   Phase 2 — Swing past pivot: for Outer rotate tangent 90° around Up and advance CornerInwardProbeDepth.
	 *                               for Inner retreat along new normal by CornerInwardProbeDepth.
	 *   Phase 3 — New ledge validation via ProbeLedgeTopSurface + ValidateLedgeLipAndClearance +
	 *             ValidateLedgeHandsPlacement on the new face.
	 *
	 * @param DirectionSign      +1.0 = right, -1.0 = left
	 * @param OutCornerType      Detected corner type (Outer / Inner)
	 * @param OutCornerPivotLoc  World position of the corner pivot point (for two-segment kinematics)
	 * @param OutLedgeLoc        World position of the new ledge edge on the far side of the corner
	 * @param OutWallNormal      Outward normal of the new ledge's wall face
	 * @return true if a valid, body-clearance-verified ledge was found around the corner
	 */
	static bool ProbeCornerLedge(
		const ACharacter* Character,
		const UThreadTalesClimbingConfig* Config,
		bool bDrawDebugTraces,
		const FVector& CurrentLedgeLoc,
		const FVector& CurrentWallNormal,
		float DirectionSign,
		EThreadTalesCornerType& OutCornerType,
		FVector& OutCornerPivotLoc,
		FVector& OutLedgeLoc,
		FVector& OutWallNormal
	);
};
