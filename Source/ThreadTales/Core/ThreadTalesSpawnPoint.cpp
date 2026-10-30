// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ThreadTalesSpawnPoint.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/SphereComponent.h"
#include "AbilitySystemComponent.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Core/ThreadTalesPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "ThreadTales.h"

AThreadTalesSpawnPoint::AThreadTalesSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(SceneRoot);
	MeshComponent->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	SpawnMarker = CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnMarker"));
	SpawnMarker->SetupAttachment(SceneRoot);
	SpawnMarker->SetRelativeLocation(FVector(120.0f, 0.0f, 0.0f));
	SpawnMarker->ArrowColor = FColor::Green;

	ProximitySphere = CreateDefaultSubobject<USphereComponent>(TEXT("ProximitySphere"));
	ProximitySphere->SetupAttachment(SceneRoot);
	ProximitySphere->SetSphereRadius(350.0f);
	ProximitySphere->SetCollisionProfileName(TEXT("Trigger"));
	ProximitySphere->SetGenerateOverlapEvents(true);
}

void AThreadTalesSpawnPoint::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && ProximitySphere)
	{
		ProximitySphere->OnComponentBeginOverlap.AddDynamic(this, &AThreadTalesSpawnPoint::HandleSphereBeginOverlap);
		ProximitySphere->OnComponentEndOverlap.AddDynamic(this, &AThreadTalesSpawnPoint::HandleSphereEndOverlap);
	}
}

void AThreadTalesSpawnPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		for (auto& Pair : ActiveRegenEffects)
		{
			if (AThreadTalesCharacter* Character = Pair.Key.Get())
			{
				if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
				{
					if (Pair.Value.IsValid())
					{
						ASC->RemoveActiveGameplayEffect(Pair.Value);
					}
				}
			}
		}
		ActiveRegenEffects.Empty();
	}

	Super::EndPlay(EndPlayReason);
}

void AThreadTalesSpawnPoint::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AThreadTalesSpawnPoint, bIsActive);
}

FTransform AThreadTalesSpawnPoint::GetSpawnTransform() const
{
	FTransform Transform = SpawnMarker ? SpawnMarker->GetComponentTransform() : GetActorTransform();

	// Lift by standard capsule half-height (~95 units) so the character spawns standing on ground, not embedded into it
	Transform.AddToTranslation(FVector(0.0f, 0.0f, 95.0f));

	// Strictly zero out pitch and roll so character orientation is completely upright
	FRotator CleanRotator(0.0f, Transform.Rotator().Yaw, 0.0f);
	Transform.SetRotation(FQuat(CleanRotator));

	return Transform;
}

void AThreadTalesSpawnPoint::SetSpawnPointActive(bool bNewActive)
{
	if (!HasAuthority() || bIsActive == bNewActive)
	{
		return;
	}

	bIsActive = bNewActive;
	OnRep_IsActive();
}

void AThreadTalesSpawnPoint::OnRep_IsActive()
{
	if (bIsActive)
	{
		OnSpawnPointActivated();
	}
	else
	{
		OnSpawnPointDeactivated();
	}

	OnSpawnPointStateChanged.Broadcast(this, bIsActive);
}

void AThreadTalesSpawnPoint::HandleSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}

	AThreadTalesCharacter* Character = Cast<AThreadTalesCharacter>(OtherActor);
	if (!Character)
	{
		return;
	}

	// Activate spawn point if first visit
	if (!bIsActive)
	{
		SetSpawnPointActive(true);
	}

	// Update PlayerController respawn point safely (individual controller for 2P Co-op)
	if (AThreadTalesPlayerController* PC = Cast<AThreadTalesPlayerController>(Character->GetController()))
	{
		PC->SetCurrentSpawnPoint(this);
	}

	// Apply health regen effect if configured
	if (HealthRegenEffectClass)
	{
		if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
		{
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddSourceObject(this);
			FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(HealthRegenEffectClass, 1.0f, Context);
			if (SpecHandle.IsValid())
			{
				FActiveGameplayEffectHandle ActiveHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
				ActiveRegenEffects.Add(Character, ActiveHandle);
			}
		}
	}
}

void AThreadTalesSpawnPoint::HandleSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}

	AThreadTalesCharacter* Character = Cast<AThreadTalesCharacter>(OtherActor);
	if (!Character)
	{
		return;
	}

	// Remove health regen effect when exiting radius
	if (FActiveGameplayEffectHandle* HandlePtr = ActiveRegenEffects.Find(Character))
	{
		if (HandlePtr->IsValid())
		{
			if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
			{
				ASC->RemoveActiveGameplayEffect(*HandlePtr);
			}
		}
		ActiveRegenEffects.Remove(Character);
	}
}
