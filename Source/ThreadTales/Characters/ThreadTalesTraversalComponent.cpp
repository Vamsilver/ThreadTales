// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesTraversalComponent.h"
#include "ThreadTales.h"
#include "ThreadTalesCharacter.h"
#include "ThreadTales/Core/ThreadTalesGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"

UThreadTalesTraversalComponent::UThreadTalesTraversalComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);

	CurrentTraversalState = EThreadTalesTraversalState::None;
	LedgeTag = FName(TEXT("Ledge"));
	bRequireLedgeTag = false;
	bAutoGrabLedge = true;
	bDrawDebugTraces = true;

	ForwardTraceDistance = 120.0f;
	ForwardTraceZOffset = 45.0f;
	LedgeDownTraceHeight = 60.0f;
	LedgeDownTraceDepth = 50.0f;

	MinGrabHeight = 10.0f;
	MaxGrabHeight = 155.0f;
	HandSpacing = 22.0f;
	MaxApproachAngle = 65.0f;
	MaxGrabVelocityZ = 400.0f;
	LedgeProbeDepths = { 5.0f, 12.0f, 20.0f };
	MaxGrabWallDistance = 55.0f;

	LipBelowWallTraceDepth = 6.0f;
	LipAboveAirTraceHeight = 6.0f;
	LipModularSeamRejectDistance = 15.0f;
	BodyClearanceRadius = 20.0f;
	BodyClearanceHeight = 40.0f;

	HangWallOffset = 24.0f;
	HangHeightOffset = 73.0f; // Calibrated: Z_hands in MD_hang_idle1 rests directly on the ledge lip

	ClimbAnimPlayRateMultiplier = 1.0f;

	// Calibrated based on 48.1 cm short animations (nominal distance 48.1 cm) - [20..58 cm]
	MinHopUpHeight = 20.0f;
	MaxHopUpHeight = 58.0f;
	MinSideJumpDistance = 25.0f;
	MaxSideJumpDistance = 58.0f;
	MinDiagonalJumpDistance = 25.0f;
	MaxDiagonalJumpDistance = 60.0f;
	MinHopDownHeight = 20.0f;
	MaxHopDownHeight = 58.0f;

	// Calibrated based on 96.2 cm long animations (nominal distance 96.2 cm) - [50..110 cm] (Overlaps Short and Leap)
	MinLongHopUpHeight = 50.0f;
	MaxLongHopUpHeight = 110.0f;
	MinLongSideJumpDistance = 50.0f;
	MaxLongSideJumpDistance = 110.0f;
	MinLongDiagonalJumpDistance = 50.0f;
	MaxLongDiagonalJumpDistance = 110.0f;
	MinLongHopDownHeight = 50.0f;
	MaxLongHopDownHeight = 110.0f;

	// Calibrated based on 200.0 cm leap animations (nominal distance 200.0 cm) - [85..200 cm]
	MinLeapHopUpHeight = 85.0f;
	MaxLeapHopUpHeight = 200.0f;
	MinLeapSideJumpDistance = 85.0f;
	MaxLeapSideJumpDistance = 200.0f;
	MinLeapDiagonalJumpDistance = 85.0f;
	MaxLeapDiagonalJumpDistance = 200.0f;
	MinLeapHopDownHeight = 85.0f;
	MaxLeapHopDownHeight = 200.0f;

	ShimmyStepDistance = 67.0f; // Matches MD_Shimmy root displacement of 67.4 cm
	ShimmyCooldownDuration = 0.55f; // Matches 0.67s animation length with playrate 1.2x
	ShimmyProbeDepths = { 4.0f, 10.0f };
	ShimmyVerticalTraceRange = 20.0f;
	ShimmyMaxZDifference = 20.0f;

	DirectionalProbeDepths = { 4.0f, 8.0f, 16.0f };
	UpJumpTopTraceOffset = 55.0f;
	DownJumpTopTraceOffset = 22.0f;
	DownJumpLeapTopTraceOffset = 28.0f;
	LateralJumpTopTraceOffset = 28.0f;
	DiagonalUpJumpTopTraceOffset = 50.0f;
	DiagonalDownJumpTopTraceOffset = 25.0f;

	ShortJumpDuration = 0.95f;
	LongJumpDuration = 1.00f;
	LeapJumpDuration = 1.00f;
	JumpFlightEndRatio = 0.85f;
	ShimmyInputDebounceDuration = 0.15f;
	JumpInputDebounceDuration = 0.35f;

	RegrabCooldownDuration = 1.5f;
	RegrabCooldownTimer = 0.0f;
	ClimbingInputDebounceTimer = 0.0f;
	bIsClimbingUp = false;

	bIsTransitioning = false;
	bIsJumpTransition = false;
	TransitionDistanceType = EThreadTalesLedgeJumpDistanceType::Short;
	TransitionTimer = 0.0f;
	TransitionDuration = 0.0f;
	TransitionStartLocation = FVector::ZeroVector;
	TransitionStartRotation = FRotator::ZeroRotator;
	TransitionTargetLocation = FVector::ZeroVector;
	TransitionTargetRotation = FRotator::ZeroRotator;
	PendingTargetLedgeLocation = FVector::ZeroVector;
	PendingTargetWallNormal = FVector::ZeroVector;
	ShimmyCooldownTimer = 0.0f;

	bAllowFreehang = true;
	bIsFreehang = false;
	bIsClimbingDown = false;
	ClimbDownTraceDistance = 70.0f;
	WallEjectHorizontalSpeed = 450.0f;
	WallEjectVerticalSpeed = 350.0f;
	BackJumpOpposingLedgeDistance = 450.0f;

	CachedLedgeLocation = FVector::ZeroVector;
	CachedWallNormal = FVector::ZeroVector;
	CachedHangLocation = FVector::ZeroVector;
	CachedHangRotation = FRotator::ZeroRotator;
}

void UThreadTalesTraversalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UThreadTalesTraversalComponent, CurrentTraversalState);
	DOREPLIFETIME(UThreadTalesTraversalComponent, bIsFreehang);
}

void UThreadTalesTraversalComponent::BeginPlay()
{
	Super::BeginPlay();
	CharacterOwner = Cast<AThreadTalesCharacter>(GetOwner());
}

void UThreadTalesTraversalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (RegrabCooldownTimer > 0.0f)
	{
		RegrabCooldownTimer -= DeltaTime;
	}

	if (!CharacterOwner)
	{
		return;
	}

	// 1. If currently transitioning between ledges (Hop Up, Side Jump, Shimmy), interpolate character
	if (CurrentTraversalState == EThreadTalesTraversalState::LedgeClimbing && bIsTransitioning)
	{
		TransitionTimer += DeltaTime;
		const float NormalizedTime = FMath::Clamp(TransitionTimer / FMath::Max(TransitionDuration, 0.01f), 0.0f, 1.0f);

		float InterpAlpha = 0.0f;
		if (bIsJumpTransition)
		{
			// Kinematic 3-phase alignment matching the animation kinematics:
			// Phase 1 [0.0 .. AnticipationEnd]: Anticipation / Hold onto starting ledge (InterpAlpha = 0.0)
			// Phase 2 [AnticipationEnd .. FlightEnd]: Flight / Leap through air from start to target
			// Phase 3 [FlightEnd .. 1.00]: Landing / Grab target ledge & absorb impact (InterpAlpha = 1.0)
			float AnticipationEnd = 0.19f;
			float FlightEnd = 0.78f;
			GetJumpKinematicPhases(TransitionDistanceType, AnticipationEnd, FlightEnd);

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

		const FVector NewLoc = FMath::Lerp(TransitionStartLocation, TransitionTargetLocation, InterpAlpha);
		const FRotator NewRot = FMath::Lerp(TransitionStartRotation, TransitionTargetRotation, InterpAlpha);

		CharacterOwner->SetActorLocationAndRotation(NewLoc, NewRot, false, nullptr, ETeleportType::TeleportPhysics);

		if (NormalizedTime >= 1.0f)
		{
			bIsTransitioning = false;
			CachedLedgeLocation = PendingTargetLedgeLocation;
			CachedWallNormal = PendingTargetWallNormal;
			CachedHangLocation = TransitionTargetLocation;
			CachedHangRotation = TransitionTargetRotation;

			// Check if player is holding lateral input for chained continuous shimmy
			bool bChainedShimmy = false;
			if (CharacterOwner->IsLocallyControlled())
			{
				float NextDirSign = 0.0f;
				if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
				{
					if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Right))
					{
						NextDirSign = 1.0f;
					}
					else if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Left))
					{
						NextDirSign = -1.0f;
					}
				}

				if (NextDirSign != 0.0f)
				{
					FVector NextLedgeLoc;
					FVector NextWallNormal;
					bool bNextContinuous = false;
					if (FindSideLedge(NextDirSign, NextLedgeLoc, NextWallNormal, bNextContinuous, false) && bNextContinuous)
					{
						bChainedShimmy = true;
						StartShimmy(NextDirSign, NextLedgeLoc, NextWallNormal);
					}
				}
			}

			if (!bChainedShimmy)
			{
				ShimmyCooldownTimer = 0.08f;
				ClimbingInputDebounceTimer = 0.35f;
				UAnimMontage* ActiveHangMont = (bIsFreehang && BarHangMontage) ? BarHangMontage : HangMontage;
				if (ActiveHangMont)
				{
					CharacterOwner->PlayAnimMontage(ActiveHangMont);
				}
			}
		}
		return;
	}

	// 2. While hanging on ledge (not transitioning, not climbing up, not climbing down), process WASD climbing input and hard-anchor
	if (CurrentTraversalState == EThreadTalesTraversalState::LedgeClimbing && !bIsClimbingUp && !bIsClimbingDown)
	{
		if (CharacterOwner->IsLocallyControlled())
		{
			ProcessClimbingInput(DeltaTime);
		}

		UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
		if (MoveComp)
		{
			MoveComp->Velocity = FVector::ZeroVector;
		}
		CharacterOwner->SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation, false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	// Auto-detect ledges when airborne and actually descending (Velocity.Z < 0).
	// Auto-detect ledges when airborne (Velocity.Z <= MaxGrabVelocityZ).
	// Allows grabbing ledges both on the upward ascent of a jump as hands reach the lip,
	// and while descending (falling) toward a ledge.
	if (CurrentTraversalState == EThreadTalesTraversalState::None && bAutoGrabLedge && RegrabCooldownTimer <= 0.0f)
	{
		UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
		if (MoveComp && MoveComp->IsFalling() && MoveComp->Velocity.Z <= MaxGrabVelocityZ)
		{
			// Only detect locally or on server
			if (CharacterOwner->IsLocallyControlled() || CharacterOwner->HasAuthority())
			{
				FVector DetectedLedgeLocation;
				FVector DetectedWallNormal;
				if (DetectLedge(DetectedLedgeLocation, DetectedWallNormal))
				{
					StartLedgeClimb(DetectedLedgeLocation, DetectedWallNormal);
				}
			}
		}
	}
}

