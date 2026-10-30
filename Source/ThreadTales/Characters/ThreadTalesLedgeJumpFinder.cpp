// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesLedgeDetector.h"
#include "Characters/ThreadTalesClimbingConfig.h"
#include "Characters/ThreadTalesClimbingKinematics.h"
#include "Characters/ThreadTalesTraversalComponent.h"
#include "ThreadTales.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "CollisionQueryParams.h"

bool FThreadTalesLedgeDetector::ProbeDirectionalLedge(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FVector& CurrentLedgeLoc,
	const FVector& ProbeStart,
	const FVector& ProbeEnd,
	float TopTraceOffsetZ,
	float MinDist,
	float MaxDist,
	const TArray<float>& ProbeDepths,
	FVector& OutLedgeLoc,
	FVector& OutWallNormal)
{
	if (!Character) { return false; }
	UWorld* World = Character->GetWorld();
	if (!World) { return false; }

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Character);
	Params.bTraceComplex = false;

	const float WallSweepRadius = 14.0f;
	FHitResult WallHit;
	bool bHitWall = World->SweepSingleByChannel(
		WallHit,
		ProbeStart,
		ProbeEnd,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(WallSweepRadius),
		Params
	);

	if (bDrawDebugTraces)
	{
		DrawDebugLine(World, ProbeStart, ProbeEnd, FColor(0, 180, 255), false, 1.5f, 0, 1.2f);
		DrawDebugSphere(World, ProbeStart, WallSweepRadius, 8, FColor(0, 180, 255), false, 1.5f, 0, 0.5f);
	}

	if (!bHitWall || !WallHit.bBlockingHit || FMath::Abs(WallHit.ImpactNormal.Z) > 0.45f)
	{
		return false;
	}

	const FVector FoundWallNorm2D = FVector(WallHit.ImpactNormal.X, WallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();

	if (bDrawDebugTraces)
	{
		DrawDebugSphere(World, WallHit.ImpactPoint, 6.0f, 8, FColor::Cyan, false, 1.5f);
	}

	FHitResult BestTopHit;
	bool bFoundValidTop = false;

	for (float Depth : ProbeDepths)
	{
		const FVector DownStart = WallHit.ImpactPoint - (FoundWallNorm2D * Depth) + FVector(0.0f, 0.0f, TopTraceOffsetZ);
		const FVector DownEnd = WallHit.ImpactPoint - (FoundWallNorm2D * Depth) - FVector(0.0f, 0.0f, TopTraceOffsetZ * 0.7f);

		FHitResult CurrentTopHit;
		if (World->LineTraceSingleByChannel(CurrentTopHit, DownStart, DownEnd, ECC_Visibility, Params))
		{
			if (bDrawDebugTraces)
			{
				DrawDebugLine(World, DownStart, DownEnd, FColor::Yellow, false, 1.5f, 0, 1.0f);
			}

			if (CurrentTopHit.ImpactNormal.Z >= 0.55f)
			{
				BestTopHit = CurrentTopHit;
				bFoundValidTop = true;
				break;
			}
		}
	}

	if (!bFoundValidTop) { return false; }

	const float LedgeTopZ = BestTopHit.ImpactPoint.Z;
	FThreadTalesLedgeAnalysisResult AnalysisResult;
	if (!PinpointAndValidateLedge(Character, Config, bDrawDebugTraces, WallHit.ImpactPoint, LedgeTopZ, FoundWallNorm2D, AnalysisResult))
	{
		return false;
	}

	const float Dist = FVector::Dist(AnalysisResult.ExactLedgeEdge, CurrentLedgeLoc);
	if (Dist < MinDist || Dist > MaxDist) { return false; }

	OutLedgeLoc = AnalysisResult.ExactLedgeEdge;
	OutWallNormal = AnalysisResult.RefinedWallNormal;
	if (bDrawDebugTraces)
	{
		DrawDebugSphere(World, OutLedgeLoc, 12.0f, 16, AnalysisResult.bIsFreehang ? FColor::Magenta : FColor::Orange, false, 3.0f, 0, 2.0f);
		DrawDebugDirectionalArrow(World, OutLedgeLoc, OutLedgeLoc + OutWallNormal * 35.0f, 12.0f, FColor::Yellow, false, 3.0f, 0, 2.0f);
	}

	return true;
}

bool FThreadTalesLedgeDetector::FindLedgeVertical(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FVector& CurrentLedgeLoc,
	const FVector& CurrentWallNormal,
	EThreadTalesLedgeJumpDirection Direction,
	EThreadTalesLedgeJumpDistanceType DistanceType,
	const FThreadTalesJumpConfigTable& JumpTable,
	FVector& OutLedgeLoc,
	FVector& OutWallNormal)
{
	float MinH = 0.0f;
	float MaxH = 0.0f;
	if (!JumpTable.GetDistanceBounds(Direction, DistanceType, MinH, MaxH))
	{
		return false;
	}

	const FVector Up = FVector::UpVector;
	const FVector WallNorm2D = CurrentWallNormal.GetSafeNormal();
	const FVector WallRight = FVector::CrossProduct(WallNorm2D, Up).GetSafeNormal();
	const TArray<float> ProbeDepths = (Config && Config->Sensing.Probing.DirectionalProbeDepths.Num() > 0) ? Config->Sensing.Probing.DirectionalProbeDepths : TArray<float>{ 2.5f, 5.0f, 10.0f, 18.0f };

	if (Direction == EThreadTalesLedgeJumpDirection::Up)
	{
		const float UpJumpTopOffset = Config ? Config->Sensing.Probing.UpJumpTopTraceOffset : 55.0f;
		TArray<float> Heights;
		switch (DistanceType)
		{
		case EThreadTalesLedgeJumpDistanceType::Short: Heights = { 30.0f, 42.0f, 55.0f }; break;
		case EThreadTalesLedgeJumpDistanceType::Long:  Heights = { 55.0f, 75.0f, 95.0f, 110.0f }; break;
		case EThreadTalesLedgeJumpDistanceType::Leap:  Heights = { 105.0f, 130.0f, 160.0f, 190.0f }; break;
		}

		for (float H : Heights)
		{
			const FVector Start = CurrentLedgeLoc + (CurrentWallNormal * 50.0f) + Up * H;
			const FVector End = Start - (CurrentWallNormal * 110.0f);
			if (ProbeDirectionalLedge(Character, Config, bDrawDebugTraces, CurrentLedgeLoc, Start, End, UpJumpTopOffset, MinH, MaxH, ProbeDepths, OutLedgeLoc, OutWallNormal))
			{
				return true;
			}
		}
		return false;
	}

	if (Direction == EThreadTalesLedgeJumpDirection::Down)
	{
		const float DownJumpTopOffset = Config ? Config->Sensing.Probing.DownJumpTopTraceOffset : 22.0f;
		const float DownJumpLeapTopOffset = Config ? Config->Sensing.Probing.DownJumpLeapTopTraceOffset : 28.0f;
		const float DownTopOffset = (DistanceType == EThreadTalesLedgeJumpDistanceType::Leap) ? DownJumpLeapTopOffset : DownJumpTopOffset;

		TArray<float> Heights;
		switch (DistanceType)
		{
		case EThreadTalesLedgeJumpDistanceType::Short: Heights = { -25.0f, -38.0f, -50.0f }; break;
		case EThreadTalesLedgeJumpDistanceType::Long:  Heights = { -45.0f, -60.0f, -75.0f, -95.0f, -115.0f }; break;
		case EThreadTalesLedgeJumpDistanceType::Leap:  Heights = { -85.0f, -110.0f, -140.0f, -170.0f, -200.0f }; break;
		}

		for (float H : Heights)
		{
			for (float Lat : { 0.0f, -18.0f, 18.0f })
			{
				const FVector Start = CurrentLedgeLoc + (WallRight * Lat) + (CurrentWallNormal * 50.0f) + Up * H;
				const FVector End = Start - (CurrentWallNormal * 110.0f);
				if (ProbeDirectionalLedge(Character, Config, bDrawDebugTraces, CurrentLedgeLoc, Start, End, DownTopOffset, MinH, MaxH, ProbeDepths, OutLedgeLoc, OutWallNormal))
				{
					return true;
				}
			}
		}
		return false;
	}

	return false;
}

