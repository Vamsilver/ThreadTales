// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesWeapon_Sword.h"
#include "Components/BoxComponent.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"

AThreadTalesWeapon_Sword::AThreadTalesWeapon_Sword()
{
	Hitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("Hitbox"));
	Hitbox->SetupAttachment(RootComponent);
	Hitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Disabled by default, enabled during attack anim
	Hitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
	Hitbox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Hitbox->OnComponentBeginOverlap.AddDynamic(this, &AThreadTalesWeapon_Sword::OnHitboxOverlap);
}

void AThreadTalesWeapon_Sword::BeginPlay()
{
	Super::BeginPlay();
}

void AThreadTalesWeapon_Sword::SetHitboxEnabled(bool bEnabled)
{
	if (bEnabled)
	{
		Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	else
	{
		Hitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AThreadTalesWeapon_Sword::OnHitboxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != GetOwner())
	{
		// TODO: Apply Gameplay Effect (Damage) to OtherActor using OwnerASC
		// Example: OwnerASC->ApplyGameplayEffectToTarget(...)
	}
}
