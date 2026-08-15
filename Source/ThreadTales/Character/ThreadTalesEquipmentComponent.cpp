// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesEquipmentComponent.h"
#include "ThreadTales.h"
#include "ThreadTalesWeapon.h"
#include "ThreadTalesGA_Melee.h"
#include "ThreadTalesGA_RangedAttack.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"

UThreadTalesEquipmentComponent::UThreadTalesEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UThreadTalesEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UThreadTalesEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnequipWeapon();
	Super::EndPlay(EndPlayReason);
}

void UThreadTalesEquipmentComponent::EquipWeapon(TSubclassOf<AThreadTalesWeapon> WeaponClass)
{
	if (!GetOwner()->HasAuthority())
	{
		ServerEquipWeapon(WeaponClass);
		return;
	}

	UnequipWeapon();

	if (WeaponClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();
		SpawnParams.Instigator = Cast<APawn>(GetOwner());
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		CurrentWeapon = GetWorld()->SpawnActor<AThreadTalesWeapon>(WeaponClass, SpawnParams);
		if (CurrentWeapon)
		{
			CurrentWeapon->Equip(GetOwner());
			OnWeaponEquipped.Broadcast(CurrentWeapon);
		}
	}
}

void UThreadTalesEquipmentComponent::ServerEquipWeapon_Implementation(TSubclassOf<AThreadTalesWeapon> WeaponClass)
{
	EquipWeapon(WeaponClass);
}

void UThreadTalesEquipmentComponent::UnequipWeapon()
{
	if (!GetOwner()->HasAuthority())
	{
		ServerUnequipWeapon();
		return;
	}

	if (CurrentWeapon)
	{
		CurrentWeapon->Unequip();
		CurrentWeapon->Destroy();
		CurrentWeapon = nullptr;
		OnCombatStanceChanged.Broadcast(false);
	}
}

void UThreadTalesEquipmentComponent::ServerUnequipWeapon_Implementation()
{
	UnequipWeapon();
}

void UThreadTalesEquipmentComponent::ToggleCombatStance()
{
	if (!CurrentWeapon) return;

	if (CurrentWeapon->IsSheathed())
	{
		DrawWeapon();
	}
	else if (CurrentWeapon->IsDrawn())
	{
		SheathWeapon();
	}
}

void UThreadTalesEquipmentComponent::DrawWeapon()
{
	if (!CurrentWeapon || !CurrentWeapon->IsSheathed()) return;

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character) return;

	UAnimMontage* DrawMontage = CurrentWeapon->GetDrawMontage();
	if (DrawMontage)
	{
		CurrentWeapon->SetWeaponState(EWeaponCombatState::Drawing);
		float Duration = Character->PlayAnimMontage(DrawMontage);
		if (Duration > 0.f)
		{
			SetMovementInputBlocked(true);

			if (Character->GetMesh() && Character->GetMesh()->GetAnimInstance())
			{
				FOnMontageEnded MontageEndedDelegate;
				MontageEndedDelegate.BindUObject(this, &UThreadTalesEquipmentComponent::OnEquipMontageEnded);
				Character->GetMesh()->GetAnimInstance()->Montage_SetEndDelegate(MontageEndedDelegate, DrawMontage);
			}
		}
	}
	else
	{
		// If no montage assigned, snap directly
		CurrentWeapon->AttachToHand();
	}

	OnCombatStanceChanged.Broadcast(true);
}

void UThreadTalesEquipmentComponent::SheathWeapon()
{
	if (!CurrentWeapon || !CurrentWeapon->IsDrawn()) return;

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character) return;

	UAnimMontage* SheathMontage = CurrentWeapon->GetSheathMontage();
	if (SheathMontage)
	{
		CurrentWeapon->SetWeaponState(EWeaponCombatState::Sheathing);
		float Duration = Character->PlayAnimMontage(SheathMontage);
		if (Duration > 0.f)
		{
			SetMovementInputBlocked(true);

			if (Character->GetMesh() && Character->GetMesh()->GetAnimInstance())
			{
				FOnMontageEnded MontageEndedDelegate;
				MontageEndedDelegate.BindUObject(this, &UThreadTalesEquipmentComponent::OnEquipMontageEnded);
				Character->GetMesh()->GetAnimInstance()->Montage_SetEndDelegate(MontageEndedDelegate, SheathMontage);
			}
		}
	}
	else
	{
		// If no montage assigned, snap directly
		CurrentWeapon->AttachToSheath();
	}

	OnCombatStanceChanged.Broadcast(false);
}

void UThreadTalesEquipmentComponent::SetMovementInputBlocked(bool bBlocked)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character) return;

	if (AController* Controller = Character->GetController())
	{
		Controller->SetIgnoreMoveInput(bBlocked);
	}

	if (bBlocked && Character->GetCharacterMovement())
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
	}
}

void UThreadTalesEquipmentComponent::OnEquipMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	SetMovementInputBlocked(false);
}

