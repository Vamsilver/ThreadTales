#include "Animation/AnimNotifyState_ComboWindow.h"
#include "ThreadTales.h"
#include "Abilities/ThreadTalesComboAbilityInterface.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

UAnimNotifyState_ComboWindow::UAnimNotifyState_ComboWindow()
{
}

static IThreadTalesComboAbilityInterface* GetActiveComboAbility(USkeletalMeshComponent* MeshComp)
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
			if (IThreadTalesComboAbilityInterface* ComboAbility = Cast<IThreadTalesComboAbilityInterface>(Spec.GetPrimaryInstance()))
			{
				return ComboAbility;
			}

			for (UGameplayAbility* Inst : Spec.GetAbilityInstances())
			{
				if (IThreadTalesComboAbilityInterface* ComboAbility = Cast<IThreadTalesComboAbilityInterface>(Inst))
				{
					return ComboAbility;
				}
			}
		}
	}

	return nullptr;
}

void UAnimNotifyState_ComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !MeshComp->GetWorld() || !MeshComp->GetWorld()->IsGameWorld())
	{
		return;
	}

	if (IThreadTalesComboAbilityInterface* ComboAbility = GetActiveComboAbility(MeshComp))
	{
		ComboAbility->NotifyComboWindowBegin();
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Combo Notify] %s: OPEN Combo Window on %s (Duration: %.3fs)"),
			*GetNameSafe(MeshComp->GetOwner()), *GetNameSafe(Animation), TotalDuration);
	}
}

void UAnimNotifyState_ComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetWorld() || !MeshComp->GetWorld()->IsGameWorld())
	{
		return;
	}

	if (IThreadTalesComboAbilityInterface* ComboAbility = GetActiveComboAbility(MeshComp))
	{
		ComboAbility->NotifyComboWindowEnd();
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Combo Notify] %s: CLOSE Combo Window on %s"),
			*GetNameSafe(MeshComp->GetOwner()), *GetNameSafe(Animation));
	}
}

FString UAnimNotifyState_ComboWindow::GetNotifyName_Implementation() const
{
	return TEXT("Combo Window");
}
