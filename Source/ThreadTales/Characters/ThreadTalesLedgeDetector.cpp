// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesLedgeDetector.h"
#include "Characters/ThreadTalesClimbingConfig.h"
#include "Characters/ThreadTalesClimbingKinematics.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "CollisionQueryParams.h"

DEFINE_LOG_CATEGORY_STATIC(LogThreadTalesLedgeDetector, Log, All);

bool FThreadTalesLedgeDetector::TraceForwardWall(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	FHitResult& OutForwardHit,
	FVector& OutWallNormal2D)
{
	if (!Character)
	{
		return false;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector ActorLoc = Character->GetActorLocation();
	const FVector Forward = Character->GetActorForwardVector();
	const FVector Up = FVector::UpVector;

	const float FwdDist = Config ? Config->ForwardTraceDistance : 80.0f;
	const float FwdZOffset = Config ? Config->ForwardTraceZOffset : 0.0f;
	const float MinGrabH = Config ? Config->MinGrabHeight : 30.0f;
	const float MaxGrabH = Config ? Config->MaxGrabHeight : 140.0f;
	const float MaxApproachDeg = Config ? Config->MaxApproachAngle : 65.0f;
	const FName LedgeTag = Config ? Config->LedgeTag : NAME_None;
	const bool bRequireTag = Config ? Config->bRequireLedgeTag : false;

	const float TraceHeights[] = {
		FwdZOffset,
		FwdZOffset + 35.0f,
		FwdZOffset + 70.0f,
		FwdZOffset + 105.0f,
		FwdZOffset - 35.0f,
		FwdZOffset - 70.0f
	};

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	QueryParams.bTraceComplex = false;

	bool bFoundWall = false;
	FHitResult BestHit;
	float ClosestDistanceSq = FLT_MAX;

	for (float HeightOffset : TraceHeights)
	{
		const FVector Start = ActorLoc + Up * HeightOffset;
		const FVector End = Start + Forward * FwdDist;

		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams);

		if (bDrawDebugTraces)
		{
			DrawDebugLine(World, Start, End, (bHit && Hit.bBlockingHit) ? FColor::Green : FColor::Red, false, -1.0f, 0, 1.0f);
		}

		if (bHit && Hit.bBlockingHit)
		{
			if (bRequireTag && !LedgeTag.IsNone())
			{
				bool bHasTag = false;
				if (Hit.GetActor() && Hit.GetActor()->ActorHasTag(LedgeTag))
				{
					bHasTag = true;
				}
				else if (Hit.GetComponent() && Hit.GetComponent()->ComponentHasTag(LedgeTag))
				{
					bHasTag = true;
				}
				if (!bHasTag)
				{
					continue;
				}
			}

			// Reject ceilings, acute overhangs, and floors
			if (FMath::Abs(Hit.ImpactNormal.Z) > 0.45f)
			{
				continue;
			}

			const FVector WallNorm2D = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.0f).GetSafeNormal();
			if (WallNorm2D.IsNearlyZero())
			{
				continue;
			}

			const float ApproachDot = FVector::DotProduct(-Forward, WallNorm2D);
			const float MinApproachDot = FMath::Cos(FMath::DegreesToRadians(MaxApproachDeg));
			if (ApproachDot < MinApproachDot)
			{
				continue;
			}

			const float DistSq = FVector::DistSquared(Start, Hit.ImpactPoint);
			if (DistSq < ClosestDistanceSq)
			{
				ClosestDistanceSq = DistSq;
				BestHit = Hit;
				bFoundWall = true;
			}
		}
	}

	if (!bFoundWall)
	{
		return false;
	}

	OutForwardHit = BestHit;
	OutWallNormal2D = FVector(BestHit.ImpactNormal.X, BestHit.ImpactNormal.Y, 0.0f).GetSafeNormal();
	return true;
}

