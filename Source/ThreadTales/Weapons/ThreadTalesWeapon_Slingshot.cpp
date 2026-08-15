// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesWeapon_Slingshot.h"
#include "Components/ArrowComponent.h"
#include "Net/UnrealNetwork.h"

AThreadTalesWeapon_Slingshot::AThreadTalesWeapon_Slingshot()
{
	ProjectileSpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("ProjectileSpawnPoint"));
	ProjectileSpawnPoint->SetupAttachment(RootComponent);

	MaxAmmo = 10;
	CurrentAmmo = MaxAmmo;
}

void AThreadTalesWeapon_Slingshot::BeginPlay()
{
	Super::BeginPlay();
}

void AThreadTalesWeapon_Slingshot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AThreadTalesWeapon_Slingshot, CurrentAmmo);
}
