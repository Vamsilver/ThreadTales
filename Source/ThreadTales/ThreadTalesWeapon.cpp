// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesWeapon.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/Engine.h"

AThreadTalesWeapon::AThreadTalesWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true); // Required for AttachToComponent replication to clients

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = WeaponMesh;
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
	if (Character)
	{
		// Attach to hand socket - name can be overridden in BP or here
		AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("hand_r"));
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Magenta, TEXT("Weapon Attached to Mesh!"));
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

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetOwner(nullptr);
}
