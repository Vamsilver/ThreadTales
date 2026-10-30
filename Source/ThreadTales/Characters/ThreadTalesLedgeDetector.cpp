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

DEFINE_LOG_CATEGORY(LogThreadTalesLedgeDetector);

namespace
{
	FORCEINLINE FCollisionQueryParams MakeLedgeQueryParams(const AActor* IgnoredActor)
	{
		FCollisionQueryParams Params;
		if (IgnoredActor)
		{
			Params.AddIgnoredActor(IgnoredActor);
		}
		Params.bTraceComplex = false;
		return Params;
	}
}

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

	const float FwdDist = Config ? Config->Sensing.Detection.ForwardTraceDistance : 80.0f;
	const float FwdZOffset = Config ? Config->Sensing.Detection.ForwardTraceZOffset : 0.0f;
	const float MinGrabH = Config ? Config->Sensing.Detection.MinGrabHeight : 30.0f;
	const float MaxGrabH = Config ? Config->Sensing.Detection.MaxGrabHeight : 140.0f;
	const float MaxApproachDeg = Config ? Config->Sensing.Detection.MaxApproachAngle : 65.0f;
	const FName LedgeTag = Config ? Config->Sensing.Detection.LedgeTag : NAME_None;
	const bool bRequireTag = Config ? Config->Sensing.Detection.bRequireLedgeTag : false;

	const float TraceHeights[] = {
		FwdZOffset - 20.0f,
		FwdZOffset,
		FwdZOffset + 15.0f,
		FwdZOffset + 25.0f
	};

	const FCollisionQueryParams QueryParams = MakeLedgeQueryParams(Character);

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

	const float DownTraceH = Config ? Config->Sensing.Detection.LedgeDownTraceHeight : 140.0f;
	const float DownTraceD = Config ? Config->Sensing.Detection.LedgeDownTraceDepth : 160.0f;
	const float MinGrabH = Config ? Config->Sensing.Detection.MinGrabHeight : 30.0f;
	const float MaxGrabH = Config ? Config->Sensing.Detection.MaxGrabHeight : 140.0f;

	TArray<float> ProbeDepths = (Config && Config->Sensing.Detection.LedgeProbeDepths.Num() > 0) ? Config->Sensing.Detection.LedgeProbeDepths : TArray<float>{ 2.5f, 5.0f, 10.0f, 18.0f };

	const FCollisionQueryParams QueryParams = MakeLedgeQueryParams(Character);

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

	const FCollisionQueryParams QueryParams = MakeLedgeQueryParams(Character);

	const FVector Up = FVector::UpVector;
	const FVector Normal2D = WallNormal2D.GetSafeNormal();

	const float LipBelowDepth = Config ? Config->Sensing.Validation.LipBelowWallTraceDepth : 35.0f;
	const float LipAboveHeight = Config ? Config->Sensing.Validation.LipAboveAirTraceHeight : 15.0f;
	const float SeamRejectDist = Config ? Config->Sensing.Validation.LipModularSeamRejectDistance : 12.0f;
	const float BodyRadius = Config ? Config->Sensing.Validation.BodyClearanceRadius : 32.0f;
	const float BodyHeight = Config ? Config->Sensing.Validation.BodyClearanceHeight : 50.0f;
	const float HangWallOff = Config ? Config->Kinematics.Offsets.HangWallOffset : 24.0f;
	const bool bAllowFreehang = Config ? Config->Traversal.General.bAllowFreehang : true;

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

	const FCollisionQueryParams QueryParams = MakeLedgeQueryParams(Character);

	const float HandSpacing = Config ? Config->Sensing.Detection.HandSpacing : 22.0f;
	const float HandsProbeDepth = Config ? Config->Sensing.Detection.LedgeHandsProbeDepth : 12.0f;
	const float HandsVertRange = Config ? Config->Sensing.Detection.LedgeHandsVerticalRange : 15.0f;
	const float SlopeMaxZDiff = Config ? Config->Sensing.Detection.LedgeSlopeMaxZDifference : 15.0f;

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

	const bool bAllowFreehang = Config ? Config->Traversal.General.bAllowFreehang : true;
	if (!bAllowFreehang)
	{
		return false;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return false;
	}

	const FCollisionQueryParams QueryParams = MakeLedgeQueryParams(Character);

	const FVector Up = FVector::UpVector;
	const FVector SafeWallNorm2D = WallNormal2D.GetSafeNormal();
	const FVector WallRight = FVector::CrossProduct(SafeWallNorm2D, Up).GetSafeNormal();

	// Probe distances:
	// TraceForward: 12 cm in front of the ledge (where player's toes hang)
	// TraceBack: 5 cm inward behind the ledge (strictly limited to 3..6 cm so it doesn't punch through beams or detect recessed walls)
	const float TraceFwd = Config ? FMath::Clamp(Config->Sensing.Freehang.FreehangWallTraceForward, 8.0f, 15.0f) : 12.0f;
	const float TraceBack = Config ? FMath::Clamp(Config->Sensing.Freehang.FreehangWallTraceBack, 3.0f, 6.0f) : 5.0f;

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

	const FCollisionQueryParams QueryParams = MakeLedgeQueryParams(Character);

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
	const float MaxGrabWallDist = Config ? Config->Sensing.Detection.MaxGrabWallDistance : 75.0f;
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

	const FCollisionQueryParams Params = MakeLedgeQueryParams(Character);

	const float Sign = (DirectionSign < 0.0f) ? -1.0f : 1.0f;
	const FVector Up = FVector::UpVector;
	const FVector Normal2D = CurrentWallNormal.GetSafeNormal();
	const FVector WallRight = FVector::CrossProduct(Normal2D, Up).GetSafeNormal();
	const FVector SideDir = WallRight * Sign;

	const float ShimmyStepDist = Config ? Config->Traversal.Shimmy.ShimmyStepDistance : 67.0f;
	const float ShimmyVertRange = Config ? Config->Traversal.Shimmy.ShimmyVerticalTraceRange : 20.0f;
	const float ShimmyMaxZDiff = Config ? Config->Traversal.Shimmy.ShimmyMaxZDifference : 20.0f;
	const TArray<float> ShimmyProbeDepths = (Config && Config->Traversal.Shimmy.ShimmyProbeDepths.Num() > 0) ? Config->Traversal.Shimmy.ShimmyProbeDepths : TArray<float>{ 4.0f, 10.0f };

	// Phase 1: Test for Continuous Ledge (Shimmy) with multi-distance steps
	if (!bForJump)
	{
		const bool bLog = UThreadTalesTraversalComponent::IsGlobalTraversalLoggingEnabled(Character);
		if (bLog)
		{
			UE_LOG(LogThreadTalesTraversal, Log, TEXT("[TT_TRAVERSAL][FIND_SIDE] Scanning continuous ledge for DirSign=%.2f from %s (Normal: %s)..."),
				DirectionSign, *CurrentLedgeLoc.ToString(), *CurrentWallNormal.ToString());
		}

		const float StepDistances[] = { ShimmyStepDist, ShimmyStepDist * 0.70f, ShimmyStepDist * 0.45f, ShimmyStepDist * 0.25f };

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
					const bool bBlocking = ShimmyTopHit.bBlockingHit && !ShimmyTopHit.bStartPenetrating;
					const bool bNormalOk = ShimmyTopHit.ImpactNormal.Z >= 0.6f;
					const float ZDiff = FMath::Abs(ShimmyTopHit.ImpactPoint.Z - CurrentLedgeLoc.Z);
					const bool bZOk = ZDiff <= ShimmyMaxZDiff;

					if (bBlocking && bNormalOk && bZOk)
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

							if (bLog)
							{
								UE_LOG(LogThreadTalesTraversal, Log, TEXT("[TT_TRAVERSAL][FIND_SIDE_SUCCESS] Step %.1f cm (Depth %.1f) FOUND LEDGE at %s (Normal: %s, Freehang: %d)"),
									StepDist, InwardDepth, *OutLedgeLoc.ToString(), *OutWallNormal.ToString(), AnalysisResult.bIsFreehang ? 1 : 0);
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
						else if (bLog)
						{
							UE_LOG(LogThreadTalesTraversal, Verbose, TEXT("[TT_TRAVERSAL][FIND_SIDE_FAIL] Step %.1f cm (Depth %.1f) Hit OK at %s, but PinpointAndValidateLedge REJECTED it."),
								StepDist, InwardDepth, *ShimmyTopHit.ImpactPoint.ToString());
						}
					}
					else if (bLog)
					{
						UE_LOG(LogThreadTalesTraversal, Verbose, TEXT("[TT_TRAVERSAL][FIND_SIDE_FAIL] Step %.1f cm (Depth %.1f) Hit rejected: Blocking=%d, NormalZ=%.2f (need >=0.6), ZDiff=%.1f (need <=%.1f)"),
							StepDist, InwardDepth, bBlocking ? 1 : 0, ShimmyTopHit.ImpactNormal.Z, ZDiff, ShimmyMaxZDiff);
					}
				}
			}
		}

		if (bLog)
		{
			UE_LOG(LogThreadTalesTraversal, Warning, TEXT("[TT_TRAVERSAL][FIND_SIDE_NONE] No continuous ledge found across all step distances for DirSign=%.2f."),
				DirectionSign);
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

bool FThreadTalesLedgeDetector::ProbeContinuousLedgePoint(
	const ACharacter* Character,
	const UThreadTalesClimbingConfig* Config,
	bool bDrawDebugTraces,
	const FVector& CandidateLedgeLoc,
	const FVector& CurrentWallNormal,
	FVector& OutLedgeLoc,
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

	const FCollisionQueryParams Params = MakeLedgeQueryParams(Character);

	const FVector Normal2D = CurrentWallNormal.GetSafeNormal();
	const float ShimmyVertRange = Config ? Config->Traversal.Shimmy.ShimmyVerticalTraceRange : 20.0f;
	const float ShimmyMaxZDiff = Config ? Config->Traversal.Shimmy.ShimmyMaxZDifference : 20.0f;
	const TArray<float> ShimmyProbeDepths = (Config && Config->Traversal.Shimmy.ShimmyProbeDepths.Num() > 0) ? Config->Traversal.Shimmy.ShimmyProbeDepths : TArray<float>{ 3.0f, 7.0f, 12.0f };

	for (float InwardDepth : ShimmyProbeDepths)
	{
		const FVector StepProbe = CandidateLedgeLoc - (Normal2D * InwardDepth);
		const FVector TopStart = StepProbe + FVector(0.0f, 0.0f, ShimmyVertRange);
		const FVector TopEnd = StepProbe - FVector(0.0f, 0.0f, ShimmyVertRange);

		FHitResult ShimmyTopHit;
		const bool bHit = World->LineTraceSingleByChannel(ShimmyTopHit, TopStart, TopEnd, ECC_Visibility, Params);

		if (bDrawDebugTraces)
		{
			DrawDebugLine(World, TopStart, TopEnd, bHit ? FColor::Yellow : FColor::Red, false, 0.05f, 0, 1.0f);
		}

		if (bHit)
		{
			if (ShimmyTopHit.bBlockingHit && !ShimmyTopHit.bStartPenetrating &&
				ShimmyTopHit.ImpactNormal.Z >= 0.6f &&
				FMath::Abs(ShimmyTopHit.ImpactPoint.Z - CandidateLedgeLoc.Z) <= ShimmyMaxZDiff)
			{
				FThreadTalesLedgeAnalysisResult AnalysisResult;
				if (PinpointAndValidateLedge(Character, Config, bDrawDebugTraces, ShimmyTopHit.ImpactPoint, ShimmyTopHit.ImpactPoint.Z, Normal2D, AnalysisResult))
				{
					OutLedgeLoc = AnalysisResult.ExactLedgeEdge;
					OutWallNormal = AnalysisResult.RefinedWallNormal;
					if (OutIsFreehang)
					{
						*OutIsFreehang = AnalysisResult.bIsFreehang;
					}

					if (bDrawDebugTraces)
					{
						DrawDebugPoint(World, OutLedgeLoc, 10.0f, FColor::Green, false, 0.1f);
					}
					return true;
				}
			}
			else if (bDrawDebugTraces)
			{
				DrawDebugPoint(World, ShimmyTopHit.ImpactPoint, 6.0f, FColor::Orange, false, 0.05f);
			}
		}
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

	const FCollisionQueryParams QueryParams = MakeLedgeQueryParams(Character);

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

	const FCollisionQueryParams QueryParams = MakeLedgeQueryParams(Character);

	const float FloorTraceFwd = Config ? Config->Sensing.Probing.ClimbDownFloorTraceForwardDist : 15.0f;
	const float FloorTraceZRange = Config ? Config->Sensing.Probing.ClimbDownFloorTraceZRange : 25.0f;
	const float EdgeLipProbeRange = Config ? Config->Sensing.Probing.ClimbDownEdgeLipProbeRange : 30.0f;
	const float EdgeLipDepth = Config ? Config->Sensing.Probing.ClimbDownEdgeLipDepth : 10.0f;

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

	const FCollisionQueryParams QueryParams = MakeLedgeQueryParams(Character);

	const float TraceDistance = Config ? Config->Traversal.General.ClimbDownTraceDistance : 70.0f;
	const float MinApproachDot = Config ? Config->Traversal.ClimbDown.ClimbDownMinApproachDot : 0.25f;

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

