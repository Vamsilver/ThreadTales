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

	const float EffMinHopUp = Config ? Config->Kinematics.Bounds.Short.MinHopUpHeight : 25.0f;
	const float EffMaxHopUp = Config ? Config->Kinematics.Bounds.Short.MaxHopUpHeight : 60.0f;
	const float EffMinSide = Config ? Config->Kinematics.Bounds.Short.MinSideJumpDistance : 25.0f;
	const float EffMaxSide = Config ? Config->Kinematics.Bounds.Short.MaxSideJumpDistance : 60.0f;
	const float EffMinDiag = Config ? Config->Kinematics.Bounds.Short.MinDiagonalJumpDistance : 25.0f;
	const float EffMaxDiag = Config ? Config->Kinematics.Bounds.Short.MaxDiagonalJumpDistance : 60.0f;
	const float EffMinHopDown = Config ? Config->Kinematics.Bounds.Short.MinHopDownHeight : 20.0f;
	const float EffMaxHopDown = Config ? Config->Kinematics.Bounds.Short.MaxHopDownHeight : 55.0f;

	const float EffMinLongHopUp = Config ? Config->Kinematics.Bounds.Long.MinLongHopUpHeight : 50.0f;
	const float EffMaxLongHopUp = Config ? Config->Kinematics.Bounds.Long.MaxLongHopUpHeight : 120.0f;
	const float EffMinLongSide = Config ? Config->Kinematics.Bounds.Long.MinLongSideJumpDistance : 50.0f;
	const float EffMaxLongSide = Config ? Config->Kinematics.Bounds.Long.MaxLongSideJumpDistance : 120.0f;
	const float EffMinLongDiag = Config ? Config->Kinematics.Bounds.Long.MinLongDiagonalJumpDistance : 50.0f;
	const float EffMaxLongDiag = Config ? Config->Kinematics.Bounds.Long.MaxLongDiagonalJumpDistance : 120.0f;
	const float EffMinLongHopDown = Config ? Config->Kinematics.Bounds.Long.MinLongHopDownHeight : 45.0f;
	const float EffMaxLongHopDown = Config ? Config->Kinematics.Bounds.Long.MaxLongHopDownHeight : 120.0f;

	const float EffMinLeapHopUp = Config ? Config->Kinematics.Bounds.Leap.MinLeapHopUpHeight : 100.0f;
	const float EffMaxLeapHopUp = Config ? Config->Kinematics.Bounds.Leap.MaxLeapHopUpHeight : 210.0f;
	const float EffMinLeapSide = Config ? Config->Kinematics.Bounds.Leap.MinLeapSideJumpDistance : 100.0f;
	const float EffMaxLeapSide = Config ? Config->Kinematics.Bounds.Leap.MaxLeapSideJumpDistance : 210.0f;
	const float EffMinLeapDiag = Config ? Config->Kinematics.Bounds.Leap.MinLeapDiagonalJumpDistance : 100.0f;
	const float EffMaxLeapDiag = Config ? Config->Kinematics.Bounds.Leap.MaxLeapDiagonalJumpDistance : 210.0f;
	const float EffMinLeapHopDown = Config ? Config->Kinematics.Bounds.Leap.MinLeapHopDownHeight : 80.0f;
	const float EffMaxLeapHopDown = Config ? Config->Kinematics.Bounds.Leap.MaxLeapHopDownHeight : 220.0f;

	// 1. Short Distance Configuration with smart directional fallbacks
	UAnimMontage* RawShortUp = Config ? Config->Montages.ShortJump.ClimbHopUpMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawShortRight = Config ? Config->Montages.ShortJump.ClimbJumpRightMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawShortLeft = Config ? Config->Montages.ShortJump.ClimbJumpLeftMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawShortDown = Config ? Config->Montages.ShortJump.ClimbHopDownMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawShortUpRight = Config ? Config->Montages.ShortJump.ClimbJumpUpRightMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawShortDownRight = Config ? Config->Montages.ShortJump.ClimbJumpDownRightMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawShortDownLeft = Config ? Config->Montages.ShortJump.ClimbJumpDownLeftMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawShortUpLeft = Config ? Config->Montages.ShortJump.ClimbJumpUpLeftMontage.LoadSynchronous() : nullptr;

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
	UAnimMontage* RawLongUp = Config ? Config->Montages.LongJump.ClimbLongHopUpMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawLongUpRight = Config ? Config->Montages.LongJump.ClimbLongJumpUpRightMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawLongRight = Config ? Config->Montages.LongJump.ClimbLongJumpRightMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawLongDownRight = Config ? Config->Montages.LongJump.ClimbLongJumpDownRightMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawLongDown = Config ? Config->Montages.LongJump.ClimbLongHopDownMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawLongDownLeft = Config ? Config->Montages.LongJump.ClimbLongJumpDownLeftMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawLongLeft = Config ? Config->Montages.LongJump.ClimbLongJumpLeftMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawLongUpLeft = Config ? Config->Montages.LongJump.ClimbLongJumpUpLeftMontage.LoadSynchronous() : nullptr;

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
	UAnimMontage* RawLeapUp = Config ? Config->Montages.LeapJump.LeapHopUpMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawLeapUpRight = Config ? Config->Montages.LeapJump.LeapJumpUpRightMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawLeapRight = Config ? Config->Montages.LeapJump.LeapJumpRightMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawLeapDownRight = Config ? Config->Montages.LeapJump.LeapJumpDownRightMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawLeapDown = Config ? Config->Montages.LeapJump.LeapHopDownMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawLeapDownLeft = Config ? Config->Montages.LeapJump.LeapJumpDownLeftMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawLeapLeft = Config ? Config->Montages.LeapJump.LeapJumpLeftMontage.LoadSynchronous() : nullptr;
	UAnimMontage* RawLeapUpLeft = Config ? Config->Montages.LeapJump.LeapJumpUpLeftMontage.LoadSynchronous() : nullptr;

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

