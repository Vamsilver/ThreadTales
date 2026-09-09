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

	RegrabCooldownDuration = 0.5f;
	RegrabCooldownTimer = 0.0f;
	bIsClimbingUp = false;

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

	// While hanging on ledge (and not yet in climb-up mantle), hard-anchor character to hang location
	if (CurrentTraversalState == EThreadTalesTraversalState::LedgeClimbing && !bIsClimbingUp)
	{
		UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
		if (MoveComp)
		{
			MoveComp->Velocity = FVector::ZeroVector;
		}
		CharacterOwner->SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation, false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	// Auto-detect ledges when airborne and descending or near jump apex (Velocity.Z <= MaxGrabVelocityZ)
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
	// STEP 4: Hand Clearance & Ledge Width Verification
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

void UThreadTalesTraversalComponent::OnClimbMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	bIsClimbingUp = false;

	UE_LOG(LogThreadTales, Warning, TEXT("[Traversal] ClimbUp finished (Interrupted: %d). Restoring MOVE_Walking."), bInterrupted);

	if (CharacterOwner)
	{
		UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
		if (MoveComp)
		{
			MoveComp->SetMovementMode(MOVE_Walking);
		}
	}

	SetTraversalState(EThreadTalesTraversalState::None);
	RegrabCooldownTimer = RegrabCooldownDuration;
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

	UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
	if (MoveComp)
	{
		MoveComp->SetMovementMode(MOVE_Falling);
	}

	SetTraversalState(EThreadTalesTraversalState::None);
	RegrabCooldownTimer = RegrabCooldownDuration;
}