bool UThreadTalesTraversalComponent::ValidateLedgeLipAndClearance(const FVector& EdgeLoc, const FVector& WallNormal2D)
{
	if (!CharacterOwner)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(CharacterOwner);
	QueryParams.bTraceComplex = false;

	const FVector Up = FVector::UpVector;
	const FVector Normal2D = WallNormal2D.GetSafeNormal();

	// -------------------------------------------------------------
	// 1. Below Lip Check: Ensure solid vertical wall face exists right under the edge
	// -------------------------------------------------------------
	const FVector BelowLipStart = EdgeLoc + (Normal2D * 25.0f) - Up * LipBelowWallTraceDepth;
	const FVector BelowLipEnd = EdgeLoc - (Normal2D * 20.0f) - Up * LipBelowWallTraceDepth;

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

	// -------------------------------------------------------------
	// 2. Above Lip Check (Flat Wall & Modular Seam Rejector):
	// Tracing from in front of the edge inward at LipAboveAirTraceHeight above the edge.
	// On a flat wall, this trace will hit the upward-continuing wall surface near EdgeLoc.XY.
	// On a true ledge (shelf, box top, roof), there is open air directly above the lip.
	// -------------------------------------------------------------
	const FVector AboveLipStart = EdgeLoc + (Normal2D * 25.0f) + Up * LipAboveAirTraceHeight;
	const FVector AboveLipEnd = EdgeLoc - (Normal2D * 8.0f) + Up * LipAboveAirTraceHeight;

	FHitResult AboveHit;
	bool bAboveHit = World->LineTraceSingleByChannel(AboveHit, AboveLipStart, AboveLipEnd, ECC_Visibility, QueryParams);

	if (bDrawDebugTraces)
	{
		DrawDebugLine(World, AboveLipStart, AboveLipEnd, (bAboveHit && AboveHit.bBlockingHit) ? FColor::Red : FColor::Green, false, 2.0f, 0, 1.5f);
	}

	if (bAboveHit && AboveHit.bBlockingHit)
	{
		const float Dist2D = FVector::Dist2D(AboveHit.ImpactPoint, EdgeLoc);
		if (Dist2D < LipModularSeamRejectDistance)
		{
			// Solid wall continues above the ledge lip -> Flat wall / internal seam!
			if (bDrawDebugTraces)
			{
				DrawDebugString(World, EdgeLoc + FVector(0, 0, 10), TEXT("REJECT: WALL ABOVE"), nullptr, FColor::Red, 2.0f, true);
			}
			return false;
		}
	}

	// -------------------------------------------------------------
	// 3. Body Clearance Check:
	// Verify open space for player's torso and head in front of the ledge (avoid hanging into ceilings/overhangs)
	// Body hangs around the hang anchor; head is at +25..+35cm. We check up to BodyClearanceHeight without colliding with upper ledge tiers.
	// -------------------------------------------------------------
	const FVector BodyStart = EdgeLoc + (Normal2D * HangWallOffset) - Up * 5.0f;
	const FVector BodyEnd = BodyStart + Up * BodyClearanceHeight;

	FHitResult BodyHit;
	bool bBodyBlocked = World->SweepSingleByChannel(
		BodyHit,
		BodyStart,
		BodyEnd,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(BodyClearanceRadius),
		QueryParams
	);

	if (bDrawDebugTraces)
	{
		DrawDebugCapsule(World, BodyStart + Up * 20.0f, 20.0f, BodyClearanceRadius, FQuat::Identity,
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

bool UThreadTalesTraversalComponent::CheckIsFreehangLedge(const FVector& EdgeLoc, const FVector& WallNormal2D) const
{
	if (!CharacterOwner || !bAllowFreehang)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(CharacterOwner);
	QueryParams.bTraceComplex = false;

	const FVector Up = FVector::UpVector;
	const FVector Normal2D = WallNormal2D.GetSafeNormal();

	// Check if there is a solid wall face beneath hands where feet/knees push (60-80cm below ledge)
	const FVector WallCheckStart = EdgeLoc + (Normal2D * 35.0f) - (Up * 70.0f);
	const FVector WallCheckEnd = EdgeLoc - (Normal2D * 25.0f) - (Up * 70.0f);

	FHitResult WallHit;
	const bool bHitWall = World->LineTraceSingleByChannel(WallHit, WallCheckStart, WallCheckEnd, ECC_Visibility, QueryParams);

	if (bDrawDebugTraces)
	{
		DrawDebugLine(World, WallCheckStart, WallCheckEnd, (bHitWall && WallHit.bBlockingHit) ? FColor::Red : FColor::Green, false, 2.0f, 0, 1.5f);
	}

	// If no wall exists at 70cm below ledge, character has nothing to put feet against -> Freehang!
	return (!bHitWall || !WallHit.bBlockingHit);
}

bool UThreadTalesTraversalComponent::DetectLedge(FVector& OutLedgeLocation, FVector& OutWallNormal)
{
	if (!CharacterOwner)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(CharacterOwner);
	QueryParams.bTraceComplex = false;

	const FVector ActorLoc = CharacterOwner->GetActorLocation();
	const FVector Forward = CharacterOwner->GetActorForwardVector();
	const FVector Up = FVector::UpVector;
	const FVector Right = CharacterOwner->GetActorRightVector();

	// -------------------------------------------------------------
	// STEP 1: Multi-Level Forward Wall Detection
	// Using a sphere sweep to robustly detect walls regardless of height variations
	// -------------------------------------------------------------
	const FVector ForwardStart = ActorLoc + Up * ForwardTraceZOffset;
	const FVector ForwardEnd = ForwardStart + Forward * ForwardTraceDistance;
	const float ForwardRadius = 15.0f;

	FHitResult ForwardHit;
	bool bHitWall = World->SweepSingleByChannel(
		ForwardHit,
		ForwardStart,
		ForwardEnd,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(ForwardRadius),
		QueryParams
	);

	if (bDrawDebugTraces)
	{
		DrawDebugLine(World, ForwardStart, ForwardEnd, (bHitWall && ForwardHit.bBlockingHit) ? FColor::Green : FColor::Red, false, -1.0f, 0, 1.5f);
		if (bHitWall && ForwardHit.bBlockingHit)
		{
			DrawDebugSphere(World, ForwardHit.ImpactPoint, ForwardRadius, 8, FColor::Green, false, -1.0f, 0, 1.0f);
		}
	}

	if (!bHitWall || !ForwardHit.bBlockingHit)
	{
		return false;
	}

	// Approach angle check: ensure character is facing roughly towards the wall (within MaxApproachAngle)
	FVector WallNormal2D = FVector(ForwardHit.ImpactNormal.X, ForwardHit.ImpactNormal.Y, 0.0f).GetSafeNormal();
	if (WallNormal2D.IsNearlyZero())
	{
		WallNormal2D = -Forward;
	}

	const float MinFacingDot = FMath::Cos(FMath::DegreesToRadians(MaxApproachAngle));
	const float FacingDot = FVector::DotProduct(Forward, -WallNormal2D);
	if (FacingDot < MinFacingDot)
	{
		return false;
	}

	// Check for Ledge tag if required
	if (bRequireLedgeTag)
	{
		bool bHasTag = false;
		if (AActor* HitActor = ForwardHit.GetActor())
		{
			if (HitActor->ActorHasTag(LedgeTag))
			{
				bHasTag = true;
			}
		}
		if (!bHasTag && ForwardHit.GetComponent())
		{
			if (ForwardHit.GetComponent()->ComponentHasTag(LedgeTag))
			{
				bHasTag = true;
			}
		}
		if (!bHasTag)
		{
			return false;
		}
	}

	// -------------------------------------------------------------
	// STEP 2: Top Surface Downward Trace (Find Ledge Lip Height)
	// Probe past the wall surface inward onto the top ledge surface.
	// We test multiple inward depths: shallow shelf (5cm), normal hold (12cm), deep platform (20cm).
	// This prevents shallow 8-10cm ledges from being overshot into the wall behind them.
	// -------------------------------------------------------------
	FHitResult TopHit;
	bool bFoundValidTop = false;

	const float DownStartZ = ActorLoc.Z + MaxGrabHeight;
	const float DownEndZ = ActorLoc.Z + MinGrabHeight - 10.0f;

	for (float Depth : LedgeProbeDepths)
	{
		const FVector InwardOffset = -WallNormal2D * Depth;
		const FVector TopProbeXY = ForwardHit.ImpactPoint + InwardOffset;
		const FVector DownStart = FVector(TopProbeXY.X, TopProbeXY.Y, DownStartZ);
		const FVector DownEnd = FVector(TopProbeXY.X, TopProbeXY.Y, DownEndZ);

		FHitResult CurrentTopHit;
		if (World->LineTraceSingleByChannel(CurrentTopHit, DownStart, DownEnd, ECC_Visibility, QueryParams))
		{
			if (bDrawDebugTraces)
			{
				DrawDebugLine(World, DownStart, DownEnd, (CurrentTopHit.bBlockingHit) ? FColor::Cyan : FColor::Orange, false, -1.0f, 0, 1.5f);
			}

			if (CurrentTopHit.bBlockingHit && CurrentTopHit.ImpactNormal.Z >= 0.55f)
			{
				const float RelH = CurrentTopHit.ImpactPoint.Z - ActorLoc.Z;
				if (RelH >= MinGrabHeight && RelH <= MaxGrabHeight)
				{
					TopHit = CurrentTopHit;
					bFoundValidTop = true;
					if (bDrawDebugTraces)
					{
						DrawDebugSphere(World, TopHit.ImpactPoint, 5.0f, 8, FColor::Emerald, false, -1.0f, 0, 1.5f);
					}
					break;
				}
			}
		}
	}

	if (!bFoundValidTop)
	{
		return false;
	}

	const float LedgeTopZ = TopHit.ImpactPoint.Z;
	const float RelativeHeight = LedgeTopZ - ActorLoc.Z;

	// -------------------------------------------------------------
	// STEP 3: Pinpoint the Exact 3D Edge (Intersection of Wall & Top Surface)
	// Trace horizontally forward into the wall at 10cm below the top surface
	// -------------------------------------------------------------
	const float EdgeTraceZ = LedgeTopZ - 10.0f;
	const FVector EdgeTraceStart = FVector(ActorLoc.X, ActorLoc.Y, EdgeTraceZ);
	const FVector EdgeTraceEnd = EdgeTraceStart + Forward * (ForwardTraceDistance + 40.0f);

	FHitResult EdgeWallHit;
	bool bEdgeWallHit = World->LineTraceSingleByChannel(EdgeWallHit, EdgeTraceStart, EdgeTraceEnd, ECC_Visibility, QueryParams);

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

	// Construct the TRUE 3D edge point: X,Y on wall surface, Z on ledge top surface
	const FVector TrueLedgeEdge = FVector(EdgePointXY.X, EdgePointXY.Y, LedgeTopZ);

	// -------------------------------------------------------------
	// STEP 4: Wall Proximity Check
	// Character must be physically close to the wall at the moment of grab.
	// Prevents "teleportation" to the top of distant walls detected by the long-range sweep.
	// We measure the forward distance from the chest-level trace start to the wall hit point.
	// Natural grab: character is running/jumping toward the ledge and is already close (< 55cm).
	// False positive: character is 60-120cm away → visible teleport → reject.
	// -------------------------------------------------------------
	{
		const float WallForwardDist = FVector::DotProduct(ForwardHit.ImpactPoint - ForwardStart, Forward);
		if (WallForwardDist > MaxGrabWallDistance)
		{
			return false;
		}
	}

	// -------------------------------------------------------------
	// STEP 5: True Ledge Lip & Body Clearance Geometry Check (Section 11.4)
	// Eliminates false positives on continuous flat walls and modular mesh seams
	// -------------------------------------------------------------
	if (!ValidateLedgeLipAndClearance(TrueLedgeEdge, FinalWallNormal2D))
	{
		return false;
	}

	// -------------------------------------------------------------
	// STEP 6: Hand Clearance & Ledge Width Verification
	// Verify surface exists along the edge for character hands
	// -------------------------------------------------------------
	const FVector WallTangent = FVector::CrossProduct(FinalWallNormal2D, Up).GetSafeNormal();
	const FVector LeftHandEdge = TrueLedgeEdge - WallTangent * HandSpacing;
	const FVector RightHandEdge = TrueLedgeEdge + WallTangent * HandSpacing;

	const FVector LeftProbe = LeftHandEdge - FinalWallNormal2D * 8.0f;
	const FVector RightProbe = RightHandEdge - FinalWallNormal2D * 8.0f;

	FHitResult LeftDownHit;
	FHitResult RightDownHit;
	bool bLeftHit = World->LineTraceSingleByChannel(LeftDownHit, LeftProbe + Up * 15.0f, LeftProbe - Up * 15.0f, ECC_Visibility, QueryParams);
	bool bRightHit = World->LineTraceSingleByChannel(RightDownHit, RightProbe + Up * 15.0f, RightProbe - Up * 15.0f, ECC_Visibility, QueryParams);

	// If both hands hit, ensure ledge is not excessively sloped diagonally (> 25 cm diff)
	if (bLeftHit && bRightHit && LeftDownHit.bBlockingHit && RightDownHit.bBlockingHit)
	{
		if (FMath::Abs(LeftDownHit.ImpactPoint.Z - RightDownHit.ImpactPoint.Z) > 25.0f)
		{
			return false;
		}
	}
	// If only one hand hit, that's fine (narrow ledge or corner grip) as long as center top hit was valid

	if (bDrawDebugTraces)
	{
		// Draw the exact ledge lip line where hands grip
		DrawDebugLine(World, LeftHandEdge, RightHandEdge, FColor::Yellow, false, -1.0f, 0, 3.0f);
		// Draw the true edge anchor point
		DrawDebugSphere(World, TrueLedgeEdge, 6.0f, 12, FColor::Green, false, -1.0f, 0, 2.0f);
		DrawDebugString(World, TrueLedgeEdge + FVector(0, 0, 15), FString::Printf(TEXT("EDGE (Z:%.0f, Rel:%.0f)"), LedgeTopZ, RelativeHeight), nullptr, FColor::Yellow, -1.0f, true);
	}

	OutLedgeLocation = TrueLedgeEdge;
	OutWallNormal = FinalWallNormal2D;

	UE_LOG(LogThreadTales, Warning, TEXT("[Traversal] LEDGE DETECTED at Exact Edge: %s, Normal: %s, RelHeight: %.1f"),
		*OutLedgeLocation.ToString(), *OutWallNormal.ToString(), RelativeHeight);

	return true;
}

void UThreadTalesTraversalComponent::StartLedgeClimb(const FVector& LedgeLocation, const FVector& WallNormal)
{
	if (CurrentTraversalState == EThreadTalesTraversalState::LedgeClimbing)
	{
		return;
	}

	if (!CharacterOwner)
	{
		return;
	}

	// Flatten wall normal to 2D horizontal plane so edge curvature doesn't tilt or displace character
	FVector WallNormal2D = FVector(WallNormal.X, WallNormal.Y, 0.0f).GetSafeNormal();
	if (WallNormal2D.IsNearlyZero())
	{
		WallNormal2D = -CharacterOwner->GetActorForwardVector();
	}

	CachedLedgeLocation = LedgeLocation;
	CachedWallNormal = WallNormal2D;
	bIsFreehang = CheckIsFreehangLedge(LedgeLocation, WallNormal2D);

	const FRotator FaceWallRotation = (-WallNormal2D).Rotation();
	CachedHangRotation = FRotator(0.0f, FaceWallRotation.Yaw, 0.0f);
	CachedHangLocation = LedgeLocation + (WallNormal2D * HangWallOffset) - FVector(0.0f, 0.0f, HangHeightOffset);

	if (!CharacterOwner->HasAuthority())
	{
		Server_StartLedgeClimb(LedgeLocation, WallNormal2D);
	}
	else
	{
		Multicast_StartLedgeClimb(LedgeLocation, WallNormal2D);
	}

	SetTraversalState(EThreadTalesTraversalState::LedgeClimbing);

	UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
	if (MoveComp)
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement(); // MOVE_None: completely immobilizes character at ledge
	}

	CharacterOwner->SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation, false, nullptr, ETeleportType::TeleportPhysics);

	UE_LOG(LogThreadTales, Warning, TEXT("[Traversal] StartLedgeClimb: Hanging at %s (Facing Wall: %s, Freehang: %d)"),
		*CachedHangLocation.ToString(), *CachedHangRotation.ToString(), bIsFreehang);

	if (bDrawDebugTraces && GetWorld())
	{
		DrawDebugSphere(GetWorld(), LedgeLocation, 8.0f, 12, FColor::Green, false, 3.0f, 0, 2.0f);
		DrawDebugString(GetWorld(), LedgeLocation + FVector(0, 0, 15), TEXT("LEDGE"), nullptr, FColor::Green, 3.0f, true);
		DrawDebugSphere(GetWorld(), CachedHangLocation, 10.0f, 12, FColor::Cyan, false, 3.0f, 0, 2.0f);
		DrawDebugString(GetWorld(), CachedHangLocation + FVector(0, 0, 15), TEXT("HANG ANCHOR"), nullptr, FColor::Cyan, 3.0f, true);
		DrawDebugDirectionalArrow(GetWorld(), LedgeLocation, LedgeLocation + WallNormal2D * 40.0f, 15.0f, FColor::Yellow, false, 3.0f, 0, 2.5f);
	}

	UAnimMontage* MontageToPlay = (bIsFreehang && BarHangMontage) ? BarHangMontage.Get() : HangMontage.Get();
	if (MontageToPlay && CharacterOwner->GetMesh())
	{
		CharacterOwner->PlayAnimMontage(MontageToPlay);
		if (UAnimInstance* AnimInst = CharacterOwner->GetMesh()->GetAnimInstance())
		{
			AnimInst->Montage_SetNextSection(FName("Default"), FName("Default"), MontageToPlay);
		}
	}
}

void UThreadTalesTraversalComponent::Internal_StartTransition(UAnimMontage* MontageToPlay, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, float CustomDuration, EThreadTalesLedgeJumpDistanceType DistanceType)
{
	if (!CharacterOwner)
	{
		return;
	}

	FVector TargetNormal2D = FVector(TargetWallNormal.X, TargetWallNormal.Y, 0.0f).GetSafeNormal();
	if (TargetNormal2D.IsNearlyZero())
	{
		TargetNormal2D = CachedWallNormal;
	}

	const FRotator TargetFaceWall = (-TargetNormal2D).Rotation();
	const FRotator TargetHangRot = FRotator(0.0f, TargetFaceWall.Yaw, 0.0f);
	const FVector TargetHangLoc = TargetLedgeLoc + (TargetNormal2D * HangWallOffset) - FVector(0.0f, 0.0f, HangHeightOffset);

	const FVector StartLoc = CharacterOwner->GetActorLocation();
	const float ActualDist = FVector::Dist(StartLoc, TargetHangLoc);

	// Distinguish jump transitions from continuous lateral shimmy
	bIsJumpTransition = (MontageToPlay != ShimmyLeftMontage && MontageToPlay != ShimmyRightMontage);
	TransitionDistanceType = DistanceType;

	float FinalPlayRate = FMath::Max(ClimbAnimPlayRateMultiplier, 0.1f);
	if (MontageToPlay && MontageToPlay == ClimbBackJumpMontage)
	{
		FinalPlayRate = 1.35f;
	}
	float Duration = CustomDuration;
	if (Duration <= 0.0f)
	{
		if (bIsJumpTransition)
		{
			switch (DistanceType)
			{
			case EThreadTalesLedgeJumpDistanceType::Leap:
				Duration = LeapJumpDuration; // Leap full kinematic cycle
				FinalPlayRate = 1.0f;
				break;
			case EThreadTalesLedgeJumpDistanceType::Long:
				Duration = LongJumpDuration; // Long hop kinematic cycle
				FinalPlayRate = 1.0f;
				break;
			case EThreadTalesLedgeJumpDistanceType::Short:
			default:
				Duration = ShortJumpDuration; // Short hop kinematic cycle
				FinalPlayRate = 1.0f;
				break;
			}
		}
		else if (MontageToPlay)
		{
			Duration = MontageToPlay->GetPlayLength();
		}
		else
		{
			Duration = 0.8f;
		}
	}

	Duration = Duration / FinalPlayRate;

	bIsTransitioning = true;
	TransitionTimer = 0.0f;
	TransitionDuration = Duration;
	TransitionStartLocation = StartLoc;
	TransitionStartRotation = CharacterOwner->GetActorRotation();
	TransitionTargetLocation = TargetHangLoc;
	TransitionTargetRotation = TargetHangRot;
	PendingTargetLedgeLocation = TargetLedgeLoc;
	PendingTargetWallNormal = TargetNormal2D;

	if (MontageToPlay)
	{
		CharacterOwner->PlayAnimMontage(MontageToPlay, FinalPlayRate);
	}
}

bool UThreadTalesTraversalComponent::GetJumpDistanceBounds(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, float& OutMinDist, float& OutMaxDist) const
{
	switch (DistanceType)
	{
	case EThreadTalesLedgeJumpDistanceType::Short:
		switch (Direction)
		{
		case EThreadTalesLedgeJumpDirection::Up:
			OutMinDist = MinHopUpHeight;
			OutMaxDist = MaxHopUpHeight;
			return true;
		case EThreadTalesLedgeJumpDirection::Down:
			OutMinDist = MinHopDownHeight;
			OutMaxDist = MaxHopDownHeight;
			return true;
		case EThreadTalesLedgeJumpDirection::Right:
		case EThreadTalesLedgeJumpDirection::Left:
			OutMinDist = MinSideJumpDistance;
			OutMaxDist = MaxSideJumpDistance;
			return true;
		case EThreadTalesLedgeJumpDirection::UpRight:
		case EThreadTalesLedgeJumpDirection::UpLeft:
		case EThreadTalesLedgeJumpDirection::DownRight:
		case EThreadTalesLedgeJumpDirection::DownLeft:
			OutMinDist = MinDiagonalJumpDistance;
			OutMaxDist = MaxDiagonalJumpDistance;
			return true;
		default:
			break;
		}
		break;

	case EThreadTalesLedgeJumpDistanceType::Long:
		switch (Direction)
		{
		case EThreadTalesLedgeJumpDirection::Up:
			OutMinDist = MinLongHopUpHeight;
			OutMaxDist = MaxLongHopUpHeight;
			return true;
		case EThreadTalesLedgeJumpDirection::Down:
			OutMinDist = MinLongHopDownHeight;
			OutMaxDist = MaxLongHopDownHeight;
			return true;
		case EThreadTalesLedgeJumpDirection::Right:
		case EThreadTalesLedgeJumpDirection::Left:
			OutMinDist = MinLongSideJumpDistance;
			OutMaxDist = MaxLongSideJumpDistance;
			return true;
		case EThreadTalesLedgeJumpDirection::UpRight:
		case EThreadTalesLedgeJumpDirection::UpLeft:
		case EThreadTalesLedgeJumpDirection::DownRight:
		case EThreadTalesLedgeJumpDirection::DownLeft:
			OutMinDist = MinLongDiagonalJumpDistance;
			OutMaxDist = MaxLongDiagonalJumpDistance;
			return true;
		default:
			break;
		}
		break;

	case EThreadTalesLedgeJumpDistanceType::Leap:
		switch (Direction)
		{
		case EThreadTalesLedgeJumpDirection::Up:
			OutMinDist = MinLeapHopUpHeight;
			OutMaxDist = MaxLeapHopUpHeight;
			return true;
		case EThreadTalesLedgeJumpDirection::Down:
			OutMinDist = MinLeapHopDownHeight;
			OutMaxDist = MaxLeapHopDownHeight;
			return true;
		case EThreadTalesLedgeJumpDirection::Right:
		case EThreadTalesLedgeJumpDirection::Left:
			OutMinDist = MinLeapSideJumpDistance;
			OutMaxDist = MaxLeapSideJumpDistance;
			return true;
		case EThreadTalesLedgeJumpDirection::UpRight:
		case EThreadTalesLedgeJumpDirection::UpLeft:
		case EThreadTalesLedgeJumpDirection::DownRight:
		case EThreadTalesLedgeJumpDirection::DownLeft:
			OutMinDist = MinLeapDiagonalJumpDistance;
			OutMaxDist = MaxLeapDiagonalJumpDistance;
			return true;
		default:
			break;
		}
		break;

	default:
		break;
	}

	OutMinDist = 0.0f;
	OutMaxDist = 0.0f;
	return false;
}

bool UThreadTalesTraversalComponent::IsLedgeDistanceValid(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, float Distance) const
{
	float MinDist = 0.0f;
	float MaxDist = 0.0f;
	if (GetJumpDistanceBounds(Direction, DistanceType, MinDist, MaxDist))
	{
		return Distance >= MinDist && Distance <= MaxDist;
	}
	return false;
}

float UThreadTalesTraversalComponent::GetNominalJumpDistance(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const
{
	switch (DistanceType)
	{
	case EThreadTalesLedgeJumpDistanceType::Short:
		return 48.1f;
	case EThreadTalesLedgeJumpDistanceType::Long:
		return 96.2f;
	case EThreadTalesLedgeJumpDistanceType::Leap:
		return 200.0f;
	default:
		break;
	}
	return 48.1f;
}

TArray<EThreadTalesLedgeJumpDistanceType> UThreadTalesTraversalComponent::GetJumpDistancePriorities(bool bIsSprint) const
{
	TArray<EThreadTalesLedgeJumpDistanceType> Priorities;
	if (bIsSprint)
	{
		// With Sprint (Shift): search wide first, then leap across chasms, then short as fallback
		Priorities.Add(EThreadTalesLedgeJumpDistanceType::Long);
		Priorities.Add(EThreadTalesLedgeJumpDistanceType::Leap);
		Priorities.Add(EThreadTalesLedgeJumpDistanceType::Short);
	}
	else
	{
		// Normal climbing: prioritize short precise hops, then long steps, then leap as last resort
		Priorities.Add(EThreadTalesLedgeJumpDistanceType::Short);
		Priorities.Add(EThreadTalesLedgeJumpDistanceType::Long);
		Priorities.Add(EThreadTalesLedgeJumpDistanceType::Leap);
	}
	return Priorities;
}

void UThreadTalesTraversalComponent::GetJumpKinematicPhases(EThreadTalesLedgeJumpDistanceType DistanceType, float& OutAnticipationEnd, float& OutFlightEnd) const
{
	switch (DistanceType)
	{
	case EThreadTalesLedgeJumpDistanceType::Leap:
		// Leap animations (1.50s): Anticipation up to 0.17s (frame 10), flight 0.17s .. 0.67s (frame 40), landing/settle 0.67s .. 1.0
		OutAnticipationEnd = 0.17f;
		OutFlightEnd = 0.67f;
		break;

	case EThreadTalesLedgeJumpDistanceType::Long:
	case EThreadTalesLedgeJumpDistanceType::Short:
		// Standard hops and jumps: Anticipation 0..0.19, flight 0.19..JumpFlightEndRatio, settle to 1.0
		OutAnticipationEnd = 0.19f;
		OutFlightEnd = JumpFlightEndRatio;
		break;
	}
}

UAnimMontage* UThreadTalesTraversalComponent::GetJumpMontage(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const
{
	// 1. Check for Leap jump montage if requested
	if (DistanceType == EThreadTalesLedgeJumpDistanceType::Leap)
	{
		UAnimMontage* LeapMontage = nullptr;
		switch (Direction)
		{
		case EThreadTalesLedgeJumpDirection::Up:        LeapMontage = LeapHopUpMontage; break;
		case EThreadTalesLedgeJumpDirection::UpRight:   LeapMontage = LeapJumpUpRightMontage; break;
		case EThreadTalesLedgeJumpDirection::Right:     LeapMontage = LeapJumpRightMontage; break;
		case EThreadTalesLedgeJumpDirection::DownRight: LeapMontage = LeapJumpDownRightMontage; break;
		case EThreadTalesLedgeJumpDirection::Down:      LeapMontage = LeapHopDownMontage; break;
		case EThreadTalesLedgeJumpDirection::DownLeft:  LeapMontage = LeapJumpDownLeftMontage; break;
		case EThreadTalesLedgeJumpDirection::Left:      LeapMontage = LeapJumpLeftMontage; break;
		case EThreadTalesLedgeJumpDirection::UpLeft:    LeapMontage = LeapJumpUpLeftMontage; break;
		default: break;
		}

		if (LeapMontage)
		{
			return LeapMontage;
		}
		// If Leap montage is not assigned, fall back to Long montage
		DistanceType = EThreadTalesLedgeJumpDistanceType::Long;
	}

	// 2. Check for Long jump montage if requested
	if (DistanceType == EThreadTalesLedgeJumpDistanceType::Long)
	{
		UAnimMontage* LongMontage = nullptr;
		switch (Direction)
		{
		case EThreadTalesLedgeJumpDirection::Up:        LongMontage = ClimbLongHopUpMontage; break;
		case EThreadTalesLedgeJumpDirection::UpRight:   LongMontage = ClimbLongJumpUpRightMontage; break;
		case EThreadTalesLedgeJumpDirection::Right:     LongMontage = ClimbLongJumpRightMontage; break;
		case EThreadTalesLedgeJumpDirection::DownRight: LongMontage = ClimbLongJumpDownRightMontage; break;
		case EThreadTalesLedgeJumpDirection::Down:      LongMontage = ClimbLongHopDownMontage; break;
		case EThreadTalesLedgeJumpDirection::DownLeft:  LongMontage = ClimbLongJumpDownLeftMontage; break;
		case EThreadTalesLedgeJumpDirection::Left:      LongMontage = ClimbLongJumpLeftMontage; break;
		case EThreadTalesLedgeJumpDirection::UpLeft:    LongMontage = ClimbLongJumpUpLeftMontage; break;
		default: break;
		}

		if (LongMontage)
		{
			return LongMontage;
		}
		// If Long montage is not assigned, fall back to Short montage
	}

	// 3. Short jump montage with smart fallbacks
	switch (Direction)
	{
	case EThreadTalesLedgeJumpDirection::Up:
		return ClimbHopUpMontage;

	case EThreadTalesLedgeJumpDirection::UpRight:
		if (ClimbJumpUpRightMontage) return ClimbJumpUpRightMontage;
		if (ClimbJumpRightMontage) return ClimbJumpRightMontage;
		return ClimbHopUpMontage;

	case EThreadTalesLedgeJumpDirection::Right:
		return ClimbJumpRightMontage;

	case EThreadTalesLedgeJumpDirection::DownRight:
		if (ClimbJumpDownRightMontage) return ClimbJumpDownRightMontage;
		return ClimbJumpRightMontage;

	case EThreadTalesLedgeJumpDirection::Down:
		if (ClimbHopDownMontage) return ClimbHopDownMontage;
		return HangMontage;

	case EThreadTalesLedgeJumpDirection::DownLeft:
		if (ClimbJumpDownLeftMontage) return ClimbJumpDownLeftMontage;
		return ClimbJumpLeftMontage;

	case EThreadTalesLedgeJumpDirection::Left:
		return ClimbJumpLeftMontage;

	case EThreadTalesLedgeJumpDirection::UpLeft:
		if (ClimbJumpUpLeftMontage) return ClimbJumpUpLeftMontage;
		if (ClimbJumpLeftMontage) return ClimbJumpLeftMontage;
		return ClimbHopUpMontage;

	default:
		break;
	}

	return nullptr;
}

void UThreadTalesTraversalComponent::PerformLedgeJump(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	if (CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing || bIsClimbingUp || bIsTransitioning)
	{
		return;
	}

	if (HangMontage && CharacterOwner)
	{
		CharacterOwner->StopAnimMontage(HangMontage);
	}

	if (!CharacterOwner->HasAuthority())
	{
		Server_LedgeJump(Direction, DistanceType, TargetLedgeLoc, TargetWallNormal);
	}
	else
	{
		Multicast_LedgeJump(Direction, DistanceType, TargetLedgeLoc, TargetWallNormal);
	}

	UAnimMontage* Montage = GetJumpMontage(Direction, DistanceType);
	const float JumpDist = FVector::Dist(CachedLedgeLocation, TargetLedgeLoc);
	const FString DistTypeStr = (DistanceType == EThreadTalesLedgeJumpDistanceType::Short) ? TEXT("Short") :
		(DistanceType == EThreadTalesLedgeJumpDistanceType::Long ? TEXT("Long") : TEXT("Leap"));
	const FString MontageName = Montage ? Montage->GetName() : TEXT("None");

	UE_LOG(LogThreadTales, Warning, TEXT("[Traversal] PerformLedgeJump: Type=%s, Dist=%.1f cm, Montage=%s"),
		*DistTypeStr, JumpDist, *MontageName);

	if (bDrawDebugTraces && GetWorld())
	{
		DrawDebugString(GetWorld(), TargetLedgeLoc + FVector(0, 0, 30),
			FString::Printf(TEXT("JUMP: %s\nType: %s (%.1f cm)"), *MontageName, *DistTypeStr, JumpDist),
			nullptr, FColor::Yellow, 3.5f, true, 1.2f);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(8881, 3.5f, FColor::Yellow,
				FString::Printf(TEXT("[Traversal Jump] %s | %s (%.1f cm)"), *MontageName, *DistTypeStr, JumpDist));
		}
	}

	Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal, -1.0f, DistanceType);
}

bool UThreadTalesTraversalComponent::FindLedgeInDirection(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, FVector& OutLedgeLoc, FVector& OutWallNormal)
{
	if (!CharacterOwner)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CharacterOwner);
	Params.bTraceComplex = false;

	const FVector Up = FVector::UpVector;
	const FVector WallRight = FVector::CrossProduct(CachedWallNormal, Up).GetSafeNormal();
	const bool bIsLong = (DistanceType == EThreadTalesLedgeJumpDistanceType::Long);

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
			// Draw sweeping search ray & volume bounds
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
			return false; // Reject floors and steep slopes - must be vertical-ish face
		}

		const FVector WallNorm2D = FVector(WallHit.ImpactNormal.X, WallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();

		if (bDrawDebugTraces)
		{
			DrawDebugSphere(World, WallHit.ImpactPoint, 6.0f, 8, FColor::Cyan, false, 1.5f);
		}

		// Multi-Depth Probing: test multiple depth penetration offsets into the hold/wall (narrow shelf: 4cm, normal hold: 8cm, deep block: 16cm)
		FHitResult BestTopHit;
		bool bFoundValidTop = false;

		for (float Depth : DirectionalProbeDepths)
		{
			const FVector DownStart = WallHit.ImpactPoint - (WallNorm2D * Depth) + FVector(0.0f, 0.0f, TopTraceOffsetZ);
			const FVector DownEnd = WallHit.ImpactPoint - (WallNorm2D * Depth) - FVector(0.0f, 0.0f, TopTraceOffsetZ * 0.7f);

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
					break; // Found highest / closest valid grabbable top face
				}
			}
		}

		if (!bFoundValidTop)
		{
			return false;
		}

		const float LedgeTopZ = BestTopHit.ImpactPoint.Z;

		// Pinpoint outer wall face horizontally at LedgeTopZ - 6.0f
		const FVector PinStart = FVector(BestTopHit.ImpactPoint.X, BestTopHit.ImpactPoint.Y, LedgeTopZ - 6.0f) + (WallNorm2D * 40.0f);
		const FVector PinEnd = PinStart - (WallNorm2D * 80.0f);

		FHitResult EdgeWallHit;
		if (!World->LineTraceSingleByChannel(EdgeWallHit, PinStart, PinEnd, ECC_Visibility, Params))
		{
			return false;
		}

		const FVector CandidateLoc = FVector(EdgeWallHit.ImpactPoint.X, EdgeWallHit.ImpactPoint.Y, LedgeTopZ);
		const FVector CandidateNormal = FVector(EdgeWallHit.ImpactNormal.X, EdgeWallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();

		// Check strict distance range from current ledge anchor
		const float Dist = FVector::Dist(CandidateLoc, CachedLedgeLocation);
		if (Dist < MinDist || Dist > MaxDist)
		{
			if (bDrawDebugTraces)
			{
				DrawDebugSphere(World, CandidateLoc, 6.0f, 8, FColor::Red, false, 1.5f);
				DrawDebugString(World, CandidateLoc + FVector(0, 0, 15),
					FString::Printf(TEXT("OUT OF RANGE (%.0fcm not in [%.0f..%.0f])"), Dist, MinDist, MaxDist),
					nullptr, FColor::Red, 1.5f);
			}
			return false;
		}

		// Run the full lip & clearance validation (Section 11.4)
		if (!ValidateLedgeLipAndClearance(CandidateLoc, CandidateNormal))
		{
			return false;
		}

		OutLedgeLoc = CandidateLoc;
		OutWallNormal = CandidateNormal;

		if (bDrawDebugTraces)
		{
			// Exact ledge lip line & candidate sphere
			DrawDebugSphere(World, OutLedgeLoc, 12.0f, 16, FColor::Orange, false, 3.0f, 0, 2.0f);
			DrawDebugDirectionalArrow(World, OutLedgeLoc, OutLedgeLoc + OutWallNormal * 35.0f, 12.0f, FColor::Yellow, false, 3.0f, 0, 2.0f);
			DrawDebugString(World, OutLedgeLoc + FVector(0.0f, 0.0f, 22.0f),
				FString::Printf(TEXT("TARGET LEDGE (%s, %.0fcm)"), *UEnum::GetValueAsString(Direction), Dist),
				nullptr, FColor::Orange, 3.0f, true);
		}

		return true;
	};

	// -------------------------------------------------------------
	// 1. UP
	// -------------------------------------------------------------
	if (Direction == EThreadTalesLedgeJumpDirection::Up)
	{
		float MinH = 0.0f;
		float MaxH = 0.0f;
		if (!GetJumpDistanceBounds(Direction, DistanceType, MinH, MaxH))
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
		default:
			break;
		}

		// Sweep through wall in depth (+50 cm in front to -60 cm inside)
		for (float H : Heights)
		{
			const FVector Start = CachedLedgeLocation + (CachedWallNormal * 50.0f) + Up * H;
			const FVector End = Start - (CachedWallNormal * 110.0f);

			if (ProbeAndValidateLedge(Start, End, UpJumpTopTraceOffset, MinH, MaxH))
			{
				return true;
			}
		}
		return false;
	}

	// -------------------------------------------------------------
	// 2. DOWN (Hop Down)
	// -------------------------------------------------------------
	if (Direction == EThreadTalesLedgeJumpDirection::Down)
	{
		float MinH = 0.0f;
		float MaxH = 0.0f;
		if (!GetJumpDistanceBounds(Direction, DistanceType, MinH, MaxH))
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
		default:
			break;
		}

		// Also check lateral offsets (+-18 cm) in case the lower hold is not directly centered
		const float LateralOffsets[] = { 0.0f, -18.0f, 18.0f };

		// For Down hops, downward probe only needs to start DownJumpTopTraceOffset above WallHit so it doesn't collide with the upper ledge above
		const float DownTopOffset = (DistanceType == EThreadTalesLedgeJumpDistanceType::Leap) ? DownJumpLeapTopTraceOffset : DownJumpTopTraceOffset;

		for (float H : Heights)
		{
			for (float Lat : LateralOffsets)
			{
				const FVector Start = CachedLedgeLocation + (WallRight * Lat) + (CachedWallNormal * 50.0f) + Up * H;
				const FVector End = Start - (CachedWallNormal * 110.0f);

				if (ProbeAndValidateLedge(Start, End, DownTopOffset, MinH, MaxH))
				{
					return true;
				}
			}
		}
		return false;
	}

	// -------------------------------------------------------------
	// 3. LEFT & RIGHT
	// -------------------------------------------------------------
	if (Direction == EThreadTalesLedgeJumpDirection::Right || Direction == EThreadTalesLedgeJumpDirection::Left)
	{
		const float SideSign = (Direction == EThreadTalesLedgeJumpDirection::Right) ? 1.0f : -1.0f;
		const FVector SideDir = WallRight * SideSign;

		float MinD = 0.0f;
		float MaxD = 0.0f;
		if (!GetJumpDistanceBounds(Direction, DistanceType, MinD, MaxD))
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
		default:
			break;
		}

		// Vertical variations (+18 cm, 0, -18 cm) for natural rock/climbing wall height offsets
		const float VertOffsets[] = { 0.0f, 18.0f, -18.0f };

		for (float Dist : Distances)
		{
			for (float Vert : VertOffsets)
			{
				const FVector Start = CachedLedgeLocation + (SideDir * Dist) + (Up * Vert) + (CachedWallNormal * 50.0f);
				const FVector End = Start - (CachedWallNormal * 110.0f);

				if (ProbeAndValidateLedge(Start, End, LateralJumpTopTraceOffset, MinD, MaxD))
				{
					return true;
				}
			}
		}
		return false;
	}

	// -------------------------------------------------------------
	// 4. DIAGONALS (UpRight, UpLeft, DownRight, DownLeft)
	// -------------------------------------------------------------
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
		if (!GetJumpDistanceBounds(Direction, DistanceType, MinD, MaxD))
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
		default:
			break;
		}

		for (float SD : SideDists)
		{
			for (float VO : VertOffsets)
			{
				const FVector Start = CachedLedgeLocation + (SideDir * SD) + (Up * VO) + (CachedWallNormal * 50.0f);
				const FVector End = Start - (CachedWallNormal * 110.0f);
				const float DiagTopOffset = bDiagUp ? DiagonalUpJumpTopTraceOffset : DiagonalDownJumpTopTraceOffset;
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

void UThreadTalesTraversalComponent::ProcessClimbingInput(float DeltaTime)
{
	if (!CharacterOwner || !CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	if (CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing || bIsClimbingUp || bIsClimbingDown || bIsTransitioning)
	{
		return;
	}

	if (ClimbingInputDebounceTimer > 0.0f)
	{
		ClimbingInputDebounceTimer -= DeltaTime;
	}

	if (ShimmyCooldownTimer > 0.0f)
	{
		ShimmyCooldownTimer -= DeltaTime;
	}

	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	if (!PC)
	{
		return;
	}

	// Check S + Space (Back Jump / Wall Eject)
	const bool bSpacePressed = PC->IsInputKeyDown(EKeys::SpaceBar) || PC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom);
	const bool bDownPressed = PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Down);

	if (bSpacePressed && bDownPressed)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT("[Input] S + Space tick detected -> PerformBackJump()"));
		}
		ClimbingInputDebounceTimer = 0.5f;
		PerformBackJump();
		return;
	}

	// 1. Read 2D Directional Intent (X = Right/Left, Y = Up/Down)
	// SpaceBar acts as UP (duplicates / substitutes W)
	float DirX = 0.0f;
	float DirY = 0.0f;

	if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Right))
	{
		DirX += 1.0f;
	}
	if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Left))
	{
		DirX -= 1.0f;
	}
	if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Up) || bSpacePressed)
	{
		DirY += 1.0f;
	}
	if (bDownPressed)
	{
		DirY -= 1.0f;
	}

	// If no directional keys are active, nothing to do
	if (FMath::IsNearlyZero(DirX) && FMath::IsNearlyZero(DirY))
	{
		return;
	}

	// Wait for debounce cooldown before initiating a new jump/action
	if (ClimbingInputDebounceTimer > 0.0f)
	{
		return;
	}

	// Check Sprint (Shift / Gamepad Thumbstick click)
	const bool bIsSprint = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift) ||
		PC->IsInputKeyDown(EKeys::Gamepad_LeftThumbstick);

	// Resolve 8 directions
	const float Threshold = 0.35f;
	EThreadTalesLedgeJumpDirection JumpDir = EThreadTalesLedgeJumpDirection::None;

	if (DirY > Threshold && FMath::Abs(DirX) <= Threshold)
	{
		JumpDir = EThreadTalesLedgeJumpDirection::Up;
	}
	else if (DirY > Threshold && DirX > Threshold)
	{
		JumpDir = EThreadTalesLedgeJumpDirection::UpRight;
	}
	else if (FMath::Abs(DirY) <= Threshold && DirX > Threshold)
	{
		JumpDir = EThreadTalesLedgeJumpDirection::Right;
	}
	else if (DirY < -Threshold && DirX > Threshold)
	{
		JumpDir = EThreadTalesLedgeJumpDirection::DownRight;
	}
	else if (DirY < -Threshold && FMath::Abs(DirX) <= Threshold)
	{
		JumpDir = EThreadTalesLedgeJumpDirection::Down;
	}
	else if (DirY < -Threshold && DirX < -Threshold)
	{
		JumpDir = EThreadTalesLedgeJumpDirection::DownLeft;
	}
	else if (FMath::Abs(DirY) <= Threshold && DirX < -Threshold)
	{
		JumpDir = EThreadTalesLedgeJumpDirection::Left;
	}
	else if (DirY > Threshold && DirX < -Threshold)
	{
		JumpDir = EThreadTalesLedgeJumpDirection::UpLeft;
	}

	if (JumpDir == EThreadTalesLedgeJumpDirection::None)
	{
		return;
	}

	// Priority resolution using configurable GetJumpDistancePriorities()
	const TArray<EThreadTalesLedgeJumpDistanceType> Priorities = GetJumpDistancePriorities(bIsSprint);

	// Case A: Pure UP (W or Space)
	if (JumpDir == EThreadTalesLedgeJumpDirection::Up)
	{
		if (bIsSprint)
		{
			// Shift + W (со спринтом): от самого дальнего к меньшему
			// 1. Leap Hop Up (100 - 200 cm)
			// 2. Long Hop Up (58 - 98 cm)
			// 3. Short Hop Up (25 - 58 cm)
			// 4. В конце всегда - CanClimbUp / Mantle
			const EThreadTalesLedgeJumpDistanceType SprintUpPriorities[] = {
				EThreadTalesLedgeJumpDistanceType::Leap,
				EThreadTalesLedgeJumpDistanceType::Long,
				EThreadTalesLedgeJumpDistanceType::Short
			};

			for (EThreadTalesLedgeJumpDistanceType DistType : SprintUpPriorities)
			{
				FVector TargetLedgeLoc;
				FVector TargetWallNormal;
				if (FindLedgeInDirection(EThreadTalesLedgeJumpDirection::Up, DistType, TargetLedgeLoc, TargetWallNormal))
				{
					ClimbingInputDebounceTimer = 0.35f;
					PerformLedgeJump(EThreadTalesLedgeJumpDirection::Up, DistType, TargetLedgeLoc, TargetWallNormal);
					return;
				}
			}

			// В конце всегда - CanClimbUp / Mantle
			FVector TargetStandingLoc;
			if (CanClimbUp(TargetStandingLoc))
			{
				ClimbingInputDebounceTimer = 0.4f;
				ClimbUp();
				return;
			}
		}
		else
		{
			// Обычное W (без спринта):
			// 1. Short Hop Up (25 - 58 cm)
			// 2. Long Hop Up (58 - 98 cm)
			// 3. Leap Hop Up (100 - 200 cm)
			// 4. В конце всегда - CanClimbUp / Mantle
			const EThreadTalesLedgeJumpDistanceType NormalUpPriorities[] = {
				EThreadTalesLedgeJumpDistanceType::Short,
				EThreadTalesLedgeJumpDistanceType::Long,
				EThreadTalesLedgeJumpDistanceType::Leap
			};

			for (EThreadTalesLedgeJumpDistanceType DistType : NormalUpPriorities)
			{
				FVector TargetLedgeLoc;
				FVector TargetWallNormal;
				if (FindLedgeInDirection(EThreadTalesLedgeJumpDirection::Up, DistType, TargetLedgeLoc, TargetWallNormal))
				{
					ClimbingInputDebounceTimer = JumpInputDebounceDuration;
					PerformLedgeJump(EThreadTalesLedgeJumpDirection::Up, DistType, TargetLedgeLoc, TargetWallNormal);
					return;
				}
			}

			// В конце всегда - CanClimbUp / Mantle
			FVector TargetStandingLoc;
			if (CanClimbUp(TargetStandingLoc))
			{
				ClimbingInputDebounceTimer = 0.4f;
				ClimbUp();
				return;
			}
		}

		// If wall continues above or ceiling is blocked, debounce input
		ClimbingInputDebounceTimer = 0.2f;
		return;
	}

	// Case B: Pure Lateral (A or D)
	if (JumpDir == EThreadTalesLedgeJumpDirection::Right || JumpDir == EThreadTalesLedgeJumpDirection::Left)
	{
		const float SideSign = (JumpDir == EThreadTalesLedgeJumpDirection::Right) ? 1.0f : -1.0f;

		// If NOT holding sprint, check continuous shimmy first (fluid movement along pipe/beam)
		if (!bIsSprint && ShimmyCooldownTimer <= 0.0f)
		{
			FVector SideLedgeLoc;
			FVector SideWallNormal;
			bool bIsContinuous = false;
			if (FindSideLedge(SideSign, SideLedgeLoc, SideWallNormal, bIsContinuous, false) && bIsContinuous)
			{
				ClimbingInputDebounceTimer = ShimmyInputDebounceDuration;
				StartShimmy(SideSign, SideLedgeLoc, SideWallNormal);
				return;
			}
		}

		// If sprint is held OR there is a gap / discrete ledge:
		for (EThreadTalesLedgeJumpDistanceType DistType : Priorities)
		{
			FVector TargetLedgeLoc;
			FVector TargetWallNormal;
			if (FindLedgeInDirection(JumpDir, DistType, TargetLedgeLoc, TargetWallNormal))
			{
				ClimbingInputDebounceTimer = JumpInputDebounceDuration;
				PerformLedgeJump(JumpDir, DistType, TargetLedgeLoc, TargetWallNormal);
				return;
			}
		}

		// If jump didn't find a distant ledge, but continuous ledge was available, fall back to shimmy
		if (bIsSprint && ShimmyCooldownTimer <= 0.0f)
		{
			FVector SideLedgeLoc;
			FVector SideWallNormal;
			bool bIsContinuous = false;
			if (FindSideLedge(SideSign, SideLedgeLoc, SideWallNormal, bIsContinuous, false) && bIsContinuous)
			{
				ClimbingInputDebounceTimer = ShimmyInputDebounceDuration;
				StartShimmy(SideSign, SideLedgeLoc, SideWallNormal);
				return;
			}
		}
		return;
	}

	// Case C: Diagonals (UpRight, UpLeft, DownRight, DownLeft)
	if (JumpDir == EThreadTalesLedgeJumpDirection::UpRight || JumpDir == EThreadTalesLedgeJumpDirection::UpLeft ||
		JumpDir == EThreadTalesLedgeJumpDirection::DownRight || JumpDir == EThreadTalesLedgeJumpDirection::DownLeft)
	{
		for (EThreadTalesLedgeJumpDistanceType DistType : Priorities)
		{
			FVector TargetLedgeLoc;
			FVector TargetWallNormal;
			if (FindLedgeInDirection(JumpDir, DistType, TargetLedgeLoc, TargetWallNormal))
			{
				ClimbingInputDebounceTimer = JumpInputDebounceDuration;
				PerformLedgeJump(JumpDir, DistType, TargetLedgeLoc, TargetWallNormal);
				return;
			}
		}
		return;
	}

	// Case D: Pure Down (S)
	if (JumpDir == EThreadTalesLedgeJumpDirection::Down)
	{
		for (EThreadTalesLedgeJumpDistanceType DistType : Priorities)
		{
			FVector TargetLedgeLoc;
			FVector TargetWallNormal;
			if (FindLedgeInDirection(EThreadTalesLedgeJumpDirection::Down, DistType, TargetLedgeLoc, TargetWallNormal))
			{
				ClimbingInputDebounceTimer = JumpInputDebounceDuration;
				PerformLedgeJump(EThreadTalesLedgeJumpDirection::Down, DistType, TargetLedgeLoc, TargetWallNormal);
				return;
			}
		}

		// User Rule: Dropping from ledge is reserved EXCLUSIVELY for Crouch (C).
		// Pressing S alone will NOT drop the character. This ensures S + Space (Back Jump) is always responsive.
		return;
	}
}