void FThreadTalesJumpConfigTable::GetKinematicPhases(
	EThreadTalesLedgeJumpDistanceType DistanceType,
	const UThreadTalesClimbingConfig* Config,
	float& OutAnticipationEnd,
	float& OutFlightEnd,
	float TransitionDuration,
	bool bIsBackJump,
	bool bIsBackJumpUp)
{
	if (bIsBackJumpUp)
	{
		// Back jump up montage (AnimSeq_Traversal_Climb_Leap_Behind_Up, trimmed or full):
		// For trimmed montage (duration ~0.75s): launch at t ~ 0.35s, grab at t ~ 0.70s.
		// For full clip (duration ~1.333s): launch at t = 0.60s, grab at t = 1.00s.
		const float Duration = FMath::Max(TransitionDuration, 0.1f);
		if (Duration <= 0.90f)
		{
			OutAnticipationEnd = FMath::Clamp(0.35f / Duration, 0.20f, 0.55f);
			OutFlightEnd = FMath::Clamp(0.70f / Duration, OutAnticipationEnd + 0.10f, 0.95f);
		}
		else
		{
			OutAnticipationEnd = FMath::Clamp(0.60f / Duration, 0.10f, 0.60f);
			OutFlightEnd = FMath::Clamp(1.00f / Duration, OutAnticipationEnd + 0.10f, 0.95f);
		}
		return;
	}

	if (bIsBackJump || TransitionDuration > 1.6f)
	{
		// Back jump montage MD_leap_short_r_back1 (2.067s clip, ~1.817s transition):
		// Launch at t = 0.18s (0.10f normalized), hands grab target ledge at t = 0.74s (0.41f normalized)
		OutAnticipationEnd = 0.10f;
		OutFlightEnd = 0.41f;
		return;
	}

	switch (DistanceType)
	{
	case EThreadTalesLedgeJumpDistanceType::Leap:
		// Leap animations (1.45s clip): athletic launch from 0.14 to 0.45, hands grab target hold at ~0.64s (45% of clip)
		OutAnticipationEnd = 0.14f;
		OutFlightEnd = 0.45f;
		break;
	case EThreadTalesLedgeJumpDistanceType::Long:
		// Long animations (0.98s clip): root translation from 0.18 to 0.66, target hold reached at ~0.65s (66% of clip)
		OutAnticipationEnd = 0.18f;
		OutFlightEnd = 0.66f;
		break;
	case EThreadTalesLedgeJumpDistanceType::Short:
	default:
		// Short animations (0.98s clip): root translation from 0.18 to 0.66, target hold reached at ~0.65s (66% of clip)
		OutAnticipationEnd = 0.18f;
		OutFlightEnd = 0.66f;
		break;
	}

	OutAnticipationEnd = FMath::Clamp(OutAnticipationEnd, 0.05f, 0.35f);
	OutFlightEnd = FMath::Clamp(OutFlightEnd, OutAnticipationEnd + 0.10f, 0.95f);
}

