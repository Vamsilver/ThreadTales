#include "Animation/AnimNotify_AttachWeapon.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
#include "Combat/ThreadTalesWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

UAnimNotify_AttachWeapon::UAnimNotify_AttachWeapon()
{
	bAttachToHand = true;
}

void UAnimNotify_AttachWeapon::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetWorld() || !MeshComp->GetWorld()->IsGameWorld()) return;

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor) return;

	UThreadTalesEquipmentComponent* EquipmentComp = OwnerActor->FindComponentByClass<UThreadTalesEquipmentComponent>();
	if (!EquipmentComp) return;

	AThreadTalesWeapon* CurrentWeapon = EquipmentComp->GetCurrentWeapon();
	if (!CurrentWeapon) return;

	if (bAttachToHand)
	{
		CurrentWeapon->AttachToHand();
	}
	else
	{
		CurrentWeapon->AttachToSheath();
	}
}

FString UAnimNotify_AttachWeapon::GetNotifyName_Implementation() const
{
	return bAttachToHand ? TEXT("Attach Weapon To Hand") : TEXT("Attach Weapon To Sheath");
}