bool FThreadTalesLedgeDetector::FindLedgeLateral(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FVector& CurrentLedgeLoc,
	const FVector& CurrentWallNormal,
	EThreadTalesLedgeJumpDirection Direction,
	EThreadTalesLedgeJumpDistanceType DistanceType,
	const FThreadTalesJumpConfigTable& JumpTable,
	FVector& OutLedgeLoc,
	FVector& OutWallNormal)
{
	float MinD = 0.0f;
	float MaxD = 0.0f;
	if (!JumpTable.GetDistanceBounds(Direction, DistanceType, MinD, MaxD))
	{
		return false;
	}

	const FVector Up = FVector::UpVector;
	const FVector WallNorm2D = CurrentWallNormal.GetSafeNormal();
	const FVector WallRight = FVector::CrossProduct(WallNorm2D, Up).GetSafeNormal();
	const float SideSign = (Direction == EThreadTalesLedgeJumpDirection::Right) ? 1.0f : -1.0f;
	const FVector SideDir = WallRight * SideSign;
	const float LateralJumpTopOffset = Config ? Config->Sensing.Probing.LateralJumpTopTraceOffset : 28.0f;
	const TArray<float> ProbeDepths = (Config && Config->Sensing.Probing.DirectionalProbeDepths.Num() > 0) ? Config->Sensing.Probing.DirectionalProbeDepths : TArray<float>{ 2.5f, 5.0f, 10.0f, 18.0f };

	TArray<float> Distances;
	switch (DistanceType)
	{
	case EThreadTalesLedgeJumpDistanceType::Short: Distances = { 30.0f, 42.0f, 55.0f }; break;
	case EThreadTalesLedgeJumpDistanceType::Long:  Distances = { 55.0f, 75.0f, 95.0f, 110.0f }; break;
	case EThreadTalesLedgeJumpDistanceType::Leap:  Distances = { 105.0f, 135.0f, 165.0f, 190.0f }; break;
	}

	for (float Dist : Distances)
	{
		for (float Vert : { 0.0f, 18.0f, -18.0f })
		{
			const FVector Start = CurrentLedgeLoc + (SideDir * Dist) + (Up * Vert) + (CurrentWallNormal * 50.0f);
			const FVector End = Start - (CurrentWallNormal * 110.0f);
			if (ProbeDirectionalLedge(Character, Config, bDrawDebugTraces, CurrentLedgeLoc, Start, End, LateralJumpTopOffset, MinD, MaxD, ProbeDepths, OutLedgeLoc, OutWallNormal))
			{
				return true;
			}
		}
	}
	return false;
}

bool FThreadTalesLedgeDetector::FindLedgeDiagonal(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FVector& CurrentLedgeLoc,
	const FVector& CurrentWallNormal,
	EThreadTalesLedgeJumpDirection Direction,
	EThreadTalesLedgeJumpDistanceType DistanceType,
	const FThreadTalesJumpConfigTable& JumpTable,
	FVector& OutLedgeLoc,
	FVector& OutWallNormal)
{
	float MinD = 0.0f;
	float MaxD = 0.0f;
	if (!JumpTable.GetDistanceBounds(Direction, DistanceType, MinD, MaxD))
	{
		return false;
	}

	const FVector Up = FVector::UpVector;
	const FVector WallNorm2D = CurrentWallNormal.GetSafeNormal();
	const FVector WallRight = FVector::CrossProduct(WallNorm2D, Up).GetSafeNormal();
	const float SideSign = (Direction == EThreadTalesLedgeJumpDirection::UpRight || Direction == EThreadTalesLedgeJumpDirection::DownRight) ? 1.0f : -1.0f;
	const bool bDiagUp = (Direction == EThreadTalesLedgeJumpDirection::UpRight || Direction == EThreadTalesLedgeJumpDirection::UpLeft);
	const FVector SideDir = WallRight * SideSign;
	const float DiagUpTopOffset = Config ? Config->Sensing.Probing.DiagonalUpJumpTopTraceOffset : 50.0f;
	const float DiagDownTopOffset = Config ? Config->Sensing.Probing.DiagonalDownJumpTopTraceOffset : 25.0f;
	const float DiagTopOffset = bDiagUp ? DiagUpTopOffset : DiagDownTopOffset;
	const TArray<float> ProbeDepths = (Config && Config->Sensing.Probing.DirectionalProbeDepths.Num() > 0) ? Config->Sensing.Probing.DirectionalProbeDepths : TArray<float>{ 2.5f, 5.0f, 10.0f, 18.0f };

	TArray<float> SideDists;
	switch (DistanceType)
	{
	case EThreadTalesLedgeJumpDistanceType::Short: SideDists = { 24.0f, 34.0f, 44.0f }; break;
	case EThreadTalesLedgeJumpDistanceType::Long:  SideDists = { 50.0f, 70.0f, 85.0f }; break;
	case EThreadTalesLedgeJumpDistanceType::Leap:  SideDists = { 80.0f, 110.0f, 140.0f }; break;
	}

	for (float SD : SideDists)
	{
		for (float DistMultiplier : { 1.0f })
		{
			const float VO = bDiagUp ? SD : -SD;
			const FVector Start = CurrentLedgeLoc + (SideDir * SD) + (Up * VO) + (CurrentWallNormal * 50.0f);
			const FVector End = Start - (CurrentWallNormal * 110.0f);
			if (ProbeDirectionalLedge(Character, Config, bDrawDebugTraces, CurrentLedgeLoc, Start, End, DiagTopOffset, MinD, MaxD, ProbeDepths, OutLedgeLoc, OutWallNormal))
			{
				return true;
			}
		}
	}
	return false;
}

bool FThreadTalesLedgeDetector::FindLedgeInDirection(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FVector& CurrentLedgeLoc,
	const FVector& CurrentWallNormal,
	EThreadTalesLedgeJumpDirection Direction,
	EThreadTalesLedgeJumpDistanceType DistanceType,
	const FThreadTalesJumpConfigTable& JumpTable,
	FVector& OutLedgeLoc,
	FVector& OutWallNormal)
{
	if (!Character || Direction == EThreadTalesLedgeJumpDirection::None) { return false; }

	switch (Direction)
	{
	case EThreadTalesLedgeJumpDirection::Up:
	case EThreadTalesLedgeJumpDirection::Down:
		return FindLedgeVertical(Character, Config, bDrawDebugTraces, CurrentLedgeLoc, CurrentWallNormal, Direction, DistanceType, JumpTable, OutLedgeLoc, OutWallNormal);

	case EThreadTalesLedgeJumpDirection::Right:
	case EThreadTalesLedgeJumpDirection::Left:
		return FindLedgeLateral(Character, Config, bDrawDebugTraces, CurrentLedgeLoc, CurrentWallNormal, Direction, DistanceType, JumpTable, OutLedgeLoc, OutWallNormal);

	case EThreadTalesLedgeJumpDirection::UpRight:
	case EThreadTalesLedgeJumpDirection::UpLeft:
	case EThreadTalesLedgeJumpDirection::DownRight:
	case EThreadTalesLedgeJumpDirection::DownLeft:
		return FindLedgeDiagonal(Character, Config, bDrawDebugTraces, CurrentLedgeLoc, CurrentWallNormal, Direction, DistanceType, JumpTable, OutLedgeLoc, OutWallNormal);

	default:
		break;
	}

	return false;
}

