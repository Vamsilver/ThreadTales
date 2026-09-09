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
	MaxGrabHeight = 135.0f;
	HandSpacing = 22.0f;
	MaxApproachAngle = 65.0f;
	MaxGrabVelocityZ = 350.0f;

	HangWallOffset = 24.0f;
	HangHeightOffset = 73.0f;

	MinHopUpHeight = 35.0f;
	MaxHopUpHeight = 85.0f;
	MinSideJumpDistance = 50.0f;
	MaxSideJumpDistance = 120.0f;
	ShimmyStepDistance = 45.0f;
	ShimmyCooldownDuration = 0.65f;

	RegrabCooldownDuration = 1.5f;
	RegrabCooldownTimer = 0.0f;
	bIsClimbingUp = false;

	bIsTransitioning = false;
	TransitionTimer = 0.0f;
	TransitionDuration = 0.0f;
	TransitionStartLocation = FVector::ZeroVector;
	TransitionStartRotation = FRotator::ZeroRotator;
	TransitionTargetLocation = FVector::ZeroVector;
	TransitionTargetRotation = FRotator::ZeroRotator;
	PendingTargetLedgeLocation = FVector::ZeroVector;
	PendingTargetWallNormal = FVector::ZeroVector;
	ShimmyCooldownTimer = 0.0f;

	CachedLedgeLocation = FVector::ZeroVector;
	CachedWallNormal = FVector::ZeroVector;
	CachedHangLocation = FVector::ZeroVector;
	CachedHangRotation = FRotator::ZeroRotator;
}

void UThreadTalesTraversalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UThreadTalesTraversalComponent, CurrentTraversalState);
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
		const float Alpha = FMath::Clamp(TransitionTimer / FMath::Max(TransitionDuration, 0.01f), 0.0f, 1.0f);
		const float SmoothAlpha = FMath::SmoothStep(0.0f, 1.0f, Alpha);

		const FVector NewLoc = FMath::Lerp(TransitionStartLocation, TransitionTargetLocation, SmoothAlpha);
		const FRotator NewRot = FMath::Lerp(TransitionStartRotation, TransitionTargetRotation, SmoothAlpha);

		CharacterOwner->SetActorLocationAndRotation(NewLoc, NewRot, false, nullptr, ETeleportType::TeleportPhysics);

		if (Alpha >= 1.0f)
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
				if (HangMontage)
				{
					CharacterOwner->PlayAnimMontage(HangMontage);
				}
			}
		}
		return;
	}

	// 2. While hanging on ledge (not transitioning, not climbing up), check continuous shimmy and hard-anchor
	if (CurrentTraversalState == EThreadTalesTraversalState::LedgeClimbing && !bIsClimbingUp)
	{
		if (ShimmyCooldownTimer > 0.0f)
		{
			ShimmyCooldownTimer -= DeltaTime;
		}
		else if (CharacterOwner->IsLocallyControlled())
		{
			// Check lateral movement input for continuous shimmy along ledge
			float DirectionSign = 0.0f;
			if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
			{
				if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Right))
				{
					DirectionSign = 1.0f;
				}
				else if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Left))
				{
					DirectionSign = -1.0f;
				}
			}

			if (DirectionSign == 0.0f)
			{
				FVector InputVec = CharacterOwner->GetPendingMovementInputVector();
				if (InputVec.IsNearlyZero())
				{
					InputVec = CharacterOwner->GetLastMovementInputVector();
				}

				if (!InputVec.IsNearlyZero())
				{
					const float RightDot = FVector::DotProduct(InputVec.GetSafeNormal(), CharacterOwner->GetActorRightVector());
					if (RightDot > 0.3f)
					{
						DirectionSign = 1.0f;
					}
					else if (RightDot < -0.3f)
					{
						DirectionSign = -1.0f;
					}
				}
			}

			if (DirectionSign != 0.0f)
			{
				FVector SideLedgeLoc;
				FVector SideWallNormal;
				bool bIsContinuous = false;
				if (FindSideLedge(DirectionSign, SideLedgeLoc, SideWallNormal, bIsContinuous, false) && bIsContinuous)
				{
					StartShimmy(DirectionSign, SideLedgeLoc, SideWallNormal);
					return;
				}
			}
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
	// We require at least a small downward velocity so the character is past the peak of the jump,
	// preventing false-positive grabs at the top of a jump against a flat wall.
	if (CurrentTraversalState == EThreadTalesTraversalState::None && bAutoGrabLedge && RegrabCooldownTimer <= 0.0f)
	{
		UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
		if (MoveComp && MoveComp->IsFalling() && MoveComp->Velocity.Z <= 50.0f)
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
	// Probe past the wall surface inward onto the top ledge surface
	// -------------------------------------------------------------
	const FVector InwardOffset = -WallNormal2D * 12.0f;
	const FVector TopProbeXY = ForwardHit.ImpactPoint + InwardOffset;

	const float DownStartZ = ActorLoc.Z + MaxGrabHeight;
	const float DownEndZ = ActorLoc.Z + MinGrabHeight - 10.0f;

	const FVector DownStart = FVector(TopProbeXY.X, TopProbeXY.Y, DownStartZ);
	const FVector DownEnd = FVector(TopProbeXY.X, TopProbeXY.Y, DownEndZ);

	FHitResult TopHit;
	bool bTopHit = World->LineTraceSingleByChannel(TopHit, DownStart, DownEnd, ECC_Visibility, QueryParams);

	if (bDrawDebugTraces)
	{
		DrawDebugLine(World, DownStart, DownEnd, (bTopHit && TopHit.bBlockingHit) ? FColor::Cyan : FColor::Orange, false, -1.0f, 0, 1.5f);
		if (bTopHit && TopHit.bBlockingHit)
		{
			DrawDebugSphere(World, TopHit.ImpactPoint, 5.0f, 8, FColor::Emerald, false, -1.0f, 0, 1.5f);
		}
	}

	if (!bTopHit || !TopHit.bBlockingHit)
	{
		return false;
	}

	// Top surface must be relatively horizontal (walkable/grabbable)
	if (TopHit.ImpactNormal.Z < 0.6f)
	{
		return false;
	}

	const float LedgeTopZ = TopHit.ImpactPoint.Z;
	const float RelativeHeight = LedgeTopZ - ActorLoc.Z;
	if (RelativeHeight < MinGrabHeight || RelativeHeight > MaxGrabHeight)
	{
		return false;
	}

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
		if (WallForwardDist > 55.0f)
		{
			return false;
		}
	}

	// -------------------------------------------------------------
	// STEP 5: Hand Clearance & Ledge Width Verification
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

	UE_LOG(LogThreadTales, Warning, TEXT("[Traversal] StartLedgeClimb: Hanging at %s (Facing Wall: %s)"),
		*CachedHangLocation.ToString(), *CachedHangRotation.ToString());

	if (bDrawDebugTraces && GetWorld())
	{
		DrawDebugSphere(GetWorld(), LedgeLocation, 8.0f, 12, FColor::Green, false, 3.0f, 0, 2.0f);
		DrawDebugString(GetWorld(), LedgeLocation + FVector(0, 0, 15), TEXT("LEDGE"), nullptr, FColor::Green, 3.0f, true);
		DrawDebugSphere(GetWorld(), CachedHangLocation, 10.0f, 12, FColor::Cyan, false, 3.0f, 0, 2.0f);
		DrawDebugString(GetWorld(), CachedHangLocation + FVector(0, 0, 15), TEXT("HANG ANCHOR"), nullptr, FColor::Cyan, 3.0f, true);
		DrawDebugDirectionalArrow(GetWorld(), LedgeLocation, LedgeLocation + WallNormal2D * 40.0f, 15.0f, FColor::Yellow, false, 3.0f, 0, 2.5f);
	}

	if (HangMontage && CharacterOwner->GetMesh() && CharacterOwner->GetMesh()->GetAnimInstance())
	{
		CharacterOwner->PlayAnimMontage(HangMontage);
	}
}