void UThreadTalesTraversalComponent::HandleLedgeJumpInput(bool bFromSpace)
{
	if (!CharacterOwner || CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing || bIsTransitioning || bIsClimbingUp || bIsClimbingDown)
	{
		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
	{
		const bool bDownPressed = PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Down);
		if (bDownPressed)
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT("[Input] Space + S detected -> PerformBackJump()"));
			}
			ClimbingInputDebounceTimer = 0.5f;
			PerformBackJump();
			return;
		}
	}

	// Reset debounce so immediate Space press is responsive
	ClimbingInputDebounceTimer = 0.0f;
	ProcessClimbingInput(0.0f);
}

bool UThreadTalesTraversalComponent::FindUpperLedge(FVector& OutLedgeLoc, FVector& OutWallNormal)
{
	return FindLedgeInDirection(EThreadTalesLedgeJumpDirection::Up, EThreadTalesLedgeJumpDistanceType::Short, OutLedgeLoc, OutWallNormal);
}

bool UThreadTalesTraversalComponent::FindSideLedge(float DirectionSign, FVector& OutLedgeLoc, FVector& OutWallNormal, bool& bOutIsContinuous, bool bForJump)
{
	if (!CharacterOwner)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CharacterOwner);
	Params.bTraceComplex = false;

	const float Sign = (DirectionSign < 0.0f) ? -1.0f : 1.0f;
	const FVector WallRight = CharacterOwner->GetActorRightVector();
	const FVector SideDir = WallRight * Sign;

	// Phase 1: Test for Continuous Ledge (Shimmy) at ShimmyStepDistance (with half-step fallback near ledge boundary)
	if (!bForJump)
	{
		const float StepDistances[] = { ShimmyStepDistance, ShimmyStepDistance * 0.5f };

		for (float StepDist : StepDistances)
		{
			for (float InwardDepth : ShimmyProbeDepths)
			{
				const FVector StepProbe = CachedLedgeLocation + (SideDir * StepDist) - (CachedWallNormal * InwardDepth);
				const FVector TopStart = StepProbe + FVector(0.0f, 0.0f, ShimmyVerticalTraceRange);
				const FVector TopEnd = StepProbe - FVector(0.0f, 0.0f, ShimmyVerticalTraceRange);

				FHitResult ShimmyTopHit;
				if (World->LineTraceSingleByChannel(ShimmyTopHit, TopStart, TopEnd, ECC_Visibility, Params))
				{
					if (ShimmyTopHit.bBlockingHit && !ShimmyTopHit.bStartPenetrating &&
						ShimmyTopHit.ImpactNormal.Z >= 0.6f &&
						FMath::Abs(ShimmyTopHit.ImpactPoint.Z - CachedLedgeLocation.Z) <= ShimmyMaxZDifference)
					{
						const FVector WallStart = FVector(ShimmyTopHit.ImpactPoint.X, ShimmyTopHit.ImpactPoint.Y, ShimmyTopHit.ImpactPoint.Z - 8.0f) + (CachedWallNormal * 30.0f);
						const FVector WallEnd = WallStart - (CachedWallNormal * 60.0f);

						FHitResult ShimmyWallHit;
						if (World->LineTraceSingleByChannel(ShimmyWallHit, WallStart, WallEnd, ECC_Visibility, Params))
						{
							const FVector CandidateLoc = FVector(ShimmyWallHit.ImpactPoint.X, ShimmyWallHit.ImpactPoint.Y, ShimmyTopHit.ImpactPoint.Z);
							const FVector CandidateNormal = FVector(ShimmyWallHit.ImpactNormal.X, ShimmyWallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();

							if (ValidateLedgeLipAndClearance(CandidateLoc, CandidateNormal))
							{
								bOutIsContinuous = true;
								OutLedgeLoc = CandidateLoc;
								OutWallNormal = CandidateNormal;

								if (bDrawDebugTraces)
								{
									DrawDebugSphere(World, OutLedgeLoc, 10.0f, 12, FColor::Cyan, false, 2.0f);
									DrawDebugString(World, OutLedgeLoc + FVector(0.0f, 0.0f, 15.0f), TEXT("SHIMMY STEP"), nullptr, FColor::Cyan, 2.0f);
								}
								return true;
							}
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
	return FindLedgeInDirection(JumpDir, EThreadTalesLedgeJumpDistanceType::Short, OutLedgeLoc, OutWallNormal);
}

void UThreadTalesTraversalComponent::HopUp(const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	PerformLedgeJump(EThreadTalesLedgeJumpDirection::Up, EThreadTalesLedgeJumpDistanceType::Short, TargetLedgeLoc, TargetWallNormal);
}

void UThreadTalesTraversalComponent::SideJump(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	const EThreadTalesLedgeJumpDirection Dir = (DirectionSign < 0.0f) ? EThreadTalesLedgeJumpDirection::Left : EThreadTalesLedgeJumpDirection::Right;
	PerformLedgeJump(Dir, EThreadTalesLedgeJumpDistanceType::Short, TargetLedgeLoc, TargetWallNormal);
}

void UThreadTalesTraversalComponent::StartShimmy(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	if (CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing || bIsClimbingUp || bIsTransitioning)
	{
		return;
	}

	UAnimMontage* Montage = (DirectionSign < 0.0f) ? ShimmyLeftMontage : ShimmyRightMontage;

	if (!CharacterOwner->HasAuthority())
	{
		Server_StartShimmy(DirectionSign, TargetLedgeLoc, TargetWallNormal);
	}
	else
	{
		Multicast_StartShimmy(DirectionSign, TargetLedgeLoc, TargetWallNormal);
	}

	Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal);
}

bool UThreadTalesTraversalComponent::CanClimbUp(FVector& OutStandingLoc) const
{
	if (!CharacterOwner || CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(CharacterOwner);
	QueryParams.bTraceComplex = false;

	const FVector WallNormal2D = CachedWallNormal.GetSafeNormal2D();
	const FVector InwardDir = -WallNormal2D;
	const FVector Up = FVector::UpVector;

	// 1. Wall Continuity Check: Does the wall continue upwards directly above the ledge?
	// If there is a wall surface above the ledge (e.g. at Z + 30 to +160), this is just a trim/cornice, not a platform edge.
	const FVector WallCheckStart = CachedLedgeLocation + (WallNormal2D * 15.0f);
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

	// 2. Floor Presence & Walkability Check:
	// Trace downward into the platform at multiple inward depths (25cm, 35cm, 15cm, 45cm)
	// to support both narrow beams/pillars (30-40cm thick) and deep platforms/roofs.
	const float InwardDistances[] = { 25.0f, 35.0f, 15.0f, 45.0f };
	FHitResult FloorHit;
	bool bFoundFloor = false;

	for (float InwardDist : InwardDistances)
	{
		const FVector PlatformProbeXY = CachedLedgeLocation + (InwardDir * InwardDist);
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
			const FVector PlatformProbeXY = CachedLedgeLocation + (InwardDir * 30.0f);
			DrawDebugLine(World, PlatformProbeXY + Up * 60.0f, PlatformProbeXY - Up * 40.0f, FColor::Red, false, 2.0f, 0, 2.0f);
			DrawDebugString(World, PlatformProbeXY, TEXT("NO FLOOR TO STAND ON"), nullptr, FColor::Red, 2.0f, true);
		}
		return false;
	}

	// 3. Full Standing Capsule Clearance Check:
	// Verify that the character's standing capsule fits cleanly on top of this floor.
	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 86.0f;
	const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 34.0f;

	const FVector TargetStandingLoc = FVector(FloorHit.ImpactPoint.X, FloorHit.ImpactPoint.Y, FloorHit.ImpactPoint.Z + HalfHeight + 2.0f);

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

void UThreadTalesTraversalComponent::ClimbUp()
{
	if (CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing || bIsClimbingUp)
	{
		return;
	}

	if (!CharacterOwner)
	{
		return;
	}

	FVector TargetStandingLoc;
	if (!CanClimbUp(TargetStandingLoc))
	{
		UE_LOG(LogThreadTales, Log, TEXT("[Traversal] ClimbUp aborted: Standing clearance or wall continuity check failed."));
		return;
	}

	bIsClimbingUp = true;

	UAnimMontage* MontageToUse = (bIsFreehang && BarClimbOnTopMontage) ? BarClimbOnTopMontage.Get() : ClimbOnTopMontage.Get();

	if (HangMontage)
	{
		CharacterOwner->StopAnimMontage(HangMontage);
	}
	if (BarHangMontage)
	{
		CharacterOwner->StopAnimMontage(BarHangMontage);
	}

	// Switch to MOVE_Flying so Root Motion can smoothly carry character through air onto ledge
	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Flying);
	}

	UE_LOG(LogThreadTales, Warning, TEXT("[Traversal] ClimbUp triggered! Playing montage: %s (Freehang: %d)"),
		*GetNameSafe(MontageToUse), bIsFreehang);

	if (!CharacterOwner->HasAuthority())
	{
		Server_ClimbUp();
	}
	else
	{
		Multicast_PlayClimbMontage(MontageToUse);
	}

	if (MontageToUse)
	{
		const float EffectivePlayRate = FMath::Max(ClimbAnimPlayRateMultiplier, 0.1f);
		const float Duration = CharacterOwner->PlayAnimMontage(MontageToUse, EffectivePlayRate);
		if (Duration > 0.0f)
		{
			if (UAnimInstance* AnimInst = CharacterOwner->GetMesh() ? CharacterOwner->GetMesh()->GetAnimInstance() : nullptr)
			{
				FOnMontageBlendingOutStarted BlendingOutDelegate;
				BlendingOutDelegate.BindUObject(this, &UThreadTalesTraversalComponent::OnClimbMontageBlendingOut);
				AnimInst->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontageToUse);

				FOnMontageEnded EndDelegate;
				EndDelegate.BindUObject(this, &UThreadTalesTraversalComponent::OnClimbMontageEnded);
				AnimInst->Montage_SetEndDelegate(EndDelegate, MontageToUse);
			}
		}
		else
		{
			OnClimbMontageEnded(MontageToUse, false);
		}
	}
	else
	{
		// Fallback: place character on top of ledge
		const FVector TopLocation = CachedLedgeLocation + FVector(0.0f, 0.0f, 90.0f) + (-CachedWallNormal * 40.0f);
		CharacterOwner->SetActorLocation(TopLocation, false, nullptr, ETeleportType::TeleportPhysics);
		OnClimbMontageEnded(nullptr, false);
	}
}

void UThreadTalesTraversalComponent::OnClimbMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (bIsClimbingUp && CharacterOwner)
	{
		UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
		if (MoveComp)
		{
			UWorld* World = GetWorld();
			UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
			if (World && Capsule)
			{
				const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
				const FVector CapsuleLoc = CharacterOwner->GetActorLocation();

				FHitResult FloorHit;
				FCollisionQueryParams FloorParams;
				FloorParams.AddIgnoredActor(CharacterOwner);

				// Trace downward directly below the character's current location (natural root motion destination)
				const FVector FloorTraceStart = FVector(CapsuleLoc.X, CapsuleLoc.Y, CapsuleLoc.Z + 20.0f);
				const FVector FloorTraceEnd = FVector(CapsuleLoc.X, CapsuleLoc.Y, CapsuleLoc.Z - HalfHeight - 60.0f);

				if (World->LineTraceSingleByChannel(FloorHit, FloorTraceStart, FloorTraceEnd, ECC_Visibility, FloorParams))
				{
					// Keep exact XY from root motion, smoothly align feet to floor surface
					const FVector TargetFloorLoc = FVector(CapsuleLoc.X, CapsuleLoc.Y, FloorHit.ImpactPoint.Z + HalfHeight + 1.0f);
					CharacterOwner->SetActorLocation(TargetFloorLoc, false, nullptr, ETeleportType::TeleportPhysics);
				}
				else
				{
					// Fallback: only if downward trace missed, place slightly inward onto platform
					const float Radius = Capsule->GetScaledCapsuleRadius();
					const FVector FallbackXY = CachedLedgeLocation - (CachedWallNormal * (Radius + 15.0f));
					const FVector FallbackLoc = FVector(FallbackXY.X, FallbackXY.Y, CachedLedgeLocation.Z + HalfHeight + 1.0f);
					CharacterOwner->SetActorLocation(FallbackLoc, false, nullptr, ETeleportType::TeleportPhysics);
				}
			}

			// Restore walking mode firmly planted on solid floor
			MoveComp->Velocity = FVector::ZeroVector;
			MoveComp->SetMovementMode(MOVE_Walking);
		}

		// Early Locomotion Handoff: Release climbing lock immediately upon floor contact!
		// Allows player to move instantly during blend out without stuck frames.
		bIsClimbingUp = false;
		SetTraversalState(EThreadTalesTraversalState::None);
		RegrabCooldownTimer = RegrabCooldownDuration;

		UE_LOG(LogThreadTales, Warning, TEXT("[Traversal] ClimbUp BlendingOut: Early locomotion handoff to MOVE_Walking executed."));
	}
}

void UThreadTalesTraversalComponent::OnClimbMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bIsClimbingUp)
	{
		bIsClimbingUp = false;
		SetTraversalState(EThreadTalesTraversalState::None);
		RegrabCooldownTimer = RegrabCooldownDuration;
	}

	if (CharacterOwner && CurrentTraversalState == EThreadTalesTraversalState::None && !bIsClimbingDown)
	{
		if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
		{
			if (MoveComp->MovementMode != MOVE_Walking)
			{
				MoveComp->Velocity = FVector::ZeroVector;
				MoveComp->SetMovementMode(MOVE_Walking);
			}
		}
	}

	UE_LOG(LogThreadTales, Warning, TEXT("[Traversal] ClimbUp finished (Interrupted: %d)."), bInterrupted);
}