bool FThreadTalesLedgeDetector::ProbeLedgeTopSurface(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FHitResult& ForwardHit,
	const FVector& WallNormal2D,
	FHitResult& OutTopHit)
{
	if (!Character)
	{
		return false;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector ActorLoc = Character->GetActorLocation();
	const FVector Up = FVector::UpVector;
	const FVector WallNormal = WallNormal2D.GetSafeNormal();

	const float DownTraceH = Config ? Config->LedgeDownTraceHeight : 140.0f;
	const float DownTraceD = Config ? Config->LedgeDownTraceDepth : 160.0f;
	const float MinGrabH = Config ? Config->MinGrabHeight : 30.0f;
	const float MaxGrabH = Config ? Config->MaxGrabHeight : 140.0f;

	TArray<float> ProbeDepths = (Config && Config->LedgeProbeDepths.Num() > 0) ? Config->LedgeProbeDepths : TArray<float>{ 4.0f, 8.0f, 16.0f, 28.0f };

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	QueryParams.bTraceComplex = false;

	bool bFoundTop = false;
	FHitResult BestTopHit;
	float HighestZ = -FLT_MAX;

	for (float InwardDepth : ProbeDepths)
	{
		const FVector WallFaceXY = ForwardHit.ImpactPoint - WallNormal * InwardDepth;
		const FVector DownTraceStart = FVector(WallFaceXY.X, WallFaceXY.Y, ActorLoc.Z + DownTraceH);
		const FVector DownTraceEnd = FVector(WallFaceXY.X, WallFaceXY.Y, ActorLoc.Z - DownTraceD);

		FHitResult DownHit;
		const bool bHitDown = World->LineTraceSingleByChannel(DownHit, DownTraceStart, DownTraceEnd, ECC_Visibility, QueryParams);

		if (bDrawDebugTraces)
		{
			DrawDebugLine(World, DownTraceStart, DownTraceEnd, (bHitDown && DownHit.bBlockingHit) ? FColor::Yellow : FColor::Orange, false, -1.0f, 0, 1.0f);
		}

		if (bHitDown && DownHit.bBlockingHit)
		{
			if (DownHit.ImpactNormal.Z >= 0.55f)
			{
				const float RelativeHeight = DownHit.ImpactPoint.Z - ActorLoc.Z;
				if (RelativeHeight >= MinGrabH && RelativeHeight <= MaxGrabH)
				{
					if (DownHit.ImpactPoint.Z > HighestZ)
					{
						HighestZ = DownHit.ImpactPoint.Z;
						BestTopHit = DownHit;
						bFoundTop = true;
					}
				}
			}
		}
	}

	if (!bFoundTop)
	{
		return false;
	}

	OutTopHit = BestTopHit;
	return true;
}

bool FThreadTalesLedgeDetector::ValidateLedgeLipAndClearance(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FVector& EdgeLoc,
	const FVector& WallNormal2D)
{
	if (!Character)
	{
		return false;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	QueryParams.bTraceComplex = false;

	const FVector Up = FVector::UpVector;
	const FVector Normal2D = WallNormal2D.GetSafeNormal();

	const float LipBelowDepth = Config ? Config->LipBelowWallTraceDepth : 35.0f;
	const float LipAboveHeight = Config ? Config->LipAboveAirTraceHeight : 15.0f;
	const float SeamRejectDist = Config ? Config->LipModularSeamRejectDistance : 12.0f;
	const float BodyRadius = Config ? Config->BodyClearanceRadius : 32.0f;
	const float BodyHeight = Config ? Config->BodyClearanceHeight : 50.0f;
	const float HangWallOff = Config ? Config->HangWallOffset : 24.0f;
	const bool bAllowFreehang = Config ? Config->bAllowFreehang : true;

	// 1. Below Lip Check: Ensure solid vertical wall face exists right under the edge
	const FVector BelowLipStart = EdgeLoc + (Normal2D * 25.0f) - Up * LipBelowDepth;
	const FVector BelowLipEnd = EdgeLoc - (Normal2D * 20.0f) - Up * LipBelowDepth;

	FHitResult BelowHit;
	bool bBelowHit = World->LineTraceSingleByChannel(BelowHit, BelowLipStart, BelowLipEnd, ECC_Visibility, QueryParams);

	if (bDrawDebugTraces)
	{
		DrawDebugLine(World, BelowLipStart, BelowLipEnd, (bBelowHit && BelowHit.bBlockingHit) ? FColor::Green : FColor::Red, false, 2.0f, 0, 1.5f);
	}

	if (!bBelowHit || !BelowHit.bBlockingHit)
	{
		if (!bAllowFreehang)
		{
			if (bDrawDebugTraces)
			{
				DrawDebugString(World, EdgeLoc, TEXT("REJECT: NO WALL BELOW"), nullptr, FColor::Red, 2.0f, true);
			}
			return false;
		}
	}

	// 2. Above Lip Check (Flat Wall & Modular Seam Rejector)
	const FVector AboveLipStart = EdgeLoc + (Normal2D * 25.0f) + Up * LipAboveHeight;
	const FVector AboveLipEnd = EdgeLoc - (Normal2D * 8.0f) + Up * LipAboveHeight;

	FHitResult AboveHit;
	bool bAboveHit = World->LineTraceSingleByChannel(AboveHit, AboveLipStart, AboveLipEnd, ECC_Visibility, QueryParams);

	if (bDrawDebugTraces)
	{
		DrawDebugLine(World, AboveLipStart, AboveLipEnd, (bAboveHit && AboveHit.bBlockingHit) ? FColor::Red : FColor::Green, false, 2.0f, 0, 1.5f);
	}

	if (bAboveHit && AboveHit.bBlockingHit)
	{
		const float Dist2D = FVector::Dist2D(AboveHit.ImpactPoint, EdgeLoc);
		if (Dist2D < SeamRejectDist)
		{
			if (bDrawDebugTraces)
			{
				DrawDebugString(World, EdgeLoc + FVector(0, 0, 10), TEXT("REJECT: WALL ABOVE"), nullptr, FColor::Red, 2.0f, true);
			}
			return false;
		}
	}

	// 3. Body Clearance Check
	const FVector BodyStart = EdgeLoc + (Normal2D * HangWallOff) - Up * 5.0f;
	const FVector BodyEnd = BodyStart + Up * BodyHeight;

	FHitResult BodyHit;
	bool bBodyBlocked = World->SweepSingleByChannel(
		BodyHit,
		BodyStart,
		BodyEnd,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(BodyRadius),
		QueryParams
	);

	if (bDrawDebugTraces)
	{
		DrawDebugCapsule(World, BodyStart + Up * 20.0f, 20.0f, BodyRadius, FQuat::Identity,
			(bBodyBlocked && BodyHit.bBlockingHit) ? FColor::Red : FColor::Cyan, false, 2.0f, 0, 1.0f);
	}

	if (bBodyBlocked && BodyHit.bBlockingHit)
	{
		if (bDrawDebugTraces)
		{
			DrawDebugString(World, BodyStart, TEXT("REJECT: BODY BLOCKED"), nullptr, FColor::Red, 2.0f, true);
		}
		return false;
	}

	return true;
}

bool FThreadTalesLedgeDetector::ValidateLedgeHandsPlacement(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FVector& TrueLedgeEdge,
	const FVector& WallNormal2D)
{
	if (!Character)
	{
		return false;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	QueryParams.bTraceComplex = false;

	const float HandSpacing = Config ? Config->HandSpacing : 22.0f;
	const float HandsProbeDepth = Config ? Config->LedgeHandsProbeDepth : 12.0f;
	const float HandsVertRange = Config ? Config->LedgeHandsVerticalRange : 15.0f;
	const float SlopeMaxZDiff = Config ? Config->LedgeSlopeMaxZDifference : 15.0f;

	const FVector Up = FVector::UpVector;
	const FVector WallTangent = FVector::CrossProduct(WallNormal2D, Up).GetSafeNormal();
	const FVector LeftHandEdge = TrueLedgeEdge - WallTangent * HandSpacing;
	const FVector RightHandEdge = TrueLedgeEdge + WallTangent * HandSpacing;

	const FVector LeftProbe = LeftHandEdge - WallNormal2D * HandsProbeDepth;
	const FVector RightProbe = RightHandEdge - WallNormal2D * HandsProbeDepth;

	FHitResult LeftDownHit;
	FHitResult RightDownHit;
	const bool bLeftHit = World->LineTraceSingleByChannel(LeftDownHit, LeftProbe + Up * HandsVertRange, LeftProbe - Up * HandsVertRange, ECC_Visibility, QueryParams);
	const bool bRightHit = World->LineTraceSingleByChannel(RightDownHit, RightProbe + Up * HandsVertRange, RightProbe - Up * HandsVertRange, ECC_Visibility, QueryParams);

	if (bLeftHit && bRightHit && LeftDownHit.bBlockingHit && RightDownHit.bBlockingHit)
	{
		if (FMath::Abs(LeftDownHit.ImpactPoint.Z - RightDownHit.ImpactPoint.Z) > SlopeMaxZDiff)
		{
			return false;
		}
	}

	if (bDrawDebugTraces)
	{
		DrawDebugLine(World, LeftHandEdge, RightHandEdge, FColor::Yellow, false, -1.0f, 0, 3.0f);
	}

	return true;
}

bool FThreadTalesLedgeDetector::CheckIsFreehangLedge(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FVector& EdgeLoc,
	const FVector& WallNormal2D)
{
	if (!Character)
	{
		return false;
	}

	const bool bAllowFreehang = Config ? Config->bAllowFreehang : true;
	if (!bAllowFreehang)
	{
		return false;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	QueryParams.bTraceComplex = false;

	const float CheckDepth = Config ? Config->FreehangWallCheckDepth : 90.0f;
	const float TraceFwd = Config ? Config->FreehangWallTraceForward : 15.0f;
	const float TraceBack = Config ? Config->FreehangWallTraceBack : 25.0f;

	const FVector WallCheckStart = EdgeLoc - FVector(0.0f, 0.0f, CheckDepth) + (WallNormal2D * TraceFwd);
	const FVector WallCheckEnd = EdgeLoc - FVector(0.0f, 0.0f, CheckDepth) - (WallNormal2D * TraceBack);

	FHitResult WallHit;
	const bool bHit = World->LineTraceSingleByChannel(WallHit, WallCheckStart, WallCheckEnd, ECC_Visibility, QueryParams);

	if (bDrawDebugTraces)
	{
		DrawDebugLine(World, WallCheckStart, WallCheckEnd, (bHit && WallHit.bBlockingHit) ? FColor::Cyan : FColor::Magenta, false, 2.0f, 0, 1.5f);
	}

	return (!bHit || !WallHit.bBlockingHit);
}

bool FThreadTalesLedgeDetector::DetectLedge(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	FVector& OutLedgeLocation,
	FVector& OutWallNormal)
{
	if (!Character)
	{
		return false;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector ActorLoc = Character->GetActorLocation();
	const FVector Forward = Character->GetActorForwardVector();

	// 1. Multi-Level Forward Wall Detection
	FHitResult ForwardHit;
	FVector WallNormal2D;
	if (!TraceForwardWall(Character, Config, bDrawDebugTraces, ForwardHit, WallNormal2D))
	{
		return false;
	}

	// 2. Top Surface Downward Trace (Find Ledge Lip Height)
	FHitResult TopHit;
	if (!ProbeLedgeTopSurface(Character, Config, bDrawDebugTraces, ForwardHit, WallNormal2D, TopHit))
	{
		return false;
	}

	const float LedgeTopZ = TopHit.ImpactPoint.Z;
	const float RelativeHeight = LedgeTopZ - ActorLoc.Z;

	// 3. Pinpoint the Exact 3D Edge (Intersection of Wall & Top Surface)
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	QueryParams.bTraceComplex = false;

	const float FwdDist = Config ? Config->ForwardTraceDistance : 80.0f;
	const float FwdZOffset = Config ? Config->ForwardTraceZOffset : 0.0f;
	const float MaxGrabWallDist = Config ? Config->MaxGrabWallDistance : 75.0f;

	const float EdgeTraceZ = LedgeTopZ - 10.0f;
	const FVector EdgeTraceStart = FVector(ActorLoc.X, ActorLoc.Y, EdgeTraceZ);
	const FVector EdgeTraceEnd = EdgeTraceStart + Forward * (FwdDist + 40.0f);

	FHitResult EdgeWallHit;
	const bool bEdgeWallHit = World->LineTraceSingleByChannel(EdgeWallHit, EdgeTraceStart, EdgeTraceEnd, ECC_Visibility, QueryParams);

	FVector EdgePointXY;
	FVector FinalWallNormal;
	if (bEdgeWallHit && EdgeWallHit.bBlockingHit)
	{
		EdgePointXY = EdgeWallHit.ImpactPoint;
		FinalWallNormal = EdgeWallHit.ImpactNormal;
	}
	else
	{
		EdgePointXY = ForwardHit.ImpactPoint;
		FinalWallNormal = ForwardHit.ImpactNormal;
	}

	FVector FinalWallNormal2D = FVector(FinalWallNormal.X, FinalWallNormal.Y, 0.0f).GetSafeNormal();
	if (FinalWallNormal2D.IsNearlyZero())
	{
		FinalWallNormal2D = WallNormal2D;
	}

	const FVector TrueLedgeEdge = FVector(EdgePointXY.X, EdgePointXY.Y, LedgeTopZ);

	// 4. Wall Proximity Check (prevent teleportation)
	const FVector ForwardStart = ActorLoc + FVector::UpVector * FwdZOffset;
	const float WallForwardDist = FVector::DotProduct(ForwardHit.ImpactPoint - ForwardStart, Forward);
	if (WallForwardDist > MaxGrabWallDist)
	{
		return false;
	}

	// 5. True Ledge Lip & Body Clearance Geometry Check
	if (!ValidateLedgeLipAndClearance(Character, Config, bDrawDebugTraces, TrueLedgeEdge, FinalWallNormal2D))
	{
		return false;
	}

	// 6. Hand Clearance & Ledge Width Verification
	if (!ValidateLedgeHandsPlacement(Character, Config, bDrawDebugTraces, TrueLedgeEdge, FinalWallNormal2D))
	{
		return false;
	}

	if (bDrawDebugTraces)
	{
		DrawDebugSphere(World, TrueLedgeEdge, 6.0f, 12, FColor::Green, false, -1.0f, 0, 2.0f);
		DrawDebugString(World, TrueLedgeEdge + FVector(0, 0, 15), FString::Printf(TEXT("EDGE (Z:%.0f, Rel:%.0f)"), LedgeTopZ, RelativeHeight), nullptr, FColor::Yellow, -1.0f, true);
	}

	OutLedgeLocation = TrueLedgeEdge;
	OutWallNormal = FinalWallNormal2D;

	UE_LOG(LogThreadTalesLedgeDetector, Log, TEXT("[Traversal] LEDGE DETECTED at Exact Edge: %s, Normal: %s, RelHeight: %.1f"),
		*OutLedgeLocation.ToString(), *OutWallNormal.ToString(), RelativeHeight);

	return true;
}

bool FThreadTalesLedgeDetector::FindUpperLedge(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FVector& CurrentLedgeLoc,
	const FVector& CurrentWallNormal,
	FVector& OutLedgeLoc,
	FVector& OutWallNormal)
{
	if (!Character)
	{
		return false;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	QueryParams.bTraceComplex = false;

	const FVector Up = FVector::UpVector;
	const FVector Normal2D = CurrentWallNormal.GetSafeNormal();

	const float TestHeights[] = { 35.0f, 50.0f, 75.0f, 100.0f, 130.0f, 160.0f, 190.0f };

	for (float H : TestHeights)
	{
		const FVector WallTraceStart = CurrentLedgeLoc + (Normal2D * 40.0f) + (Up * H);
		const FVector WallTraceEnd = CurrentLedgeLoc - (Normal2D * 60.0f) + (Up * H);

		FHitResult WallHit;
		if (World->LineTraceSingleByChannel(WallHit, WallTraceStart, WallTraceEnd, ECC_Visibility, QueryParams))
		{
			const FVector FoundWallNormal2D = FVector(WallHit.ImpactNormal.X, WallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();

			const FVector TopTraceStart = WallHit.ImpactPoint - (FoundWallNormal2D * 12.0f) + Up * 30.0f;
			const FVector TopTraceEnd = WallHit.ImpactPoint - (FoundWallNormal2D * 12.0f) - Up * 20.0f;

			FHitResult TopHit;
			if (World->LineTraceSingleByChannel(TopHit, TopTraceStart, TopTraceEnd, ECC_Visibility, QueryParams))
			{
				if (TopHit.ImpactNormal.Z >= 0.55f)
				{
					const float EdgeZ = TopHit.ImpactPoint.Z;
					const FVector CandidateEdge = FVector(WallHit.ImpactPoint.X, WallHit.ImpactPoint.Y, EdgeZ);

					if (ValidateLedgeLipAndClearance(Character, Config, bDrawDebugTraces, CandidateEdge, FoundWallNormal2D))
					{
						OutLedgeLoc = CandidateEdge;
						OutWallNormal = FoundWallNormal2D;
						return true;
					}
				}
			}
		}
	}

	return false;
}

bool FThreadTalesLedgeDetector::FindSideLedge(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FVector& CurrentLedgeLoc,
	const FVector& CurrentWallNormal,
	float DirectionSign,
	bool bForJump,
	FVector& OutLedgeLoc,
	FVector& OutWallNormal,
	bool& bOutIsContinuous)
{
	bOutIsContinuous = false;
	if (!Character)
	{
		return false;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	QueryParams.bTraceComplex = false;

	const FVector Up = FVector::UpVector;
	const FVector Normal2D = CurrentWallNormal.GetSafeNormal();
	const FVector WallTangent = FVector::CrossProduct(Normal2D, Up).GetSafeNormal() * FMath::Sign(DirectionSign);

	const float ShimmyStepDist = Config ? Config->ShimmyStepDistance : 67.0f;
	const float ShimmyVertRange = Config ? Config->ShimmyVerticalTraceRange : 20.0f;
	const float ShimmyMaxZDiff = Config ? Config->ShimmyMaxZDifference : 20.0f;
	const TArray<float> ShimmyProbeDepths = (Config && Config->ShimmyProbeDepths.Num() > 0) ? Config->ShimmyProbeDepths : TArray<float>{ 8.0f, 15.0f, 25.0f };

	// Phase 1: Test for Continuous Ledge (Shimmy)
	if (!bForJump)
	{
		const float StepDistances[] = { ShimmyStepDist, ShimmyStepDist * 0.5f };
		for (float StepDist : StepDistances)
		{
			const FVector LateralTarget = CurrentLedgeLoc + (WallTangent * StepDist);

			for (float Depth : ShimmyProbeDepths)
			{
				const FVector TopTraceStart = LateralTarget - (Normal2D * Depth) + Up * ShimmyVertRange;
				const FVector TopTraceEnd = LateralTarget - (Normal2D * Depth) - Up * ShimmyVertRange;

				FHitResult TopHit;
				if (World->LineTraceSingleByChannel(TopHit, TopTraceStart, TopTraceEnd, ECC_Visibility, QueryParams))
				{
					if (TopHit.ImpactNormal.Z >= 0.55f && FMath::Abs(TopHit.ImpactPoint.Z - CurrentLedgeLoc.Z) <= ShimmyMaxZDiff)
					{
						const float LedgeTopZ = TopHit.ImpactPoint.Z;
						const FVector PinStart = FVector(TopHit.ImpactPoint.X, TopHit.ImpactPoint.Y, LedgeTopZ - 6.0f) + (Normal2D * 30.0f);
						const FVector PinEnd = PinStart - (Normal2D * 60.0f);

						FHitResult WallHit;
						if (World->LineTraceSingleByChannel(WallHit, PinStart, PinEnd, ECC_Visibility, QueryParams))
						{
							const FVector CandidateEdge = FVector(WallHit.ImpactPoint.X, WallHit.ImpactPoint.Y, LedgeTopZ);
							const FVector FoundWallNormal2D = FVector(WallHit.ImpactNormal.X, WallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();

							if (ValidateLedgeLipAndClearance(Character, Config, bDrawDebugTraces, CandidateEdge, FoundWallNormal2D))
							{
								OutLedgeLoc = CandidateEdge;
								OutWallNormal = FoundWallNormal2D;
								bOutIsContinuous = true;
								return true;
							}
						}
					}
				}
			}
		}
	}

	// Phase 2: Probe for Jump across gap
	const float SideMinDist = Config ? Config->MinSideJumpDistance : 25.0f;
	const float SideMaxDist = Config ? Config->MaxSideJumpDistance : 60.0f;
	const float JumpDistances[] = { 35.0f, 50.0f, 65.0f, 85.0f, 110.0f, 140.0f };

	for (float Dist : JumpDistances)
	{
		const FVector WallTraceStart = CurrentLedgeLoc + (WallTangent * Dist) + (Normal2D * 40.0f);
		const FVector WallTraceEnd = CurrentLedgeLoc + (WallTangent * Dist) - (Normal2D * 60.0f);

		FHitResult WallHit;
		if (World->LineTraceSingleByChannel(WallHit, WallTraceStart, WallTraceEnd, ECC_Visibility, QueryParams))
		{
			const FVector FoundWallNormal2D = FVector(WallHit.ImpactNormal.X, WallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();

			const FVector TopTraceStart = WallHit.ImpactPoint - (FoundWallNormal2D * 12.0f) + Up * 25.0f;
			const FVector TopTraceEnd = WallHit.ImpactPoint - (FoundWallNormal2D * 12.0f) - Up * 25.0f;

			FHitResult TopHit;
			if (World->LineTraceSingleByChannel(TopHit, TopTraceStart, TopTraceEnd, ECC_Visibility, QueryParams))
			{
				if (TopHit.ImpactNormal.Z >= 0.55f)
				{
					const float EdgeZ = TopHit.ImpactPoint.Z;
					const FVector CandidateEdge = FVector(WallHit.ImpactPoint.X, WallHit.ImpactPoint.Y, EdgeZ);

					const float ActualDist = FVector::Dist(CandidateEdge, CurrentLedgeLoc);
					if (ActualDist >= SideMinDist && ActualDist <= SideMaxDist)
					{
						if (ValidateLedgeLipAndClearance(Character, Config, bDrawDebugTraces, CandidateEdge, FoundWallNormal2D))
						{
							OutLedgeLoc = CandidateEdge;
							OutWallNormal = FoundWallNormal2D;
							bOutIsContinuous = false;
							return true;
						}
					}
				}
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
	if (!Character || Direction == EThreadTalesLedgeJumpDirection::None)
	{
		return false;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Character);
	Params.bTraceComplex = false;

	const FVector Up = FVector::UpVector;
	const FVector WallNorm2D = CurrentWallNormal.GetSafeNormal();
	const FVector WallRight = FVector::CrossProduct(WallNorm2D, Up).GetSafeNormal();

	const TArray<float> ProbeDepths = (Config && Config->DirectionalProbeDepths.Num() > 0) ? Config->DirectionalProbeDepths : TArray<float>{ 4.0f, 8.0f, 16.0f };
	const float UpJumpTopOffset = Config ? Config->UpJumpTopTraceOffset : 35.0f;
	const float DownJumpTopOffset = Config ? Config->DownJumpTopTraceOffset : 15.0f;
	const float DownJumpLeapTopOffset = Config ? Config->DownJumpLeapTopTraceOffset : 18.0f;
	const float LateralJumpTopOffset = Config ? Config->LateralJumpTopTraceOffset : 25.0f;
	const float DiagUpTopOffset = Config ? Config->DiagonalUpJumpTopTraceOffset : 25.0f;
	const float DiagDownTopOffset = Config ? Config->DiagonalDownJumpTopTraceOffset : 15.0f;

	auto ProbeAndValidateLedge = [&](const FVector& RayStart, const FVector& RayEnd, float TopTraceOffsetZ, float MinDist, float MaxDist) -> bool
	{
		FHitResult WallHit;
		if (!World->LineTraceSingleByChannel(WallHit, RayStart, RayEnd, ECC_Visibility, Params))
		{
			return false;
		}

		if (FMath::Abs(WallHit.ImpactNormal.Z) > 0.45f)
		{
			return false;
		}

		FHitResult BestTopHit;
		bool bFoundValidTop = false;

		for (float Depth : ProbeDepths)
		{
			const FVector DownStart = WallHit.ImpactPoint - (WallNorm2D * Depth) + FVector(0.0f, 0.0f, TopTraceOffsetZ);
			const FVector DownEnd = WallHit.ImpactPoint - (WallNorm2D * Depth) - FVector(0.0f, 0.0f, TopTraceOffsetZ * 0.7f);

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
			return false;
		}

		const float LedgeTopZ = BestTopHit.ImpactPoint.Z;
		const FVector PinStart = FVector(BestTopHit.ImpactPoint.X, BestTopHit.ImpactPoint.Y, LedgeTopZ - 6.0f) + (WallNorm2D * 40.0f);
		const FVector PinEnd = PinStart - (WallNorm2D * 80.0f);

		FHitResult EdgeWallHit;
		if (!World->LineTraceSingleByChannel(EdgeWallHit, PinStart, PinEnd, ECC_Visibility, Params))
		{
			return false;
		}

		const FVector CandidateLoc = FVector(EdgeWallHit.ImpactPoint.X, EdgeWallHit.ImpactPoint.Y, LedgeTopZ);
		const FVector CandidateNormal = FVector(EdgeWallHit.ImpactNormal.X, EdgeWallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();

		const float Dist = FVector::Dist(CandidateLoc, CurrentLedgeLoc);
		if (Dist < MinDist || Dist > MaxDist)
		{
			return false;
		}

		if (!ValidateLedgeLipAndClearance(Character, Config, bDrawDebugTraces, CandidateLoc, CandidateNormal))
		{
			return false;
		}

		OutLedgeLoc = CandidateLoc;
		OutWallNormal = CandidateNormal;

		if (bDrawDebugTraces)
		{
			DrawDebugSphere(World, OutLedgeLoc, 12.0f, 16, FColor::Orange, false, 3.0f, 0, 2.0f);
			DrawDebugDirectionalArrow(World, OutLedgeLoc, OutLedgeLoc + OutWallNormal * 35.0f, 12.0f, FColor::Yellow, false, 3.0f, 0, 2.0f);
		}

		return true;
	};

	// 1. UP
	if (Direction == EThreadTalesLedgeJumpDirection::Up)
	{
		float MinH = 0.0f;
		float MaxH = 0.0f;
		if (!JumpTable.GetDistanceBounds(Direction, DistanceType, MinH, MaxH))
		{
			return false;
		}

		TArray<float> Heights;
		switch (DistanceType)
		{
		case EThreadTalesLedgeJumpDistanceType::Short:
			Heights = { 30.0f, 42.0f, 55.0f };
			break;
		case EThreadTalesLedgeJumpDistanceType::Long:
			Heights = { 55.0f, 75.0f, 95.0f, 110.0f };
			break;
		case EThreadTalesLedgeJumpDistanceType::Leap:
			Heights = { 105.0f, 130.0f, 160.0f, 190.0f };
			break;
		}

		for (float H : Heights)
		{
			const FVector Start = CurrentLedgeLoc + (CurrentWallNormal * 50.0f) + Up * H;
			const FVector End = Start - (CurrentWallNormal * 110.0f);
			if (ProbeAndValidateLedge(Start, End, UpJumpTopOffset, MinH, MaxH))
			{
				return true;
			}
		}
		return false;
	}

	// 2. DOWN
	if (Direction == EThreadTalesLedgeJumpDirection::Down)
	{
		float MinH = 0.0f;
		float MaxH = 0.0f;
		if (!JumpTable.GetDistanceBounds(Direction, DistanceType, MinH, MaxH))
		{
			return false;
		}

		TArray<float> Heights;
		switch (DistanceType)
		{
		case EThreadTalesLedgeJumpDistanceType::Short:
			Heights = { -25.0f, -38.0f, -50.0f };
			break;
		case EThreadTalesLedgeJumpDistanceType::Long:
			Heights = { -45.0f, -60.0f, -75.0f, -95.0f, -115.0f };
			break;
		case EThreadTalesLedgeJumpDistanceType::Leap:
			Heights = { -85.0f, -110.0f, -140.0f, -170.0f, -200.0f };
			break;
		}

		const float LateralOffsets[] = { 0.0f, -18.0f, 18.0f };
		const float DownTopOffset = (DistanceType == EThreadTalesLedgeJumpDistanceType::Leap) ? DownJumpLeapTopOffset : DownJumpTopOffset;

		for (float H : Heights)
		{
			for (float Lat : LateralOffsets)
			{
				const FVector Start = CurrentLedgeLoc + (WallRight * Lat) + (CurrentWallNormal * 50.0f) + Up * H;
				const FVector End = Start - (CurrentWallNormal * 110.0f);
				if (ProbeAndValidateLedge(Start, End, DownTopOffset, MinH, MaxH))
				{
					return true;
				}
			}
		}
		return false;
	}

	// 3. LEFT & RIGHT
	if (Direction == EThreadTalesLedgeJumpDirection::Right || Direction == EThreadTalesLedgeJumpDirection::Left)
	{
		const float SideSign = (Direction == EThreadTalesLedgeJumpDirection::Right) ? 1.0f : -1.0f;
		const FVector SideDir = WallRight * SideSign;

		float MinD = 0.0f;
		float MaxD = 0.0f;
		if (!JumpTable.GetDistanceBounds(Direction, DistanceType, MinD, MaxD))
		{
			return false;
		}

		TArray<float> Distances;
		switch (DistanceType)
		{
		case EThreadTalesLedgeJumpDistanceType::Short:
			Distances = { 30.0f, 42.0f, 55.0f };
			break;
		case EThreadTalesLedgeJumpDistanceType::Long:
			Distances = { 55.0f, 75.0f, 95.0f, 110.0f };
			break;
		case EThreadTalesLedgeJumpDistanceType::Leap:
			Distances = { 105.0f, 135.0f, 165.0f, 190.0f };
			break;
		}

		const float VertOffsets[] = { 0.0f, 18.0f, -18.0f };

		for (float Dist : Distances)
		{
			for (float Vert : VertOffsets)
			{
				const FVector Start = CurrentLedgeLoc + (SideDir * Dist) + (Up * Vert) + (CurrentWallNormal * 50.0f);
				const FVector End = Start - (CurrentWallNormal * 110.0f);
				if (ProbeAndValidateLedge(Start, End, LateralJumpTopOffset, MinD, MaxD))
				{
					return true;
				}
			}
		}
		return false;
	}

	// 4. DIAGONALS
	if (Direction == EThreadTalesLedgeJumpDirection::UpRight ||
		Direction == EThreadTalesLedgeJumpDirection::UpLeft ||
		Direction == EThreadTalesLedgeJumpDirection::DownRight ||
		Direction == EThreadTalesLedgeJumpDirection::DownLeft)
	{
		const float SideSign = (Direction == EThreadTalesLedgeJumpDirection::UpRight || Direction == EThreadTalesLedgeJumpDirection::DownRight) ? 1.0f : -1.0f;
		const bool bDiagUp = (Direction == EThreadTalesLedgeJumpDirection::UpRight || Direction == EThreadTalesLedgeJumpDirection::UpLeft);
		const FVector SideDir = WallRight * SideSign;

		float MinD = 0.0f;
		float MaxD = 0.0f;
		if (!JumpTable.GetDistanceBounds(Direction, DistanceType, MinD, MaxD))
		{
			return false;
		}

		TArray<float> SideDists;
		TArray<float> VertOffsets;
		switch (DistanceType)
		{
		case EThreadTalesLedgeJumpDistanceType::Short:
			SideDists = { 24.0f, 34.0f, 44.0f };
			VertOffsets = bDiagUp ? TArray<float>{ 24.0f, 34.0f, 44.0f } : TArray<float>{ -24.0f, -34.0f, -44.0f };
			break;
		case EThreadTalesLedgeJumpDistanceType::Long:
			SideDists = { 50.0f, 70.0f, 85.0f };
			VertOffsets = bDiagUp ? TArray<float>{ 50.0f, 70.0f, 85.0f } : TArray<float>{ -50.0f, -70.0f, -85.0f };
			break;
		case EThreadTalesLedgeJumpDistanceType::Leap:
			SideDists = { 80.0f, 110.0f, 140.0f };
			VertOffsets = bDiagUp ? TArray<float>{ 80.0f, 110.0f, 140.0f } : TArray<float>{ -80.0f, -110.0f, -140.0f };
			break;
		}

		for (float SD : SideDists)
		{
			for (float VO : VertOffsets)
			{
				const FVector Start = CurrentLedgeLoc + (SideDir * SD) + (Up * VO) + (CurrentWallNormal * 50.0f);
				const FVector End = Start - (CurrentWallNormal * 110.0f);
				const float DiagTopOffset = bDiagUp ? DiagUpTopOffset : DiagDownTopOffset;
				if (ProbeAndValidateLedge(Start, End, DiagTopOffset, MinD, MaxD))
				{
					return true;
				}
			}
		}
		return false;
	}

	return false;
}

bool FThreadTalesLedgeDetector::TraceClimbDownFloorCliff(
	const ACharacter* Character,
	bool bDrawDebugTraces,
	const FVector& ProbeDir,
	float Dist,
	FHitResult& OutFloorHit)
{
	if (!Character)
	{
		return false;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	QueryParams.bTraceComplex = false;

	const FVector ActorLoc = Character->GetActorLocation();
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.0f;
	const float FeetZ = ActorLoc.Z - HalfHeight;

	const FVector FloorCheckStart = ActorLoc + (ProbeDir * Dist) + FVector(0.0f, 0.0f, 15.0f);
	const FVector FloorCheckEnd = FloorCheckStart - FVector(0.0f, 0.0f, 140.0f);

	const bool bHitFloor = World->LineTraceSingleByChannel(OutFloorHit, FloorCheckStart, FloorCheckEnd, ECC_Visibility, QueryParams);

	if (bDrawDebugTraces)
	{
		DrawDebugLine(World, FloorCheckStart, FloorCheckEnd, (!bHitFloor || OutFloorHit.ImpactPoint.Z < FeetZ - 40.0f) ? FColor::Green : FColor::Red, false, 2.5f, 0, 1.5f);
	}

	return (!bHitFloor || (OutFloorHit.bBlockingHit && OutFloorHit.ImpactPoint.Z < FeetZ - 40.0f));
}

bool FThreadTalesLedgeDetector::PinpointClimbDownLedgeLip(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FHitResult& WallHit,
	const FVector& WallNormal2D,
	float FeetZ,
	FVector& OutEdgeLoc)
{
	if (!Character)
	{
		return false;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	QueryParams.bTraceComplex = false;

	const float FloorTraceFwd = Config ? Config->ClimbDownFloorTraceForwardDist : 15.0f;
	const float FloorTraceZRange = Config ? Config->ClimbDownFloorTraceZRange : 25.0f;
	const float EdgeLipProbeRange = Config ? Config->ClimbDownEdgeLipProbeRange : 30.0f;
	const float EdgeLipDepth = Config ? Config->ClimbDownEdgeLipDepth : 10.0f;

	// 1. Pinpoint exact top surface height of the ledge lip inside the wall face
	const FVector TopProbeStart = FVector(
		WallHit.ImpactPoint.X - (WallNormal2D.X * FloorTraceFwd),
		WallHit.ImpactPoint.Y - (WallNormal2D.Y * FloorTraceFwd),
		FeetZ + FloorTraceZRange
	);
	const FVector TopProbeEnd = FVector(TopProbeStart.X, TopProbeStart.Y, FeetZ - FloorTraceZRange);

	FHitResult TopHit;
	const bool bHitTop = World->LineTraceSingleByChannel(TopHit, TopProbeStart, TopProbeEnd, ECC_Visibility, QueryParams);

	if (bDrawDebugTraces)
	{
		DrawDebugLine(World, TopProbeStart, TopProbeEnd, (bHitTop && TopHit.bBlockingHit) ? FColor::Cyan : FColor::Orange, false, 2.5f, 0, 1.5f);
	}

	if (!bHitTop || !TopHit.bBlockingHit || TopHit.ImpactNormal.Z < 0.6f)
	{
		return false;
	}

	const float LedgeTopZ = TopHit.ImpactPoint.Z;

	// 2. Pinpoint exact edge XY below the top surface
	const FVector EdgeProbeStart = FVector(
		WallHit.ImpactPoint.X + (WallNormal2D.X * EdgeLipProbeRange),
		WallHit.ImpactPoint.Y + (WallNormal2D.Y * EdgeLipProbeRange),
		LedgeTopZ - EdgeLipDepth
	);
	const FVector EdgeProbeEnd = FVector(
		WallHit.ImpactPoint.X - (WallNormal2D.X * EdgeLipProbeRange),
		WallHit.ImpactPoint.Y - (WallNormal2D.Y * EdgeLipProbeRange),
		LedgeTopZ - EdgeLipDepth
	);

	FHitResult EdgeLipHit;
	FVector EdgeXY = WallHit.ImpactPoint;
	if (World->LineTraceSingleByChannel(EdgeLipHit, EdgeProbeStart, EdgeProbeEnd, ECC_Visibility, QueryParams) && EdgeLipHit.bBlockingHit)
	{
		EdgeXY = EdgeLipHit.ImpactPoint;
	}

	if (bDrawDebugTraces)
	{
		DrawDebugLine(World, EdgeProbeStart, EdgeProbeEnd, FColor::Yellow, false, 2.5f, 0, 1.5f);
	}

	OutEdgeLoc = FVector(EdgeXY.X, EdgeXY.Y, LedgeTopZ);
	return true;
}

bool FThreadTalesLedgeDetector::FindEdgeForClimbDown(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	FVector& OutLedgeLoc,
	FVector& OutWallNormal,
	bool& bOutIsFreehang)
{
	if (!Character)
	{
		return false;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return false;
	}

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.0f;
	const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 35.0f;

	const FVector ActorLoc = Character->GetActorLocation();
	const float FeetZ = ActorLoc.Z - HalfHeight;
	const FVector Forward = Character->GetActorForwardVector();
	const FVector Up = FVector::UpVector;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	QueryParams.bTraceComplex = false;

	const float TraceDistance = Config ? Config->ClimbDownTraceDistance : 70.0f;
	const float MinApproachDot = Config ? Config->ClimbDownMinApproachDot : 0.25f;

	TArray<FVector> ProbeDirections;
	// Priority 1: Forward and near-forward angles
	ProbeDirections.Add(Forward);
	ProbeDirections.Add(Forward.RotateAngleAxis(25.0f, Up));
	ProbeDirections.Add(Forward.RotateAngleAxis(-25.0f, Up));
	ProbeDirections.Add(Forward.RotateAngleAxis(50.0f, Up));
	ProbeDirections.Add(Forward.RotateAngleAxis(-50.0f, Up));
	// Priority 2: Backward
	ProbeDirections.Add(-Forward);
	ProbeDirections.Add((-Forward).RotateAngleAxis(25.0f, Up));
	ProbeDirections.Add((-Forward).RotateAngleAxis(-25.0f, Up));
	// Priority 3: Flanks
	ProbeDirections.Add(Forward.RotateAngleAxis(75.0f, Up));
	ProbeDirections.Add(Forward.RotateAngleAxis(-75.0f, Up));

	const float DropDistances[] = { Radius + 15.0f, Radius + 35.0f, TraceDistance };

	for (const FVector& ProbeDir : ProbeDirections)
	{
		bool bFoundDrop = false;
		float DropDistUsed = 0.0f;

		for (float Dist : DropDistances)
		{
			FHitResult FloorHit;
			if (TraceClimbDownFloorCliff(Character, bDrawDebugTraces, ProbeDir, Dist, FloorHit))
			{
				bFoundDrop = true;
				DropDistUsed = Dist;
				break;
			}
		}

		if (!bFoundDrop)
		{
			continue;
		}

		const FVector WallTraceStart = ActorLoc + (ProbeDir * (DropDistUsed + 30.0f)) - FVector(0.0f, 0.0f, HalfHeight + 15.0f);
		const FVector WallTraceEnd = ActorLoc - FVector(0.0f, 0.0f, HalfHeight + 15.0f);

		FHitResult WallHit;
		const bool bHitWall = World->LineTraceSingleByChannel(WallHit, WallTraceStart, WallTraceEnd, ECC_Visibility, QueryParams);

		if (bDrawDebugTraces)
		{
			DrawDebugLine(World, WallTraceStart, WallTraceEnd, (bHitWall && WallHit.bBlockingHit) ? FColor::Green : FColor::Red, false, 2.5f, 0, 1.5f);
		}

		if (!bHitWall || !WallHit.bBlockingHit)
		{
			continue;
		}

		if (FMath::Abs(WallHit.ImpactNormal.Z) > 0.35f)
		{
			continue;
		}

		const FVector WallNormal2D = FVector(WallHit.ImpactNormal.X, WallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();
		if (WallNormal2D.IsNearlyZero())
		{
			continue;
		}

		if (FVector::DotProduct(WallNormal2D, ProbeDir) < MinApproachDot)
		{
			continue;
		}

		FVector TrueEdge;
		if (!PinpointClimbDownLedgeLip(Character, Config, bDrawDebugTraces, WallHit, WallNormal2D, FeetZ, TrueEdge))
		{
			continue;
		}

		bOutIsFreehang = CheckIsFreehangLedge(Character, Config, bDrawDebugTraces, TrueEdge, WallNormal2D);

		if (ValidateLedgeLipAndClearance(Character, Config, bDrawDebugTraces, TrueEdge, WallNormal2D))
		{
			OutLedgeLoc = TrueEdge;
			OutWallNormal = WallNormal2D;

			if (bDrawDebugTraces)
			{
				DrawDebugSphere(World, TrueEdge, 10.0f, 12, FColor::Orange, false, 3.5f);
				DrawDebugString(World, TrueEdge + FVector(0.0f, 0.0f, 20.0f), TEXT("CLIMB DOWN EDGE"), nullptr, FColor::Orange, 3.5f);
				DrawDebugDirectionalArrow(World, TrueEdge, TrueEdge + WallNormal2D * 40.0f, 15.0f, FColor::Orange, false, 3.5f);
			}

			return true;
		}
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
	if (!Character)
	{
		return false;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	QueryParams.bTraceComplex = false;

	const float WallProbeDepth = Config ? Config->MantleWallContinuityProbeDepth : 15.0f;
	const float StandForwardOff = Config ? Config->MantleStandForwardOffset : 40.0f;
	const float StandUpOff = Config ? Config->MantleStandUpOffset : 5.0f;
	const float StandCapsuleHalfH = Config ? Config->MantleStandCapsuleHalfHeight : 82.0f;
	const float StandCapsuleR = Config ? Config->MantleStandCapsuleRadius : 31.0f;

	const FVector WallNormal2D = CurrentWallNormal.GetSafeNormal2D();
	const FVector InwardDir = -WallNormal2D;
	const FVector Up = FVector::UpVector;

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
		if (bDrawDebugTraces)
		{
			DrawDebugCapsule(World, TargetStandingLoc, HalfHeight * 0.95f, Radius * 0.9f, FQuat::Identity, FColor::Red, false, 2.0f);
			DrawDebugString(World, TargetStandingLoc, TEXT("CLEARANCE BLOCKED"), nullptr, FColor::Red, 2.0f, true);
		}
		return false;
	}

	if (bDrawDebugTraces)
	{
		DrawDebugCapsule(World, TargetStandingLoc, HalfHeight * 0.95f, Radius * 0.9f, FQuat::Identity, FColor::Green, false, 2.0f);
		DrawDebugString(World, TargetStandingLoc, TEXT("CAN CLIMB UP"), nullptr, FColor::Green, 2.0f, true);
	}

	OutStandingLoc = TargetStandingLoc;
	return true;
}
