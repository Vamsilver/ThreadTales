// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ThreadTalesTargetLockComponent.h"
#include "Combat/ThreadTalesTargetableInterface.h"
#include "Characters/ThreadTalesBaseCharacter.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"
#include "ThreadTales.h"

UThreadTalesTargetLockComponent::UThreadTalesTargetLockComponent()
{
	PrimaryComponentTick.bCanEverTick = true; // [TICK-ALWAYS-OK]
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

	LockSearchRadius = 2500.0f;
	BreakDistance = 3200.0f;
	MaxOcclusionTime = 1.5f;
	CameraInterpSpeed = 8.0f;
	MinPitchAngle = -35.0f;
	MaxPitchAngle = 20.0f;
	AngleWeight = 0.6f;
	DistanceWeight = 0.4f;
	TargetSwitchCooldown = 0.25f;
	bDrawDebug = false;

	OcclusionTimer = 0.0f;
	LastToggleTime = 0.0;
	LastSwitchTime = 0.0;
}

void UThreadTalesTargetLockComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UThreadTalesTargetLockComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (HasAnyFlags(RF_ClassDefaultObject) || (GetOwner() && GetOwner()->HasAnyFlags(RF_ClassDefaultObject)))
	{
		return;
	}

	if (!CurrentTarget.IsValid())
	{
		return;
	}

	AActor* Target = CurrentTarget.Get();
	AActor* OwnerActor = GetOwner();

	// 0. Validate Combat Stance / Tag and Owner State
	if (!CanLockOn())
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[TargetLock] %s: Lost combat stance or InCombat tag — unlocking."), *GetNameSafe(OwnerActor));
		Unlock();
		return;
	}

	// 1. Validate Target Validity
	if (!IsValidTarget(Target))
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[TargetLock] %s: Target became invalid or dead — unlocking."), *GetNameSafe(OwnerActor));
		Unlock();
		return;
	}

	// 2. Validate Distance Break Threshold
	const float Dist = FVector::Dist(OwnerActor->GetActorLocation(), Target->GetActorLocation());
	if (Dist > BreakDistance)
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[TargetLock] %s: Distance %.1f exceeded BreakDistance %.1f — unlocking."),
			*GetNameSafe(OwnerActor), Dist, BreakDistance);
		Unlock();
		return;
	}

	// 3. Validate Line of Sight / Geometry Occlusion
	if (!HasLineOfSightToTarget(Target))
	{
		OcclusionTimer += DeltaTime;
		if (OcclusionTimer >= MaxOcclusionTime)
		{
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[TargetLock] %s: Target occluded for %.2fs — breaking lock."),
				*GetNameSafe(OwnerActor), OcclusionTimer);
			Unlock();
			return;
		}
	}
	else
	{
		OcclusionTimer = 0.0f;
	}

	// 4. Update Camera Tracking
	UpdateCameraTracking(DeltaTime);

	// 5. Ensure Strafe locomotion mode is preserved while target lock is active
	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(OwnerActor))
	{
		if (!TTChar->IsClimbing())
		{
			if (UCharacterMovementComponent* MoveComp = TTChar->GetCharacterMovement())
			{
				if (MoveComp->bOrientRotationToMovement || !MoveComp->bUseControllerDesiredRotation)
				{
					SetCharacterStrafeMode(true);
				}
			}
		}
	}
}

bool UThreadTalesTargetLockComponent::CanLockOn() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return false;
	}

	const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetOwner());
	if (!TTChar || TTChar->HasAnyFlags(RF_ClassDefaultObject) || !TTChar->IsAlive() || TTChar->IsClimbing())
	{
		return false;
	}

	const bool bHasInCombatTag = TTChar->GetAbilitySystemComponent() &&
		TTChar->GetAbilitySystemComponent()->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_InCombat);

	return bHasInCombatTag || TTChar->IsInCombatStance();
}

void UThreadTalesTargetLockComponent::ToggleTargetLock()
{
	if (HasAnyFlags(RF_ClassDefaultObject) || (GetOwner() && GetOwner()->HasAnyFlags(RF_ClassDefaultObject)))
	{
		return;
	}

	const double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastToggleTime < 0.25)
	{
		return;
	}
	LastToggleTime = CurrentTime;

	if (IsTargetLocked())
	{
		Unlock();
	}
	else
	{
		if (!CanLockOn())
		{
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[TargetLock] %s: Cannot lock on — character is not in combat stance (missing InCombat tag)."), *GetNameSafe(GetOwner()));
			return;
		}

		AActor* BestTarget = FindBestTarget();
		if (BestTarget)
		{
			LockOn(BestTarget);
		}
		else
		{
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[TargetLock] %s: No valid target candidates found in front of camera."), *GetNameSafe(GetOwner()));
		}
	}
}