bool FThreadTalesLedgeDetector::CanClimbUp(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FVector& CurrentLedgeLoc,
	const FVector& CurrentWallNormal,
	FVector& OutStandingLoc)
{
	if (!Character) { return false; }
	UWorld* World = Character->GetWorld();
	if (!World) { return false; }

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	QueryParams.bTraceComplex = false;

	const float WallProbeDepth = Config ? Config->Traversal.Mantle.MantleWallContinuityProbeDepth : 15.0f;
	const float StandForwardOff = Config ? Config->Traversal.Mantle.MantleStandForwardOffset : 40.0f;
	const float StandUpOff = Config ? Config->Traversal.Mantle.MantleStandUpOffset : 5.0f;
	const float StandCapsuleHalfH = Config ? Config->Traversal.Mantle.MantleStandCapsuleHalfHeight : 82.0f;
	const float StandCapsuleR = Config ? Config->Traversal.Mantle.MantleStandCapsuleRadius : 31.0f;

	const FVector WallNormal2D = CurrentWallNormal.GetSafeNormal2D();
	const FVector InwardDir = -WallNormal2D;
	const FVector Up = FVector::UpVector;

	const bool bLog = UThreadTalesTraversalComponent::IsGlobalTraversalLoggingEnabled(Character);

	// 1. Wall Continuity Check: Does the wall continue upwards directly above the ledge?
	const FVector WallCheckStart = CurrentLedgeLoc + (WallNormal2D * WallProbeDepth);
	const float WallCheckZOffsets[] = { 30.0f, 70.0f, 120.0f, 160.0f };
	for (float ZOffset : WallCheckZOffsets)
	{
		FHitResult WallHit;
		const FVector RayStart = WallCheckStart + Up * ZOffset;
		const FVector RayEnd = RayStart + InwardDir * 35.0f;
		if (World->LineTraceSingleByChannel(WallHit, RayStart, RayEnd, ECC_Visibility, QueryParams))
		{
			if (bLog)
			{
				UE_LOG(LogThreadTalesTraversal, Verbose, TEXT("[TT_TRAVERSAL][MANTLE_REJECT] Wall continues upwards at ZOffset=%.1f (Hit at %s). Cannot mantle."), ZOffset, *WallHit.ImpactPoint.ToString());
			}
			if (bDrawDebugTraces)
			{
				DrawDebugLine(World, RayStart, WallHit.ImpactPoint, FColor::Red, false, 2.0f, 0, 2.0f);
				DrawDebugSphere(World, WallHit.ImpactPoint, 6.0f, 8, FColor::Red, false, 2.0f);
				DrawDebugString(World, WallHit.ImpactPoint + FVector(0, 0, 10), TEXT("WALL CONTINUES UP"), nullptr, FColor::Red, 2.0f, true);
			}
			return false;
		}
	}

	// 2. Floor Presence & Walkability Check
	const float InwardDistances[] = { StandForwardOff, 25.0f, 15.0f, 50.0f };
	FHitResult FloorHit;
	bool bFoundFloor = false;

	for (float InwardDist : InwardDistances)
	{
		const FVector PlatformProbeXY = CurrentLedgeLoc + (InwardDir * InwardDist);
		const FVector FloorTraceStart = PlatformProbeXY + Up * 60.0f;
		const FVector FloorTraceEnd = PlatformProbeXY - Up * 40.0f;

		if (World->LineTraceSingleByChannel(FloorHit, FloorTraceStart, FloorTraceEnd, ECC_Visibility, QueryParams))
		{
			if (FloorHit.ImpactNormal.Z >= 0.7f)
			{
				bFoundFloor = true;
				if (bDrawDebugTraces)
				{
					DrawDebugLine(World, FloorTraceStart, FloorHit.ImpactPoint, FColor::Green, false, 2.0f, 0, 2.0f);
					DrawDebugPoint(World, FloorHit.ImpactPoint, 10.0f, FColor::Green, false, 2.0f);
				}
				break;
			}
		}
	}

	if (!bFoundFloor)
	{
		if (bLog)
		{
			UE_LOG(LogThreadTalesTraversal, Warning, TEXT("[TT_TRAVERSAL][MANTLE_REJECT] No walkable floor found above ledge at %s."), *CurrentLedgeLoc.ToString());
		}
		if (bDrawDebugTraces)
		{
			const FVector PlatformProbeXY = CurrentLedgeLoc + (InwardDir * StandForwardOff);
			DrawDebugLine(World, PlatformProbeXY + Up * 60.0f, PlatformProbeXY - Up * 40.0f, FColor::Red, false, 2.0f, 0, 2.0f);
			DrawDebugString(World, PlatformProbeXY, TEXT("NO FLOOR TO STAND ON"), nullptr, FColor::Red, 2.0f, true);
		}
		return false;
	}

	// 3. Full Standing Capsule Clearance Check
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : StandCapsuleHalfH;
	const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : StandCapsuleR;

	const FVector TargetStandingLoc = FVector(FloorHit.ImpactPoint.X, FloorHit.ImpactPoint.Y, FloorHit.ImpactPoint.Z + HalfHeight + StandUpOff);

	FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(Radius * 0.9f, HalfHeight * 0.95f);
	FHitResult SweepHit;
	const bool bHit = World->SweepSingleByChannel(SweepHit, TargetStandingLoc, TargetStandingLoc + FVector(0.0f, 0.0f, 1.0f), FQuat::Identity, ECC_Visibility, CapsuleShape, QueryParams);

	if (bHit || SweepHit.bStartPenetrating)
	{
		if (bLog)
		{
			UE_LOG(LogThreadTalesTraversal, Warning, TEXT("[TT_TRAVERSAL][MANTLE_REJECT] Standing capsule clearance blocked at %s by actor '%s'."),
				*TargetStandingLoc.ToString(), SweepHit.GetActor() ? *SweepHit.GetActor()->GetName() : TEXT("Unknown"));
		}
		if (bDrawDebugTraces)
		{
			DrawDebugCapsule(World, TargetStandingLoc, HalfHeight * 0.95f, Radius * 0.9f, FQuat::Identity, FColor::Red, false, 2.0f);
			DrawDebugString(World, TargetStandingLoc, TEXT("CLEARANCE BLOCKED"), nullptr, FColor::Red, 2.0f, true);
		}
		return false;
	}

	if (bLog)
	{
		UE_LOG(LogThreadTalesTraversal, Log, TEXT("[TT_TRAVERSAL][MANTLE_OK] CanClimbUp passed! TargetStandingLoc: %s"),
			*TargetStandingLoc.ToString());
	}

	if (bDrawDebugTraces)
	{
		DrawDebugCapsule(World, TargetStandingLoc, HalfHeight * 0.95f, Radius * 0.9f, FQuat::Identity, FColor::Green, false, 2.0f);
		DrawDebugString(World, TargetStandingLoc, TEXT("CAN CLIMB UP"), nullptr, FColor::Green, 2.0f, true);
	}

	OutStandingLoc = TargetStandingLoc;
	return true;
}

