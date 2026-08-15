// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesGA_Melee.h"
#include "ThreadTales.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "ThreadTalesEquipmentComponent.h"
#include "ThreadTalesWeapon.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

UThreadTalesGA_Melee::UThreadTalesGA_Melee()
{
	AbilityName = TEXT("Melee Attack Combo");
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Melee")));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Combat.Attacking")));

	ComboSectionNames.Add(FName("Combo1"));
	ComboSectionNames.Add(FName("Combo2"));
	ComboSectionNames.Add(FName("Combo3"));
	ComboSectionNames.Add(FName("Combo4"));

	CurrentComboIndex = 0;
	bComboWindowOpen = false;
	bQueueNextCombo = false;
	LastAttackInputTime = -100.0f;
	CurrentPlayingMontage = nullptr;
}

void UThreadTalesGA_Melee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	CurrentComboIndex = 0;
	bComboWindowOpen = false;
	bQueueNextCombo = false;
	CurrentPlayingMontage = nullptr;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogThreadTalesCombat, Warning, TEXT("[Melee GA] Failed to Commit Ability!"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAnimMontage* MontageToPlay = FallbackAttackMontage;

	// Check if current weapon has its own combo montage
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (Avatar)
	{
		UThreadTalesEquipmentComponent* EquipComp = Avatar->FindComponentByClass<UThreadTalesEquipmentComponent>();
		if (EquipComp && EquipComp->GetCurrentWeapon())
		{
			UAnimMontage* WeaponMontage = EquipComp->GetCurrentWeapon()->GetComboAttackMontage();
			if (WeaponMontage)
			{
				MontageToPlay = WeaponMontage;
			}
		}
	}

	if (MontageToPlay)
	{
		CurrentPlayingMontage = MontageToPlay;
		FName StartSection = ComboSectionNames.Num() > 0 ? ComboSectionNames[0] : NAME_None;

		UE_LOG(LogThreadTalesCombat, Log, TEXT("[Melee GA] Started Combo Strike 1 on Montage: %s (Section: %s)"), *GetNameSafe(MontageToPlay), *StartSection.ToString());

		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay, 1.f, StartSection, true, 1.f);
		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageCompleted);
			MontageTask->OnInterrupted.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageInterrupted);
			MontageTask->OnCancelled.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageCancelled);
			MontageTask->ReadyForActivation();
		}
	}
	else
	{
		UE_LOG(LogThreadTalesCombat, Error, TEXT("[Melee GA] ERROR: No Combo Attack Montage assigned in Weapon or Ability!"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UThreadTalesGA_Melee::NotifyComboInput()
{
	LastAttackInputTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	if (bComboWindowOpen)
	{
		UE_LOG(LogThreadTalesCombat, Log, TEXT("[Melee GA] Attack Input Received while window open -> Jumping to next strike!"));
		JumpToNextComboSection();
	}
	else
	{
		bQueueNextCombo = true;
		UE_LOG(LogThreadTalesCombat, Log, TEXT("[Melee GA] Attack Input Received before window -> Buffered for window open."));
	}
}

void UThreadTalesGA_Melee::NotifyComboWindowBegin()
{
	bComboWindowOpen = true;

	// Check if player clicked shortly before the window opened (Input Buffering)
	if (bQueueNextCombo || (GetWorld() && (GetWorld()->GetTimeSeconds() - LastAttackInputTime <= InputBufferTolerance)))
	{
		UE_LOG(LogThreadTalesCombat, Log, TEXT("[Melee GA] Combo Window OPEN with Buffered Input -> Jumping to next strike!"));
		bQueueNextCombo = false;
		JumpToNextComboSection();
		return;
	}

	UE_LOG(LogThreadTalesCombat, Log, TEXT("[Melee GA] >>> Combo Window OPEN for Strike %d <<<"), CurrentComboIndex + 1);
}

void UThreadTalesGA_Melee::NotifyComboWindowEnd()
{
	bComboWindowOpen = false;

	UE_LOG(LogThreadTalesCombat, Log, TEXT("[Melee GA] <<< Combo Window CLOSED for Strike %d >>>"), CurrentComboIndex + 1);

	if (bQueueNextCombo)
	{
		JumpToNextComboSection();
	}
}

void UThreadTalesGA_Melee::JumpToNextComboSection()
{
	CurrentComboIndex++;
	bComboWindowOpen = false;
	bQueueNextCombo = false;

	if (CurrentComboIndex < ComboSectionNames.Num())
	{
		FName NextSection = ComboSectionNames[CurrentComboIndex];

		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Melee GA] === Transitioning to Section: %s (Strike %d) ==="), *NextSection.ToString(), CurrentComboIndex + 1);

		ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
		if (Character && Character->GetMesh() && Character->GetMesh()->GetAnimInstance())
		{
			Character->GetMesh()->GetAnimInstance()->Montage_JumpToSection(NextSection, CurrentPlayingMontage);
		}
	}
	else
	{
		UE_LOG(LogThreadTalesCombat, Log, TEXT("[Melee GA] All combo strikes in chain completed."));
	}
}

void UThreadTalesGA_Melee::OnMontageCompleted()
{
	UE_LOG(LogThreadTalesCombat, Log, TEXT("[Melee GA] Montage Completed normally."));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UThreadTalesGA_Melee::OnMontageInterrupted()
{
	UE_LOG(LogThreadTalesCombat, Log, TEXT("[Melee GA] Montage Interrupted."));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UThreadTalesGA_Melee::OnMontageCancelled()
{
	UE_LOG(LogThreadTalesCombat, Log, TEXT("[Melee GA] Montage Cancelled."));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
