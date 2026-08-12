// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesGA_Melee.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/Character.h"
#include "Engine/Engine.h"

UThreadTalesGA_Melee::UThreadTalesGA_Melee()
{
	AbilityName = TEXT("Melee Attack");
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UThreadTalesGA_Melee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("Ability: Melee Activated!"));

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (AttackMontage)
	{
		UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage, 1.f, NAME_None, true, 1.f);
		if (Task)
		{
			Task->OnCompleted.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageCompleted);
			Task->OnInterrupted.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageInterrupted);
			Task->OnCancelled.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageCancelled);
			Task->ReadyForActivation();
		}
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UThreadTalesGA_Melee::OnMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UThreadTalesGA_Melee::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UThreadTalesGA_Melee::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