static bool EvaluateOpposingWallHit(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FHitResult& WallHit,
	const FVector& HitNormal2D,
	float BaseZ,
	const FVector& ActorLoc,
	const FVector& SectorCenterDir,
	const FCollisionQueryParams& QueryParams,
	EThreadTalesOpposingLedgeFilter Filter,
	float& OutScore,
	FVector& OutLedgeLoc,
	FVector& OutWallNormal)
{
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!World) { return false; }

	const TArray<float> OpposingProbeDepths = (Config && Config->Sensing.Detection.LedgeProbeDepths.Num() > 0)
		? Config->Sensing.Detection.LedgeProbeDepths
		: TArray<float>{ 2.5f, 5.0f, 10.0f, 18.0f };

	bool bFoundTop = false;
	FHitResult BestEdgeHit;

	for (float InwardDepth : OpposingProbeDepths)
	{
		const FVector InwardOffset = -HitNormal2D * InwardDepth;
		const float TopTraceStartZ = FMath::Max(WallHit.ImpactPoint.Z + 250.0f, BaseZ + 250.0f);
		const float TopTraceEndZ = FMath::Min(WallHit.ImpactPoint.Z - 60.0f, BaseZ - 180.0f);
		const FVector TopTraceStart = FVector(WallHit.ImpactPoint.X + InwardOffset.X, WallHit.ImpactPoint.Y + InwardOffset.Y, TopTraceStartZ);
		const FVector TopTraceEnd = FVector(TopTraceStart.X, TopTraceStart.Y, TopTraceEndZ);

		FHitResult EdgeHit;
		const bool bHit = World->LineTraceSingleByChannel(EdgeHit, TopTraceStart, TopTraceEnd, ECC_Visibility, QueryParams);

		if (bDrawDebugTraces)
		{
			DrawDebugLine(World, TopTraceStart, TopTraceEnd, (bHit && EdgeHit.bBlockingHit) ? FColor::Cyan : FColor::Red, false, 2.0f, 0, 1.5f);
		}

		if (bHit && EdgeHit.bBlockingHit && EdgeHit.ImpactNormal.Z >= 0.65f)
		{
			BestEdgeHit = EdgeHit;
			bFoundTop = true;
			break;
		}
	}

	if (!bFoundTop) { return false; }

	FThreadTalesLedgeAnalysisResult AnalysisResult;
	if (!FThreadTalesLedgeDetector::PinpointAndValidateLedge(Character, Config, bDrawDebugTraces, WallHit.ImpactPoint, BestEdgeHit.ImpactPoint.Z, HitNormal2D, AnalysisResult))
	{
		return false;
	}

	const float LedgeHeightDelta = AnalysisResult.ExactLedgeEdge.Z - BaseZ;
	if (LedgeHeightDelta > 450.0f)
	{
		return false; // Exceeds max opposing reach
	}

	if (Filter == EThreadTalesOpposingLedgeFilter::UpOnly && LedgeHeightDelta < 25.0f)
	{
		return false; // Filter: candidate is not elevated above current ledge
	}
	else if (Filter == EThreadTalesOpposingLedgeFilter::DownOnly && LedgeHeightDelta > 50.0f)
	{
		return false; // Filter: candidate is elevated above horizontal/down threshold
	}

	const float Dist = FVector::Dist(ActorLoc, AnalysisResult.ExactLedgeEdge);
	const FVector DirToCand = (AnalysisResult.ExactLedgeEdge - ActorLoc).GetSafeNormal2D();
	const float AlignDot = FVector::DotProduct(DirToCand, SectorCenterDir);
	float HeightBonus = 0.0f;
	if (Filter == EThreadTalesOpposingLedgeFilter::UpOnly)
	{
		HeightBonus = LedgeHeightDelta * 0.8f;
	}
	else if (Filter == EThreadTalesOpposingLedgeFilter::DownOnly)
	{
		HeightBonus = -LedgeHeightDelta * 0.8f;
	}
	OutScore = (AlignDot * 500.0f) - Dist + HeightBonus;
	OutLedgeLoc = AnalysisResult.ExactLedgeEdge;
	OutWallNormal = AnalysisResult.RefinedWallNormal;
	return true;
}

bool FThreadTalesLedgeDetector::FindOpposingLedge(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FVector& CurrentLedgeLoc,
	const FVector& CurrentWallNormal,
	FVector& OutLedgeLoc,
	FVector& OutWallNormal,
	EThreadTalesOpposingLedgeFilter Filter)
{
	if (!Character) { return false; }
	UWorld* World = Character->GetWorld();
	if (!World) { return false; }

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	QueryParams.bTraceComplex = false;

	const FVector WallNormal2D = FVector(CurrentWallNormal.X, CurrentWallNormal.Y, 0.0f).GetSafeNormal();
	FVector SectorCenterDir = WallNormal2D;
	if (const AController* Controller = Character->GetController())
	{
		const FRotator ControlRot = Controller->GetControlRotation();
		const FVector CamFwd2D = FRotationMatrix(ControlRot).GetUnitAxis(EAxis::X).GetSafeNormal2D();
		if (FVector::DotProduct(CamFwd2D, WallNormal2D) > 0.05f)
		{
			SectorCenterDir = CamFwd2D;
		}
	}
	const float MaxOpposingDist = Config ? Config->Traversal.WallEject.BackJumpOpposingLedgeDistance : 450.0f;
	const float SectorHalfAngle = 60.0f;
	const int32 RayCount = 11;
	const float NormalToleranceDot = -0.30f;

	const FVector ActorLoc = Character->GetActorLocation();
	const float BaseZ = CurrentLedgeLoc.Z;

	bool bFoundOpposingLedge = false;
	FVector BestOpposingLedgeLoc = FVector::ZeroVector;
	FVector BestOpposingWallNormal = FVector::ZeroVector;
	float BestCandidateScore = -FLT_MAX;

	const TArray<float> ZOffsets = { -160.0f, -125.0f, -90.0f, -60.0f, -30.0f, 0.0f, 30.0f, 65.0f, 100.0f, 130.0f, 165.0f, 200.0f, 230.0f };

	for (int32 i = 0; i < RayCount; ++i)
	{
		const float Fraction = (RayCount > 1) ? (float)i / (float)(RayCount - 1) : 0.5f;
		const float Angle = FMath::Lerp(-SectorHalfAngle, SectorHalfAngle, Fraction);
		const FVector RayDir = SectorCenterDir.RotateAngleAxis(Angle, FVector::UpVector);

		for (float ZOffset : ZOffsets)
		{
			const FVector Start = FVector(ActorLoc.X, ActorLoc.Y, BaseZ + ZOffset);
			const FVector End = Start + (RayDir * MaxOpposingDist);

			FHitResult Hit;
			const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams);

			if (bDrawDebugTraces)
			{
				DrawDebugLine(World, Start, End, (bHit && Hit.bBlockingHit) ? FColor::Yellow : FColor(128, 0, 128), false, 2.0f, 0, 1.5f);
			}

			if (bHit && Hit.bBlockingHit)
			{
				const FVector HitNormal2D = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.0f).GetSafeNormal();
				if (FVector::DotProduct(HitNormal2D, SectorCenterDir) <= NormalToleranceDot)
				{
					float CandScore = 0.0f;
					FVector CandLoc = FVector::ZeroVector;
					FVector CandNormal = FVector::ZeroVector;
					if (EvaluateOpposingWallHit(Character, Config, bDrawDebugTraces, Hit, HitNormal2D, BaseZ, ActorLoc, SectorCenterDir, QueryParams, Filter, CandScore, CandLoc, CandNormal))
					{
						if (CandScore > BestCandidateScore)
						{
							BestCandidateScore = CandScore;
							BestOpposingLedgeLoc = CandLoc;
							BestOpposingWallNormal = CandNormal;
							bFoundOpposingLedge = true;
						}
					}
				}
			}
		}
	}

	if (bFoundOpposingLedge)
	{
		OutLedgeLoc = BestOpposingLedgeLoc;
		OutWallNormal = BestOpposingWallNormal;

		if (bDrawDebugTraces)
		{
			DrawDebugSphere(World, OutLedgeLoc, 15.0f, 12, FColor::Green, false, 3.0f, 0, 2.0f);
			DrawDebugDirectionalArrow(World, OutLedgeLoc, OutLedgeLoc + (OutWallNormal * 50.0f), 20.0f, FColor::Green, false, 3.0f, 0, 2.0f);
		}
		UE_LOG(LogThreadTalesLedgeDetector, Log, TEXT("[Climbing] FindOpposingLedge: SUCCESS! Found opposing ledge at %s, normal=%s (dist=%.1f)"), *OutLedgeLoc.ToString(), *OutWallNormal.ToString(), FVector::Dist(ActorLoc, OutLedgeLoc));
		return true;
	}

	if (bDrawDebugTraces)
	{
		UE_LOG(LogThreadTalesLedgeDetector, Verbose, TEXT("[Climbing] FindOpposingLedge: No opposing ledge found in sector behind %s"), *Character->GetName());
	}
	return false;
}

