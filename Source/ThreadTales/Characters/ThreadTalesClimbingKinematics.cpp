// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesClimbingKinematics.h"
#include "Characters/ThreadTalesClimbingConfig.h"
#include "Animation/AnimMontage.h"

void FThreadTalesJumpConfigTable::SetConfig(EThreadTalesLedgeJumpDirection Dir, EThreadTalesLedgeJumpDistanceType DistType, UAnimMontage* Montage, float MinDist, float MaxDist, float NominalDist)
{
	const uint8 DirIdx = static_cast<uint8>(Dir);
	const uint8 DistIdx = static_cast<uint8>(DistType);
	if (DirIdx < 9 && DistIdx < 3)
	{
		Table[DirIdx][DistIdx].Montage = Montage;
		Table[DirIdx][DistIdx].MinDistance = MinDist;
		Table[DirIdx][DistIdx].MaxDistance = MaxDist;
		Table[DirIdx][DistIdx].NominalDistance = NominalDist;
	}
}

void FThreadTalesJumpConfigTable::Build(const UThreadTalesClimbingConfig* Config, UAnimMontage* FallbackHangMontage)
{
	// Clear table
	for (int32 d = 0; d < 9; ++d)
	{
		for (int32 dist = 0; dist < 3; ++dist)
		{
			Table[d][dist] = FThreadTalesLedgeJumpConfig();
		}
	}

	const float EffMinHopUp = Config ? Config->MinHopUpHeight : 25.0f;
	const float EffMaxHopUp = Config ? Config->MaxHopUpHeight : 60.0f;
	const float EffMinSide = Config ? Config->MinSideJumpDistance : 25.0f;
	const float EffMaxSide = Config ? Config->MaxSideJumpDistance : 60.0f;
	const float EffMinDiag = Config ? Config->MinDiagonalJumpDistance : 25.0f;
	const float EffMaxDiag = Config ? Config->MaxDiagonalJumpDistance : 60.0f;
	const float EffMinHopDown = Config ? Config->MinHopDownHeight : 20.0f;
	const float EffMaxHopDown = Config ? Config->MaxHopDownHeight : 55.0f;

	const float EffMinLongHopUp = Config ? Config->MinLongHopUpHeight : 50.0f;
	const float EffMaxLongHopUp = Config ? Config->MaxLongHopUpHeight : 120.0f;
	const float EffMinLongSide = Config ? Config->MinLongSideJumpDistance : 50.0f;
	const float EffMaxLongSide = Config ? Config->MaxLongSideJumpDistance : 120.0f;
	const float EffMinLongDiag = Config ? Config->MinLongDiagonalJumpDistance : 50.0f;
	const float EffMaxLongDiag = Config ? Config->MaxLongDiagonalJumpDistance : 120.0f;
	const float EffMinLongHopDown = Config ? Config->MinLongHopDownHeight : 45.0f;
	const float EffMaxLongHopDown = Config ? Config->MaxLongHopDownHeight : 120.0f;

	const float EffMinLeapHopUp = Config ? Config->MinLeapHopUpHeight : 100.0f;
	const float EffMaxLeapHopUp = Config ? Config->MaxLeapHopUpHeight : 210.0f;
	const float EffMinLeapSide = Config ? Config->MinLeapSideJumpDistance : 100.0f;
	const float EffMaxLeapSide = Config ? Config->MaxLeapSideJumpDistance : 210.0f;
	const float EffMinLeapDiag = Config ? Config->MinLeapDiagonalJumpDistance : 100.0f;
	const float EffMaxLeapDiag = Config ? Config->MaxLeapDiagonalJumpDistance : 210.0f;
	const float EffMinLeapHopDown = Config ? Config->MinLeapHopDownHeight : 80.0f;
	const float EffMaxLeapHopDown = Config ? Config->MaxLeapHopDownHeight : 220.0f;

	// 1. Short Distance Configuration with smart directional fallbacks
	UAnimMontage* RawShortUp = Config ? Config->ClimbHopUpMontage.Get() : nullptr;
	UAnimMontage* RawShortRight = Config ? Config->ClimbJumpRightMontage.Get() : nullptr;
	UAnimMontage* RawShortLeft = Config ? Config->ClimbJumpLeftMontage.Get() : nullptr;
	UAnimMontage* RawShortDown = Config ? Config->ClimbHopDownMontage.Get() : nullptr;
	UAnimMontage* RawShortUpRight = Config ? Config->ClimbJumpUpRightMontage.Get() : nullptr;
	UAnimMontage* RawShortDownRight = Config ? Config->ClimbJumpDownRightMontage.Get() : nullptr;
	UAnimMontage* RawShortDownLeft = Config ? Config->ClimbJumpDownLeftMontage.Get() : nullptr;
	UAnimMontage* RawShortUpLeft = Config ? Config->ClimbJumpUpLeftMontage.Get() : nullptr;

	UAnimMontage* ShortUp = RawShortUp;
	UAnimMontage* ShortRight = RawShortRight;
	UAnimMontage* ShortLeft = RawShortLeft;
	UAnimMontage* ShortDown = RawShortDown ? RawShortDown : FallbackHangMontage;

	UAnimMontage* ShortUpRight = RawShortUpRight ? RawShortUpRight : (ShortRight ? ShortRight : ShortUp);
	UAnimMontage* ShortDownRight = RawShortDownRight ? RawShortDownRight : ShortRight;
	UAnimMontage* ShortDownLeft = RawShortDownLeft ? RawShortDownLeft : ShortLeft;
	UAnimMontage* ShortUpLeft = RawShortUpLeft ? RawShortUpLeft : (ShortLeft ? ShortLeft : ShortUp);

	SetConfig(EThreadTalesLedgeJumpDirection::Up,        EThreadTalesLedgeJumpDistanceType::Short, ShortUp,        EffMinHopUp,   EffMaxHopUp,   48.1f);
	SetConfig(EThreadTalesLedgeJumpDirection::UpRight,   EThreadTalesLedgeJumpDistanceType::Short, ShortUpRight,   EffMinDiag,    EffMaxDiag,    48.1f);
	SetConfig(EThreadTalesLedgeJumpDirection::Right,     EThreadTalesLedgeJumpDistanceType::Short, ShortRight,     EffMinSide,    EffMaxSide,    48.1f);
	SetConfig(EThreadTalesLedgeJumpDirection::DownRight, EThreadTalesLedgeJumpDistanceType::Short, ShortDownRight, EffMinDiag,    EffMaxDiag,    48.1f);
	SetConfig(EThreadTalesLedgeJumpDirection::Down,      EThreadTalesLedgeJumpDistanceType::Short, ShortDown,      EffMinHopDown, EffMaxHopDown, 48.1f);
	SetConfig(EThreadTalesLedgeJumpDirection::DownLeft,  EThreadTalesLedgeJumpDistanceType::Short, ShortDownLeft,  EffMinDiag,    EffMaxDiag,    48.1f);
	SetConfig(EThreadTalesLedgeJumpDirection::Left,      EThreadTalesLedgeJumpDistanceType::Short, ShortLeft,      EffMinSide,    EffMaxSide,    48.1f);
	SetConfig(EThreadTalesLedgeJumpDirection::UpLeft,    EThreadTalesLedgeJumpDistanceType::Short, ShortUpLeft,    EffMinDiag,    EffMaxDiag,    48.1f);

	// 2. Long Distance Configuration (fall back to Short if unassigned)
	UAnimMontage* RawLongUp = Config ? Config->ClimbLongHopUpMontage.Get() : nullptr;
	UAnimMontage* RawLongUpRight = Config ? Config->ClimbLongJumpUpRightMontage.Get() : nullptr;
	UAnimMontage* RawLongRight = Config ? Config->ClimbLongJumpRightMontage.Get() : nullptr;
	UAnimMontage* RawLongDownRight = Config ? Config->ClimbLongJumpDownRightMontage.Get() : nullptr;
	UAnimMontage* RawLongDown = Config ? Config->ClimbLongHopDownMontage.Get() : nullptr;
	UAnimMontage* RawLongDownLeft = Config ? Config->ClimbLongJumpDownLeftMontage.Get() : nullptr;
	UAnimMontage* RawLongLeft = Config ? Config->ClimbLongJumpLeftMontage.Get() : nullptr;
	UAnimMontage* RawLongUpLeft = Config ? Config->ClimbLongJumpUpLeftMontage.Get() : nullptr;

	UAnimMontage* LongUp = RawLongUp ? RawLongUp : ShortUp;
	UAnimMontage* LongUpRight = RawLongUpRight ? RawLongUpRight : ShortUpRight;
	UAnimMontage* LongRight = RawLongRight ? RawLongRight : ShortRight;
	UAnimMontage* LongDownRight = RawLongDownRight ? RawLongDownRight : ShortDownRight;
	UAnimMontage* LongDown = RawLongDown ? RawLongDown : ShortDown;
	UAnimMontage* LongDownLeft = RawLongDownLeft ? RawLongDownLeft : ShortDownLeft;
	UAnimMontage* LongLeft = RawLongLeft ? RawLongLeft : ShortLeft;
	UAnimMontage* LongUpLeft = RawLongUpLeft ? RawLongUpLeft : ShortUpLeft;

	SetConfig(EThreadTalesLedgeJumpDirection::Up,        EThreadTalesLedgeJumpDistanceType::Long, LongUp,        EffMinLongHopUp,   EffMaxLongHopUp,   96.2f);
	SetConfig(EThreadTalesLedgeJumpDirection::UpRight,   EThreadTalesLedgeJumpDistanceType::Long, LongUpRight,   EffMinLongDiag,    EffMaxLongDiag,    96.2f);
	SetConfig(EThreadTalesLedgeJumpDirection::Right,     EThreadTalesLedgeJumpDistanceType::Long, LongRight,     EffMinLongSide,    EffMaxLongSide,    96.2f);
	SetConfig(EThreadTalesLedgeJumpDirection::DownRight, EThreadTalesLedgeJumpDistanceType::Long, LongDownRight, EffMinLongDiag,    EffMaxLongDiag,    96.2f);
	SetConfig(EThreadTalesLedgeJumpDirection::Down,      EThreadTalesLedgeJumpDistanceType::Long, LongDown,      EffMinLongHopDown, EffMaxLongHopDown, 96.2f);
	SetConfig(EThreadTalesLedgeJumpDirection::DownLeft,  EThreadTalesLedgeJumpDistanceType::Long, LongDownLeft,  EffMinLongDiag,    EffMaxLongDiag,    96.2f);
	SetConfig(EThreadTalesLedgeJumpDirection::Left,      EThreadTalesLedgeJumpDistanceType::Long, LongLeft,      EffMinLongSide,    EffMaxLongSide,    96.2f);
	SetConfig(EThreadTalesLedgeJumpDirection::UpLeft,    EThreadTalesLedgeJumpDistanceType::Long, LongUpLeft,    EffMinLongDiag,    EffMaxLongDiag,    96.2f);

	// 3. Leap Distance Configuration (fall back to Long if unassigned)
	UAnimMontage* RawLeapUp = Config ? Config->LeapHopUpMontage.Get() : nullptr;
	UAnimMontage* RawLeapUpRight = Config ? Config->LeapJumpUpRightMontage.Get() : nullptr;
	UAnimMontage* RawLeapRight = Config ? Config->LeapJumpRightMontage.Get() : nullptr;
	UAnimMontage* RawLeapDownRight = Config ? Config->LeapJumpDownRightMontage.Get() : nullptr;
	UAnimMontage* RawLeapDown = Config ? Config->LeapHopDownMontage.Get() : nullptr;
	UAnimMontage* RawLeapDownLeft = Config ? Config->LeapJumpDownLeftMontage.Get() : nullptr;
	UAnimMontage* RawLeapLeft = Config ? Config->LeapJumpLeftMontage.Get() : nullptr;
	UAnimMontage* RawLeapUpLeft = Config ? Config->LeapJumpUpLeftMontage.Get() : nullptr;

	UAnimMontage* LeapUp = RawLeapUp ? RawLeapUp : LongUp;
	UAnimMontage* LeapUpRight = RawLeapUpRight ? RawLeapUpRight : LongUpRight;
	UAnimMontage* LeapRight = RawLeapRight ? RawLeapRight : LongRight;
	UAnimMontage* LeapDownRight = RawLeapDownRight ? RawLeapDownRight : LongDownRight;
	UAnimMontage* LeapDown = RawLeapDown ? RawLeapDown : LongDown;
	UAnimMontage* LeapDownLeft = RawLeapDownLeft ? RawLeapDownLeft : LongDownLeft;
	UAnimMontage* LeapLeft = RawLeapLeft ? RawLeapLeft : LongLeft;
	UAnimMontage* LeapUpLeft = RawLeapUpLeft ? RawLeapUpLeft : LongUpLeft;

	SetConfig(EThreadTalesLedgeJumpDirection::Up,        EThreadTalesLedgeJumpDistanceType::Leap, LeapUp,        EffMinLeapHopUp,   EffMaxLeapHopUp,   200.0f);
	SetConfig(EThreadTalesLedgeJumpDirection::UpRight,   EThreadTalesLedgeJumpDistanceType::Leap, LeapUpRight,   EffMinLeapDiag,    EffMaxLeapDiag,    200.0f);
	SetConfig(EThreadTalesLedgeJumpDirection::Right,     EThreadTalesLedgeJumpDistanceType::Leap, LeapRight,     EffMinLeapSide,    EffMaxLeapSide,    200.0f);
	SetConfig(EThreadTalesLedgeJumpDirection::DownRight, EThreadTalesLedgeJumpDistanceType::Leap, LeapDownRight, EffMinLeapDiag,    EffMaxLeapDiag,    200.0f);
	SetConfig(EThreadTalesLedgeJumpDirection::Down,      EThreadTalesLedgeJumpDistanceType::Leap, LeapDown,      EffMinLeapHopDown, EffMaxLeapHopDown, 200.0f);
	SetConfig(EThreadTalesLedgeJumpDirection::DownLeft,  EThreadTalesLedgeJumpDistanceType::Leap, LeapDownLeft,  EffMinLeapDiag,    EffMaxLeapDiag,    200.0f);
	SetConfig(EThreadTalesLedgeJumpDirection::Left,      EThreadTalesLedgeJumpDistanceType::Leap, LeapLeft,      EffMinLeapSide,    EffMaxLeapSide,    200.0f);
	SetConfig(EThreadTalesLedgeJumpDirection::UpLeft,    EThreadTalesLedgeJumpDistanceType::Leap, LeapUpLeft,    EffMinLeapDiag,    EffMaxLeapDiag,    200.0f);
}

