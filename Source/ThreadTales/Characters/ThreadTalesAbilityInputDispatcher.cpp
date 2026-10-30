// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesAbilityInputDispatcher.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Abilities/ThreadTalesAbilitySystemComponent.h"
#include "Characters/ThreadTalesCharacterStateConfig.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
#include "Combat/ThreadTalesWeapon.h"
#include "Combat/ThreadTalesTargetLockComponent.h"
#include "Combat/ThreadTalesOverlayDebug.h"
#include "Input/ThreadTalesInputConfig.h"
#include "Input/ThreadTalesInputComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "ThreadTales.h"

UThreadTalesAbilityInputDispatcher::UThreadTalesAbilityInputDispatcher()
{
	PrimaryComponentTick.bCanEverTick = false;
}

AThreadTalesCharacter* UThreadTalesAbilityInputDispatcher::GetOwningCharacter() const
{
	return Cast<AThreadTalesCharacter>(GetOwner());
}

void UThreadTalesAbilityInputDispatcher::BindAbilityActions(
	UInputComponent* PlayerInputComponent,
	const UThreadTalesInputConfig* InputConfig,
	TArray<uint32>& OutBindHandles)
{
	if (!PlayerInputComponent || !InputConfig)
	{
		return;
	}

	if (UThreadTalesInputComponent* TTInputComponent = Cast<UThreadTalesInputComponent>(PlayerInputComponent))
	{
		TTInputComponent->BindAbilityActions(
			InputConfig,
			this,
			&UThreadTalesAbilityInputDispatcher::HandleInputTagPressed,
			&UThreadTalesAbilityInputDispatcher::HandleInputTagReleased,
			OutBindHandles
		);
	}
	else if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		for (const FThreadTalesInputAction& Action : InputConfig->AbilityInputActions)
		{
			if (Action.InputAction && Action.InputTag.IsValid())
			{
				EnhancedInputComponent->BindAction(Action.InputAction, ETriggerEvent::Started, this, &UThreadTalesAbilityInputDispatcher::HandleInputTagPressed, Action.InputTag);
				EnhancedInputComponent->BindAction(Action.InputAction, ETriggerEvent::Completed, this, &UThreadTalesAbilityInputDispatcher::HandleInputTagReleased, Action.InputTag);
			}
		}
	}
}

void UThreadTalesAbilityInputDispatcher::HandleInputTagPressed(FGameplayTag InputTag)
{
	AThreadTalesCharacter* Character = GetOwningCharacter();
	if (!Character || Character->IsStaggered())
	{
		return;
	}

	FGameplayTag ResolvedTag = InputTag;
	UThreadTalesAbilitySystemComponent* ASC = Cast<UThreadTalesAbilitySystemComponent>(Character->GetAbilitySystemComponent());

	if (const UThreadTalesCharacterStateConfig* StateConfig = Character->GetStateConfig())
	{
		if (!Character->IsClimbing())
		{
			FGameplayTag RedirectedTag;
			if (StateConfig->ResolveInputRedirect(Character, ASC, InputTag, RedirectedTag))
			{
				UE_LOG(LogThreadTales, Display, TEXT("[InputDispatcher] Input tag '%s' redirected to '%s' (Owner: %s)"),
					*InputTag.ToString(), *RedirectedTag.ToString(), *Character->GetName());
				ResolvedTag = RedirectedTag;
				ActiveInputRedirections.Add(InputTag, RedirectedTag);
			}
		}
	}

	if (ResolvedTag.MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Ability_Jump))
	{
		Character->Jump();
		return;
	}

	if (ResolvedTag.MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Crouch))
	{
		Character->RequestCrouchOrClimbDown();
		return;
	}

	if (ResolvedTag.MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Ability_TargetLock))
	{
		if (UThreadTalesTargetLockComponent* TargetLockComp = Character->GetTargetLockComponent())
		{
			TargetLockComp->ToggleTargetLock();
		}
	}

	if (ResolvedTag.MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Ability_Melee))
	{
		if (UThreadTalesEquipmentComponent* EquipComp = Character->GetEquipmentComponent())
		{
			if (AThreadTalesWeapon* Weapon = EquipComp->GetCurrentWeapon())
			{
				if (Weapon->IsSheathed())
				{
					UE_LOG(LogThreadTalesCombat, Display, TEXT("[InputDispatcher] Attack pressed while weapon sheathed -> initiating DrawWeapon on '%s'"), *Character->GetName());
					EquipComp->DrawWeapon();
					return;
				}
				else if (Weapon->IsDrawing() || Weapon->IsSheathing())
				{
					return;
				}
			}
		}
	}

	ResolvedTag = FThreadTalesOverlayDebug::ResolveWeaponInputTag(Character, ResolvedTag);

	if (ResolvedTag.MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Ability_Ranged) ||
	    ResolvedTag.MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Ability_Block))
	{
		Character->StopSprinting();
	}

	if (ASC)
	{
		ASC->AbilityInputTagPressed(ResolvedTag);
	}
}

void UThreadTalesAbilityInputDispatcher::HandleInputTagReleased(FGameplayTag InputTag)
{
	AThreadTalesCharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return;
	}

	FGameplayTag ResolvedTag = InputTag;

	if (const FGameplayTag* FoundRedirectedTag = ActiveInputRedirections.Find(InputTag))
	{
		ResolvedTag = *FoundRedirectedTag;
		ActiveInputRedirections.Remove(InputTag);
	}

	if (ResolvedTag.MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Ability_Jump))
	{
		Character->StopJumping();
	}

	if (ResolvedTag.MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Sprint))
	{
		Character->StopSprinting();
	}

	ResolvedTag = FThreadTalesOverlayDebug::ResolveWeaponInputTag(Character, ResolvedTag);

	if (UThreadTalesAbilitySystemComponent* ASC = Cast<UThreadTalesAbilitySystemComponent>(Character->GetAbilitySystemComponent()))
	{
		ASC->AbilityInputTagReleased(ResolvedTag);
	}
}