bool FThreadTalesLedgeDetector::ProbeInnerCorner(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FVector& CurrentLedgeLoc,
	const FVector& WallNormal2D,
	const FVector& SideDir,
	float WallFaceZ,
	float ProbeDistance,
	float MaxAngleDeg,
	FVector& OutCornerPivotLoc,
	FVector& OutLedgeLoc,
	FVector& OutWallNormal)
{
	UWorld* World = Character->GetWorld();
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	QueryParams.bTraceComplex = false;
	const FVector Up = FVector::UpVector;

	const FVector InnerTraceStart = FVector(
		CurrentLedgeLoc.X + WallNormal2D.X * 10.0f,
		CurrentLedgeLoc.Y + WallNormal2D.Y * 10.0f,
		WallFaceZ
	);
	const FVector InnerTraceEnd = InnerTraceStart + SideDir * (ProbeDistance + 35.0f);

	if (bDrawDebugTraces)
	{
		DrawDebugLine(World, InnerTraceStart, InnerTraceEnd, FColor::Cyan, false, 2.0f, 0, 1.5f);
	}

	FHitResult InnerHit;
	if (!World->LineTraceSingleByChannel(InnerHit, InnerTraceStart, InnerTraceEnd, ECC_Visibility, QueryParams) || !InnerHit.bBlockingHit)
	{
		return false;
	}

	const FVector HitNormal2D = FVector(InnerHit.ImpactNormal.X, InnerHit.ImpactNormal.Y, 0.0f).GetSafeNormal();
	const float OpposeDot = FVector::DotProduct(HitNormal2D, -SideDir);
	if (OpposeDot < 0.4f) { return false; }

	const float AngleBetweenNormals = FMath::RadiansToDegrees(FMath::Acos(
		FMath::Clamp(FVector::DotProduct(WallNormal2D, HitNormal2D), -1.0f, 1.0f)));
	const float CornerAngle = 180.0f - AngleBetweenNormals;
	if (CornerAngle > MaxAngleDeg) { return false; }

	const FVector PivotPoint = FVector(
		InnerHit.ImpactPoint.X - WallNormal2D.X * 10.0f,
		InnerHit.ImpactPoint.Y - WallNormal2D.Y * 10.0f,
		CurrentLedgeLoc.Z
	);

	const TArray<float> InnerStepDistances = { 50.0f, 45.0f, 40.0f, 35.0f };
	for (float StepDist : InnerStepDistances)
	{
		const FVector InnerLedgeCenter = PivotPoint + WallNormal2D * StepDist;
		const FVector TopTraceStart = InnerLedgeCenter - HitNormal2D * 10.0f + Up * 30.0f;
		const FVector TopTraceEnd   = TopTraceStart - Up * 60.0f;

		FHitResult TopHit;
		if (World->LineTraceSingleByChannel(TopHit, TopTraceStart, TopTraceEnd, ECC_Visibility, QueryParams) && TopHit.bBlockingHit && TopHit.ImpactNormal.Z >= 0.6f)
		{
			const FVector WallTraceStart = InnerLedgeCenter + HitNormal2D * 30.0f - Up * 10.0f;
			const FVector WallTraceEnd   = InnerLedgeCenter - HitNormal2D * 10.0f - Up * 10.0f;

			FHitResult WallHit;
			if (World->LineTraceSingleByChannel(WallHit, WallTraceStart, WallTraceEnd, ECC_Visibility, QueryParams) && WallHit.bBlockingHit)
			{
				FThreadTalesLedgeAnalysisResult AnalysisResult;
				if (PinpointAndValidateLedge(Character, Config, bDrawDebugTraces, WallHit.ImpactPoint, TopHit.ImpactPoint.Z, HitNormal2D, AnalysisResult))
				{
					const float WallDot = FVector::DotProduct(AnalysisResult.RefinedWallNormal, WallNormal2D);
					if (FMath::Abs(WallDot) > 0.45f || FVector::Dist(AnalysisResult.ExactLedgeEdge, CurrentLedgeLoc) < 20.0f)
					{
						continue;
					}

					OutCornerPivotLoc = PivotPoint;
					OutLedgeLoc = AnalysisResult.ExactLedgeEdge;
					OutWallNormal = AnalysisResult.RefinedWallNormal;

					if (bDrawDebugTraces)
					{
						DrawDebugSphere(World, OutCornerPivotLoc, 8.0f, 8, FColor::Cyan, false, 3.0f);
						DrawDebugSphere(World, OutLedgeLoc, 12.0f, 12, FColor::Green, false, 4.0f);
						DrawDebugDirectionalArrow(World, OutLedgeLoc, OutLedgeLoc + OutWallNormal * 40.0f, 15.0f, FColor::Green, false, 4.0f);
					}
					UE_LOG(LogThreadTalesLedgeDetector, Log, TEXT("[Climbing] ProbeCornerLedge: SUCCESS! Inner corner at %s -> new ledge %s (normal=%s)"), *OutCornerPivotLoc.ToString(), *OutLedgeLoc.ToString(), *OutWallNormal.ToString());
					return true;
				}
			}
		}
	}

	return false;
}