bool UThreadTalesTargetLockComponent::LockOn(AActor* NewTarget)
{
	if (!CanLockOn() || !IsValidTarget(NewTarget))
	{
		return false;
	}

	if (CurrentTarget.Get() == NewTarget)
	{
		return true;
	}

	// Disengage previous target if any
	if (CurrentTarget.IsValid())
	{
		AActor* OldTarget = CurrentTarget.Get();
		if (OldTarget->GetClass()->ImplementsInterface(UThreadTalesTargetableInterface::StaticClass()))
		{
			IThreadTalesTargetableInterface::Execute_OnTargetUnlocked(OldTarget, GetOwner());
		}
	}

	CurrentTarget = NewTarget;
	OcclusionTimer = 0.0f;

	// Notify new target
	if (NewTarget->GetClass()->ImplementsInterface(UThreadTalesTargetableInterface::StaticClass()))
	{
		IThreadTalesTargetableInterface::Execute_OnTargetLocked(NewTarget, GetOwner());
	}

	// Set Character Strafe Locomotion Mode
	SetCharacterStrafeMode(true);

	// Grant GAS Tag to Owner
	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetOwner()))
	{
		if (UAbilitySystemComponent* ASC = TTChar->GetAbilitySystemComponent())
		{
			ASC->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_TargetLocked);
		}
	}

	OnTargetLockChanged.Broadcast(NewTarget);

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[TargetLock] %s: Locked onto target '%s'."), *GetNameSafe(GetOwner()), *GetNameSafe(NewTarget));
	return true;
}

void UThreadTalesTargetLockComponent::Unlock()
{
	if (!CurrentTarget.IsValid())
	{
		return;
	}

	AActor* OldTarget = CurrentTarget.Get();
	CurrentTarget = nullptr;
	OcclusionTimer = 0.0f;

	// Notify old target
	if (OldTarget && OldTarget->GetClass()->ImplementsInterface(UThreadTalesTargetableInterface::StaticClass()))
	{
		IThreadTalesTargetableInterface::Execute_OnTargetUnlocked(OldTarget, GetOwner());
	}

	// Restore normal locomotion
	SetCharacterStrafeMode(false);

	// Remove GAS Tag from Owner
	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetOwner()))
	{
		if (UAbilitySystemComponent* ASC = TTChar->GetAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_TargetLocked);
		}
	}

	OnTargetLockChanged.Broadcast(nullptr);

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[TargetLock] %s: Unlocked from target '%s'."), *GetNameSafe(GetOwner()), *GetNameSafe(OldTarget));
}

void UThreadTalesTargetLockComponent::SwitchTarget(bool bRight)
{
	if (!IsTargetLocked() || !CanLockOn())
	{
		return;
	}

	const double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastSwitchTime < TargetSwitchCooldown)
	{
		return;
	}

	APlayerController* PC = GetPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	TArray<AActor*> Candidates;
	GetTargetCandidates(Candidates);
	if (Candidates.Num() <= 1)
	{
		return;
	}

	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FVector CurrentTargetLoc = GetTargetLocation();
	const FRotator CurrentTargetRot = (CurrentTargetLoc - CamLoc).Rotation();

	AActor* BestCandidate = nullptr;
	float BestScore = MAX_FLT;

	// Wrap-around fallback: leftmost candidate (min DeltaYaw) or rightmost candidate (max DeltaYaw)
	AActor* WrapCandidateLeft = nullptr;
	float MinDeltaYaw = MAX_FLT;

	AActor* WrapCandidateRight = nullptr;
	float MaxDeltaYaw = -MAX_FLT;

	for (AActor* Candidate : Candidates)
	{
		if (Candidate == CurrentTarget.Get() || !HasLineOfSightToTarget(Candidate))
		{
			continue;
		}

		FVector CandidateLoc = Candidate->GetActorLocation();
		if (Candidate->GetClass()->ImplementsInterface(UThreadTalesTargetableInterface::StaticClass()))
		{
			CandidateLoc = IThreadTalesTargetableInterface::Execute_GetTargetLockLocation(Candidate, GetOwner());
		}

		const float Dist = FVector::Dist(CamLoc, CandidateLoc);
		if (Dist > LockSearchRadius)
		{
			continue;
		}

		const FRotator CandidateRot = (CandidateLoc - CamLoc).Rotation();
		const float DeltaYaw = FRotator::NormalizeAxis(CandidateRot.Yaw - CurrentTargetRot.Yaw);

		if (DeltaYaw < MinDeltaYaw)
		{
			MinDeltaYaw = DeltaYaw;
			WrapCandidateLeft = Candidate;
		}
		if (DeltaYaw > MaxDeltaYaw)
		{
			MaxDeltaYaw = DeltaYaw;
			WrapCandidateRight = Candidate;
		}

		const bool bIsMatch = bRight ? (DeltaYaw > 3.0f) : (DeltaYaw < -3.0f);
		if (bIsMatch)
		{
			const float AngleScore = bRight ? DeltaYaw : -DeltaYaw;
			const float Score = AngleScore + (Dist / LockSearchRadius) * 20.0f;
			if (Score < BestScore)
			{
				BestScore = Score;
				BestCandidate = Candidate;
			}
		}
	}

	// If no target was found in the scroll direction, wrap around from the opposite end
	if (!BestCandidate)
	{
		BestCandidate = bRight ? WrapCandidateLeft : WrapCandidateRight;
	}

	if (BestCandidate)
	{
		LastSwitchTime = CurrentTime;
		LockOn(BestCandidate);
	}
}

