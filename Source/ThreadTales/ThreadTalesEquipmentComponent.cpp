// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesEquipmentComponent.h"
#include "ThreadTalesWeapon.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"

UThreadTalesEquipmentComponent::UThreadTalesEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UThreadTalesEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UThreadTalesEquipmentComponent::EquipWeapon(TSubclassOf<AThreadTalesWeapon> WeaponClass)
{
	if (!GetOwner()->HasAuthority())
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, TEXT("Client requesting EquipWeapon from Server..."));
		ServerEquipWeapon(WeaponClass);
		return;
	}

	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Server: EquipWeapon called!"));

	UnequipWeapon();

	if (WeaponClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();
		SpawnParams.Instigator = Cast<APawn>(GetOwner());
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		CurrentWeapon = GetWorld()->SpawnActor<AThreadTalesWeapon>(WeaponClass, SpawnParams);
		if (CurrentWeapon)
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, TEXT("Server: Weapon Spawned Successfully!"));
			CurrentWeapon->Equip(GetOwner());
		}
		else
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Server: Failed to spawn weapon!"));
		}
	}
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Server: WeaponClass is null!"));
	}
}

void UThreadTalesEquipmentComponent::ServerEquipWeapon_Implementation(TSubclassOf<AThreadTalesWeapon> WeaponClass)
{
	EquipWeapon(WeaponClass);
}

void UThreadTalesEquipmentComponent::UnequipWeapon()
{
	if (!GetOwner()->HasAuthority())
	{
		ServerUnequipWeapon();
		return;
	}

	if (CurrentWeapon)
	{
		CurrentWeapon->Unequip();
		CurrentWeapon->Destroy();
		CurrentWeapon = nullptr;
	}
}

void UThreadTalesEquipmentComponent::ServerUnequipWeapon_Implementation()
{
	UnequipWeapon();
}

void UThreadTalesEquipmentComponent::OnRep_CurrentWeapon(AThreadTalesWeapon* OldWeapon)
{
	// Client side notification
}

void UThreadTalesEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UThreadTalesEquipmentComponent, CurrentWeapon);
}