bool FThreadTalesLedgeDetector::ProbeOuterCorner(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FVector& CurrentLedgeLoc,
	const FVector& WallNormal2D,
	const FVector& SideDir,
	float WallFaceZ,
	float ProbeDistance,
	float MaxAngleDeg,
	FVector& OutCornerPivotLoc,
	FVector& OutLedgeLoc,
	FVector& OutWallNormal)
{
	UWorld* World = Character->GetWorld();
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	QueryParams.bTraceComplex = false;
	const FVector Up = FVector::UpVector;

	const TArray<float> TestDepths = { 10.0f, 20.0f, 35.0f, 50.0f };
	for (float Depth : TestDepths)
	{
		const FVector SideFaceProbeStart = FVector(
			CurrentLedgeLoc.X + SideDir.X * (ProbeDistance + 15.0f) - WallNormal2D.X * Depth,
			CurrentLedgeLoc.Y + SideDir.Y * (ProbeDistance + 15.0f) - WallNormal2D.Y * Depth,
			WallFaceZ
		);
		const FVector SideFaceProbeEnd = FVector(
			CurrentLedgeLoc.X - WallNormal2D.X * Depth,
			CurrentLedgeLoc.Y - WallNormal2D.Y * Depth,
			WallFaceZ
		);

		if (bDrawDebugTraces)
		{
			DrawDebugLine(World, SideFaceProbeStart, SideFaceProbeEnd, FColor::Orange, false, 2.0f, 0, 1.2f);
		}

		FHitResult SideWallHit;
		if (World->LineTraceSingleByChannel(SideWallHit, SideFaceProbeStart, SideFaceProbeEnd, ECC_Visibility, QueryParams) && SideWallHit.bBlockingHit)
		{
			const FVector SideWallNormal2D = FVector(SideWallHit.ImpactNormal.X, SideWallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();
			const float SideDot = FVector::DotProduct(SideWallNormal2D, SideDir);
			if (SideDot >= 0.4f)
			{
				const float AngleBetweenNormals = FMath::RadiansToDegrees(FMath::Acos(
					FMath::Clamp(FVector::DotProduct(WallNormal2D, SideWallNormal2D), -1.0f, 1.0f)));

				if (AngleBetweenNormals <= MaxAngleDeg)
				{
					const FVector PivotPoint = FVector(
						SideWallHit.ImpactPoint.X + WallNormal2D.X * Depth,
						SideWallHit.ImpactPoint.Y + WallNormal2D.Y * Depth,
						CurrentLedgeLoc.Z
					);

					const TArray<float> DesiredStepDistances = { 50.0f, 40.0f, 30.0f, 20.0f };
					for (float StepDist : DesiredStepDistances)
					{
						const FVector SideLedgeCenter = PivotPoint - WallNormal2D * StepDist;
						const FVector TopTraceStart = SideLedgeCenter - SideWallNormal2D * 10.0f + Up * 30.0f;
						const FVector TopTraceEnd   = TopTraceStart - Up * 60.0f;

						FHitResult TopHit;
						if (World->LineTraceSingleByChannel(TopHit, TopTraceStart, TopTraceEnd, ECC_Visibility, QueryParams) && TopHit.bBlockingHit && TopHit.ImpactNormal.Z >= 0.6f)
						{
							const FVector WallTraceStart = SideLedgeCenter + SideWallNormal2D * 30.0f - Up * 10.0f;
							const FVector WallTraceEnd   = SideLedgeCenter - SideWallNormal2D * 10.0f - Up * 10.0f;

							FHitResult WallHit;
							if (World->LineTraceSingleByChannel(WallHit, WallTraceStart, WallTraceEnd, ECC_Visibility, QueryParams) && WallHit.bBlockingHit)
							{
								FThreadTalesLedgeAnalysisResult AnalysisResult;
								if (PinpointAndValidateLedge(Character, Config, bDrawDebugTraces, WallHit.ImpactPoint, TopHit.ImpactPoint.Z, SideWallNormal2D, AnalysisResult))
								{
									const float WallDot = FVector::DotProduct(AnalysisResult.RefinedWallNormal, WallNormal2D);
									if (FMath::Abs(WallDot) > 0.45f || FVector::Dist(AnalysisResult.ExactLedgeEdge, CurrentLedgeLoc) < 20.0f)
									{
										continue;
									}

									OutCornerPivotLoc = PivotPoint;
									OutLedgeLoc = AnalysisResult.ExactLedgeEdge;
									OutWallNormal = AnalysisResult.RefinedWallNormal;

									if (bDrawDebugTraces)
									{
										DrawDebugSphere(World, OutCornerPivotLoc, 8.0f, 8, FColor::Orange, false, 3.0f);
										DrawDebugSphere(World, OutLedgeLoc, 12.0f, 12, FColor::Green, false, 4.0f);
										DrawDebugDirectionalArrow(World, OutLedgeLoc, OutLedgeLoc + OutWallNormal * 40.0f, 15.0f, FColor::Green, false, 4.0f);
									}
									UE_LOG(LogThreadTalesLedgeDetector, Log, TEXT("[Climbing] ProbeCornerLedge: SUCCESS! Outer corner at %s -> new ledge %s (normal=%s)"), *OutCornerPivotLoc.ToString(), *OutLedgeLoc.ToString(), *OutWallNormal.ToString());
									return true;
								}
							}
						}
					}
				}
			}
		}
	}

	return false;
}

bool FThreadTalesLedgeDetector::ProbeCornerLedge(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FVector& CurrentLedgeLoc,
	const FVector& CurrentWallNormal,
	float DirectionSign,
	EThreadTalesCornerType& OutCornerType,
	FVector& OutCornerPivotLoc,
	FVector& OutLedgeLoc,
	FVector& OutWallNormal)
{
	OutCornerType = EThreadTalesCornerType::None;
	OutCornerPivotLoc = FVector::ZeroVector;

	if (!Character) { return false; }
	UWorld* World = Character->GetWorld();
	if (!World) { return false; }

	const float ProbeDistance    = Config ? Config->Sensing.Corner.CornerProbeDistance    : 80.0f;
	const float MaxAngleDeg      = Config ? Config->Sensing.Corner.CornerMaxAngleDegrees  : 120.0f;

	const FVector WallNormal2D = FVector(CurrentWallNormal.X, CurrentWallNormal.Y, 0.0f).GetSafeNormal();
	const FVector Up = FVector::UpVector;
	const float Sign = (DirectionSign < 0.0f) ? -1.0f : 1.0f;
	const FVector WallRight = FVector::CrossProduct(WallNormal2D, Up).GetSafeNormal();
	const FVector SideDir = WallRight * Sign;
	const float WallFaceZ = CurrentLedgeLoc.Z - 15.0f;

	if (ProbeInnerCorner(Character, Config, bDrawDebugTraces, CurrentLedgeLoc, WallNormal2D, SideDir, WallFaceZ, ProbeDistance, MaxAngleDeg, OutCornerPivotLoc, OutLedgeLoc, OutWallNormal))
	{
		OutCornerType = EThreadTalesCornerType::Inner;
		return true;
	}

	if (ProbeOuterCorner(Character, Config, bDrawDebugTraces, CurrentLedgeLoc, WallNormal2D, SideDir, WallFaceZ, ProbeDistance, MaxAngleDeg, OutCornerPivotLoc, OutLedgeLoc, OutWallNormal))
	{
		OutCornerType = EThreadTalesCornerType::Outer;
		return true;
	}

	return false;
}

void FThreadTalesLedgeDetector::ClassifyCandidateDirectionAndTier(
	const FVector& Displacement,
	const FVector& WallNormal2D,
	const FVector& WallRight,
	const FThreadTalesJumpConfigTable& JumpTable,
	EThreadTalesLedgeJumpDirection& OutDirection,
	EThreadTalesLedgeJumpDistanceType& OutDistanceType)
{
	const float Dist = Displacement.Size();

	// 1. Distance Tier classification based on calibrated thresholds
	if (Dist <= 60.0f) { OutDistanceType = EThreadTalesLedgeJumpDistanceType::Short; }
	else if (Dist <= 115.0f) { OutDistanceType = EThreadTalesLedgeJumpDistanceType::Long; }
	else { OutDistanceType = EThreadTalesLedgeJumpDistanceType::Leap; }

	// 2. Project displacement onto wall plane: Lateral (X) along WallRight, Vertical (Y) along UpVector
	const float ProjX = FVector::DotProduct(Displacement, WallRight);
	const float ProjY = Displacement.Z;

	// Angle in degrees in [-180, 180]
	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Atan2(ProjY, ProjX));

	// 8-directional classification with 45? sectors
	if (AngleDegrees >= -22.5f && AngleDegrees <= 22.5f)
	{
		OutDirection = EThreadTalesLedgeJumpDirection::Right;
	}
	else if (AngleDegrees > 22.5f && AngleDegrees <= 67.5f)
	{
		OutDirection = EThreadTalesLedgeJumpDirection::UpRight;
	}
	else if (AngleDegrees > 67.5f && AngleDegrees <= 112.5f)
	{
		OutDirection = EThreadTalesLedgeJumpDirection::Up;
	}
	else if (AngleDegrees > 112.5f && AngleDegrees <= 157.5f)
	{
		OutDirection = EThreadTalesLedgeJumpDirection::UpLeft;
	}
	else if (AngleDegrees > 157.5f || AngleDegrees < -157.5f)
	{
		OutDirection = EThreadTalesLedgeJumpDirection::Left;
	}
	else if (AngleDegrees >= -157.5f && AngleDegrees < -112.5f)
	{
		OutDirection = EThreadTalesLedgeJumpDirection::DownLeft;
	}
	else if (AngleDegrees >= -112.5f && AngleDegrees < -67.5f)
	{
		OutDirection = EThreadTalesLedgeJumpDirection::Down;
	}
	else // AngleDegrees >= -67.5f && AngleDegrees < -22.5f
	{
		OutDirection = EThreadTalesLedgeJumpDirection::DownRight;
	}
}