void UThreadTalesEquipmentComponent::PerformPrimaryAttack()
{
	if (!CurrentWeapon)
	{
		UE_LOG(LogThreadTalesCombat, Warning, TEXT("[Equipment] PerformPrimaryAttack: No weapon equipped!"));
		return;
	}

	if (CurrentWeapon->IsSheathed())
	{
		UE_LOG(LogThreadTalesCombat, Log, TEXT("[Equipment] Weapon is Sheathed -> Drawing Weapon first."));
		DrawWeapon();
		return;
	}

	if (CurrentWeapon->IsDrawn())
	{
		IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOwner());
		if (!ASI)
		{
			UE_LOG(LogThreadTalesCombat, Error, TEXT("[Equipment] Owner does NOT implement IAbilitySystemInterface!"));
			return;
		}

		UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
		if (!ASC)
		{
			UE_LOG(LogThreadTalesCombat, Error, TEXT("[Equipment] Owner has null AbilitySystemComponent!"));
			return;
		}

		// Check if a Melee combo ability is currently running
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.IsActive())
			{
				if (UThreadTalesGA_Melee* MeleeAbility = Cast<UThreadTalesGA_Melee>(Spec.GetPrimaryInstance()))
				{
					UE_LOG(LogThreadTalesCombat, Log, TEXT("[Equipment] Buffered Attack into active Melee Ability (PrimaryInstance)!"));
					MeleeAbility->NotifyComboInput();
					return;
				}

				for (UGameplayAbility* Inst : Spec.GetAbilityInstances())
				{
					if (UThreadTalesGA_Melee* MeleeAbility = Cast<UThreadTalesGA_Melee>(Inst))
					{
						UE_LOG(LogThreadTalesCombat, Log, TEXT("[Equipment] Buffered Attack into active Melee Ability (AbilityInstance)!"));
						MeleeAbility->NotifyComboInput();
						return;
					}
				}
			}
		}

		// Otherwise, trigger the appropriate attack ability (Melee or Ranged)
		FGameplayTag MeleeTag = FGameplayTag::RequestGameplayTag(FName("Ability.Melee"), false);
		FGameplayTag RangedTag = FGameplayTag::RequestGameplayTag(FName("Ability.Ranged"), false);
		FGameplayTag SlingshotTag = FGameplayTag::RequestGameplayTag(FName("Ability.Ranged.Slingshot"), false);

		FGameplayTagContainer AttackTags;
		if (MeleeTag.IsValid()) AttackTags.AddTag(MeleeTag);
		if (RangedTag.IsValid()) AttackTags.AddTag(RangedTag);
		if (SlingshotTag.IsValid()) AttackTags.AddTag(SlingshotTag);

		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Equipment] PerformPrimaryAttack trying tags: %s | ASC abilities count: %d"), *AttackTags.ToStringSimple(), ASC->GetActivatableAbilities().Num());
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			FString AbilityName = Spec.Ability ? Spec.Ability->GetName() : TEXT("NULL");
			FString TagsStr = Spec.Ability ? Spec.Ability->GetAssetTags().ToStringSimple() : TEXT("None");
			UE_LOG(LogThreadTalesCombat, Display, TEXT("   -> Ability: %s | Tags: %s | Active: %s"), *AbilityName, *TagsStr, Spec.IsActive() ? TEXT("YES") : TEXT("NO"));
		}

		bool bActivated = false;
		if (MeleeTag.IsValid() && ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(MeleeTag)))
		{
			bActivated = true;
		}
		else if (SlingshotTag.IsValid() && ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(SlingshotTag)))
		{
			bActivated = true;
		}
		else if (RangedTag.IsValid() && ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(RangedTag)))
		{
			bActivated = true;
		}

		UE_LOG(LogThreadTalesCombat, Log, TEXT("[Equipment] TryActivateAbilitiesByTag (Melee/Ranged): %s"), bActivated ? TEXT("SUCCESS") : TEXT("FAILED"));
	}
}

void UThreadTalesEquipmentComponent::ReleasePrimaryAttack()
{
	if (!CurrentWeapon) return;

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOwner());
	if (!ASI) return;

	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
	if (!ASC) return;

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.IsActive())
		{
			if (UThreadTalesGA_RangedAttack* RangedAbility = Cast<UThreadTalesGA_RangedAttack>(Spec.GetPrimaryInstance()))
			{
				RangedAbility->ReleaseAndFire();
				return;
			}

			for (UGameplayAbility* Inst : Spec.GetAbilityInstances())
			{
				if (UThreadTalesGA_RangedAttack* RangedAbility = Cast<UThreadTalesGA_RangedAttack>(Inst))
				{
					RangedAbility->ReleaseAndFire();
					return;
				}
			}
		}
	}
}

bool UThreadTalesEquipmentComponent::IsInCombatStance() const
{
	return CurrentWeapon != nullptr && CurrentWeapon->IsDrawn();
}

EWeaponCombatState UThreadTalesEquipmentComponent::GetCurrentWeaponCombatState() const
{
	return CurrentWeapon ? CurrentWeapon->GetWeaponState() : EWeaponCombatState::Sheathed;
}

void UThreadTalesEquipmentComponent::OnRep_CurrentWeapon(AThreadTalesWeapon* OldWeapon)
{
	if (CurrentWeapon)
	{
		OnWeaponEquipped.Broadcast(CurrentWeapon);
	}
}

void UThreadTalesEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UThreadTalesEquipmentComponent, CurrentWeapon);
}