float FThreadTalesClimbingKinematics::CalculateFlightAlpha(
	float NormalizedTime,
	EThreadTalesLedgeJumpDistanceType DistanceType,
	const UThreadTalesClimbingConfig* Config,
	float TransitionDuration,
	bool bIsBackJump,
	bool bIsBackJumpUp)
{
	float AnticipationEnd = 0.18f;
	float FlightEnd = 0.66f;
	FThreadTalesJumpConfigTable::GetKinematicPhases(DistanceType, Config, AnticipationEnd, FlightEnd, TransitionDuration, bIsBackJump, bIsBackJumpUp);

	if (NormalizedTime <= AnticipationEnd)
	{
		return 0.0f;
	}
	if (NormalizedTime >= FlightEnd)
	{
		return 1.0f;
	}

	const float FlightFraction = (NormalizedTime - AnticipationEnd) / FMath::Max(FlightEnd - AnticipationEnd, 0.001f);

	float DefaultExp = 1.0f;
	if (DistanceType == EThreadTalesLedgeJumpDistanceType::Long)
	{
		DefaultExp = 1.15f;
	}
	else if (DistanceType == EThreadTalesLedgeJumpDistanceType::Leap)
	{
		DefaultExp = 1.20f;
	}
	else // Short
	{
		DefaultExp = 1.0f;
	}

	float Exponent = DefaultExp;
	bool bUseEaseOut = false;
	if (Config)
	{
		if (DistanceType == EThreadTalesLedgeJumpDistanceType::Leap)
		{
			Exponent = (Config->Kinematics.Timings.LeapJumpFlightExponent >= 1.0f) ? Config->Kinematics.Timings.LeapJumpFlightExponent : 1.20f;
		}
		else if (DistanceType == EThreadTalesLedgeJumpDistanceType::Short)
		{
			Exponent = (Config->Kinematics.Timings.ShortJumpFlightExponent >= 1.0f) ? Config->Kinematics.Timings.ShortJumpFlightExponent : 1.0f;
			bUseEaseOut = Config->Kinematics.Timings.bShortJumpUseEaseOut;
		}
		else
		{
			Exponent = (Config->Kinematics.Timings.JumpFlightExponent >= 1.0f) ? Config->Kinematics.Timings.JumpFlightExponent : DefaultExp;
		}
	}

	if (bUseEaseOut)
	{
		return FMath::InterpEaseOut(0.0f, 1.0f, FlightFraction, Exponent);
	}
	return FMath::InterpEaseInOut(0.0f, 1.0f, FlightFraction, Exponent);
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
	FRotator& OutRotation,
	float TransitionDuration,
	bool bIsBackJump,
	bool bIsBackJumpUp)
{
	float InterpAlpha = 0.0f;
	float AnticipationEnd = 0.27f;
	float FlightEnd = 0.59f;

	if (bIsJumpTransition)
	{
		FThreadTalesJumpConfigTable::GetKinematicPhases(DistanceType, Config, AnticipationEnd, FlightEnd, TransitionDuration, bIsBackJump, bIsBackJumpUp);
		InterpAlpha = CalculateFlightAlpha(NormalizedTime, DistanceType, Config, TransitionDuration, bIsBackJump, bIsBackJumpUp);
	}
	else
	{
		// Shimmy: continuous smooth displacement across full duration
		InterpAlpha = FMath::SmoothStep(0.0f, 1.0f, NormalizedTime);
	}

	OutLocation = FMath::Lerp(StartLocation, TargetLocation, InterpAlpha);

	if (bIsBackJumpUp)
	{
		// Target downward offset to keep supporting hand planted firmly on ledge surface.
		// In AnimSeq_Traversal_Climb_Leap_Behind_Up, the braced coiling pose elevates the hand by ~21cm.
		// A 28cm downward offset places the palm directly onto the ledge top.
		constexpr float TargetVerticalOffset = 28.0f;
		float BackJumpUpVerticalOffset = TargetVerticalOffset;

		if (NormalizedTime <= AnticipationEnd)
		{
			// Smoothly ease in the vertical offset during anticipation to perfectly match montage BlendIn,
			// eliminating any initial pop or jerkiness from idle hang and keeping the hand anchored.
			const float AnticipationFraction = FMath::Clamp(NormalizedTime / FMath::Max(AnticipationEnd, 0.001f), 0.0f, 1.0f);
			BackJumpUpVerticalOffset = FMath::Lerp(0.0f, TargetVerticalOffset, FMath::SmoothStep(0.0f, 1.0f, AnticipationFraction));
		}
		else if (NormalizedTime > FlightEnd)
		{
			// During settle phase (FlightEnd -> 1.0f), smoothly blend offset back to 0.0f
			// so the actor arrives seamlessly at TargetLocation without popping upon entering IdleHang.
			const float SettleFraction = FMath::Clamp((NormalizedTime - FlightEnd) / FMath::Max(1.0f - FlightEnd, 0.001f), 0.0f, 1.0f);
			BackJumpUpVerticalOffset = FMath::Lerp(TargetVerticalOffset, 0.0f, FMath::SmoothStep(0.0f, 1.0f, SettleFraction));
		}
		OutLocation.Z -= BackJumpUpVerticalOffset;
	}

	float RotationAlpha = InterpAlpha;
	if (bIsBackJumpUp)
	{
		// When bracing against the wall (anticipation), keep actor strictly at StartRotation (0.0f)
		// so hands remain anchored to the starting ledge without detachment.
		if (NormalizedTime <= AnticipationEnd)
		{
			RotationAlpha = 0.0f;
		}
		else if (NormalizedTime >= FlightEnd)
		{
			RotationAlpha = 1.0f;
		}
		else
		{
			// Rotate 180 degrees smoothly while airborne during the flight arc
			const float FlightFraction = (NormalizedTime - AnticipationEnd) / FMath::Max(FlightEnd - AnticipationEnd, 0.001f);
			RotationAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, FlightFraction, 1.5f);
		}
	}
	OutRotation = FMath::Lerp(StartRotation, TargetRotation, RotationAlpha);

	if (bIsJumpTransition && Config)
	{
		float ArcHeight = Config->Kinematics.Timings.JumpArcHeight;
		if (DistanceType == EThreadTalesLedgeJumpDistanceType::Leap)
		{
			ArcHeight = Config->Kinematics.Timings.LeapJumpArcHeight;
		}
		else if (DistanceType == EThreadTalesLedgeJumpDistanceType::Short)
		{
			ArcHeight = Config->Kinematics.Timings.ShortJumpArcHeight;
		}

		if (ArcHeight > 0.0f && NormalizedTime >= AnticipationEnd && NormalizedTime <= FlightEnd)
		{
			const float FlightFraction = (NormalizedTime - AnticipationEnd) / FMath::Max(FlightEnd - AnticipationEnd, 0.001f);
			const float ArcFactor = 4.0f * FlightFraction * (1.0f - FlightFraction);
			const FVector WallOutward = -StartRotation.Vector();
			OutLocation += WallOutward * (ArcHeight * ArcFactor);

			if (bIsBackJumpUp)
			{
				// Native Motion Warping (RootMotionModifier_SkewWarp) drives the natural 3D trajectory
				// and vertical arc directly from the animation root motion. Synthetic VerticalArcOvershoot is bypassed.
				const float VerticalArcOvershoot = 0.0f;
				(void)VerticalArcOvershoot;
			}
		}
	}
}