void UThreadTalesTraversalComponent::ReleaseLedge()
{
	if (CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing)
	{
		return;
	}

	if (!CharacterOwner)
	{
		return;
	}

	if (HangMontage)
	{
		CharacterOwner->StopAnimMontage(HangMontage);
	}
	if (BarHangMontage)
	{
		CharacterOwner->StopAnimMontage(BarHangMontage);
	}

	bIsTransitioning = false;
	bIsClimbingUp = false;
	bIsClimbingDown = false;
	bIsFreehang = false;

	if (!CharacterOwner->HasAuthority())
	{
		Server_ReleaseLedge();
	}
	else
	{
		Multicast_ReleaseLedge();
	}

	UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
	if (MoveComp)
	{
		MoveComp->SetMovementMode(MOVE_Falling);
	}

	SetTraversalState(EThreadTalesTraversalState::None);
	RegrabCooldownTimer = RegrabCooldownDuration;
}

void UThreadTalesTraversalComponent::SetTraversalState(EThreadTalesTraversalState NewState)
{
	if (CurrentTraversalState == NewState)
	{
		return;
	}

	const EThreadTalesTraversalState OldState = CurrentTraversalState;
	CurrentTraversalState = NewState;

	if (CharacterOwner)
	{
		if (UAbilitySystemComponent* ASC = CharacterOwner->GetAbilitySystemComponent())
		{
			if (NewState == EThreadTalesTraversalState::LedgeClimbing)
			{
				ASC->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing);
			}
			else if (OldState == EThreadTalesTraversalState::LedgeClimbing)
			{
				ASC->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing);
			}
		}
	}

	OnTraversalStateChanged.Broadcast(NewState, OldState);
}

