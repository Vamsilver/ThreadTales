// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifyState_ComboWindow.h"
#include "ThreadTales.h"
#include "ThreadTalesGA_Melee.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

UAnimNotifyState_ComboWindow::UAnimNotifyState_ComboWindow()
{
}

static UThreadTalesGA_Melee* GetActiveMeleeAbility(USkeletalMeshComponent* MeshComp)
{
	if (!MeshComp) return nullptr;

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor) return nullptr;

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(OwnerActor);
	if (!ASI) return nullptr;

	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
	if (!ASC) return nullptr;

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.IsActive())
		{
			if (UThreadTalesGA_Melee* MeleeAbility = Cast<UThreadTalesGA_Melee>(Spec.GetPrimaryInstance()))
			{
				return MeleeAbility;
			}

			for (UGameplayAbility* Inst : Spec.GetAbilityInstances())
			{
				if (UThreadTalesGA_Melee* MeleeAbility = Cast<UThreadTalesGA_Melee>(Inst))
				{
					return MeleeAbility;
				}
			}
		}
	}

	return nullptr;
}

void UAnimNotifyState_ComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UThreadTalesGA_Melee* MeleeAbility = GetActiveMeleeAbility(MeshComp))
	{
		MeleeAbility->NotifyComboWindowBegin();
	}
	else
	{
		UE_LOG(LogThreadTalesCombat, Warning, TEXT("[Combo Notify] Warning: Melee Combo Window started, but no active ThreadTalesGA_Melee ability was found on character!"));
	}
}

void UAnimNotifyState_ComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (UThreadTalesGA_Melee* MeleeAbility = GetActiveMeleeAbility(MeshComp))
	{
		MeleeAbility->NotifyComboWindowEnd();
	}
}

FString UAnimNotifyState_ComboWindow::GetNotifyName_Implementation() const
{
	return TEXT("Combo Window");
}
