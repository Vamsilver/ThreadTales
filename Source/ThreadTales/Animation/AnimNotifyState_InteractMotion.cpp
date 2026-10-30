// Copyright ThreadTales. All Rights Reserved.

#include "Animation/AnimNotifyState_InteractMotion.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/Character.h"
#include "GameFramework/Actor.h"

UAnimNotifyState_InteractMotion::UAnimNotifyState_InteractMotion()
{
	BeginEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Interact.Motion.Begin"), false);
	EndEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Interact.Motion.End"), false);
}


void UAnimNotifyState_InteractMotion::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !MeshComp->GetWorld() || !MeshComp->GetWorld()->IsGameWorld()) return;
	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor) return;

	FGameplayEventData Payload;
	Payload.Instigator = OwnerActor;
	Payload.Target = OwnerActor;

	if (MeshComp->DoesSocketExist(HandSocketName))
	{
		const FVector HandLoc = MeshComp->GetSocketLocation(HandSocketName); // DoesSocketExist verified
		const FVector CharLoc = OwnerActor->GetActorLocation();
		const FVector CharFwd = OwnerActor->GetActorForwardVector().GetSafeNormal2D();
		Payload.EventMagnitude = FVector::DotProduct(HandLoc - CharLoc, CharFwd);
	}

	const FGameplayTag EventTag = BeginEventTag.IsValid() ? BeginEventTag : FGameplayTag::RequestGameplayTag(FName("Event.Interact.Motion.Begin"), false);
	if (EventTag.IsValid())
	{
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);
	}

	// Also broadcast legacy trigger event for backwards compatibility
	const FGameplayTag LegacyTag = FGameplayTag::RequestGameplayTag(FName("Event.Interact.Trigger"), false);
	if (LegacyTag.IsValid())
	{
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, LegacyTag, Payload);
	}
}

void UAnimNotifyState_InteractMotion::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetWorld() || !MeshComp->GetWorld()->IsGameWorld()) return;
	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor) return;

	FGameplayEventData Payload;
	Payload.Instigator = OwnerActor;
	Payload.Target = OwnerActor;

	const FGameplayTag EventTag = EndEventTag.IsValid() ? EndEventTag : FGameplayTag::RequestGameplayTag(FName("Event.Interact.Motion.End"), false);
	if (EventTag.IsValid())
	{
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);
	}
}

FString UAnimNotifyState_InteractMotion::GetNotifyName_Implementation() const
{
	return TEXT("Interact Motion");
}


