// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesWeapon_Slingshot.h"
#include "ThreadTalesProjectile.h"
#include "Abilities/ThreadTalesGA_RangedAttack.h"
#include "ThreadTales.h"
#include "ThreadTalesGameplayTags.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

AThreadTalesWeapon_Slingshot::AThreadTalesWeapon_Slingshot()
{
	PrimaryAttackTag = ThreadTalesGameplayTags::TAG_Ability_Ranged_Slingshot;

	ProjectileSpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("ProjectileSpawnPoint"));
	ProjectileSpawnPoint->SetupAttachment(WeaponMesh);

	bHasSheath = false;
	bStartDrawn = true;
	HandSocketName = TEXT("weapon_hand_r_socket");
	SheathSocketName = NAME_None;

	MinChargeTime = 0.15f;
	MaxChargeTime = 1.0f;
	MinDamage = 20.0f;
	MaxDamage = 65.0f;

	MaxAmmo = 99;
	CurrentAmmo = MaxAmmo;
}

void AThreadTalesWeapon_Slingshot::BeginPlay()
{
	Super::BeginPlay();
}

AThreadTalesProjectile* AThreadTalesWeapon_Slingshot::FireProjectile(FVector SpawnLocation, FRotator AimRotation, float ChargeAlpha)
{
	if (!ProjectileClass)
	{
		UE_LOG(LogThreadTalesCombat, Warning, TEXT("[Slingshot] FireProjectile failed: ProjectileClass is not assigned!"));
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World) return nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = Cast<APawn>(GetOwner());
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AThreadTalesProjectile* Projectile = World->SpawnActor<AThreadTalesProjectile>(ProjectileClass, SpawnLocation, AimRotation, SpawnParams);
	if (Projectile)
	{
		float Damage = FMath::Lerp(MinDamage, MaxDamage, ChargeAlpha);
		Projectile->InitializeProjectile(ChargeAlpha, Damage);

		if (ReleaseSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, ReleaseSound, SpawnLocation);
		}

		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Slingshot] Fired projectile with Charge: %.2f, Damage: %.1f"), ChargeAlpha, Damage);
	}

	return Projectile;
}

void AThreadTalesWeapon_Slingshot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AThreadTalesWeapon_Slingshot, CurrentAmmo);
}