void UThreadTalesTraversalComponent::Internal_StartTransition(UAnimMontage* MontageToPlay, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, float CustomDuration)
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

	float Duration = CustomDuration;
	if (Duration <= 0.0f && MontageToPlay)
	{
		Duration = MontageToPlay->GetPlayLength();
	}
	if (Duration <= 0.0f)
	{
		Duration = 0.8f;
	}

	bIsTransitioning = true;
	TransitionTimer = 0.0f;
	TransitionDuration = Duration;
	TransitionStartLocation = CharacterOwner->GetActorLocation();
	TransitionStartRotation = CharacterOwner->GetActorRotation();
	TransitionTargetLocation = TargetHangLoc;
	TransitionTargetRotation = TargetHangRot;
	PendingTargetLedgeLocation = TargetLedgeLoc;
	PendingTargetWallNormal = TargetNormal2D;

	if (MontageToPlay)
	{
		CharacterOwner->PlayAnimMontage(MontageToPlay);
	}
}

void UThreadTalesTraversalComponent::HandleLedgeJumpInput()
{
	if (CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing || bIsClimbingUp || bIsTransitioning)
	{
		return;
	}

	if (!CharacterOwner)
	{
		return;
	}

	// 1. Check if lateral input is active (A / D)
	float DirectionSign = 0.0f;
	if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
	{
		if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Right))
		{
			DirectionSign = 1.0f;
		}
		else if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Left))
		{
			DirectionSign = -1.0f;
		}
	}

	if (DirectionSign == 0.0f)
	{
		FVector InputVec = CharacterOwner->GetPendingMovementInputVector();
		if (InputVec.IsNearlyZero())
		{
			InputVec = CharacterOwner->GetLastMovementInputVector();
		}

		if (!InputVec.IsNearlyZero())
		{
			const float RightDot = FVector::DotProduct(InputVec.GetSafeNormal(), CharacterOwner->GetActorRightVector());
			if (RightDot > 0.3f)
			{
				DirectionSign = 1.0f;
			}
			else if (RightDot < -0.3f)
			{
				DirectionSign = -1.0f;
			}
		}
	}

	if (DirectionSign != 0.0f)
	{
		// Lateral jump requested (A or D + Space)
		FVector SideLedgeLoc;
		FVector SideWallNormal;
		bool bIsContinuous = false;
		if (FindSideLedge(DirectionSign, SideLedgeLoc, SideWallNormal, bIsContinuous, true /* bForJump */))
		{
			SideJump(DirectionSign, SideLedgeLoc, SideWallNormal);
			return;
		}

		// When player explicitly holds lateral input, DO NOT mantle up onto roof!
		UE_LOG(LogThreadTales, Warning, TEXT("[Traversal] Side jump requested (Dir=%.1f) but no ledge found in range"), DirectionSign);
		return;
	}

	// 2. Pure Space (no lateral input held): Probe for upper ledge
	FVector UpperLedgeLoc;
	FVector UpperWallNormal;
	if (FindUpperLedge(UpperLedgeLoc, UpperWallNormal))
	{
		HopUp(UpperLedgeLoc, UpperWallNormal);
		return;
	}

	// 3. No upper ledge -> mantle onto floor (ClimbUp)
	ClimbUp();
}

