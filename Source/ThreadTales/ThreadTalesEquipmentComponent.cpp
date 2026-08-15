// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesEquipmentComponent.h"
#include "ThreadTales.h"
#include "ThreadTalesWeapon.h"
#include "ThreadTalesGA_Melee.h"
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

		// Otherwise, start the melee attack combo ability
		FGameplayTag MeleeTag = FGameplayTag::RequestGameplayTag(FName("Ability.Melee"));
		bool bActivated = ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(MeleeTag));
		UE_LOG(LogThreadTalesCombat, Log, TEXT("[Equipment] TryActivateAbilitiesByTag(Ability.Melee): %s"), bActivated ? TEXT("SUCCESS") : TEXT("FAILED (Ability not granted to ASC or blocked?)"));
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