FVector UThreadTalesTargetLockComponent::GetTargetLocation() const
{
	if (!CurrentTarget.IsValid())
	{
		return FVector::ZeroVector;
	}

	AActor* Target = CurrentTarget.Get();
	if (Target->GetClass()->ImplementsInterface(UThreadTalesTargetableInterface::StaticClass()))
	{
		return IThreadTalesTargetableInterface::Execute_GetTargetLockLocation(Target, GetOwner());
	}

	// Fallback to Actor center of mass
	return Target->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
}

AActor* UThreadTalesTargetLockComponent::FindBestTarget() const
{
	APlayerController* PC = GetPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return nullptr;
	}

	TArray<AActor*> Candidates;
	GetTargetCandidates(Candidates);

	if (Candidates.Num() == 0)
	{
		return nullptr;
	}

	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FVector CamForward = PC->PlayerCameraManager->GetCameraRotation().Vector();

	AActor* BestTarget = nullptr;
	float BestScore = MAX_FLT;

	for (AActor* Candidate : Candidates)
	{
		FVector CandidateLoc = Candidate->GetActorLocation();
		if (Candidate->GetClass()->ImplementsInterface(UThreadTalesTargetableInterface::StaticClass()))
		{
			CandidateLoc = IThreadTalesTargetableInterface::Execute_GetTargetLockLocation(Candidate, GetOwner());
		}

		const FVector DirToTarget = (CandidateLoc - CamLoc).GetSafeNormal();
		const float Dot = FVector::DotProduct(CamForward, DirToTarget);

		// Must be in front of the camera (cone of ~70 degrees half-angle: Dot > 0.34)
		if (Dot < 0.34f)
		{
			continue;
		}

		if (!HasLineOfSightToTarget(Candidate))
		{
			continue;
		}

		const float Dist = FVector::Dist(CamLoc, CandidateLoc);
		const float AnglePenalty = (1.0f - Dot) * AngleWeight;
		const float DistancePenalty = (Dist / LockSearchRadius) * DistanceWeight;
		const float Score = AnglePenalty + DistancePenalty;

		if (Score < BestScore)
		{
			BestScore = Score;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

void UThreadTalesTargetLockComponent::GetTargetCandidates(TArray<AActor*>& OutCandidates) const
{
	OutCandidates.Reset();
	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();

	if (!World || !OwnerActor)
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(LockSearchRadius);
	FCollisionQueryParams QueryParams(TEXT("TargetLockSearch"), false, OwnerActor);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	World->OverlapMultiByObjectType(
		Overlaps,
		OwnerActor->GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		SphereShape,
		QueryParams
	);

	TSet<AActor*> UniqueActors;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* OverlapActor = Overlap.GetActor();
		if (OverlapActor && OverlapActor != OwnerActor && !UniqueActors.Contains(OverlapActor))
		{
			if (IsValidTarget(OverlapActor))
			{
				UniqueActors.Add(OverlapActor);
				OutCandidates.Add(OverlapActor);
			}
		}
	}
}

bool UThreadTalesTargetLockComponent::IsValidTarget(const AActor* TargetActor) const
{
	if (!IsValid(TargetActor) || TargetActor == GetOwner())
	{
		return false;
	}

	// Interface check
	if (TargetActor->GetClass()->ImplementsInterface(UThreadTalesTargetableInterface::StaticClass()))
	{
		return IThreadTalesTargetableInterface::Execute_CanBeTargeted(TargetActor, GetOwner());
	}

	// Character fallback
	if (const AThreadTalesBaseCharacter* TTChar = Cast<AThreadTalesBaseCharacter>(TargetActor))
	{
		return TTChar->IsAlive();
	}

	return false;
}

bool UThreadTalesTargetLockComponent::HasLineOfSightToTarget(const AActor* TargetActor) const
{
	UWorld* World = GetWorld();
	APlayerController* PC = GetPlayerController();
	if (!World || !PC || !PC->PlayerCameraManager || !IsValid(TargetActor))
	{
		return false;
	}

	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	FVector TargetLoc = TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);

	if (TargetActor->GetClass()->ImplementsInterface(UThreadTalesTargetableInterface::StaticClass()))
	{
		TargetLoc = IThreadTalesTargetableInterface::Execute_GetTargetLockLocation(TargetActor, GetOwner());
	}

	FCollisionQueryParams QueryParams(TEXT("TargetLockLOS"), false, GetOwner());
	QueryParams.AddIgnoredActor(TargetActor);

	TArray<AActor*> AttachedToOwner;
	GetOwner()->GetAttachedActors(AttachedToOwner);
	for (AActor* Attached : AttachedToOwner)
	{
		QueryParams.AddIgnoredActor(Attached);
	}

	TArray<AActor*> AttachedToTarget;
	TargetActor->GetAttachedActors(AttachedToTarget);
	for (AActor* Attached : AttachedToTarget)
	{
		QueryParams.AddIgnoredActor(Attached);
	}

	FHitResult HitResult;
	const bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		CamLoc,
		TargetLoc,
		ECC_Visibility,
		QueryParams
	);

	if (bDrawDebug)
	{
		DrawDebugLine(World, CamLoc, TargetLoc, bHit ? FColor::Red : FColor::Green, false, -1.0f, 0, 1.5f);
	}

	// If trace didn't hit any geometry, line of sight is clear
	return !bHit;
}

