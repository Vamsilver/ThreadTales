#include "Characters/ThreadTalesEquipmentComponent.h"
#include "Characters/ThreadTalesBaseCharacter.h"
#include "Characters/ThreadTalesCharacter.h"
#include "ThreadTales.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Abilities/ThreadTalesAbilitySystemComponent.h"
#include "Combat/ThreadTalesWeapon.h"
#include "Abilities/ThreadTalesGA_Melee.h"
#include "Abilities/ThreadTalesGA_RangedAttack.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Controller.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/ThreadTalesMotionMatchingLibrary.h"

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
			if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
			{
				if (UCapsuleComponent* Capsule = Char->GetCapsuleComponent())
				{
					Capsule->IgnoreActorWhenMoving(CurrentWeapon, true);
				}
			}
			CurrentWeapon->Equip(GetOwner());
			CurrentWeapon->AddWeaponMappingContext();
			OnWeaponEquipped.Broadcast(CurrentWeapon);
			if (AThreadTalesBaseCharacter* TTChar = Cast<AThreadTalesBaseCharacter>(GetOwner()))
			{
				if (CurrentWeapon->IsDrawn())
				{
					TTChar->SetOverlayPoseByName(CurrentWeapon->GetOverlayPoseName());
				}
				else
				{
					TTChar->SetOverlayPoseByName(FName("Default"));
				}
			}
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Equipment] %s successfully equipped weapon '%s' (Class: %s)"),
				*GetNameSafe(GetOwner()), *CurrentWeapon->GetName(), *WeaponClass->GetName());
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
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Equipment] %s unequipping weapon '%s'"),
			*GetNameSafe(GetOwner()), *CurrentWeapon->GetName());
		CurrentWeapon->Unequip();
		CurrentWeapon->Destroy();
		CurrentWeapon = nullptr;
		OnCombatStanceChanged.Broadcast(false);

		if (AThreadTalesBaseCharacter* TTChar = Cast<AThreadTalesBaseCharacter>(GetOwner()))
		{
			TTChar->SetOverlayPoseByName(FName("Default"));
		}
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

	if (!CurrentWeapon)
	{
		UE_LOG(LogThreadTalesCombat, Warning, TEXT("[Equipment] %s ToggleCombatStance: No weapon equipped!"),
			*GetNameSafe(GetOwner()));
		return;
	}

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Equipment] %s ToggleCombatStance: CurrentState=%d (0=Sheathed, 1=Drawing, 2=Drawn, 3=Sheathing)"),
		*GetNameSafe(GetOwner()), (int32)CurrentWeapon->GetWeaponState());

	if (CurrentWeapon->GetWeaponState() == EWeaponCombatState::Drawing ||
	    CurrentWeapon->GetWeaponState() == EWeaponCombatState::Sheathing)
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Equipment] %s ToggleCombatStance ignored: weapon is transitioning (%d)"),
			*GetNameSafe(GetOwner()), (int32)CurrentWeapon->GetWeaponState());
		return;
	}

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
	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Equipment] %s DrawWeapon initiated (Montage: %s)"),
		*Character->GetName(), *GetNameSafe(DrawMontage));

	if (DrawMontage)
	{
		CurrentWeapon->SetWeaponState(EWeaponCombatState::Drawing);

		// Pre-emptively switch overlay pose so that when DrawMontage reaches BlendOutTriggerTime (PlayLength - BlendOutTime),
		// it seamlessly blends into the Katana overlay pose instead of dropping into unarmed!
		if (AThreadTalesBaseCharacter* TTChar = Cast<AThreadTalesBaseCharacter>(Character))
		{
			TTChar->SetOverlayPoseByName(CurrentWeapon->GetOverlayPoseName());
		}

		float Duration = Character->PlayAnimMontage(DrawMontage);
		if (Duration > 0.f)
		{
			// Seamless locomotion: do NOT block movement input or stop velocity.
			// Character continues running/walking at current speed with full 360 directional control.

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
		if (AThreadTalesBaseCharacter* TTChar = Cast<AThreadTalesBaseCharacter>(Character))
		{
			TTChar->SetOverlayPoseByName(CurrentWeapon->GetOverlayPoseName());
		}
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
	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Equipment] %s SheathWeapon initiated (Montage: %s)"),
		*Character->GetName(), *GetNameSafe(SheathMontage));

	if (SheathMontage)
	{
		CurrentWeapon->SetWeaponState(EWeaponCombatState::Sheathing);

		// Pre-emptively switch overlay pose to Default so that when SheathMontage reaches BlendOutTriggerTime (PlayLength - BlendOutTime),
		// it seamlessly blends into unarmed locomotion instead of bouncing back into the Katana overlay pose!
		if (AThreadTalesBaseCharacter* TTChar = Cast<AThreadTalesBaseCharacter>(Character))
		{
			TTChar->SetAimInputDown(false);
			TTChar->SetOverlayPoseByName(FName("Default"));
		}

		float Duration = Character->PlayAnimMontage(SheathMontage);
		if (Duration > 0.f)
		{
			// Seamless locomotion: do NOT block movement input or stop velocity.
			// Character continues running/walking at current speed with full 360 directional control.

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
		if (AThreadTalesBaseCharacter* TTChar = Cast<AThreadTalesBaseCharacter>(Character))
		{
			TTChar->SetAimInputDown(false);
			TTChar->SetOverlayPoseByName(FName("Default"));
		}
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
	// Deliberately no-op to preserve seamless player locomotion across weapon transitions.
	// Player retains full velocity and directional control during weapon draw and sheath.
}

void UThreadTalesEquipmentComponent::OnEquipMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (CurrentWeapon)
	{
		// Fail-safe: ensure scabbard/sheath is always returned to hip upon montage completion or interruption
		CurrentWeapon->AttachSheathToHip();

		if (CurrentWeapon->GetWeaponState() == EWeaponCombatState::Drawing)
		{
			CurrentWeapon->SetWeaponState(EWeaponCombatState::Drawn);
			CurrentWeapon->AttachToHand();
		}
		else if (CurrentWeapon->GetWeaponState() == EWeaponCombatState::Sheathing || CurrentWeapon->IsSheathed())
		{
			CurrentWeapon->SetWeaponState(EWeaponCombatState::Sheathed);
			CurrentWeapon->AttachToSheath();
		}

		if (AThreadTalesBaseCharacter* TTChar = Cast<AThreadTalesBaseCharacter>(GetOwner()))
		{
			if (CurrentWeapon->IsDrawn())
			{
				TTChar->SetOverlayPoseByName(CurrentWeapon->GetOverlayPoseName());
			}
			else
			{
				TTChar->SetOverlayPoseByName(FName("Default"));
			}
		}
	}
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

	// Phase protection: protect initial draw gesture (~0.35s) until weapon is physically attached to hand.
	if (CurrentWeapon->GetWeaponState() == EWeaponCombatState::Drawing)
	{
		UE_LOG(LogThreadTalesCombat, Verbose, TEXT("[Equipment] PerformPrimaryAttack: Weapon is still Drawing — waiting for Drawn phase."));
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
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character && Character->GetCharacterMovement() && (!Character->GetCharacterMovement()->IsMovingOnGround() || Character->GetCharacterMovement()->IsFalling()))
	{
		return;
	}

	// Phase protection: protect initial draw gesture (~0.35s) until weapon is physically attached to hand.
	if (CurrentWeapon && CurrentWeapon->GetWeaponState() == EWeaponCombatState::Drawing)
	{
		UE_LOG(LogThreadTalesCombat, Verbose, TEXT("[Equipment] PerformEvade: Weapon is still Drawing — waiting for Drawn phase."));
		return;
	}

	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
		{
			ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(ThreadTalesGameplayTags::TAG_Ability_Evade));
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
		if (AThreadTalesBaseCharacter* TTChar = Cast<AThreadTalesBaseCharacter>(GetOwner()))
		{
			if (CurrentWeapon->IsDrawn())
			{
				TTChar->SetOverlayPoseByName(CurrentWeapon->GetOverlayPoseName());
			}
			else
			{
				TTChar->SetOverlayPoseByName(FName("Default"));
			}
		}
	}
	else
	{
		if (AThreadTalesBaseCharacter* TTChar = Cast<AThreadTalesBaseCharacter>(GetOwner()))
		{
			TTChar->SetOverlayPoseByName(FName("Default"));
		}
	}
}

void UThreadTalesEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UThreadTalesEquipmentComponent, CurrentWeapon);
}
