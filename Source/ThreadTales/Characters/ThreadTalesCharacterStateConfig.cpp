// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesCharacterStateConfig.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"

UThreadTalesCharacterStateConfig::UThreadTalesCharacterStateConfig()
{
}

bool UThreadTalesCharacterStateConfig::ResolveInputRedirect(
	const ACharacter* Character,
	const UAbilitySystemComponent* ASC,
	const FGameplayTag& InInputTag,
	FGameplayTag& OutRedirectedTag) const
{
	if (!InInputTag.IsValid())
	{
		return false;
	}

	for (const FThreadTalesInputRedirectRule& Rule : InputRedirectRules)
	{
		if (!Rule.SourceInputTag.MatchesTagExact(InInputTag))
		{
			continue;
		}

		if (Character)
		{
			const UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
			if (MoveComp)
			{
				if (Rule.bRequiresGrounded && !MoveComp->IsMovingOnGround())
				{
					continue;
				}
				if (Rule.bRequiresInAir && !MoveComp->IsFalling())
				{
					continue;
				}
			}
		}

		if (ASC)
		{
			if (!Rule.RequiredTags.IsEmpty())
			{
				bool bHasRequiredTags = ASC->HasAllMatchingGameplayTags(Rule.RequiredTags);
				// Fail-safe: if required tags include State.Combat.InCombat, check Character combat stance directly
				if (!bHasRequiredTags && Character)
				{
					if (const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(Character))
					{
						if (TTChar->IsInCombatStance() && Rule.RequiredTags.HasTagExact(ThreadTalesGameplayTags::TAG_State_Combat_InCombat))
						{
							bHasRequiredTags = true;
						}
					}
				}

				if (!bHasRequiredTags)
				{
					continue;
				}
			}

			if (!Rule.BlockedTags.IsEmpty() && ASC->HasAnyMatchingGameplayTags(Rule.BlockedTags))
			{
				continue;
			}
		}
		else if (!Rule.RequiredTags.IsEmpty())
		{
			// Required tags specified but no ASC available - check Character stance
			if (Character)
			{
				if (const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(Character))
				{
					if (TTChar->IsInCombatStance() && Rule.RequiredTags.HasTagExact(ThreadTalesGameplayTags::TAG_State_Combat_InCombat))
					{
						OutRedirectedTag = Rule.TargetInputTag;
						return true;
					}
				}
			}
			continue;
		}

		OutRedirectedTag = Rule.TargetInputTag;
		return true;
	}

	return false;
}

bool UThreadTalesCharacterStateConfig::ShouldBlockTraversal(const UAbilitySystemComponent* ASC, const ACharacter* Character) const
{
	if (ASC)
	{
		if (ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_InCombat))
		{
			return true;
		}
		if (!BlockedTraversalTags.IsEmpty() && ASC->HasAnyMatchingGameplayTags(BlockedTraversalTags))
		{
			return true;
		}
	}

	if (Character)
	{
		if (const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(Character))
		{
			if (TTChar->IsInCombatStance())
			{
				return true;
			}
		}
	}

	return false;
}

bool UThreadTalesCharacterStateConfig::ShouldBlockParkour(const UAbilitySystemComponent* ASC, const ACharacter* Character) const
{
	if (ASC)
	{
		if (ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_InCombat))
		{
			return true;
		}
		if (!BlockedParkourTags.IsEmpty() && ASC->HasAnyMatchingGameplayTags(BlockedParkourTags))
		{
			return true;
		}
	}

	if (Character)
	{
		if (const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(Character))
		{
			if (TTChar->IsInCombatStance())
			{
				return true;
			}
		}
	}

	return false;
}