FVector FThreadTalesClimbingKinematics::CalculateWallEjectVelocity(const FVector& WallNormal, const UThreadTalesClimbingConfig* Config)
{
	const float HorizSpeed = Config ? Config->Traversal.WallEject.WallEjectHorizontalSpeed : 450.0f;
	const float VertSpeed = Config ? Config->Traversal.WallEject.WallEjectVerticalSpeed : 350.0f;
	return (WallNormal * HorizSpeed) + (FVector::UpVector * VertSpeed);
}

void FThreadTalesClimbingKinematics::CalculateClimbDownPreAlignment(
	const FVector& LedgeLoc,
	const FVector& WallNormal2D,
	const UThreadTalesClimbingConfig* Config,
	FVector& OutStartLoc,
	FRotator& OutStartRot)
{
	const float HangWallOffset = Config ? Config->Kinematics.Offsets.HangWallOffset : 24.0f;
	const float HangHeightOffset = Config ? Config->Kinematics.Offsets.HangHeightOffset : 70.0f;
	const float RootMotionFwd = Config ? Config->Traversal.ClimbDown.ClimbDownRootMotionForwardOffset : 72.2f;
	const float RootMotionDown = Config ? Config->Traversal.ClimbDown.ClimbDownRootMotionDownwardOffset : 171.32f;

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

