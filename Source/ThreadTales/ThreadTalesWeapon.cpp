// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesWeapon.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"

AThreadTalesWeapon::AThreadTalesWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(RootComponent);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SheathMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SheathMesh"));
	SheathMesh->SetupAttachment(RootComponent);
	SheathMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HandSocketName = FName("weapon_hand_r_socket");
	SheathSocketName = FName("weapon_sheath_socket");

	WeaponState = EWeaponCombatState::Sheathed;
}

void AThreadTalesWeapon::BeginPlay()
{
	Super::BeginPlay();
}

void AThreadTalesWeapon::Equip(AActor* NewOwner)
{
	if (!HasAuthority()) return;

	SetOwner(NewOwner);

	ACharacter* Character = Cast<ACharacter>(NewOwner);
	if (Character && Character->GetMesh())
	{
		// Attach Sheath (scabbard) to hip / sheath socket
		if (SheathMesh && SheathMesh->GetStaticMesh())
		{
			SheathMesh->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SheathSocketName);
		}

		// Initially, attach the blade into the Sheath socket
		AttachToSheath();
	}

	// Grant Abilities
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(NewOwner);
	if (ASI)
	{
		OwnerASC = ASI->GetAbilitySystemComponent();
		if (OwnerASC)
		{
			for (TSubclassOf<UGameplayAbility> AbilityClass : GrantedAbilities)
			{
				if (AbilityClass)
				{
					FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);
					FGameplayAbilitySpecHandle Handle = OwnerASC->GiveAbility(Spec);
					GrantedAbilityHandles.Add(Handle);
				}
			}
		}
	}
}

void AThreadTalesWeapon::Unequip()
{
	if (!HasAuthority()) return;

	// Remove Abilities
	if (OwnerASC)
	{
		for (const FGameplayAbilitySpecHandle& Handle : GrantedAbilityHandles)
		{
			OwnerASC->ClearAbility(Handle);
		}
	}
	GrantedAbilityHandles.Empty();
	OwnerASC = nullptr;

	if (WeaponMesh)
	{
		WeaponMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}
	if (SheathMesh)
	{
		SheathMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetOwner(nullptr);
}

void AThreadTalesWeapon::AttachToHand()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character && Character->GetMesh() && WeaponMesh)
	{
		WeaponMesh->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, HandSocketName);
		SetWeaponState(EWeaponCombatState::Drawn);
	}
}

void AThreadTalesWeapon::AttachToSheath()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character && Character->GetMesh() && WeaponMesh)
	{
		WeaponMesh->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SheathSocketName);
		SetWeaponState(EWeaponCombatState::Sheathed);
	}
}

void AThreadTalesWeapon::SetWeaponState(EWeaponCombatState NewState)
{
	if (WeaponState != NewState)
	{
		WeaponState = NewState;
		OnWeaponStateChanged.Broadcast(WeaponState);
	}
}

void AThreadTalesWeapon::OnRep_WeaponState(EWeaponCombatState OldState)
{
	OnWeaponStateChanged.Broadcast(WeaponState);
}

void AThreadTalesWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AThreadTalesWeapon, WeaponState);
}