void UThreadTalesTraversalComponent::OnRep_TraversalState(EThreadTalesTraversalState PrevState)
{
	SetTraversalState(CurrentTraversalState);

	if (CharacterOwner)
	{
		UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
		if (CurrentTraversalState == EThreadTalesTraversalState::LedgeClimbing)
		{
			if (MoveComp)
			{
				MoveComp->StopMovementImmediately();
				MoveComp->SetMovementMode(MOVE_Flying);
			}
		}
		else if (PrevState == EThreadTalesTraversalState::LedgeClimbing && !bIsClimbingUp)
		{
			if (MoveComp && MoveComp->MovementMode == MOVE_Flying)
			{
				MoveComp->SetMovementMode(MOVE_Falling);
			}
		}
	}
}

void UThreadTalesTraversalComponent::Server_StartLedgeClimb_Implementation(const FVector& LedgeLocation, const FVector& WallNormal)
{
	StartLedgeClimb(LedgeLocation, WallNormal);
}

void UThreadTalesTraversalComponent::Server_ClimbUp_Implementation()
{
	ClimbUp();
}

void UThreadTalesTraversalComponent::Server_ReleaseLedge_Implementation()
{
	ReleaseLedge();
}

void UThreadTalesTraversalComponent::Server_HopUp_Implementation(const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	Multicast_HopUp(TargetLedgeLoc, TargetWallNormal);
}

