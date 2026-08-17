#include "ThreadTalesEquipmentComponent.h"
#include "ThreadTales.h"
#include "ThreadTalesGameplayTags.h"
#include "ThreadTalesAbilitySystemComponent.h"
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
	if (!GetOwner()->HasAuthority())
	{
		ServerToggleCombatStance();
		return;
	}

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

void UThreadTalesEquipmentComponent::ServerToggleCombatStance_Implementation()
{
	ToggleCombatStance();
}

void UThreadTalesEquipmentComponent::DrawWeapon()
{
	if (!GetOwner()->HasAuthority())
	{
		ServerDrawWeapon();
		return;
	}

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

	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
		{
			ASC->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_InCombat);
		}
	}

	OnCombatStanceChanged.Broadcast(true);
}

void UThreadTalesEquipmentComponent::ServerDrawWeapon_Implementation()
{
	DrawWeapon();
}

void UThreadTalesEquipmentComponent::SheathWeapon()
{
	if (!GetOwner()->HasAuthority())
	{
		ServerSheathWeapon();
		return;
	}

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

	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_InCombat);
		}
	}

	OnCombatStanceChanged.Broadcast(false);
}

void UThreadTalesEquipmentComponent::ServerSheathWeapon_Implementation()
{
	SheathWeapon();
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

	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		if (UThreadTalesAbilitySystemComponent* ASC = Cast<UThreadTalesAbilitySystemComponent>(ASI->GetAbilitySystemComponent()))
		{
			ASC->AbilityInputTagPressed(ThreadTalesGameplayTags::TAG_Input_Ability_Melee);
		}
	}
}

void UThreadTalesEquipmentComponent::ReleasePrimaryAttack()
{
	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		if (UThreadTalesAbilitySystemComponent* ASC = Cast<UThreadTalesAbilitySystemComponent>(ASI->GetAbilitySystemComponent()))
		{
			ASC->AbilityInputTagReleased(ThreadTalesGameplayTags::TAG_Input_Ability_Ranged);
		}
	}
}

void UThreadTalesEquipmentComponent::PerformEvade()
{
	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		if (UThreadTalesAbilitySystemComponent* ASC = Cast<UThreadTalesAbilitySystemComponent>(ASI->GetAbilitySystemComponent()))
		{
			ASC->AbilityInputTagPressed(ThreadTalesGameplayTags::TAG_Input_Ability_Evade);
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
