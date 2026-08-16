// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesGA_Melee.h"
#include "ThreadTales.h"
#include "ThreadTalesGameplayTags.h"
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

	SetAssetTags(FGameplayTagContainer(ThreadTalesGameplayTags::TAG_Ability_Melee));
	ActivationOwnedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Attacking);

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

	PlayComboStrike(0);
}

void UThreadTalesGA_Melee::PlayComboStrike(int32 Index)
{
	// Orient character towards camera or movement direction
	AlignCharacterToCameraDirection();

	// 1. Check if weapon or ability provides an array of separate combo strike montages
	TArray<UAnimMontage*> ActiveStrikeMontages = ComboStrikeMontages;
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (Avatar)
	{
		UThreadTalesEquipmentComponent* EquipComp = Avatar->FindComponentByClass<UThreadTalesEquipmentComponent>();
		if (EquipComp && EquipComp->GetCurrentWeapon() && EquipComp->GetCurrentWeapon()->GetComboStrikeMontages().Num() > 0)
		{
			ActiveStrikeMontages = EquipComp->GetCurrentWeapon()->GetComboStrikeMontages();
		}
	}

	if (ActiveStrikeMontages.Num() > 0)
	{
		if (ActiveStrikeMontages.IsValidIndex(Index) && ActiveStrikeMontages[Index])
		{
			UAnimMontage* MontageToPlay = ActiveStrikeMontages[Index];
			CurrentPlayingMontage = MontageToPlay;

			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Melee GA] Playing Separate Strike %d Montage: %s"), Index + 1, *GetNameSafe(MontageToPlay));

			if (MontageTask)
			{
				MontageTask->EndTask();
			}

			MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay, 1.f, NAME_None, true, 1.f);
			if (MontageTask)
			{
				MontageTask->OnCompleted.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageCompleted);
				MontageTask->OnInterrupted.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageInterrupted);
				MontageTask->OnCancelled.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageCancelled);
				MontageTask->ReadyForActivation();
			}
			return;
		}
		else
		{
			UE_LOG(LogThreadTalesCombat, Log, TEXT("[Melee GA] Reached end of separate combo strike chain (%d strikes)."), ActiveStrikeMontages.Num());
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			return;
		}
	}

	// 2. Fallback to Section-based single Montage
	UAnimMontage* SingleMontage = FallbackAttackMontage;
	if (Avatar)
	{
		UThreadTalesEquipmentComponent* EquipComp = Avatar->FindComponentByClass<UThreadTalesEquipmentComponent>();
		if (EquipComp && EquipComp->GetCurrentWeapon() && EquipComp->GetCurrentWeapon()->GetComboAttackMontage())
		{
			SingleMontage = EquipComp->GetCurrentWeapon()->GetComboAttackMontage();
		}
	}

	if (SingleMontage)
	{
		CurrentPlayingMontage = SingleMontage;
		if (Index == 0)
		{
			FName StartSection = ComboSectionNames.Num() > 0 ? ComboSectionNames[0] : NAME_None;
			UE_LOG(LogThreadTalesCombat, Log, TEXT("[Melee GA] Started Section Combo Strike 1 on Montage: %s (Section: %s)"), *GetNameSafe(SingleMontage), *StartSection.ToString());

			MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, SingleMontage, 1.f, StartSection, true, 1.f);
			if (MontageTask)
			{
				MontageTask->OnCompleted.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageCompleted);
				MontageTask->OnInterrupted.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageInterrupted);
				MontageTask->OnCancelled.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageCancelled);
				MontageTask->ReadyForActivation();
			}
		}
		else if (ComboSectionNames.IsValidIndex(Index))
		{
			FName NextSection = ComboSectionNames[Index];
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Melee GA] === Transitioning to Section: %s (Strike %d) ==="), *NextSection.ToString(), Index + 1);

			ACharacter* Character = Cast<ACharacter>(Avatar);
			if (Character && Character->GetMesh() && Character->GetMesh()->GetAnimInstance())
			{
				Character->GetMesh()->GetAnimInstance()->Montage_JumpToSection(NextSection, SingleMontage);
			}
		}
		else
		{
			UE_LOG(LogThreadTalesCombat, Log, TEXT("[Melee GA] All combo sections in chain completed."));
		}
	}
	else
	{
		UE_LOG(LogThreadTalesCombat, Error, TEXT("[Melee GA] ERROR: No Combo Attack Montage assigned in Weapon or Ability!"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
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

	PlayComboStrike(CurrentComboIndex);
}

void UThreadTalesGA_Melee::AlignCharacterToCameraDirection()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character) return;

	AController* Controller = Character->GetController();
	if (!Controller) return;

	// Calculate target Yaw from Camera/Controller
	FRotator ControlRot = Controller->GetControlRotation();
	FRotator TargetRotation(0.0f, ControlRot.Yaw, 0.0f);

	// If player is holding a movement input (WASD), attack in that movement direction, otherwise towards camera forward
	FVector LastInput = Character->GetLastMovementInputVector();
	if (!LastInput.IsNearlyZero())
	{
		TargetRotation = FRotator(0.0f, LastInput.Rotation().Yaw, 0.0f);
	}

	Character->SetActorRotation(TargetRotation);
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