void UThreadTalesTraversalComponent::Multicast_HopUp_Implementation(const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	if (!CharacterOwner || CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	if (HangMontage)
	{
		CharacterOwner->StopAnimMontage(HangMontage);
	}

	Internal_StartTransition(ClimbHopUpMontage, TargetLedgeLoc, TargetWallNormal);
}

void UThreadTalesTraversalComponent::Server_SideJump_Implementation(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	Multicast_SideJump(DirectionSign, TargetLedgeLoc, TargetWallNormal);
}

void UThreadTalesTraversalComponent::Multicast_SideJump_Implementation(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	if (!CharacterOwner || CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	if (HangMontage)
	{
		CharacterOwner->StopAnimMontage(HangMontage);
	}

	UAnimMontage* Montage = (DirectionSign < 0.0f) ? ClimbJumpLeftMontage : ClimbJumpRightMontage;
	Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal);
}

void UThreadTalesTraversalComponent::Server_LedgeJump_Implementation(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	Multicast_LedgeJump(Direction, DistanceType, TargetLedgeLoc, TargetWallNormal);
}

void UThreadTalesTraversalComponent::Multicast_LedgeJump_Implementation(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	if (!CharacterOwner || CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	if (HangMontage)
	{
		CharacterOwner->StopAnimMontage(HangMontage);
	}

	UAnimMontage* Montage = GetJumpMontage(Direction, DistanceType);
	Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal, -1.0f, DistanceType);
}

void UThreadTalesTraversalComponent::Server_StartShimmy_Implementation(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	Multicast_StartShimmy(DirectionSign, TargetLedgeLoc, TargetWallNormal);
}

void UThreadTalesTraversalComponent::Multicast_StartShimmy_Implementation(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	if (!CharacterOwner || CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	UAnimMontage* Montage = (DirectionSign < 0.0f) ? ShimmyLeftMontage : ShimmyRightMontage;
	Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal);
}

void UThreadTalesTraversalComponent::Multicast_StartLedgeClimb_Implementation(const FVector& LedgeLocation, const FVector& WallNormal)
{
	if (!CharacterOwner || CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	FVector WallNormal2D = FVector(WallNormal.X, WallNormal.Y, 0.0f).GetSafeNormal();
	if (WallNormal2D.IsNearlyZero())
	{
		WallNormal2D = -CharacterOwner->GetActorForwardVector();
	}

	CachedLedgeLocation = LedgeLocation;
	CachedWallNormal = WallNormal2D;
	const FRotator FaceWallRotation = (-WallNormal2D).Rotation();
	CachedHangRotation = FRotator(0.0f, FaceWallRotation.Yaw, 0.0f);
	CachedHangLocation = LedgeLocation + (WallNormal2D * HangWallOffset) - FVector(0.0f, 0.0f, HangHeightOffset);

	SetTraversalState(EThreadTalesTraversalState::LedgeClimbing);

	UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
	if (MoveComp)
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}

	CharacterOwner->SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation, false, nullptr, ETeleportType::TeleportPhysics);

	bIsFreehang = CheckIsFreehangLedge(LedgeLocation, WallNormal2D);
	UAnimMontage* MontageToPlay = (bIsFreehang && BarHangMontage) ? BarHangMontage.Get() : HangMontage.Get();
	if (MontageToPlay && CharacterOwner->GetMesh())
	{
		CharacterOwner->PlayAnimMontage(MontageToPlay);
		if (UAnimInstance* AnimInst = CharacterOwner->GetMesh()->GetAnimInstance())
		{
			AnimInst->Montage_SetNextSection(FName("Default"), FName("Default"), MontageToPlay);
		}
	}
}

