// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesLedgeDetector.h"
#include "Characters/ThreadTalesClimbingConfig.h"
#include "Characters/ThreadTalesClimbingKinematics.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
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
		FwdZOffset - 20.0f,
		FwdZOffset,
		FwdZOffset + 15.0f,
		FwdZOffset + 25.0f
	};

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	QueryParams.bTraceComplex = false;

	const float WallSweepRadius = 12.0f;
	const FCollisionShape SweepSphere = FCollisionShape::MakeSphere(WallSweepRadius);

	// Multi-angle directions: Forward, plus velocity-aligned and small lateral fan
	TArray<FVector> TraceDirs;
	TraceDirs.Add(Forward);

	if (const UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
	{
		FVector Vel2D = MoveComp->Velocity;
		Vel2D.Z = 0.0f;
		if (Vel2D.SizeSquared() > FMath::Square(80.0f))
		{
			const FVector MoveDir = Vel2D.GetSafeNormal();
			if (FVector::DotProduct(MoveDir, Forward) > 0.4f && FVector::DotProduct(MoveDir, Forward) < 0.98f)
			{
				TraceDirs.Add(MoveDir);
			}
		}
	}
	TraceDirs.Add(Forward.RotateAngleAxis(18.0f, Up));
	TraceDirs.Add(Forward.RotateAngleAxis(-18.0f, Up));

	bool bFoundWall = false;
	FHitResult BestHit;

	for (const FVector& Dir : TraceDirs)
	{
		for (float HeightOffset : TraceHeights)
		{
			const FVector Start = ActorLoc + Up * HeightOffset;
			const FVector End = Start + Dir * FwdDist;

			FHitResult Hit;
			const bool bHit = World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, SweepSphere, QueryParams);

			if (bDrawDebugTraces)
			{
				DrawDebugLine(World, Start, End, (bHit && Hit.bBlockingHit) ? FColor::Green : FColor::Red, false, -1.0f, 0, 1.0f);
				if (bHit && Hit.bBlockingHit)
				{
					DrawDebugSphere(World, Hit.ImpactPoint, 6.0f, 8, FColor::Cyan, false, -1.0f, 0, 1.0f);
				}
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

				const float ApproachDot = FVector::DotProduct(-Dir, WallNorm2D);
				const float MinApproachDot = FMath::Cos(FMath::DegreesToRadians(MaxApproachDeg));
				if (ApproachDot < MinApproachDot)
				{
					continue;
				}

				// Prioritize higher wall hits (closest to the top ledge lip)
				if (!bFoundWall || Hit.ImpactPoint.Z > BestHit.ImpactPoint.Z)
				{
					BestHit = Hit;
					bFoundWall = true;
				}
			}
		}

		if (bFoundWall)
		{
			break;
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

	TArray<float> ProbeDepths = (Config && Config->LedgeProbeDepths.Num() > 0) ? Config->LedgeProbeDepths : TArray<float>{ 2.5f, 5.0f, 10.0f, 18.0f };

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	QueryParams.bTraceComplex = false;

	bool bFoundTop = false;
	FHitResult BestTopHit;
	float HighestZ = -FLT_MAX;

	for (float InwardDepth : ProbeDepths)
	{
		const FVector WallFaceXY = ForwardHit.ImpactPoint - WallNormal * InwardDepth;
		const float BaseZ = ForwardHit.ImpactPoint.Z;
		const FVector DownTraceStart = FVector(WallFaceXY.X, WallFaceXY.Y, BaseZ + DownTraceH);
		const FVector DownTraceEnd = FVector(WallFaceXY.X, WallFaceXY.Y, BaseZ - DownTraceD);

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

	// 2. Above Lip Check (Flat Wall & Modular Seam Rejector vs Real Wall Protrusions)
	const FVector AboveLipStart = EdgeLoc + (Normal2D * 25.0f) + Up * LipAboveHeight;
	const FVector AboveLipEnd = EdgeLoc - (Normal2D * 20.0f) + Up * LipAboveHeight;

	FHitResult AboveHit;
	bool bAboveHit = World->LineTraceSingleByChannel(AboveHit, AboveLipStart, AboveLipEnd, ECC_Visibility, QueryParams);

	if (bAboveHit && AboveHit.bBlockingHit)
	{
		// Physical horizontal shelf depth: how far the wall above is recessed behind the front edge of the ledge
		const float ShelfDepth = FVector::DotProduct(EdgeLoc - AboveHit.ImpactPoint, Normal2D);
		const bool bIsWallBlocking = (ShelfDepth < SeamRejectDist);

		if (bDrawDebugTraces)
		{
			DrawDebugLine(World, AboveLipStart, AboveHit.ImpactPoint, bIsWallBlocking ? FColor::Red : FColor::Green, false, 2.0f, 0, 1.5f);
		}

		if (bIsWallBlocking)
		{
			if (bDrawDebugTraces)
			{
				DrawDebugString(World, EdgeLoc + FVector(0, 0, 10), TEXT("REJECT: WALL ABOVE / SEAM"), nullptr, FColor::Red, 2.0f, true);
			}
			return false;
		}
	}
	else if (bDrawDebugTraces)
	{
		DrawDebugLine(World, AboveLipStart, AboveLipEnd, FColor::Green, false, 2.0f, 0, 1.5f);
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

	const FVector Up = FVector::UpVector;
	const FVector SafeWallNorm2D = WallNormal2D.GetSafeNormal();
	const FVector WallRight = FVector::CrossProduct(SafeWallNorm2D, Up).GetSafeNormal();

	// Probe distances:
	// TraceForward: 12 cm in front of the ledge (where player's toes hang)
	// TraceBack: 5 cm inward behind the ledge (strictly limited to 3..6 cm so it doesn't punch through beams or detect recessed walls)
	const float TraceFwd = Config ? FMath::Clamp(Config->FreehangWallTraceForward, 8.0f, 15.0f) : 12.0f;
	const float TraceBack = Config ? FMath::Clamp(Config->FreehangWallTraceBack, 3.0f, 6.0f) : 5.0f;

	// Lateral offsets: center, left side (-20 cm), right side (+20 cm)
	const float LateralOffsets[] = { 0.0f, -20.0f, 20.0f };
	const float SweepRadius = 4.0f;

	auto CheckWallAtDepth = [&](float DepthZ) -> bool
	{
		for (float LatOffset : LateralOffsets)
		{
			const FVector ProbeCenter = EdgeLoc + (WallRight * LatOffset) - FVector(0.0f, 0.0f, DepthZ);
			const FVector SweepStart = ProbeCenter + (SafeWallNorm2D * TraceFwd);
			const FVector SweepEnd = ProbeCenter - (SafeWallNorm2D * TraceBack);

			FHitResult Hit;
			const bool bHit = World->SweepSingleByChannel(
				Hit,
				SweepStart,
				SweepEnd,
				FQuat::Identity,
				ECC_Visibility,
				FCollisionShape::MakeSphere(SweepRadius),
				QueryParams
			);

			if (bHit && Hit.bBlockingHit)
			{
				// Check that the hit surface is vertical enough to be a wall (not a horizontal floor or ceiling)
				if (FMath::Abs(Hit.ImpactNormal.Z) < 0.65f)
				{
					// Check that the surface faces outward in the general direction of the ledge normal
					const float OpposeDot = FVector::DotProduct(Hit.ImpactNormal, SafeWallNorm2D);
					if (OpposeDot > 0.35f)
					{
						if (bDrawDebugTraces)
						{
							DrawDebugSphere(World, Hit.ImpactPoint, SweepRadius, 8, FColor::Cyan, false, 2.0f, 0, 1.0f);
							DrawDebugLine(World, SweepStart, Hit.ImpactPoint, FColor::Cyan, false, 2.0f, 0, 1.2f);
						}
						return true;
					}
				}
			}

			if (bDrawDebugTraces)
			{
				DrawDebugLine(World, SweepStart, SweepEnd, FColor(255, 0, 255, 40), false, 1.5f, 0, 0.5f);
			}
		}
		return false;
	};

	// Foot-brace check (Zone where feet/toes push against the wall in MD_hang_idle1):
	// In the standard wall-hang pose, feet are braced against the wall around 105 to 128 cm below the hands.
	// If there is solid wall backing at foot level, the character can brace their feet -> Braced hang (false).
	// If there is NO wall at foot level (thin bar, pipe, or ~40-50 cm parapet where feet dangle in open air) -> Freehang (true).
	bool bFootSupport = false;
	const float FootDepths[] = { 105.0f, 118.0f, 128.0f };
	for (float FDepth : FootDepths)
	{
		if (CheckWallAtDepth(FDepth))
		{
			bFootSupport = true;
			break;
		}
	}

	return !bFootSupport;
}

bool FThreadTalesLedgeDetector::PinpointAndValidateLedge(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FVector& ProbeLocationOnWall,
	float LedgeTopZ,
	const FVector& ApproximateWallNormal2D,
	FThreadTalesLedgeAnalysisResult& OutResult)
{
	OutResult = FThreadTalesLedgeAnalysisResult();

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

	const FVector Normal2D = ApproximateWallNormal2D.GetSafeNormal();

	// 1. Pinpoint exact outer lip & refined wall normal below the top surface (8 cm down)
	const float EdgeLipDepth = 8.0f;
	const float EdgeLipOutward = 35.0f;
	const float EdgeLipInward = 50.0f;

	const FVector PinStart = FVector(ProbeLocationOnWall.X, ProbeLocationOnWall.Y, LedgeTopZ - EdgeLipDepth) + (Normal2D * EdgeLipOutward);
	const FVector PinEnd = PinStart - (Normal2D * (EdgeLipOutward + EdgeLipInward));

	FHitResult EdgeWallHit;
	const bool bHitWall = World->LineTraceSingleByChannel(EdgeWallHit, PinStart, PinEnd, ECC_Visibility, QueryParams);

	FVector EdgePointXY;
	FVector FinalWallNormal2D;

	if (bHitWall && EdgeWallHit.bBlockingHit && FMath::Abs(EdgeWallHit.ImpactNormal.Z) < 0.45f)
	{
		EdgePointXY = EdgeWallHit.ImpactPoint;
		FinalWallNormal2D = FVector(EdgeWallHit.ImpactNormal.X, EdgeWallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();
		if (FinalWallNormal2D.IsNearlyZero())
		{
			FinalWallNormal2D = Normal2D;
		}
	}
	else
	{
		EdgePointXY = ProbeLocationOnWall;
		FinalWallNormal2D = Normal2D;
	}

	const FVector TrueLedgeEdge = FVector(EdgePointXY.X, EdgePointXY.Y, LedgeTopZ);

	// 2. Physical lip & body clearance validation
	if (!ValidateLedgeLipAndClearance(Character, Config, bDrawDebugTraces, TrueLedgeEdge, FinalWallNormal2D))
	{
		return false;
	}

	// 3. Hand placement & slope validation
	if (!ValidateLedgeHandsPlacement(Character, Config, bDrawDebugTraces, TrueLedgeEdge, FinalWallNormal2D))
	{
		return false;
	}

	// 4. Freehang classification (solely foot-support based)
	const bool bIsFreehang = CheckIsFreehangLedge(Character, Config, bDrawDebugTraces, TrueLedgeEdge, FinalWallNormal2D);

	OutResult.bIsValid = true;
	OutResult.ExactLedgeEdge = TrueLedgeEdge;
	OutResult.RefinedWallNormal = FinalWallNormal2D;
	OutResult.bIsFreehang = bIsFreehang;

	if (bDrawDebugTraces)
	{
		DrawDebugSphere(World, TrueLedgeEdge, 6.0f, 12, bIsFreehang ? FColor::Magenta : FColor::Green, false, 2.0f, 0, 2.0f);
		DrawDebugString(World, TrueLedgeEdge + FVector(0, 0, 15),
			FString::Printf(TEXT("LEDGE [%s] (Z:%.0f)"), bIsFreehang ? TEXT("FREEHANG") : TEXT("BRACED"), LedgeTopZ),
			nullptr, bIsFreehang ? FColor::Magenta : FColor::Green, 2.0f, true);
	}

	return true;
}

bool FThreadTalesLedgeDetector::DetectLedge(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	FVector& OutLedgeLocation,
	FVector& OutWallNormal,
	bool* OutIsFreehang)
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

	// 3. Wall Proximity Check (prevent grabbing distant walls)
	const float MaxGrabWallDist = Config ? Config->MaxGrabWallDistance : 75.0f;
	const float WallDist2D = FVector::Dist2D(ActorLoc, ForwardHit.ImpactPoint);
	if (WallDist2D > MaxGrabWallDist)
	{
		return false;
	}

	// 4. Unified Pinpoint, Lip, Hands, and Freehang Validation
	FThreadTalesLedgeAnalysisResult AnalysisResult;
	if (!PinpointAndValidateLedge(Character, Config, bDrawDebugTraces, ForwardHit.ImpactPoint, LedgeTopZ, WallNormal2D, AnalysisResult))
	{
		return false;
	}

	OutLedgeLocation = AnalysisResult.ExactLedgeEdge;
	OutWallNormal = AnalysisResult.RefinedWallNormal;
	if (OutIsFreehang)
	{
		*OutIsFreehang = AnalysisResult.bIsFreehang;
	}

	UE_LOG(LogThreadTalesLedgeDetector, Log, TEXT("[Traversal] LEDGE DETECTED at Exact Edge: %s, Normal: %s, RelHeight: %.1f, Freehang: %s"),
		*OutLedgeLocation.ToString(), *OutWallNormal.ToString(), RelativeHeight, AnalysisResult.bIsFreehang ? TEXT("TRUE") : TEXT("FALSE"));

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
	FThreadTalesJumpConfigTable JumpTable;
	JumpTable.Build(Config, nullptr);
	return FindLedgeInDirection(Character, Config, bDrawDebugTraces, CurrentLedgeLoc, CurrentWallNormal, EThreadTalesLedgeJumpDirection::Up, EThreadTalesLedgeJumpDistanceType::Short, JumpTable, OutLedgeLoc, OutWallNormal);
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
	bool& bOutIsContinuous,
	bool* OutIsFreehang)
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

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Character);
	Params.bTraceComplex = false;

	const float Sign = (DirectionSign < 0.0f) ? -1.0f : 1.0f;
	const FVector WallRight = Character->GetActorRightVector();
	const FVector SideDir = WallRight * Sign;
	const FVector Up = FVector::UpVector;
	const FVector Normal2D = CurrentWallNormal.GetSafeNormal();

	const float ShimmyStepDist = Config ? Config->ShimmyStepDistance : 67.0f;
	const float ShimmyVertRange = Config ? Config->ShimmyVerticalTraceRange : 20.0f;
	const float ShimmyMaxZDiff = Config ? Config->ShimmyMaxZDifference : 20.0f;
	const TArray<float> ShimmyProbeDepths = (Config && Config->ShimmyProbeDepths.Num() > 0) ? Config->ShimmyProbeDepths : TArray<float>{ 4.0f, 10.0f };

	// Phase 1: Test for Continuous Ledge (Shimmy) at ShimmyStepDistance (with half-step fallback near ledge boundary)
	if (!bForJump)
	{
		const float StepDistances[] = { ShimmyStepDist, ShimmyStepDist * 0.5f };

		for (float StepDist : StepDistances)
		{
			for (float InwardDepth : ShimmyProbeDepths)
			{
				const FVector StepProbe = CurrentLedgeLoc + (SideDir * StepDist) - (Normal2D * InwardDepth);
				const FVector TopStart = StepProbe + FVector(0.0f, 0.0f, ShimmyVertRange);
				const FVector TopEnd = StepProbe - FVector(0.0f, 0.0f, ShimmyVertRange);

				FHitResult ShimmyTopHit;
				if (World->LineTraceSingleByChannel(ShimmyTopHit, TopStart, TopEnd, ECC_Visibility, Params))
				{
					if (ShimmyTopHit.bBlockingHit && !ShimmyTopHit.bStartPenetrating &&
						ShimmyTopHit.ImpactNormal.Z >= 0.6f &&
						FMath::Abs(ShimmyTopHit.ImpactPoint.Z - CurrentLedgeLoc.Z) <= ShimmyMaxZDiff)
					{
						FThreadTalesLedgeAnalysisResult AnalysisResult;
						if (PinpointAndValidateLedge(Character, Config, bDrawDebugTraces, ShimmyTopHit.ImpactPoint, ShimmyTopHit.ImpactPoint.Z, Normal2D, AnalysisResult))
						{
							bOutIsContinuous = true;
							OutLedgeLoc = AnalysisResult.ExactLedgeEdge;
							OutWallNormal = AnalysisResult.RefinedWallNormal;
							if (OutIsFreehang)
							{
								*OutIsFreehang = AnalysisResult.bIsFreehang;
							}

							if (bDrawDebugTraces)
							{
								DrawDebugSphere(World, OutLedgeLoc, 10.0f, 12, AnalysisResult.bIsFreehang ? FColor::Magenta : FColor::Cyan, false, 2.0f);
								DrawDebugString(World, OutLedgeLoc + FVector(0.0f, 0.0f, 15.0f),
									FString::Printf(TEXT("SHIMMY STEP [%s]"), AnalysisResult.bIsFreehang ? TEXT("FREEHANG") : TEXT("BRACED")),
									nullptr, AnalysisResult.bIsFreehang ? FColor::Magenta : FColor::Cyan, 2.0f);
							}
							return true;
						}
					}
				}
			}
		}
		return false;
	}

	// Phase 2: Jump across gap or along wall -> delegate to FindLedgeInDirection
	bOutIsContinuous = false;
	const EThreadTalesLedgeJumpDirection JumpDir = (DirectionSign < 0.0f) ? EThreadTalesLedgeJumpDirection::Left : EThreadTalesLedgeJumpDirection::Right;
	FThreadTalesJumpConfigTable JumpTable;
	JumpTable.Build(Config, nullptr);
	return FindLedgeInDirection(Character, Config, bDrawDebugTraces, CurrentLedgeLoc, CurrentWallNormal, JumpDir, EThreadTalesLedgeJumpDistanceType::Short, JumpTable, OutLedgeLoc, OutWallNormal);
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

	const TArray<float> ProbeDepths = (Config && Config->DirectionalProbeDepths.Num() > 0) ? Config->DirectionalProbeDepths : TArray<float>{ 2.5f, 5.0f, 10.0f, 18.0f };
	const float UpJumpTopOffset = Config ? Config->UpJumpTopTraceOffset : 55.0f;
	const float DownJumpTopOffset = Config ? Config->DownJumpTopTraceOffset : 22.0f;
	const float DownJumpLeapTopOffset = Config ? Config->DownJumpLeapTopTraceOffset : 28.0f;
	const float LateralJumpTopOffset = Config ? Config->LateralJumpTopTraceOffset : 28.0f;
	const float DiagUpTopOffset = Config ? Config->DiagonalUpJumpTopTraceOffset : 50.0f;
	const float DiagDownTopOffset = Config ? Config->DiagonalDownJumpTopTraceOffset : 25.0f;

	// Unified volumetric lambda to probe wall surfaces with Sphere Sweeps, test multiple depth offsets, and pinpoint outer lip
	auto ProbeAndValidateLedge = [&](const FVector& ProbeStart, const FVector& ProbeEnd, float TopTraceOffsetZ, float MinDist, float MaxDist) -> bool
	{
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
			DrawDebugSphere(World, ProbeEnd, WallSweepRadius, 8, FColor(0, 180, 255), false, 1.5f, 0, 0.5f);
		}

		if (!bHitWall || !WallHit.bBlockingHit)
		{
			return false;
		}

		if (FMath::Abs(WallHit.ImpactNormal.Z) > 0.45f)
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

		if (!bFoundValidTop)
		{
			return false;
		}

		const float LedgeTopZ = BestTopHit.ImpactPoint.Z;
		FThreadTalesLedgeAnalysisResult AnalysisResult;
		if (!PinpointAndValidateLedge(Character, Config, bDrawDebugTraces, WallHit.ImpactPoint, LedgeTopZ, FoundWallNorm2D, AnalysisResult))
		{
			return false;
		}

		const float Dist = FVector::Dist(AnalysisResult.ExactLedgeEdge, CurrentLedgeLoc);
		if (Dist < MinDist || Dist > MaxDist)
		{
			return false;
		}

		OutLedgeLoc = AnalysisResult.ExactLedgeEdge;
		OutWallNormal = AnalysisResult.RefinedWallNormal;

		if (bDrawDebugTraces)
		{
			DrawDebugSphere(World, OutLedgeLoc, 12.0f, 16, AnalysisResult.bIsFreehang ? FColor::Magenta : FColor::Orange, false, 3.0f, 0, 2.0f);
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
	FVector& OutEdgeLoc,
	FVector& OutRefinedWallNormal)
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

	OutRefinedWallNormal = WallNormal2D;

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
		if (FMath::Abs(EdgeLipHit.ImpactNormal.Z) < 0.35f)
		{
			const FVector RefinedNormal2D = FVector(EdgeLipHit.ImpactNormal.X, EdgeLipHit.ImpactNormal.Y, 0.0f).GetSafeNormal();
			if (!RefinedNormal2D.IsNearlyZero())
			{
				OutRefinedWallNormal = RefinedNormal2D;
			}
		}
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
		FVector RefinedWallNormal = WallNormal2D;
		if (!PinpointClimbDownLedgeLip(Character, Config, bDrawDebugTraces, WallHit, WallNormal2D, FeetZ, TrueEdge, RefinedWallNormal))
		{
			continue;
		}

		FThreadTalesLedgeAnalysisResult AnalysisResult;
		if (!PinpointAndValidateLedge(Character, Config, bDrawDebugTraces, TrueEdge, TrueEdge.Z, RefinedWallNormal, AnalysisResult))
		{
			continue;
		}

		bOutIsFreehang = AnalysisResult.bIsFreehang;
		OutLedgeLoc = AnalysisResult.ExactLedgeEdge;
		OutWallNormal = AnalysisResult.RefinedWallNormal;

		if (bDrawDebugTraces)
		{
			DrawDebugSphere(World, OutLedgeLoc, 10.0f, 12, bOutIsFreehang ? FColor::Magenta : FColor::Orange, false, 3.5f);
			DrawDebugString(World, OutLedgeLoc + FVector(0.0f, 0.0f, 20.0f), TEXT("CLIMB DOWN EDGE"), nullptr, FColor::Orange, 3.5f);
			DrawDebugDirectionalArrow(World, OutLedgeLoc, OutLedgeLoc + OutWallNormal * 40.0f, 15.0f, FColor::Orange, false, 3.5f);
		}

		return true;
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

bool FThreadTalesLedgeDetector::FindOpposingLedge(
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

	const FVector WallNormal2D = FVector(CurrentWallNormal.X, CurrentWallNormal.Y, 0.0f).GetSafeNormal();
	const FVector SectorCenterDir = WallNormal2D;
	const float MaxOpposingDist = Config ? Config->BackJumpOpposingLedgeDistance : 450.0f;
	const float SectorHalfAngle = Config ? Config->WallEjectSectorHalfAngle : 45.0f;
	const int32 RayCount = Config ? Config->WallEjectSectorRayCount : 7;
	const float MinOpposingHeight = Config ? Config->WallEjectMinOpposingHeight : -100.0f;
	const float MaxOpposingHeight = Config ? Config->WallEjectMaxOpposingHeight : 150.0f;
	const float NormalToleranceDot = Config ? Config->WallEjectNormalToleranceDot : -0.7f;

	const FVector ActorLoc = Character->GetActorLocation();

	bool bFoundOpposingLedge = false;
	FVector BestOpposingLedgeLoc = FVector::ZeroVector;
	FVector BestOpposingWallNormal = FVector::ZeroVector;
	float ClosestDistSq = FLT_MAX;

	const TArray<float> ZOffsets = {
		MinOpposingHeight,
		MinOpposingHeight * 0.5f,
		0.0f,
		MaxOpposingHeight * 0.5f,
		MaxOpposingHeight
	};

	for (int32 i = 0; i < RayCount; ++i)
	{
		const float Fraction = (RayCount > 1) ? (float)i / (float)(RayCount - 1) : 0.5f;
		const float Angle = FMath::Lerp(-SectorHalfAngle, SectorHalfAngle, Fraction);
		const FVector RayDir = SectorCenterDir.RotateAngleAxis(Angle, FVector::UpVector);

		for (float ZOffset : ZOffsets)
		{
			const FVector Start = ActorLoc + FVector(0.0f, 0.0f, ZOffset);
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
					// Hit a valid opposing wall facing the character.
					// Probe downward behind the wall face into the top surface:
					// HitNormal2D points OUT of the opposing wall towards the character.
					// Inward offset into the wall is -HitNormal2D * 15.0f.
					const FVector InwardOffset = -HitNormal2D * 15.0f;
					const FVector TopTraceStart = Hit.ImpactPoint + InwardOffset + FVector(0.0f, 0.0f, 150.0f);
					const FVector TopTraceEnd = Hit.ImpactPoint + InwardOffset - FVector(0.0f, 0.0f, 150.0f);

					FHitResult EdgeHit;
					const bool bEdgeHit = World->LineTraceSingleByChannel(EdgeHit, TopTraceStart, TopTraceEnd, ECC_Visibility, QueryParams);

					if (bDrawDebugTraces)
					{
						DrawDebugLine(World, TopTraceStart, TopTraceEnd, (bEdgeHit && EdgeHit.bBlockingHit) ? FColor::Cyan : FColor::Red, false, 2.0f, 0, 1.5f);
					}

					if (bEdgeHit && EdgeHit.bBlockingHit)
					{
						if (EdgeHit.ImpactNormal.Z >= 0.65f)
						{
							FThreadTalesLedgeAnalysisResult AnalysisResult;
							if (PinpointAndValidateLedge(Character, Config, bDrawDebugTraces, Hit.ImpactPoint, EdgeHit.ImpactPoint.Z, HitNormal2D, AnalysisResult))
							{
								const float DistSq = FVector::DistSquared(ActorLoc, AnalysisResult.ExactLedgeEdge);
								if (DistSq < ClosestDistSq)
								{
									ClosestDistSq = DistSq;
									BestOpposingLedgeLoc = AnalysisResult.ExactLedgeEdge;
									BestOpposingWallNormal = AnalysisResult.RefinedWallNormal;
									bFoundOpposingLedge = true;
								}
							}
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

		UE_LOG(LogThreadTalesLedgeDetector, Log, TEXT("[Climbing] FindOpposingLedge: SUCCESS! Found opposing ledge at %s, normal=%s (dist=%.1f)"),
			*OutLedgeLoc.ToString(), *OutWallNormal.ToString(), FMath::Sqrt(ClosestDistSq));
		return true;
	}

	if (bDrawDebugTraces)
	{
		UE_LOG(LogThreadTalesLedgeDetector, Verbose, TEXT("[Climbing] FindOpposingLedge: No opposing ledge found in sector behind %s"), *Character->GetName());
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

	if (!Character)
	{
		return false;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return false;
	}

	// ---- Config resolution ----
	const float ProbeDistance    = Config ? Config->CornerProbeDistance    : 80.0f;
	const float InwardProbeDepth = Config ? Config->CornerInwardProbeDepth : 30.0f;
	const float MaxAngleDeg      = Config ? Config->CornerMaxAngleDegrees  : 120.0f;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	QueryParams.bTraceComplex = false;

	// Flatten wall normal to 2D
	const FVector WallNormal2D = FVector(CurrentWallNormal.X, CurrentWallNormal.Y, 0.0f).GetSafeNormal();
	const FVector Up = FVector::UpVector;

	// Use ActorRightVector * Sign to match FindSideLedge orientation exactly!
	// +1 = right, -1 = left relative to the hanging character
	const float Sign = (DirectionSign < 0.0f) ? -1.0f : 1.0f;
	const FVector SideDir = Character->GetActorRightVector() * Sign;

	// Lower ray by 15cm below the ledge top so traces hit solid vertical wall faces,
	// rather than grazing or skimming over the top edge in empty air.
	const float WallFaceZ = CurrentLedgeLoc.Z - 15.0f;

	// ================================================================
	// 1. TEST FOR INNER CORNER (Concave): Obstacle / Wall directly in front along SideDir
	// ================================================================
	// Start trace 10cm in front of Wall 1 in open air so it never grazes or gets blocked by Wall 1!
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
	if (World->LineTraceSingleByChannel(InnerHit, InnerTraceStart, InnerTraceEnd, ECC_Visibility, QueryParams) && InnerHit.bBlockingHit)
	{
		const FVector HitNormal2D = FVector(InnerHit.ImpactNormal.X, InnerHit.ImpactNormal.Y, 0.0f).GetSafeNormal();

		// The blocking wall normal must point roughly opposite to SideDir (facing the character)
		const float OpposeDot = FVector::DotProduct(HitNormal2D, -SideDir);
		if (OpposeDot >= 0.4f)
		{
			const float AngleBetweenNormals = FMath::RadiansToDegrees(FMath::Acos(
				FMath::Clamp(FVector::DotProduct(WallNormal2D, HitNormal2D), -1.0f, 1.0f)));
			const float CornerAngle = 180.0f - AngleBetweenNormals;

			if (CornerAngle <= MaxAngleDeg)
			{
				// Corner vertex is where Wall 1 and Wall 2 meet (trace was 10cm in front of Wall 1):
				const FVector PivotPoint = FVector(
					InnerHit.ImpactPoint.X - WallNormal2D.X * 10.0f,
					InnerHit.ImpactPoint.Y - WallNormal2D.Y * 10.0f,
					CurrentLedgeLoc.Z
				);

				// Step along the inner wall face away from the corner seam (into the room).
				// Distances must be >= 35cm so the body clearance sphere (radius 32cm) clears Wall 1!
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
								OutCornerType = EThreadTalesCornerType::Inner;
								OutCornerPivotLoc = PivotPoint;
								OutLedgeLoc = AnalysisResult.ExactLedgeEdge;
								OutWallNormal = AnalysisResult.RefinedWallNormal;

								if (bDrawDebugTraces)
								{
									DrawDebugSphere(World, OutCornerPivotLoc, 8.0f, 8, FColor::Cyan, false, 3.0f);
									DrawDebugString(World, OutCornerPivotLoc + FVector(0, 0, 15), TEXT("INNER PIVOT"), nullptr, FColor::Cyan, 3.0f, true);
									DrawDebugSphere(World, OutLedgeLoc, 12.0f, 12, FColor::Green, false, 4.0f);
									DrawDebugDirectionalArrow(World, OutLedgeLoc, OutLedgeLoc + OutWallNormal * 40.0f, 15.0f, FColor::Green, false, 4.0f);
									DrawDebugString(World, OutLedgeLoc + FVector(0, 0, 20), TEXT("INNER CORNER OK"), nullptr, FColor::Green, 4.0f, true);
								}
								UE_LOG(LogThreadTalesLedgeDetector, Log, TEXT("[Climbing] ProbeCornerLedge: SUCCESS! Inner corner at %s → new ledge %s (normal=%s)"),
									*OutCornerPivotLoc.ToString(), *OutLedgeLoc.ToString(), *OutWallNormal.ToString());
								return true;
							}
						}
					}
				}
			}
		}
	}

	// ================================================================
	// 2. TEST FOR OUTER CORNER (Convex): Current wall ends and turns around the corner
	// ================================================================
	// Support beams and walls of various thicknesses (from 10cm thin beams to thick blocks)
	// by testing multiple inward depths into the wall geometry.
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

			// The side wall normal must point roughly in SideDir (out of the side face)
			const float SideDot = FVector::DotProduct(SideWallNormal2D, SideDir);
			if (SideDot >= 0.4f)
			{
				// Verify angle between front wall normal and side wall normal (~90 degrees)
				const float AngleBetweenNormals = FMath::RadiansToDegrees(FMath::Acos(
					FMath::Clamp(FVector::DotProduct(WallNormal2D, SideWallNormal2D), -1.0f, 1.0f)));

				if (AngleBetweenNormals <= MaxAngleDeg)
				{
					// Corner vertex is the intersection of front wall and side wall planes:
					const FVector PivotPoint = FVector(
						SideWallHit.ImpactPoint.X + WallNormal2D.X * Depth,
						SideWallHit.ImpactPoint.Y + WallNormal2D.Y * Depth,
						CurrentLedgeLoc.Z
					);

					// Search along the side wall ledge for a comfortable landing point.
					// Outer corner animation has a natural reach of ~57cm; test from 50cm down to 20cm
					// to support both long walls and shorter beams/blocks.
					const TArray<float> DesiredStepDistances = { 50.0f, 40.0f, 30.0f, 20.0f };
					for (float StepDist : DesiredStepDistances)
					{
						const FVector SideLedgeCenter = PivotPoint - WallNormal2D * StepDist;
						const FVector TopTraceStart = SideLedgeCenter - SideWallNormal2D * 10.0f + Up * 30.0f;
						const FVector TopTraceEnd   = TopTraceStart - Up * 60.0f;

						FHitResult TopHit;
						if (World->LineTraceSingleByChannel(TopHit, TopTraceStart, TopTraceEnd, ECC_Visibility, QueryParams) && TopHit.bBlockingHit && TopHit.ImpactNormal.Z >= 0.6f)
						{
							// Pinpoint the side wall vertical face at this step distance
							const FVector WallTraceStart = SideLedgeCenter + SideWallNormal2D * 30.0f - Up * 10.0f;
							const FVector WallTraceEnd   = SideLedgeCenter - SideWallNormal2D * 10.0f - Up * 10.0f;

							FHitResult WallHit;
							if (World->LineTraceSingleByChannel(WallHit, WallTraceStart, WallTraceEnd, ECC_Visibility, QueryParams) && WallHit.bBlockingHit)
							{
								FThreadTalesLedgeAnalysisResult AnalysisResult;
								if (PinpointAndValidateLedge(Character, Config, bDrawDebugTraces, WallHit.ImpactPoint, TopHit.ImpactPoint.Z, SideWallNormal2D, AnalysisResult))
								{
									OutCornerType = EThreadTalesCornerType::Outer;
									OutCornerPivotLoc = PivotPoint;
									OutLedgeLoc = AnalysisResult.ExactLedgeEdge;
									OutWallNormal = AnalysisResult.RefinedWallNormal;

									if (bDrawDebugTraces)
									{
										DrawDebugSphere(World, OutCornerPivotLoc, 8.0f, 8, FColor::Orange, false, 3.0f);
										DrawDebugString(World, OutCornerPivotLoc + FVector(0, 0, 15), TEXT("OUTER PIVOT"), nullptr, FColor::Orange, 3.0f, true);
										DrawDebugSphere(World, OutLedgeLoc, 12.0f, 12, FColor::Green, false, 4.0f);
										DrawDebugDirectionalArrow(World, OutLedgeLoc, OutLedgeLoc + OutWallNormal * 40.0f, 15.0f, FColor::Green, false, 4.0f);
										DrawDebugString(World, OutLedgeLoc + FVector(0, 0, 20), TEXT("OUTER CORNER OK"), nullptr, FColor::Green, 4.0f, true);
									}

									UE_LOG(LogThreadTalesLedgeDetector, Log, TEXT("[Climbing] ProbeCornerLedge: SUCCESS! Outer corner at %s → new ledge %s (normal=%s)"),
										*OutCornerPivotLoc.ToString(), *OutLedgeLoc.ToString(), *OutWallNormal.ToString());

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
	if (Dist <= 60.0f)
	{
		OutDistanceType = EThreadTalesLedgeJumpDistanceType::Short;
	}
	else if (Dist <= 115.0f)
	{
		OutDistanceType = EThreadTalesLedgeJumpDistanceType::Long;
	}
	else
	{
		OutDistanceType = EThreadTalesLedgeJumpDistanceType::Leap;
	}

	// 2. Project displacement onto wall plane: Lateral (X) along WallRight, Vertical (Y) along UpVector
	const float ProjX = FVector::DotProduct(Displacement, WallRight);
	const float ProjY = Displacement.Z;

	// Angle in degrees in [-180, 180]
	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Atan2(ProjY, ProjX));

	// 8-directional classification with 45° sectors
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

	if (!Character)
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

	const float HalfAngleH = Config ? Config->RadialScanConeAngleH : 45.0f;
	const float HalfAngleV = Config ? Config->RadialScanConeAngleV : 35.0f;
	const int32 StepsH = Config ? FMath::Max(Config->RadialScanRaysH, 3) : 7;
	const int32 StepsV = Config ? FMath::Max(Config->RadialScanRaysV, 1) : 5;
	const float MaxDist = Config ? Config->RadialScanMaxDistance : 220.0f;
	const float AngleWeight = Config ? Config->RadialScanAngleWeight : 0.65f;
	const float DistWeight = Config ? Config->RadialScanDistanceWeight : 0.35f;
	const TArray<float> ProbeDepths = (Config && Config->DirectionalProbeDepths.Num() > 0) ? Config->DirectionalProbeDepths : TArray<float>{ 2.5f, 5.0f, 10.0f, 18.0f };

	const FVector CentralDir = IntentDirectionWorld.IsNearlyZero() ? Up : IntentDirectionWorld.GetSafeNormal();
	const float IntentVertical = CentralDir.Z;
	const float IntentLateral = FVector::DotProduct(CentralDir, WallRight);

	// Orthonormal basis for scanning cone around CentralDir
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

	const float WallSweepRadius = 14.0f;
	TArray<FThreadTalesLedgeCandidate> UnvalidatedCandidates;

	// Lambda to evaluate a candidate hit on a wall surface (Phase 1: Fast Edge Discovery)
	auto ProcessWallHit = [&](const FHitResult& WallHit)
	{
		if (!WallHit.bBlockingHit || FMath::Abs(WallHit.ImpactNormal.Z) > 0.45f)
		{
			return;
		}

		const FVector FoundWallNorm2D = FVector(WallHit.ImpactNormal.X, WallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();
		if (FoundWallNorm2D.IsNearlyZero())
		{
			return;
		}

		// Probe downward for top surface
		FHitResult BestTopHit;
		bool bFoundValidTop = false;

		for (float Depth : ProbeDepths)
		{
			// When searching downward, target ledge lip CANNOT be at or above current ledge.
			// Clamp DownStartZ so it never probes back into the current ledge or higher geometry!
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

		// Pinpoint exact outer lip
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

		// Directional Sanity Filter: reject candidates physically opposing intent
		if (IntentVertical < -0.3f && CandidateLoc.Z > CurrentLedgeLoc.Z - 15.0f)
		{
			return; // Downward intent requires candidate to be distinctly below current ledge
		}
		if (IntentVertical > 0.3f && CandidateLoc.Z < CurrentLedgeLoc.Z + 15.0f)
		{
			return; // Upward intent requires candidate to be distinctly above current ledge
		}

		const float CandidateLateral = FVector::DotProduct(CandidateLoc - CurrentLedgeLoc, WallRight);
		if (IntentLateral > 0.5f && CandidateLateral < 10.0f)
		{
			return; // Right intent requires candidate to be to the right
		}
		if (IntentLateral < -0.5f && CandidateLateral > -10.0f)
		{
			return; // Left intent requires candidate to be to the left
		}

		// Avoid duplicate candidates
		for (const FThreadTalesLedgeCandidate& Existing : UnvalidatedCandidates)
		{
			if (FVector::DistSquared(Existing.LedgeLocation, CandidateLoc) < (18.0f * 18.0f))
			{
				return;
			}
		}

		const FVector DirToCand = (CandidateLoc - CurrentLedgeLoc).GetSafeNormal();
		const float RawAlignmentDot = FVector::DotProduct(CentralDir, DirToCand);

		// STRICT GATING: Candidate must be inside or close to the intent cone.
		// A dot product < 0.25 means angle > 75 degrees from intent vector (perpendicular / reverse).
		if (RawAlignmentDot < 0.25f)
		{
			return;
		}

		// Fast candidate registration & preliminary scoring (deferred heavy clearance checks)
		FThreadTalesLedgeCandidate Cand;
		Cand.LedgeLocation = CandidateLoc;
		Cand.WallNormal = CandidateNormal;
		Cand.Distance = Dist;

		ClassifyCandidateDirectionAndTier(CandidateLoc - CurrentLedgeLoc, CandidateNormal, WallRight, JumpTable, Cand.Direction, Cand.DistanceType);

		// Reject incompatible classified direction
		if (IntentVertical < -0.3f && (Cand.Direction == EThreadTalesLedgeJumpDirection::Up
			|| Cand.Direction == EThreadTalesLedgeJumpDirection::UpLeft
			|| Cand.Direction == EThreadTalesLedgeJumpDirection::UpRight
			|| Cand.Direction == EThreadTalesLedgeJumpDirection::Left
			|| Cand.Direction == EThreadTalesLedgeJumpDirection::Right))
		{
			return;
		}
		if (IntentVertical > 0.3f && (Cand.Direction == EThreadTalesLedgeJumpDirection::Down
			|| Cand.Direction == EThreadTalesLedgeJumpDirection::DownLeft
			|| Cand.Direction == EThreadTalesLedgeJumpDirection::DownRight))
		{
			return;
		}

		const float ProximityRatio = 1.0f - FMath::Clamp(Dist / MaxDist, 0.0f, 1.0f);
		// Quadratic alignment falloff ensures well-aligned candidates always dominate over poorly-aligned ones
		const float AlignmentFactor = FMath::Square(FMath::Clamp(RawAlignmentDot, 0.0f, 1.0f));
		Cand.Score = AlignmentFactor * (AngleWeight + (ProximityRatio * DistWeight));

		UnvalidatedCandidates.Add(Cand);

		if (bDrawDebugTraces)
		{
			DrawDebugSphere(World, CandidateLoc, 7.0f, 8, FColor::Cyan, false, 1.5f, 0, 1.0f);
		}
	};

	// 4 calibrated distance tiers covering Short (45), Long (85), Leap (140), and MaxLeap (190)
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

			// A. If ray points into the room (away from wall), do a 3D volumetric sweep
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
					ProcessWallHit(WallHit);
				}
			}
			else
			{
				// B. Ray is in/along the wall plane (e.g. Up, Down, Lateral, Diagonals).
				// We sample distance tiers along RayDir and probe HORIZONTALLY INTO THE WALL (-WallNorm2D)!
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
						ProcessWallHit(WallHit);
					}
				}
			}
		}
	}

	if (UnvalidatedCandidates.Num() == 0)
	{
		return false;
	}

	// Sort unvalidated candidates by Score descending
	UnvalidatedCandidates.Sort([](const FThreadTalesLedgeCandidate& A, const FThreadTalesLedgeCandidate& B)
	{
		return A.Score > B.Score;
	});

	// Phase 2: Lazy Fine Validation on top-scoring candidates only
	const int32 MaxValidations = Config ? FMath::Max(Config->RadialScanMaxValidations, 1) : 3;
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

	if (!bFoundWinner)
	{
		return false;
	}

	if (bDrawDebugTraces)
	{
		DrawDebugSphere(World, OutBestCandidate.LedgeLocation, 16.0f, 16, FColor::Orange, false, 3.5f, 0, 2.5f);
		DrawDebugDirectionalArrow(World, CurrentLedgeLoc, OutBestCandidate.LedgeLocation, 16.0f, FColor::Emerald, false, 3.5f, 0, 3.0f);
		DrawDebugString(World, OutBestCandidate.LedgeLocation + FVector(0, 0, 25),
			FString::Printf(TEXT("WINNER (Dist:%.0f, Sc:%.2f)"), OutBestCandidate.Distance, OutBestCandidate.Score),
			nullptr, FColor::Emerald, 3.5f, true);
	}

	UE_LOG(LogThreadTalesLedgeDetector, Log, TEXT("[RadialScanner] Found %d coarse candidates. WINNER: %s (Dist=%.1f, Score=%.2f, Dir=%d, Tier=%d)"),
		UnvalidatedCandidates.Num(), *OutBestCandidate.LedgeLocation.ToString(), OutBestCandidate.Distance, OutBestCandidate.Score,
		(int32)OutBestCandidate.Direction, (int32)OutBestCandidate.DistanceType);

	return true;
}