void FThreadTalesLedgeDetector::EvaluateWallHitCandidate(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FHitResult& WallHit,
	const FVector& CurrentLedgeLoc,
	const FVector& CentralDir,
	const FVector& WallRight,
	float IntentVertical,
	float IntentLateral,
	float MaxDist,
	float AngleWeight,
	float DistWeight,
	const TArray<float>& ProbeDepths,
	const FThreadTalesJumpConfigTable& JumpTable,
	TArray<FThreadTalesLedgeCandidate>& InOutCandidates)
{
	if (!Character || !WallHit.bBlockingHit || FMath::Abs(WallHit.ImpactNormal.Z) > 0.45f)
	{
		return;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return;
	}

	const FVector FoundWallNorm2D = FVector(WallHit.ImpactNormal.X, WallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();
	if (FoundWallNorm2D.IsNearlyZero())
	{
		return;
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Character);
	Params.bTraceComplex = false;

	FHitResult BestTopHit;
	bool bFoundValidTop = false;

	for (float Depth : ProbeDepths)
	{
		const float MaxTopZ = (IntentVertical < -0.2f) ? (CurrentLedgeLoc.Z - 10.0f) : (WallHit.ImpactPoint.Z + 55.0f);
		const float DownStartZ = FMath::Min(WallHit.ImpactPoint.Z + 55.0f, MaxTopZ);
		const float DownEndZ = WallHit.ImpactPoint.Z - 40.0f;

		if (DownStartZ <= DownEndZ + 5.0f)
		{
			continue;
		}

		const FVector DownStart = FVector(WallHit.ImpactPoint.X - (FoundWallNorm2D.X * Depth), WallHit.ImpactPoint.Y - (FoundWallNorm2D.Y * Depth), DownStartZ);
		const FVector DownEnd   = FVector(WallHit.ImpactPoint.X - (FoundWallNorm2D.X * Depth), WallHit.ImpactPoint.Y - (FoundWallNorm2D.Y * Depth), DownEndZ);

		FHitResult CurrentTopHit;
		if (World->LineTraceSingleByChannel(CurrentTopHit, DownStart, DownEnd, ECC_Visibility, Params))
		{
			if (CurrentTopHit.ImpactNormal.Z >= 0.55f)
			{
				BestTopHit = CurrentTopHit;
				bFoundValidTop = true;
				break;
			}
		}
	}

	if (!bFoundValidTop)
	{
		return;
	}

	const float LedgeTopZ = BestTopHit.ImpactPoint.Z;
	const FVector PinStart = FVector(BestTopHit.ImpactPoint.X, BestTopHit.ImpactPoint.Y, LedgeTopZ - 6.0f) + (FoundWallNorm2D * 40.0f);
	const FVector PinEnd = PinStart - (FoundWallNorm2D * 80.0f);

	FHitResult EdgeWallHit;
	if (!World->LineTraceSingleByChannel(EdgeWallHit, PinStart, PinEnd, ECC_Visibility, Params))
	{
		return;
	}

	const FVector CandidateLoc = FVector(EdgeWallHit.ImpactPoint.X, EdgeWallHit.ImpactPoint.Y, LedgeTopZ);
	const FVector CandidateNormal = FVector(EdgeWallHit.ImpactNormal.X, EdgeWallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();

	const float Dist = FVector::Dist(CandidateLoc, CurrentLedgeLoc);
	if (Dist < 20.0f || Dist > MaxDist)
	{
		return;
	}

	if (IntentVertical < -0.3f && CandidateLoc.Z > CurrentLedgeLoc.Z - 15.0f)
	{
		return;
	}
	if (IntentVertical > 0.3f && CandidateLoc.Z < CurrentLedgeLoc.Z + 15.0f)
	{
		return;
	}

	const float CandidateLateral = FVector::DotProduct(CandidateLoc - CurrentLedgeLoc, WallRight);
	if (IntentLateral > 0.5f && CandidateLateral < 10.0f)
	{
		return;
	}
	if (IntentLateral < -0.5f && CandidateLateral > -10.0f)
	{
		return;
	}

	for (const FThreadTalesLedgeCandidate& Existing : InOutCandidates)
	{
		if (FVector::DistSquared(Existing.LedgeLocation, CandidateLoc) < (18.0f * 18.0f))
		{
			return;
		}
	}

	const FVector DirToCand = (CandidateLoc - CurrentLedgeLoc).GetSafeNormal();
	const float RawAlignmentDot = FVector::DotProduct(CentralDir, DirToCand);
	if (RawAlignmentDot < 0.25f)
	{
		return;
	}

	FThreadTalesLedgeCandidate Cand;
	Cand.LedgeLocation = CandidateLoc;
	Cand.WallNormal = CandidateNormal;
	Cand.Distance = Dist;

	ClassifyCandidateDirectionAndTier(CandidateLoc - CurrentLedgeLoc, CandidateNormal, WallRight, JumpTable, Cand.Direction, Cand.DistanceType);

	if (IntentVertical < -0.3f && (Cand.Direction == EThreadTalesLedgeJumpDirection::Up || Cand.Direction == EThreadTalesLedgeJumpDirection::UpLeft || Cand.Direction == EThreadTalesLedgeJumpDirection::UpRight || Cand.Direction == EThreadTalesLedgeJumpDirection::Left || Cand.Direction == EThreadTalesLedgeJumpDirection::Right)) { return; }
	if (IntentVertical > 0.3f && (Cand.Direction == EThreadTalesLedgeJumpDirection::Down || Cand.Direction == EThreadTalesLedgeJumpDirection::DownLeft || Cand.Direction == EThreadTalesLedgeJumpDirection::DownRight)) { return; }

	const float ProximityRatio = 1.0f - FMath::Clamp(Dist / MaxDist, 0.0f, 1.0f);
	const float AlignmentFactor = FMath::Square(FMath::Clamp(RawAlignmentDot, 0.0f, 1.0f));
	Cand.Score = AlignmentFactor * (AngleWeight + (ProximityRatio * DistWeight));

	InOutCandidates.Add(Cand);

	if (bDrawDebugTraces)
	{
		DrawDebugSphere(World, CandidateLoc, 7.0f, 8, FColor::Cyan, false, 1.5f, 0, 1.0f);
	}
}

void FThreadTalesLedgeDetector::ScanConeVolume(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FVector& CurrentLedgeLoc,
	const FVector& CurrentWallNormal,
	const FVector& CentralDir,
	const FThreadTalesJumpConfigTable& JumpTable,
	TArray<FThreadTalesLedgeCandidate>& OutCandidates)
{
	UWorld* World = Character->GetWorld();
	const FVector Up = FVector::UpVector;
	const FVector WallNorm2D = CurrentWallNormal.GetSafeNormal();
	const FVector WallRight = FVector::CrossProduct(WallNorm2D, Up).GetSafeNormal();

	const float HalfAngleH = Config ? Config->Sensing.RadialScanner.RadialScanConeAngleH : 45.0f;
	const float HalfAngleV = Config ? Config->Sensing.RadialScanner.RadialScanConeAngleV : 35.0f;
	const int32 StepsH = Config ? FMath::Max(Config->Sensing.RadialScanner.RadialScanRaysH, 3) : 7;
	const int32 StepsV = Config ? FMath::Max(Config->Sensing.RadialScanner.RadialScanRaysV, 1) : 5;
	const float MaxDist = Config ? Config->Sensing.RadialScanner.RadialScanMaxDistance : 220.0f;
	const float AngleWeight = Config ? Config->Sensing.RadialScanner.RadialScanAngleWeight : 0.65f;
	const float DistWeight = Config ? Config->Sensing.RadialScanner.RadialScanDistanceWeight : 0.35f;
	const TArray<float> ProbeDepths = (Config && Config->Sensing.Probing.DirectionalProbeDepths.Num() > 0) ? Config->Sensing.Probing.DirectionalProbeDepths : TArray<float>{ 2.5f, 5.0f, 10.0f, 18.0f };

	FVector ConeRight;
	FVector ConeUp;
	if (FMath::Abs(FVector::DotProduct(CentralDir, Up)) > 0.92f)
	{
		ConeRight = WallRight;
		ConeUp = FVector::CrossProduct(ConeRight, CentralDir).GetSafeNormal();
	}
	else
	{
		ConeRight = FVector::CrossProduct(CentralDir, Up).GetSafeNormal();
		ConeUp = FVector::CrossProduct(ConeRight, CentralDir).GetSafeNormal();
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Character);
	Params.bTraceComplex = false;
	const float WallSweepRadius = 14.0f;
	const float DistanceTiers[] = { 45.0f, 85.0f, 140.0f, 190.0f };

	for (int32 h = 0; h < StepsH; ++h)
	{
		const float FracH = (StepsH > 1) ? ((float)h / (float)(StepsH - 1) * 2.0f - 1.0f) : 0.0f;
		const float AngleH = FracH * HalfAngleH;

		for (int32 v = 0; v < StepsV; ++v)
		{
			const float FracV = (StepsV > 1) ? ((float)v / (float)(StepsV - 1) * 2.0f - 1.0f) : 0.0f;
			const float AngleV = FracV * HalfAngleV;

			const FQuat RotH = FQuat(ConeUp, FMath::DegreesToRadians(AngleH));
			const FQuat RotV = FQuat(ConeRight, FMath::DegreesToRadians(AngleV));
			const FVector RayDir = (RotH * RotV).RotateVector(CentralDir).GetSafeNormal();
			const float OutwardDot = FVector::DotProduct(RayDir, WallNorm2D);

			if (OutwardDot > 0.25f)
			{
				const FVector SweepStart = CurrentLedgeLoc + (WallNorm2D * 15.0f);
				const FVector SweepEnd = SweepStart + (RayDir * MaxDist);

				if (bDrawDebugTraces)
				{
					DrawDebugLine(World, SweepStart, SweepEnd, FColor(0, 180, 255, 60), false, 1.5f, 0, 0.7f);
				}

				FHitResult WallHit;
				if (World->SweepSingleByChannel(WallHit, SweepStart, SweepEnd, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(WallSweepRadius), Params))
				{
					EvaluateWallHitCandidate(Character, Config, bDrawDebugTraces, WallHit, CurrentLedgeLoc, CentralDir, WallRight, CentralDir.Z, FVector::DotProduct(CentralDir, WallRight), MaxDist, AngleWeight, DistWeight, ProbeDepths, JumpTable, OutCandidates);
				}
			}
			else
			{
				const FVector WallPlaneDir = FVector::VectorPlaneProject(RayDir, WallNorm2D).GetSafeNormal();
				if (WallPlaneDir.IsNearlyZero())
				{
					continue;
				}

				for (float TierDist : DistanceTiers)
				{
					if (TierDist > MaxDist)
					{
						break;
					}

					const FVector TargetPlanePoint = CurrentLedgeLoc + (WallPlaneDir * TierDist);
					const FVector SweepStart = TargetPlanePoint + (WallNorm2D * 45.0f);
					const FVector SweepEnd   = TargetPlanePoint - (WallNorm2D * 85.0f);

					if (bDrawDebugTraces)
					{
						DrawDebugLine(World, SweepStart, SweepEnd, FColor(0, 220, 255, 80), false, 1.2f, 0, 0.8f);
					}

					FHitResult WallHit;
					if (World->SweepSingleByChannel(WallHit, SweepStart, SweepEnd, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(WallSweepRadius), Params))
					{
						EvaluateWallHitCandidate(Character, Config, bDrawDebugTraces, WallHit, CurrentLedgeLoc, CentralDir, WallRight, CentralDir.Z, FVector::DotProduct(CentralDir, WallRight), MaxDist, AngleWeight, DistWeight, ProbeDepths, JumpTable, OutCandidates);
					}
				}
			}
		}
	}
}

bool FThreadTalesLedgeDetector::FindBestLedgeInCone(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FVector& CurrentLedgeLoc,
	const FVector& CurrentWallNormal,
	const FVector& IntentDirectionWorld,
	const FThreadTalesJumpConfigTable& JumpTable,
	FThreadTalesLedgeCandidate& OutBestCandidate)
{
	OutBestCandidate = FThreadTalesLedgeCandidate();

	if (!Character) { return false; }
	UWorld* World = Character->GetWorld();
	if (!World) { return false; }

	const FVector Up = FVector::UpVector;
	const FVector CentralDir = IntentDirectionWorld.IsNearlyZero() ? Up : IntentDirectionWorld.GetSafeNormal();

	TArray<FThreadTalesLedgeCandidate> UnvalidatedCandidates;
	ScanConeVolume(Character, Config, bDrawDebugTraces, CurrentLedgeLoc, CurrentWallNormal, CentralDir, JumpTable, UnvalidatedCandidates);

	if (UnvalidatedCandidates.Num() == 0)
	{
		return false;
	}

	UnvalidatedCandidates.Sort([](const FThreadTalesLedgeCandidate& A, const FThreadTalesLedgeCandidate& B)
	{
		return A.Score > B.Score;
	});

	const int32 MaxValidations = Config ? FMath::Max(Config->Sensing.RadialScanner.RadialScanMaxValidations, 1) : 3;
	const int32 CandidatesToTest = FMath::Min(UnvalidatedCandidates.Num(), MaxValidations);

	bool bFoundWinner = false;
	for (int32 i = 0; i < CandidatesToTest; ++i)
	{
		const FThreadTalesLedgeCandidate& Cand = UnvalidatedCandidates[i];

		FThreadTalesLedgeAnalysisResult AnalysisResult;
		if (!PinpointAndValidateLedge(Character, Config, bDrawDebugTraces, Cand.LedgeLocation, Cand.LedgeLocation.Z, Cand.WallNormal, AnalysisResult))
		{
			continue;
		}

		OutBestCandidate = Cand;
		OutBestCandidate.LedgeLocation = AnalysisResult.ExactLedgeEdge;
		OutBestCandidate.WallNormal = AnalysisResult.RefinedWallNormal;
		OutBestCandidate.bIsFreehang = AnalysisResult.bIsFreehang;
		bFoundWinner = true;
		break;
	}

	if (!bFoundWinner) { return false; }

	if (bDrawDebugTraces)
	{
		DrawDebugSphere(World, OutBestCandidate.LedgeLocation, 16.0f, 16, FColor::Orange, false, 3.5f, 0, 2.5f);
		DrawDebugDirectionalArrow(World, CurrentLedgeLoc, OutBestCandidate.LedgeLocation, 16.0f, FColor::Emerald, false, 3.5f, 0, 3.0f);
		DrawDebugString(World, OutBestCandidate.LedgeLocation + FVector(0, 0, 25), FString::Printf(TEXT("WINNER (Dist:%.0f, Sc:%.2f)"), OutBestCandidate.Distance, OutBestCandidate.Score), nullptr, FColor::Emerald, 3.5f, true);
	}

	UE_LOG(LogThreadTalesLedgeDetector, Log, TEXT("[RadialScanner] Found %d coarse candidates. WINNER: %s (Dist=%.1f, Score=%.2f, Dir=%d, Tier=%d)"),
		UnvalidatedCandidates.Num(), *OutBestCandidate.LedgeLocation.ToString(), OutBestCandidate.Distance, OutBestCandidate.Score, (int32)OutBestCandidate.Direction, (int32)OutBestCandidate.DistanceType);

	return true;
}