void UThreadTalesTraversalComponent::Multicast_PlayClimbMontage_Implementation(UAnimMontage* MontageToPlay)
{
	if (!CharacterOwner || CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	bIsClimbingUp = true;

	if (HangMontage)
	{
		CharacterOwner->StopAnimMontage(HangMontage);
	}
	if (BarHangMontage)
	{
		CharacterOwner->StopAnimMontage(BarHangMontage);
	}

	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Flying);
	}

	if (MontageToPlay)
	{
		CharacterOwner->PlayAnimMontage(MontageToPlay);
	}
}

void UThreadTalesTraversalComponent::Multicast_ReleaseLedge_Implementation()
{
	if (!CharacterOwner || CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	if (HangMontage)
	{
		CharacterOwner->StopAnimMontage(HangMontage);
	}
	if (BarHangMontage)
	{
		CharacterOwner->StopAnimMontage(BarHangMontage);
	}

	bIsTransitioning = false;
	bIsClimbingUp = false;
	bIsClimbingDown = false;
	bIsFreehang = false;

	UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
	if (MoveComp)
	{
		MoveComp->SetMovementMode(MOVE_Falling);
	}

	SetTraversalState(EThreadTalesTraversalState::None);
	RegrabCooldownTimer = RegrabCooldownDuration;
}

bool UThreadTalesTraversalComponent::TryClimbDown()
{
	if (!CharacterOwner || CurrentTraversalState != EThreadTalesTraversalState::None || bIsTransitioning || bIsClimbingUp || bIsClimbingDown)
	{
		return false;
	}

	UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
	if (!MoveComp || !MoveComp->IsMovingOnGround())
	{
		return false;
	}

	FVector LedgeLoc = FVector::ZeroVector;
	FVector WallNormal = FVector::ZeroVector;
	bool bFreehang = false;

	if (FindEdgeForClimbDown(LedgeLoc, WallNormal, bFreehang))
	{
		StartClimbDown(LedgeLoc, WallNormal, bFreehang);
		return true;
	}

	return false;
}

bool UThreadTalesTraversalComponent::FindEdgeForClimbDown(FVector& OutLedgeLoc, FVector& OutWallNormal, bool& bOutIsFreehang) const
{
	if (!CharacterOwner)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.0f;
	const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 35.0f;

	const FVector ActorLoc = CharacterOwner->GetActorLocation();
	const float FeetZ = ActorLoc.Z - HalfHeight;
	const FVector Forward = CharacterOwner->GetActorForwardVector();

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(CharacterOwner);
	QueryParams.bTraceComplex = false;

	// Test forward direction first, then backward direction
	const FVector ProbeDirections[] = { Forward, -Forward };

	for (const FVector& ProbeDir : ProbeDirections)
	{
		// 1. Verify there is a drop-off in front of or behind the character
		const FVector FloorCheckStart = ActorLoc + (ProbeDir * (Radius + 25.0f)) + FVector(0.0f, 0.0f, 15.0f);
		const FVector FloorCheckEnd = FloorCheckStart - FVector(0.0f, 0.0f, 120.0f);

		FHitResult FloorHit;
		const bool bHitFloor = World->LineTraceSingleByChannel(FloorHit, FloorCheckStart, FloorCheckEnd, ECC_Visibility, QueryParams);

		// If solid walkable floor exists at roughly the same elevation, no ledge drop-off here
		if (bHitFloor && FloorHit.bBlockingHit && (FloorHit.ImpactPoint.Z >= FeetZ - 20.0f))
		{
			continue;
		}

		// 2. Drop-off detected! Trace inward horizontally below floor level to locate vertical wall face
		const FVector WallTraceStart = ActorLoc + (ProbeDir * (Radius + ClimbDownTraceDistance)) - FVector(0.0f, 0.0f, HalfHeight + 15.0f);
		const FVector WallTraceEnd = ActorLoc - FVector(0.0f, 0.0f, HalfHeight + 15.0f);

		FHitResult WallHit;
		const bool bHitWall = World->LineTraceSingleByChannel(WallHit, WallTraceStart, WallTraceEnd, ECC_Visibility, QueryParams);

		FVector WallFacePoint;
		FVector WallNormal2D;

		if (bHitWall && WallHit.bBlockingHit)
		{
			WallFacePoint = WallHit.ImpactPoint;
			WallNormal2D = FVector(WallHit.ImpactNormal.X, WallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();
		}
		else
		{
			// Thin railing/beam or freehang edge
			WallFacePoint = ActorLoc + (ProbeDir * (Radius + 20.0f));
			WallNormal2D = -ProbeDir;
		}

		if (WallNormal2D.IsNearlyZero())
		{
			WallNormal2D = -ProbeDir;
		}

		// 3. Pinpoint exact top surface height of the ledge lip
		const FVector TopProbeStart = WallFacePoint - (WallNormal2D * 8.0f) + FVector(0.0f, 0.0f, 40.0f);
		const FVector TopProbeEnd = TopProbeStart - FVector(0.0f, 0.0f, 60.0f);

		FHitResult TopHit;
		if (World->LineTraceSingleByChannel(TopHit, TopProbeStart, TopProbeEnd, ECC_Visibility, QueryParams))
		{
			if (TopHit.bBlockingHit && TopHit.ImpactNormal.Z >= 0.55f)
			{
				const FVector TrueEdge = FVector(WallFacePoint.X, WallFacePoint.Y, TopHit.ImpactPoint.Z);

				bOutIsFreehang = CheckIsFreehangLedge(TrueEdge, WallNormal2D);

				if (const_cast<UThreadTalesTraversalComponent*>(this)->ValidateLedgeLipAndClearance(TrueEdge, WallNormal2D))
				{
					OutLedgeLoc = TrueEdge;
					OutWallNormal = WallNormal2D;

					if (bDrawDebugTraces)
					{
						DrawDebugSphere(World, TrueEdge, 8.0f, 12, FColor::Orange, false, 3.0f);
						DrawDebugString(World, TrueEdge + FVector(0.0f, 0.0f, 20.0f), TEXT("CLIMB DOWN EDGE"), nullptr, FColor::Orange, 3.0f);
						DrawDebugDirectionalArrow(World, TrueEdge, TrueEdge + WallNormal2D * 40.0f, 15.0f, FColor::Orange, false, 3.0f);
					}

					return true;
				}
			}
		}
	}

	return false;
}

void UThreadTalesTraversalComponent::StartClimbDown(const FVector& LedgeLoc, const FVector& WallNormal, bool bFreehang)
{
	if (!CharacterOwner)
	{
		return;
	}

	FVector WallNormal2D = FVector(WallNormal.X, WallNormal.Y, 0.0f).GetSafeNormal();
	if (WallNormal2D.IsNearlyZero())
	{
		WallNormal2D = -CharacterOwner->GetActorForwardVector();
	}

	bIsFreehang = bFreehang;
	bIsClimbingDown = true;
	CachedLedgeLocation = LedgeLoc;
	CachedWallNormal = WallNormal2D;

	const FRotator FaceWallRotation = (-WallNormal2D).Rotation();
	CachedHangRotation = FRotator(0.0f, FaceWallRotation.Yaw, 0.0f);
	CachedHangLocation = LedgeLoc + (WallNormal2D * HangWallOffset) - FVector(0.0f, 0.0f, HangHeightOffset);

	SetTraversalState(EThreadTalesTraversalState::LedgeClimbing);

	UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
	if (MoveComp)
	{
		MoveComp->StopMovementImmediately();
		MoveComp->SetMovementMode(MOVE_Flying);
	}

	if (!CharacterOwner->HasAuthority())
	{
		Server_ClimbDown(LedgeLoc, WallNormal2D, bFreehang);
	}
	else
	{
		Multicast_ClimbDown(LedgeLoc, WallNormal2D, bFreehang);
	}

	UAnimMontage* MontageToPlay = (bFreehang && BarClimbDownMontage) ? BarClimbDownMontage.Get() : ClimbDownMontage.Get();
	UE_LOG(LogThreadTales, Warning, TEXT("[Traversal] StartClimbDown triggered! Playing montage: %s (Freehang: %d)"),
		*GetNameSafe(MontageToPlay), bFreehang);

	if (MontageToPlay)
	{
		const float EffectivePlayRate = FMath::Max(ClimbAnimPlayRateMultiplier, 0.1f);
		const float Duration = CharacterOwner->PlayAnimMontage(MontageToPlay, EffectivePlayRate);
		if (Duration > 0.0f)
		{
			if (UAnimInstance* AnimInst = CharacterOwner->GetMesh() ? CharacterOwner->GetMesh()->GetAnimInstance() : nullptr)
			{
				FOnMontageBlendingOutStarted BlendingOutDelegate;
				BlendingOutDelegate.BindUObject(this, &UThreadTalesTraversalComponent::OnClimbDownMontageBlendingOut);
				AnimInst->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontageToPlay);

				FOnMontageEnded EndDelegate;
				EndDelegate.BindUObject(this, &UThreadTalesTraversalComponent::OnClimbDownMontageEnded);
				AnimInst->Montage_SetEndDelegate(EndDelegate, MontageToPlay);
			}
		}
		else
		{
			OnClimbDownMontageEnded(MontageToPlay, false);
		}
	}
	else
	{
		CharacterOwner->SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation, false, nullptr, ETeleportType::TeleportPhysics);
		OnClimbDownMontageEnded(nullptr, false);
	}
}