const FThreadTalesLedgeJumpConfig& FThreadTalesJumpConfigTable::GetConfig(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const
{
	const uint8 DirIdx = static_cast<uint8>(Direction);
	const uint8 DistIdx = static_cast<uint8>(DistanceType);
	if (DirIdx < 9 && DistIdx < 3)
	{
		return Table[DirIdx][DistIdx];
	}
	static const FThreadTalesLedgeJumpConfig EmptyConfig;
	return EmptyConfig;
}

bool FThreadTalesJumpConfigTable::GetDistanceBounds(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, float& OutMinDist, float& OutMaxDist) const
{
	if (Direction == EThreadTalesLedgeJumpDirection::None)
	{
		OutMinDist = 0.0f;
		OutMaxDist = 0.0f;
		return false;
	}

	const FThreadTalesLedgeJumpConfig& Config = GetConfig(Direction, DistanceType);
	OutMinDist = Config.MinDistance;
	OutMaxDist = Config.MaxDistance;
	return (Config.MaxDistance > 0.0f);
}

bool FThreadTalesJumpConfigTable::IsDistanceValid(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, float Distance) const
{
	const FThreadTalesLedgeJumpConfig& Config = GetConfig(Direction, DistanceType);
	return (Distance >= Config.MinDistance && Distance <= Config.MaxDistance);
}

float FThreadTalesJumpConfigTable::GetNominalDistance(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const
{
	return GetConfig(Direction, DistanceType).NominalDistance;
}

UAnimMontage* FThreadTalesJumpConfigTable::GetJumpMontage(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const
{
	return GetConfig(Direction, DistanceType).Montage;
}

TArray<EThreadTalesLedgeJumpDistanceType> FThreadTalesJumpConfigTable::GetDistancePriorities(bool bIsSprint)
{
	TArray<EThreadTalesLedgeJumpDistanceType> Priorities;
	if (bIsSprint)
	{
		Priorities.Add(EThreadTalesLedgeJumpDistanceType::Long);
		Priorities.Add(EThreadTalesLedgeJumpDistanceType::Leap);
		Priorities.Add(EThreadTalesLedgeJumpDistanceType::Short);
	}
	else
	{
		Priorities.Add(EThreadTalesLedgeJumpDistanceType::Short);
		Priorities.Add(EThreadTalesLedgeJumpDistanceType::Long);
		Priorities.Add(EThreadTalesLedgeJumpDistanceType::Leap);
	}
	return Priorities;
}

void FThreadTalesJumpConfigTable::GetKinematicPhases(EThreadTalesLedgeJumpDistanceType DistanceType, const UThreadTalesClimbingConfig* Config, float& OutAnticipationEnd, float& OutFlightEnd)
{
	switch (DistanceType)
	{
	case EThreadTalesLedgeJumpDistanceType::Leap:
		// Leap animations (1.50s): Anticipation up to 0.17s, flight 0.17s .. 0.67s, landing/settle 0.67s .. 1.0
		OutAnticipationEnd = 0.17f;
		OutFlightEnd = 0.67f;
		break;
	case EThreadTalesLedgeJumpDistanceType::Long:
		// Long animations (1.20s): Anticipation up to 0.18s, flight 0.18s .. 0.75s, landing/settle 0.75s .. 1.0
		OutAnticipationEnd = 0.18f;
		OutFlightEnd = 0.75f;
		break;
	case EThreadTalesLedgeJumpDistanceType::Short:
	default:
		// Short animations (0.95s): Anticipation up to 0.19s, flight 0.19s .. 0.78s, landing/settle 0.78s .. 1.0
		OutAnticipationEnd = 0.19f;
		OutFlightEnd = 0.78f;
		break;
	}
}

void FThreadTalesClimbingKinematics::CalculateTransitionTransform(
	float NormalizedTime,
	bool bIsJumpTransition,
	EThreadTalesLedgeJumpDistanceType DistanceType,
	const UThreadTalesClimbingConfig* Config,
	const FVector& StartLocation,
	const FVector& TargetLocation,
	const FRotator& StartRotation,
	const FRotator& TargetRotation,
	FVector& OutLocation,
	FRotator& OutRotation)
{
	float InterpAlpha = 0.0f;
	if (bIsJumpTransition)
	{
		float AnticipationEnd = 0.19f;
		float FlightEnd = 0.78f;
		FThreadTalesJumpConfigTable::GetKinematicPhases(DistanceType, Config, AnticipationEnd, FlightEnd);

		if (NormalizedTime < AnticipationEnd)
		{
			InterpAlpha = 0.0f;
		}
		else if (NormalizedTime > FlightEnd)
		{
			InterpAlpha = 1.0f;
		}
		else
		{
			const float FlightFraction = (NormalizedTime - AnticipationEnd) / FMath::Max(FlightEnd - AnticipationEnd, 0.001f);
			InterpAlpha = FMath::SmoothStep(0.0f, 1.0f, FlightFraction);
		}
	}
	else
	{
		// Shimmy: continuous smooth displacement across full duration
		InterpAlpha = FMath::SmoothStep(0.0f, 1.0f, NormalizedTime);
	}

	OutLocation = FMath::Lerp(StartLocation, TargetLocation, InterpAlpha);
	OutRotation = FMath::Lerp(StartRotation, TargetRotation, InterpAlpha);
}

FVector FThreadTalesClimbingKinematics::CalculateWallEjectVelocity(const FVector& WallNormal, const UThreadTalesClimbingConfig* Config)
{
	const float HorizSpeed = Config ? Config->WallEjectHorizontalSpeed : 450.0f;
	const float VertSpeed = Config ? Config->WallEjectVerticalSpeed : 350.0f;
	return (WallNormal * HorizSpeed) + (FVector::UpVector * VertSpeed);
}

void FThreadTalesClimbingKinematics::CalculateClimbDownPreAlignment(
	const FVector& LedgeLoc,
	const FVector& WallNormal2D,
	const UThreadTalesClimbingConfig* Config,
	FVector& OutStartLoc,
	FRotator& OutStartRot)
{
	const float HangWallOffset = Config ? Config->HangWallOffset : 24.0f;
	const float HangHeightOffset = Config ? Config->HangHeightOffset : 70.0f;
	const float RootMotionFwd = Config ? Config->ClimbDownRootMotionForwardOffset : 72.2f;
	const float RootMotionDown = Config ? Config->ClimbDownRootMotionDownwardOffset : 171.32f;

	const FVector HangLoc = LedgeLoc + (WallNormal2D * HangWallOffset) - FVector(0.0f, 0.0f, HangHeightOffset);

	OutStartRot = FRotator(0.0f, WallNormal2D.Rotation().Yaw, 0.0f);
	OutStartLoc = FVector(
		HangLoc.X - (WallNormal2D.X * RootMotionFwd),
		HangLoc.Y - (WallNormal2D.Y * RootMotionFwd),
		HangLoc.Z + RootMotionDown
	);
}

void FThreadTalesClimbingKinematics::CalculateCornerTransitionTransform(
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
	FRotator& OutRot)
{
	const float T = FMath::Clamp(NormalizedTime, 0.0f, 1.0f);

	// The active turning motion in the corner montage completes at ~0.55s.
	// We dynamically calculate ActiveTurnFraction based on TransitionDuration so the capsule
	// reliably finishes turning and arrives at TargetLoc at exactly 0.55s, regardless of montage length!
	const float ActiveTurnFraction = FMath::Clamp(0.55f / FMath::Max(TransitionDuration, 0.01f), 0.1f, 1.0f);

	if (T >= ActiveTurnFraction)
	{
		// Active turn complete: character is firmly on the new ledge, playing settle/re-grip
		OutLoc = TargetLoc;
		OutRot = TargetRot;
		return;
	}

	const float TurnT = T / ActiveTurnFraction;

	// Rotations point into walls, so wall normals point away from walls (outward):
	const FVector WallNormal1 = (-StartRot.Vector()).GetSafeNormal2D();
	const FVector WallNormal2 = (-TargetRot.Vector()).GetSafeNormal2D();

	// Calculate corner apex for the capsule center:
	// In both outer (convex) and inner (concave) 90-degree corners, offsetting from CornerPivotLoc
	// by (WallNormal1 + WallNormal2) * HangWallOffset places the apex at exactly
	// HangWallOffset clearance from BOTH walls simultaneously, smoothly curving around the corner
	// rather than penetrating the solid wall geometry!
	const FVector CornerApex = FVector(
		CornerPivotLoc.X + (WallNormal1.X + WallNormal2.X) * HangWallOffset,
		CornerPivotLoc.Y + (WallNormal1.Y + WallNormal2.Y) * HangWallOffset,
		FMath::Lerp(StartLoc.Z, TargetLoc.Z, 0.5f)
	);

	FVector HorizontalLoc;
	if (TurnT <= 0.5f)
	{
		// Segment 1: StartLoc → CornerApex
		const float SegT = FMath::SmoothStep(0.0f, 1.0f, TurnT / 0.5f);
		HorizontalLoc = FMath::Lerp(StartLoc, CornerApex, SegT);
	}
	else
	{
		// Segment 2: CornerApex → TargetLoc
		const float SegT = FMath::SmoothStep(0.0f, 1.0f, (TurnT - 0.5f) / 0.5f);
		HorizontalLoc = FMath::Lerp(CornerApex, TargetLoc, SegT);
	}

	const float BlendedZ = FMath::Lerp(StartLoc.Z, TargetLoc.Z, TurnT);
	OutLoc = FVector(HorizontalLoc.X, HorizontalLoc.Y, BlendedZ);

	// Smooth rotation Slerp across the active turn phase
	const float RotT = FMath::SmoothStep(0.0f, 1.0f, TurnT);
	const FQuat StartQuat  = FQuat(StartRot);
	const FQuat TargetQuat = FQuat(TargetRot);
	OutRot = FQuat::Slerp(StartQuat, TargetQuat, RotT).Rotator();
}

