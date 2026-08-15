// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifyState_WeaponHitbox.h"
#include "ThreadTalesEquipmentComponent.h"
#include "ThreadTalesWeapon_Sword.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

UAnimNotifyState_WeaponHitbox::UAnimNotifyState_WeaponHitbox()
{
}

void UAnimNotifyState_WeaponHitbox::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp) return;

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor) return;

	UThreadTalesEquipmentComponent* EquipComp = OwnerActor->FindComponentByClass<UThreadTalesEquipmentComponent>();
	if (!EquipComp) return;

	if (AThreadTalesWeapon_Sword* Sword = Cast<AThreadTalesWeapon_Sword>(EquipComp->GetCurrentWeapon()))
	{
		Sword->SetHitboxEnabled(true);
	}
}

void UAnimNotifyState_WeaponHitbox::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor) return;

	UThreadTalesEquipmentComponent* EquipComp = OwnerActor->FindComponentByClass<UThreadTalesEquipmentComponent>();
	if (!EquipComp) return;

	if (AThreadTalesWeapon_Sword* Sword = Cast<AThreadTalesWeapon_Sword>(EquipComp->GetCurrentWeapon()))
	{
		Sword->SetHitboxEnabled(false);
	}
}

FString UAnimNotifyState_WeaponHitbox::GetNotifyName_Implementation() const
{
	return TEXT("Weapon Hitbox");
}
