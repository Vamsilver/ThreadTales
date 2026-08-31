#include "Animation/AnimNotifyState_WeaponHitbox.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
#include "Combat/ThreadTalesWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

UAnimNotifyState_WeaponHitbox::UAnimNotifyState_WeaponHitbox()
{
}

void UAnimNotifyState_WeaponHitbox::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !MeshComp->GetWorld() || !MeshComp->GetWorld()->IsGameWorld()) return;

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor) return;

	UThreadTalesEquipmentComponent* EquipComp = OwnerActor->FindComponentByClass<UThreadTalesEquipmentComponent>();
	if (!EquipComp) return;

	if (AThreadTalesWeapon* CurrentWeapon = EquipComp->GetCurrentWeapon())
	{
		CurrentWeapon->SetHitboxEnabled(true);
	}
}

void UAnimNotifyState_WeaponHitbox::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetWorld() || !MeshComp->GetWorld()->IsGameWorld()) return;

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor) return;

	UThreadTalesEquipmentComponent* EquipComp = OwnerActor->FindComponentByClass<UThreadTalesEquipmentComponent>();
	if (!EquipComp) return;

	if (AThreadTalesWeapon* CurrentWeapon = EquipComp->GetCurrentWeapon())
	{
		CurrentWeapon->SetHitboxEnabled(false);
	}
}

FString UAnimNotifyState_WeaponHitbox::GetNotifyName_Implementation() const
{
	return TEXT("Weapon Hitbox");
}