bool UThreadTalesTraversalComponent::FindUpperLedge(FVector& OutLedgeLoc, FVector& OutWallNormal)
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

	// 1. Probe for upper wall surface at multiple height intervals calibrated for short hop (35..85 cm)
	bool bFoundUpperWall = false;
	FHitResult UpperWallHit;
	const float TestHeights[] = { 45.0f, 60.0f, 75.0f };

	for (float H : TestHeights)
	{
		const FVector Start = CachedLedgeLocation + (CachedWallNormal * 45.0f) + FVector(0.0f, 0.0f, H);
		const FVector End = CachedLedgeLocation - (CachedWallNormal * 50.0f) + FVector(0.0f, 0.0f, H);

		if (World->LineTraceSingleByChannel(UpperWallHit, Start, End, ECC_Visibility, Params))
		{
			// Verify it's a nearly vertical wall surface (Normal.Z near 0)
			if (FMath::Abs(UpperWallHit.ImpactNormal.Z) < 0.35f)
			{
				bFoundUpperWall = true;
				break;
			}
		}
	}

	if (!bFoundUpperWall)
	{
		return false;
	}

	const FVector WallNormal2D = FVector(UpperWallHit.ImpactNormal.X, UpperWallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();

	// 2. Trace down inside the upper wall to find the exact top surface
	const FVector DownTraceStart = UpperWallHit.ImpactPoint - (WallNormal2D * 12.0f) + FVector(0.0f, 0.0f, 60.0f);
	const FVector DownTraceEnd = UpperWallHit.ImpactPoint - (WallNormal2D * 12.0f) - FVector(0.0f, 0.0f, 60.0f);

	FHitResult TopHit;
	if (!World->LineTraceSingleByChannel(TopHit, DownTraceStart, DownTraceEnd, ECC_Visibility, Params))
	{
		return false;
	}

	if (TopHit.ImpactNormal.Z < 0.6f)
	{
		return false;
	}

	const float UpperLedgeTopZ = TopHit.ImpactPoint.Z;
	const float DeltaZ = UpperLedgeTopZ - CachedLedgeLocation.Z;
	if (DeltaZ < MinHopUpHeight || DeltaZ > MaxHopUpHeight)
	{
		return false;
	}

	// 3. Pinpoint horizontal retrace at UpperLedgeTopZ - 8.0f to find exact outer wall face
	const FVector PinStart = FVector(TopHit.ImpactPoint.X, TopHit.ImpactPoint.Y, UpperLedgeTopZ - 8.0f) + (WallNormal2D * 35.0f);
	const FVector PinEnd = PinStart - (WallNormal2D * 70.0f);

	FHitResult EdgeWallHit;
	if (!World->LineTraceSingleByChannel(EdgeWallHit, PinStart, PinEnd, ECC_Visibility, Params))
	{
		return false;
	}

	const FVector CandidateLedgeLoc = FVector(EdgeWallHit.ImpactPoint.X, EdgeWallHit.ImpactPoint.Y, UpperLedgeTopZ);
	const FVector CandidateWallNormal = FVector(EdgeWallHit.ImpactNormal.X, EdgeWallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();

	// 4. Air clearance / drop-off check:
	// In front of the upper ledge, there MUST be open air space.
	// If tracing down 15cm in front of the ledge hits solid geometry near the ledge height,
	// then it's a flat surface/floor, NOT a grabbable ledge edge!
	const FVector AirCheckStart = CandidateLedgeLoc + (CandidateWallNormal * 15.0f) + FVector(0.0f, 0.0f, 5.0f);
	const FVector AirCheckEnd = AirCheckStart - FVector(0.0f, 0.0f, 25.0f);

	FHitResult AirHit;
	if (World->LineTraceSingleByChannel(AirHit, AirCheckStart, AirCheckEnd, ECC_Visibility, Params))
	{
		return false;
	}

	OutLedgeLoc = CandidateLedgeLoc;
	OutWallNormal = CandidateWallNormal;

	if (bDrawDebugTraces)
	{
		DrawDebugSphere(World, OutLedgeLoc, 12.0f, 12, FColor::Green, false, 2.5f);
		DrawDebugString(World, OutLedgeLoc + FVector(0.0f, 0.0f, 20.0f),
			FString::Printf(TEXT("UPPER LEDGE (+%.0f)"), DeltaZ), nullptr, FColor::Green, 2.5f);
	}

	return true;
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

	const float Sign = (DirectionSign < 0.0f) ? -1.0f : 1.0f;
	const FVector WallRight = CharacterOwner->GetActorRightVector();
	const FVector SideDir = WallRight * Sign;

	// Phase 1: Test for Continuous Ledge (Shimmy) at ShimmyStepDistance
	if (!bForJump)
	{
		const FVector StepProbe = CachedLedgeLocation + (SideDir * ShimmyStepDistance) - (CachedWallNormal * 12.0f);
		const FVector TopStart = StepProbe + FVector(0.0f, 0.0f, 30.0f);
		const FVector TopEnd = StepProbe - FVector(0.0f, 0.0f, 30.0f);

		FHitResult ShimmyTopHit;
		if (World->LineTraceSingleByChannel(ShimmyTopHit, TopStart, TopEnd, ECC_Visibility, Params))
		{
			if (ShimmyTopHit.ImpactNormal.Z >= 0.6f && FMath::Abs(ShimmyTopHit.ImpactPoint.Z - CachedLedgeLocation.Z) <= 20.0f)
			{
				const FVector WallStart = FVector(ShimmyTopHit.ImpactPoint.X, ShimmyTopHit.ImpactPoint.Y, ShimmyTopHit.ImpactPoint.Z - 8.0f) + (CachedWallNormal * 30.0f);
				const FVector WallEnd = WallStart - (CachedWallNormal * 60.0f);

				FHitResult ShimmyWallHit;
				if (World->LineTraceSingleByChannel(ShimmyWallHit, WallStart, WallEnd, ECC_Visibility, Params))
				{
					const FVector CandidateLoc = FVector(ShimmyWallHit.ImpactPoint.X, ShimmyWallHit.ImpactPoint.Y, ShimmyTopHit.ImpactPoint.Z);
					const FVector CandidateNormal = FVector(ShimmyWallHit.ImpactNormal.X, ShimmyWallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();

					// Air clearance check in front of candidate edge
					const FVector AirCheckStart = CandidateLoc + (CandidateNormal * 15.0f) + FVector(0.0f, 0.0f, 5.0f);
					const FVector AirCheckEnd = AirCheckStart - FVector(0.0f, 0.0f, 25.0f);
					FHitResult AirHit;
					if (!World->LineTraceSingleByChannel(AirHit, AirCheckStart, AirCheckEnd, ECC_Visibility, Params))
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
		return false;
	}

	// Phase 2: Test for Adjacent Ledge (Side Jump across gap OR along long ledge)
	const float JumpDistances[] = { 85.0f, 105.0f, 65.0f };
	for (float Dist : JumpDistances)
	{
		if (Dist < MinSideJumpDistance || Dist > MaxSideJumpDistance)
		{
			continue;
		}

		const FVector WallProbeStart = CachedLedgeLocation + (SideDir * Dist) + (CachedWallNormal * 45.0f);
		const FVector WallProbeEnd = WallProbeStart - (CachedWallNormal * 80.0f);

		FHitResult SideWallHit;
		if (World->LineTraceSingleByChannel(SideWallHit, WallProbeStart, WallProbeEnd, ECC_Visibility, Params))
		{
			if (FMath::Abs(SideWallHit.ImpactNormal.Z) < 0.35f)
			{
				const FVector Normal2D = FVector(SideWallHit.ImpactNormal.X, SideWallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();
				const FVector TopStart = SideWallHit.ImpactPoint - (Normal2D * 12.0f) + FVector(0.0f, 0.0f, 40.0f);
				const FVector TopEnd = SideWallHit.ImpactPoint - (Normal2D * 12.0f) - FVector(0.0f, 0.0f, 80.0f);

				FHitResult SideTopHit;
				if (World->LineTraceSingleByChannel(SideTopHit, TopStart, TopEnd, ECC_Visibility, Params))
				{
					if (SideTopHit.ImpactNormal.Z >= 0.6f && FMath::Abs(SideTopHit.ImpactPoint.Z - CachedLedgeLocation.Z) <= 40.0f)
					{
						const FVector EdgeStart = FVector(SideTopHit.ImpactPoint.X, SideTopHit.ImpactPoint.Y, SideTopHit.ImpactPoint.Z - 8.0f) + (Normal2D * 35.0f);
						const FVector EdgeEnd = EdgeStart - (Normal2D * 70.0f);

						FHitResult SideEdgeHit;
						if (World->LineTraceSingleByChannel(SideEdgeHit, EdgeStart, EdgeEnd, ECC_Visibility, Params))
						{
							const FVector CandidateLoc = FVector(SideEdgeHit.ImpactPoint.X, SideEdgeHit.ImpactPoint.Y, SideTopHit.ImpactPoint.Z);
							const FVector CandidateNormal = FVector(SideEdgeHit.ImpactNormal.X, SideEdgeHit.ImpactNormal.Y, 0.0f).GetSafeNormal();

							// Air clearance check in front of candidate edge
							const FVector AirCheckStart = CandidateLoc + (CandidateNormal * 15.0f) + FVector(0.0f, 0.0f, 5.0f);
							const FVector AirCheckEnd = AirCheckStart - FVector(0.0f, 0.0f, 25.0f);
							FHitResult AirHit;
							if (!World->LineTraceSingleByChannel(AirHit, AirCheckStart, AirCheckEnd, ECC_Visibility, Params))
							{
								bOutIsContinuous = false;
								OutLedgeLoc = CandidateLoc;
								OutWallNormal = CandidateNormal;

								if (bDrawDebugTraces)
								{
									DrawDebugSphere(World, OutLedgeLoc, 12.0f, 12, FColor::Orange, false, 2.0f);
									DrawDebugString(World, OutLedgeLoc + FVector(0.0f, 0.0f, 20.0f),
										FString::Printf(TEXT("SIDE JUMP (%.0fcm)"), Dist), nullptr, FColor::Orange, 2.0f);
								}
								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}

void UThreadTalesTraversalComponent::HopUp(const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
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
		Server_HopUp(TargetLedgeLoc, TargetWallNormal);
	}
	else
	{
		Multicast_HopUp(TargetLedgeLoc, TargetWallNormal);
	}

	Internal_StartTransition(ClimbHopUpMontage, TargetLedgeLoc, TargetWallNormal);
}

void UThreadTalesTraversalComponent::SideJump(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	if (CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing || bIsClimbingUp || bIsTransitioning)
	{
		return;
	}

	if (HangMontage && CharacterOwner)
	{
		CharacterOwner->StopAnimMontage(HangMontage);
	}

	UAnimMontage* Montage = (DirectionSign < 0.0f) ? ClimbJumpLeftMontage : ClimbJumpRightMontage;

	if (!CharacterOwner->HasAuthority())
	{
		Server_SideJump(DirectionSign, TargetLedgeLoc, TargetWallNormal);
	}
	else
	{
		Multicast_SideJump(DirectionSign, TargetLedgeLoc, TargetWallNormal);
	}

	Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal);
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

	bIsClimbingUp = true;

	if (HangMontage)
	{
		CharacterOwner->StopAnimMontage(HangMontage);
	}

	// Switch to MOVE_Flying so Root Motion can smoothly carry character through air onto ledge
	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Flying);
	}

	UE_LOG(LogThreadTales, Warning, TEXT("[Traversal] ClimbUp triggered! Playing montage: %s"),
		*GetNameSafe(ClimbOnTopMontage));

	if (!CharacterOwner->HasAuthority())
	{
		Server_ClimbUp();
	}
	else
	{
		Multicast_PlayClimbMontage(ClimbOnTopMontage);
	}

	if (ClimbOnTopMontage)
	{
		const float Duration = CharacterOwner->PlayAnimMontage(ClimbOnTopMontage);
		if (Duration > 0.0f)
		{
			if (UAnimInstance* AnimInst = CharacterOwner->GetMesh() ? CharacterOwner->GetMesh()->GetAnimInstance() : nullptr)
			{
				FOnMontageBlendingOutStarted BlendingOutDelegate;
				BlendingOutDelegate.BindUObject(this, &UThreadTalesTraversalComponent::OnClimbMontageBlendingOut);
				AnimInst->Montage_SetBlendingOutDelegate(BlendingOutDelegate, ClimbOnTopMontage);

				FOnMontageEnded EndDelegate;
				EndDelegate.BindUObject(this, &UThreadTalesTraversalComponent::OnClimbMontageEnded);
				AnimInst->Montage_SetEndDelegate(EndDelegate, ClimbOnTopMontage);
			}
		}
		else
		{
			OnClimbMontageEnded(ClimbOnTopMontage, false);
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
	}
}

void UThreadTalesTraversalComponent::OnClimbMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	bIsClimbingUp = false;

	if (CharacterOwner)
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

	SetTraversalState(EThreadTalesTraversalState::None);
	RegrabCooldownTimer = RegrabCooldownDuration;

	UE_LOG(LogThreadTales, Warning, TEXT("[Traversal] ClimbUp finished (Interrupted: %d). State reset to None."), bInterrupted);
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

	bIsTransitioning = false;
	bIsClimbingUp = false;

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

	if (HangMontage)
	{
		CharacterOwner->PlayAnimMontage(HangMontage);
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

	bIsTransitioning = false;
	bIsClimbingUp = false;

	UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
	if (MoveComp)
	{
		MoveComp->SetMovementMode(MOVE_Falling);
	}

	SetTraversalState(EThreadTalesTraversalState::None);
	RegrabCooldownTimer = RegrabCooldownDuration;
}
