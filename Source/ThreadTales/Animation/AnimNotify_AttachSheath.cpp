#include "Animation/AnimNotify_AttachSheath.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
#include "Combat/ThreadTalesWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

UAnimNotify_AttachSheath::UAnimNotify_AttachSheath()
{
	bAttachToHand = true;
}

void UAnimNotify_AttachSheath::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
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
		CurrentWeapon->AttachSheathToHand();
	}
	else
	{
		CurrentWeapon->AttachSheathToHip();
	}
}

FString UAnimNotify_AttachSheath::GetNotifyName_Implementation() const
{
	return bAttachToHand ? TEXT("Attach Sheath To Hand") : TEXT("Attach Sheath To Hip");
}
