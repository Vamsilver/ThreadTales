// Copyright ThreadTales. All Rights Reserved.

#include "ThreadTalesInteractionDetectorComponent.h"
#include "Combat/ThreadTalesInteractableComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"

UThreadTalesInteractionDetectorComponent::UThreadTalesInteractionDetectorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = false;
}

void UThreadTalesInteractionDetectorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		const float Interval = QueryInterval > 0.0f ? QueryInterval : 0.1f;
		World->GetTimerManager().SetTimer(DetectionTimerHandle, this, &UThreadTalesInteractionDetectorComponent::PerformDetectionSweep, Interval, true);
	}
}

void UThreadTalesInteractionDetectorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DetectionTimerHandle);
	}

	SetCurrentInteractable(nullptr);
	Super::EndPlay(EndPlayReason);
}

void UThreadTalesInteractionDetectorComponent::PerformDetectionSweep()
{
	if (CurrentInteractable.IsValid() && CurrentInteractable->IsBusy())
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		SetCurrentInteractable(nullptr);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector CharLoc = Character->GetActorLocation();
	const FVector CharFacing = Character->GetActorForwardVector();
	const float SearchRadius = DetectionRadius > 0.0f ? DetectionRadius : 250.0f;
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(ConeHalfAngle > 0.0f ? ConeHalfAngle : 60.0f));

	// 1. Multi-sphere overlap query for WorldDynamic and WorldStatic interactable actors
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(SearchRadius);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);

	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	World->OverlapMultiByObjectType(Overlaps, CharLoc, FQuat::Identity, ObjectQueryParams, SphereShape, QueryParams);

	UThreadTalesInteractableComponent* BestCandidate = nullptr;
	float BestScore = -1.0f;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* OverlapActor = Overlap.GetActor();
		if (!OverlapActor)
		{
			continue;
		}

		UThreadTalesInteractableComponent* InteractableComp = OverlapActor->FindComponentByClass<UThreadTalesInteractableComponent>();
		if (!InteractableComp || !InteractableComp->CanInteract(Character))
		{
			continue;
		}

		const FVector TargetLoc = InteractableComp->GetInteractionLocation();
		FVector DirToTarget = (TargetLoc - CharLoc).GetSafeNormal2D();
		if (DirToTarget.IsNearlyZero())
		{
			DirToTarget = CharFacing;
		}

		const float FacingDot = FVector::DotProduct(CharFacing, DirToTarget);
		if (FacingDot < MinDot)
		{
			continue;
		}

		// 2. Line of sight check to prevent interacting through walls
		FHitResult HitResult;
		FCollisionQueryParams TraceParams;
		TraceParams.AddIgnoredActor(Character);
		TraceParams.AddIgnoredActor(OverlapActor);
		if (AActor* OwnerActor = GetOwner())
		{
			TraceParams.AddIgnoredActor(OwnerActor);
		}
		TArray<AActor*> AttachedActors;
		OverlapActor->GetAttachedActors(AttachedActors, true, true);
		for (AActor* Attached : AttachedActors)
		{
			TraceParams.AddIgnoredActor(Attached);
		}

		// Trace from character eye viewpoint to interaction target slightly elevated to avoid skimming ground geometry
		const FVector CharEyeLoc = Character->GetPawnViewLocation();
		const FVector TargetTraceLoc = TargetLoc + FVector(0.0f, 0.0f, 15.0f);
		const bool bBlocked = World->LineTraceSingleByChannel(HitResult, CharEyeLoc, TargetTraceLoc, OcclusionTraceChannel, TraceParams);
		if (bBlocked)
		{
			continue;
		}

		// Score based on alignment and proximity
		const float Dist = FVector::Dist2D(TargetLoc, CharLoc);
		const float DistRatio = 1.0f - FMath::Clamp(Dist / SearchRadius, 0.0f, 1.0f);
		const float Score = (FacingDot * 0.6f) + (DistRatio * 0.4f);

		if (Score > BestScore)
		{
			BestScore = Score;
			BestCandidate = InteractableComp;
		}
	}

	SetCurrentInteractable(BestCandidate);
}

void UThreadTalesInteractionDetectorComponent::SetCurrentInteractable(UThreadTalesInteractableComponent* NewCandidate)
{
	UThreadTalesInteractableComponent* OldCandidate = CurrentInteractable.Get();
	if (OldCandidate == NewCandidate)
	{
		return;
	}

	if (OldCandidate)
	{
		OldCandidate->SetFocused(false);
	}

	CurrentInteractable = NewCandidate;

	if (NewCandidate)
	{
		NewCandidate->SetFocused(true, Cast<ACharacter>(GetOwner()));
	}

	OnFocusedInteractableChanged.Broadcast(NewCandidate);
}

bool UThreadTalesInteractionDetectorComponent::TryTriggerCurrentInteraction()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return false;
	}

	UThreadTalesInteractableComponent* Target = CurrentInteractable.Get();
	if (!Target || !Target->CanInteract(Character))
	{
		return false;
	}

	return Target->TriggerInteraction(Character);
}

void UThreadTalesInteractionDetectorComponent::SetInteractionHandIKTarget(const FVector& TargetLocation, float Alpha)
{
	InteractionHandIKLocation = TargetLocation;
	InteractionHandIKAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	bEnableInteractionHandIK = InteractionHandIKAlpha > KINDA_SMALL_NUMBER;
}

void UThreadTalesInteractionDetectorComponent::ClearInteractionHandIK()
{
	InteractionHandIKLocation = FVector::ZeroVector;
	InteractionHandIKAlpha = 0.0f;
	bEnableInteractionHandIK = false;
}
