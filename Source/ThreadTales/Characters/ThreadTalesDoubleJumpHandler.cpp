// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesDoubleJumpHandler.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Abilities/ThreadTalesAbilitySystemComponent.h"
#include "Characters/ThreadTalesCharacterStateConfig.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "UObject/ConstructorHelpers.h"
#include "ThreadTales.h"

UThreadTalesDoubleJumpHandler::UThreadTalesDoubleJumpHandler()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Default Directional Double Jump Montages
	static ConstructorHelpers::FObjectFinder<UAnimMontage> DJNeutralObj(TEXT("/Game/Animations/KatanaAnimations_Retarget/Montages/DoubleJump/AM_DoubleJump_0.AM_DoubleJump_0"));
	if (DJNeutralObj.Succeeded())
	{
		DoubleJumpMontage = DJNeutralObj.Object;
		DoubleJumpNeutralMontage = DJNeutralObj.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> DJForwardObj(TEXT("/Game/Animations/KatanaAnimations_Retarget/Montages/DoubleJump/AM_DoubleJump_F_0.AM_DoubleJump_F_0"));
	if (DJForwardObj.Succeeded())
	{
		DoubleJumpForwardMontage = DJForwardObj.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> DJBackwardObj(TEXT("/Game/Animations/KatanaAnimations_Retarget/Montages/DoubleJump/AM_DoubleJump_B_180.AM_DoubleJump_B_180"));
	if (DJBackwardObj.Succeeded())
	{
		DoubleJumpBackwardMontage = DJBackwardObj.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> DJLeftObj(TEXT("/Game/Animations/KatanaAnimations_Retarget/Montages/DoubleJump/AM_DoubleJump_L_90.AM_DoubleJump_L_90"));
	if (DJLeftObj.Succeeded())
	{
		DoubleJumpLeftMontage = DJLeftObj.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> DJRightObj(TEXT("/Game/Animations/KatanaAnimations_Retarget/Montages/DoubleJump/AM_DoubleJump_R_90.AM_DoubleJump_R_90"));
	if (DJRightObj.Succeeded())
	{
		DoubleJumpRightMontage = DJRightObj.Object;
	}
}

AThreadTalesCharacter* UThreadTalesDoubleJumpHandler::GetOwningCharacter() const
{
	return Cast<AThreadTalesCharacter>(GetOwner());
}

bool UThreadTalesDoubleJumpHandler::IsDoubleJumping() const
{
	if (const AThreadTalesCharacter* Character = GetOwningCharacter())
	{
		return Character->JumpCurrentCount > 1;
	}
	return false;
}

void UThreadTalesDoubleJumpHandler::PerformDoubleJump()
{
	if (AThreadTalesCharacter* Character = GetOwningCharacter())
	{
		if (Character->CanJump())
		{
			Character->Jump();
		}
	}
}

bool UThreadTalesDoubleJumpHandler::OnDoubleJump()
{
	AThreadTalesCharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return false;
	}

	bool bActivatedViaGAS = false;
	if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
	{
		bActivatedViaGAS = ASC->TryActivateAbilitiesByTag(
			FGameplayTagContainer(ThreadTalesGameplayTags::TAG_Ability_Movement_DoubleJump)
		);
	}

	if (!bActivatedViaGAS)
	{
		UE_LOG(LogThreadTales, Warning, TEXT("[DoubleJump] GAS failed to activate Double Jump ability for %s! Check DefaultAbilities or stuck abilities."), *Character->GetName());
		return false;
	}

	OnDoubleJumped.Broadcast(Character);
	BP_OnDoubleJump();
	return true;
}

void UThreadTalesDoubleJumpHandler::ResetDoubleJumpOnLanded()
{
	bHasDoubleJumpedInAir = false;

	if (AThreadTalesCharacter* Character = GetOwningCharacter())
	{
		// Softly blend out double jump montage if still playing so landing transitions cleanly
		if (Character->GetMesh())
		{
			if (UAnimInstance* AnimInst = Character->GetMesh()->GetAnimInstance())
			{
				AnimInst->Montage_Stop(LandedMontageStopBlendTime);
			}
		}

		if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
		{
			FGameplayTagContainer TagsToCancel;
			TagsToCancel.AddTag(ThreadTalesGameplayTags::TAG_Ability_Movement_DoubleJump);
			ASC->CancelAbilities(&TagsToCancel);
		}
	}
}

bool UThreadTalesDoubleJumpHandler::TryTriggerInAirJump()
{
	AThreadTalesCharacter* Character = GetOwningCharacter();
	if (!Character || !Character->GetCharacterMovement())
	{
		return false;
	}

	if (Character->GetCharacterMovement()->IsFalling() && !bHasDoubleJumpedInAir)
	{
		// Suppress double jump during active regrab cooldown after wall eject (allows kick-off momentum to play out)
		if (Character->IsRegrabCooldownActive())
		{
			return false;
		}

		// Double jump is strictly forbidden when weapon is drawn in hand or combat/traversal is blocked
		if (Character->IsInCombatStance() || Character->IsTraversalBlocked())
		{
			return false;
		}

		const bool bTraversalBlocked = Character->GetStateConfig() && 
			Character->GetStateConfig()->ShouldBlockTraversal(Character->GetAbilitySystemComponent(), Character);

		if (Character->JumpCurrentCount < MaxJumpCount && !bTraversalBlocked && Character->CanSpendStamina())
		{
			UE_LOG(LogThreadTales, Warning, TEXT("[DoubleJump] %s Jump() in air -> Triggering OnDoubleJump!"), *Character->GetName());
			return OnDoubleJump();
		}
	}
	return false;
}