void UThreadTalesTargetLockComponent::UpdateCameraTracking(float DeltaTime)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = GetPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	const FVector TargetLoc = GetTargetLocation();
	const FVector CharacterLoc = OwnerPawn->GetActorLocation();
	const FVector EyeLoc = CharacterLoc + FVector(0.0f, 0.0f, 60.0f);

	// 1. Calculate true Yaw directly from Character center to Target center (eliminates over-the-shoulder offset error)
	const FVector DirToTarget = TargetLoc - CharacterLoc;
	const float TargetYaw = DirToTarget.Rotation().Yaw;

	// 2. Calculate Pitch from eye level to target with safe bounds clamping
	float TargetPitch = (TargetLoc - EyeLoc).Rotation().Pitch;
	TargetPitch = FMath::Clamp(TargetPitch, MinPitchAngle, MaxPitchAngle);

	const FRotator DesiredRot(TargetPitch, TargetYaw, 0.0f);

	// 3. Smoothly interpolate Controller Rotation for Camera
	const FRotator CurrentRot = PC->GetControlRotation();
	const FRotator NewRot = FMath::RInterpTo(CurrentRot, DesiredRot, DeltaTime, CameraInterpSpeed);
	PC->SetControlRotation(NewRot);

	// 4. Smoothly and directly align Character Actor Rotation towards Target
	if (const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(OwnerPawn))
	{
		if (!TTChar->IsClimbing())
		{
			const FRotator CurrentActorRot = OwnerPawn->GetActorRotation();
			const FRotator DesiredActorRot(0.0f, TargetYaw, 0.0f);
			OwnerPawn->SetActorRotation(FMath::RInterpTo(CurrentActorRot, DesiredActorRot, DeltaTime, 20.0f));
		}
	}
	else
	{
		const FRotator CurrentActorRot = OwnerPawn->GetActorRotation();
		const FRotator DesiredActorRot(0.0f, TargetYaw, 0.0f);
		OwnerPawn->SetActorRotation(FMath::RInterpTo(CurrentActorRot, DesiredActorRot, DeltaTime, 20.0f));
	}
}

void UThreadTalesTargetLockComponent::SetCharacterStrafeMode(bool bEnableStrafe)
{
	AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetOwner());
	if (!TTChar || TTChar->IsClimbing())
	{
		return;
	}

	// Update CharacterMovementComponent rotation mode for zero-latency camera strafe orientation
	if (UCharacterMovementComponent* MoveComp = TTChar->GetCharacterMovement())
	{
		MoveComp->bUseControllerDesiredRotation = bEnableStrafe;
		MoveComp->bOrientRotationToMovement = !bEnableStrafe;
	}

	// Sync with Character Strafe input state for pure strafe locomotion (without Block/Aim)
	TTChar->SetStrafeInputDown(bEnableStrafe);
}

APlayerController* UThreadTalesTargetLockComponent::GetPlayerController() const
{
	if (const APawn* PawnOwner = Cast<APawn>(GetOwner()))
	{
		return Cast<APlayerController>(PawnOwner->GetController());
	}
	return nullptr;
}