void UThreadTalesTraversalComponent::OnClimbDownMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	UE_LOG(LogThreadTales, Log, TEXT("[Traversal] ClimbDown BlendingOut started. Root motion placement completing smoothly."));
}

void UThreadTalesTraversalComponent::OnClimbDownMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bIsClimbingDown)
	{
		bIsClimbingDown = false;
	}

	if (CharacterOwner && CurrentTraversalState == EThreadTalesTraversalState::LedgeClimbing)
	{
		UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
		if (MoveComp)
		{
			MoveComp->StopMovementImmediately();
			MoveComp->DisableMovement();
		}

		CharacterOwner->SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation, false, nullptr, ETeleportType::TeleportPhysics);

		UAnimMontage* ActiveHangMont = (bIsFreehang && BarHangMontage) ? BarHangMontage.Get() : HangMontage.Get();
		if (ActiveHangMont && CharacterOwner->GetMesh())
		{
			CharacterOwner->PlayAnimMontage(ActiveHangMont);
			if (UAnimInstance* AnimInst = CharacterOwner->GetMesh()->GetAnimInstance())
			{
				AnimInst->Montage_SetNextSection(FName("Default"), FName("Default"), ActiveHangMont);
			}
		}

		UE_LOG(LogThreadTales, Warning, TEXT("[Traversal] ClimbDown Montage Ended: Smoothly settled into hang state (Freehang: %d)."), bIsFreehang);
	}
}

void UThreadTalesTraversalComponent::PerformBackJump()
{
	if (!CharacterOwner || CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing || bIsTransitioning || bIsClimbingUp || bIsClimbingDown)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("[BackJump] S + Space triggered -> Probing behind character..."));
	}

	// 1. Probe behind character (+CachedWallNormal) for opposing ledge
	const FVector BackDir = CachedWallNormal;
	const FVector Up = FVector::UpVector;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(CharacterOwner);
	QueryParams.bTraceComplex = false;

	bool bFoundOpposingLedge = false;
	FVector OpposingLedgeLoc = FVector::ZeroVector;
	FVector OpposingWallNormal = FVector::ZeroVector;

	const float MaxBackJumpDistance = 450.0f;
	const float VertOffsets[] = { -30.0f, -60.0f, 0.0f, -90.0f };

	for (float Vert : VertOffsets)
	{
		// Offset 5cm out from the current wall in BackDir to guarantee we never start inside the current wall's collision
		const FVector WallProbeStart = CachedLedgeLocation + (BackDir * 5.0f) + (Up * Vert);
		const FVector WallProbeEnd = WallProbeStart + (BackDir * (MaxBackJumpDistance + 50.0f));

		FHitResult OpposingWallHit;
		const bool bHitWall = World->LineTraceSingleByChannel(OpposingWallHit, WallProbeStart, WallProbeEnd, ECC_Visibility, QueryParams);

		if (bDrawDebugTraces)
		{
			DrawDebugLine(World, WallProbeStart, WallProbeEnd, bHitWall ? FColor::Green : FColor::Red, false, 3.5f, 0, 2.0f);
		}

		if (bHitWall && OpposingWallHit.bBlockingHit)
		{
			if (bDrawDebugTraces)
			{
				DrawDebugPoint(World, OpposingWallHit.ImpactPoint, 14.0f, FColor::Green, false, 3.5f);
			}

			const FVector OppNormal2D = FVector(OpposingWallHit.ImpactNormal.X, OpposingWallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();
			if (FVector::DotProduct(OppNormal2D, -BackDir) >= 0.45f)
			{
				const float HitDist = FVector::Dist2D(OpposingWallHit.ImpactPoint, CachedLedgeLocation);
				if (HitDist >= 70.0f && HitDist <= MaxBackJumpDistance)
				{
					if (GEngine)
					{
						GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, FString::Printf(TEXT("[BackJump] Opposing Wall Hit! Dist: %.1f"), HitDist));
					}

					// Probe downward from above to find the exact top surface
					// Test multiple probe depths (10, 18, 5, 25 cm) to support both narrow beams/pillars and thick walls
					bool bHitTop = false;
					FHitResult OpposingTopHit;
					const float TestDepths[] = { 10.0f, 18.0f, 5.0f, 25.0f };

					for (float Depth : TestDepths)
					{
						const FVector TopProbeStart = FVector(
							OpposingWallHit.ImpactPoint.X - OppNormal2D.X * Depth,
							OpposingWallHit.ImpactPoint.Y - OppNormal2D.Y * Depth,
							CachedLedgeLocation.Z + 150.0f
						);
						const FVector TopProbeEnd = FVector(TopProbeStart.X, TopProbeStart.Y, CachedLedgeLocation.Z - 180.0f);

						if (World->LineTraceSingleByChannel(OpposingTopHit, TopProbeStart, TopProbeEnd, ECC_Visibility, QueryParams))
						{
							if (OpposingTopHit.bBlockingHit && OpposingTopHit.ImpactNormal.Z >= 0.55f)
							{
								bHitTop = true;
								if (bDrawDebugTraces)
								{
									DrawDebugLine(World, TopProbeStart, OpposingTopHit.ImpactPoint, FColor::Cyan, false, 3.5f, 0, 2.0f);
									DrawDebugPoint(World, OpposingTopHit.ImpactPoint, 10.0f, FColor::Cyan, false, 3.5f);
								}
								break;
							}
						}
					}

					if (bHitTop)
					{
						const float LedgeTopZ = OpposingTopHit.ImpactPoint.Z;

						// Pinpoint edge lip at 10cm below top by tracing strictly around opposing wall face
						const FVector EdgeProbeStart = FVector(
							OpposingWallHit.ImpactPoint.X + OppNormal2D.X * 30.0f,
							OpposingWallHit.ImpactPoint.Y + OppNormal2D.Y * 30.0f,
							LedgeTopZ - 10.0f
						);
						const FVector EdgeProbeEnd = FVector(
							OpposingWallHit.ImpactPoint.X - OppNormal2D.X * 30.0f,
							OpposingWallHit.ImpactPoint.Y - OppNormal2D.Y * 30.0f,
							LedgeTopZ - 10.0f
						);

						FHitResult EdgeHit;
						FVector EdgeXY = OpposingWallHit.ImpactPoint;
						if (World->LineTraceSingleByChannel(EdgeHit, EdgeProbeStart, EdgeProbeEnd, ECC_Visibility, QueryParams) && EdgeHit.bBlockingHit)
						{
							EdgeXY = EdgeHit.ImpactPoint;
							if (bDrawDebugTraces)
							{
								DrawDebugLine(World, EdgeProbeStart, EdgeHit.ImpactPoint, FColor::Yellow, false, 3.5f, 0, 2.0f);
							}
						}

						const FVector CandidateOpposingLedge = FVector(EdgeXY.X, EdgeXY.Y, LedgeTopZ);

						if (bDrawDebugTraces)
						{
							DrawDebugSphere(World, CandidateOpposingLedge, 14.0f, 12, FColor::Magenta, false, 3.5f);
						}

						if (ValidateLedgeLipAndClearance(CandidateOpposingLedge, OppNormal2D))
						{
							bFoundOpposingLedge = true;
							OpposingLedgeLoc = CandidateOpposingLedge;
							OpposingWallNormal = OppNormal2D;
							break;
						}
						else
						{
							if (GEngine)
							{
								GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("[BackJump] ValidateLedgeLip failed on opposing ledge"));
							}
						}
					}
					else
					{
						if (GEngine)
						{
							GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange, TEXT("[BackJump] Opposing top hit NOT found (no flat top surface)"));
						}
					}
				}
			}
		}
		if (bFoundOpposingLedge)
		{
			break;
		}
	}

	if (bFoundOpposingLedge)
	{
		const float BackJumpDist = FVector::Dist2D(OpposingLedgeLoc, CachedLedgeLocation);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Green, FString::Printf(TEXT("[BackJump] OPPOSING LEDGE FOUND! Dist: %.1f cm -> Leaping!"), BackJumpDist));
		}
		UE_LOG(LogThreadTales, Warning, TEXT("[Traversal] Back Jump: Opposing ledge detected at %s (Dist: %.1f)! Leaping across..."), *OpposingLedgeLoc.ToString(), BackJumpDist);
		UAnimMontage* LeapMontage = ClimbBackJumpMontage ? ClimbBackJumpMontage.Get() : LeapHopDownMontage.Get();
		const float BackJumpDuration = LeapMontage ? (LeapMontage->GetPlayLength() / 1.35f) : 1.5f;
		Internal_StartTransition(LeapMontage, OpposingLedgeLoc, OpposingWallNormal, BackJumpDuration, EThreadTalesLedgeJumpDistanceType::Leap);
		return;
	}

	// 2. No opposing ledge -> execute Wall Eject!
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Orange, TEXT("[BackJump] No opposing ledge found -> WALL EJECT kickoff!"));
	}
	UE_LOG(LogThreadTales, Warning, TEXT("[Traversal] Back Jump: No opposing ledge. Executing Wall Eject kickoff!"));
	WallEject();
}

void UThreadTalesTraversalComponent::WallEject()
{
	if (!CharacterOwner || CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing)
	{
		return;
	}

	if (HangMontage)
	{
		CharacterOwner->StopAnimMontage(HangMontage);
	}
	if (BarHangMontage)
	{
		CharacterOwner->StopAnimMontage(BarHangMontage);
	}

	bIsTransitioning = false;
	bIsClimbingUp = false;
	bIsClimbingDown = false;
	bIsFreehang = false;

	if (!CharacterOwner->HasAuthority())
	{
		Server_WallEject();
	}
	else
	{
		Multicast_WallEject();
	}

	UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
	if (MoveComp)
	{
		MoveComp->SetMovementMode(MOVE_Falling);
		const FVector EjectVelocity = (CachedWallNormal * WallEjectHorizontalSpeed) + (FVector::UpVector * WallEjectVerticalSpeed);
		MoveComp->Velocity = EjectVelocity;
	}

	SetTraversalState(EThreadTalesTraversalState::None);
	RegrabCooldownTimer = 0.55f;
}

void UThreadTalesTraversalComponent::Server_ClimbDown_Implementation(const FVector& LedgeLocation, const FVector& WallNormal, bool bFreehang)
{
	Multicast_ClimbDown(LedgeLocation, WallNormal, bFreehang);
}

void UThreadTalesTraversalComponent::Multicast_ClimbDown_Implementation(const FVector& LedgeLocation, const FVector& WallNormal, bool bFreehang)
{
	if (!CharacterOwner || CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	bIsFreehang = bFreehang;
	bIsClimbingDown = true;
	CachedLedgeLocation = LedgeLocation;
	CachedWallNormal = WallNormal;

	const FRotator FaceWallRotation = (-WallNormal).Rotation();
	CachedHangRotation = FRotator(0.0f, FaceWallRotation.Yaw, 0.0f);
	CachedHangLocation = LedgeLocation + (WallNormal * HangWallOffset) - FVector(0.0f, 0.0f, HangHeightOffset);

	SetTraversalState(EThreadTalesTraversalState::LedgeClimbing);

	UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
	if (MoveComp)
	{
		MoveComp->StopMovementImmediately();
		MoveComp->SetMovementMode(MOVE_Flying);
	}

	UAnimMontage* MontageToPlay = (bFreehang && BarClimbDownMontage) ? BarClimbDownMontage.Get() : ClimbDownMontage.Get();
	if (MontageToPlay)
	{
		CharacterOwner->PlayAnimMontage(MontageToPlay, ClimbAnimPlayRateMultiplier);
	}
}

void UThreadTalesTraversalComponent::Server_WallEject_Implementation()
{
	Multicast_WallEject();
}

void UThreadTalesTraversalComponent::Multicast_WallEject_Implementation()
{
	if (!CharacterOwner || CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	if (HangMontage)
	{
		CharacterOwner->StopAnimMontage(HangMontage);
	}
	if (BarHangMontage)
	{
		CharacterOwner->StopAnimMontage(BarHangMontage);
	}

	bIsTransitioning = false;
	bIsClimbingUp = false;
	bIsClimbingDown = false;
	bIsFreehang = false;

	UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
	if (MoveComp)
	{
		MoveComp->SetMovementMode(MOVE_Falling);
		const FVector EjectVelocity = (CachedWallNormal * WallEjectHorizontalSpeed) + (FVector::UpVector * WallEjectVerticalSpeed);
		MoveComp->Velocity = EjectVelocity;
	}

	SetTraversalState(EThreadTalesTraversalState::None);
	RegrabCooldownTimer = 0.55f;
}
