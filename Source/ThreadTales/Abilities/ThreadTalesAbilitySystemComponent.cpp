// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesAbilitySystemComponent.h"
#include "ThreadTalesGameplayTags.h"
#include "ThreadTalesEquipmentComponent.h"
#include "ThreadTales.h"

UThreadTalesAbilitySystemComponent::UThreadTalesAbilitySystemComponent()
{
	// Allows this component to be replicated across the network
	SetIsReplicated(true);
}

void UThreadTalesAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
}

FGameplayTag UThreadTalesAbilitySystemComponent::GetAbilityTagForInputTag(const FGameplayTag& InputTag)
{
	if (InputTag.MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Ability_Melee))
	{
		return ThreadTalesGameplayTags::TAG_Ability_Melee;
	}
	else if (InputTag.MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Ability_Ranged))
	{
		return ThreadTalesGameplayTags::TAG_Ability_Ranged;
	}
	else if (InputTag.MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Ability_Evade))
	{
		return ThreadTalesGameplayTags::TAG_Ability_Evade;
	}
	else if (InputTag.MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Ability_Jump))
	{
		return ThreadTalesGameplayTags::TAG_Ability_Movement_DoubleJump;
	}

	return FGameplayTag::EmptyTag;
}

void UThreadTalesAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	if (InputTag.MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Ability_ToggleStance))
	{
		AActor* Avatar = GetAvatarActor();
		if (Avatar)
		{
			if (UThreadTalesEquipmentComponent* EquipComp = Avatar->FindComponentByClass<UThreadTalesEquipmentComponent>())
			{
				EquipComp->ToggleCombatStance();
			}
		}
	}

	const FGameplayTag MatchingAbilityTag = GetAbilityTagForInputTag(InputTag);

	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.Ability)
		{
			continue;
		}

		bool bMatches = Spec.DynamicAbilityTags.HasTagExact(InputTag) || 
		                Spec.Ability->GetAssetTags().HasTag(InputTag);

		if (!bMatches && MatchingAbilityTag.IsValid())
		{
			bMatches = Spec.DynamicAbilityTags.HasTag(MatchingAbilityTag) ||
			           Spec.Ability->GetAssetTags().HasTag(MatchingAbilityTag);
		}

		if (bMatches)
		{
			if (Spec.IsActive())
			{
				AbilitySpecInputPressed(Spec);
			}
			else
			{
				TryActivateAbility(Spec.Handle);
			}
		}
	}
}

void UThreadTalesAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	const FGameplayTag MatchingAbilityTag = GetAbilityTagForInputTag(InputTag);

	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.Ability)
		{
			continue;
		}

		bool bMatches = Spec.DynamicAbilityTags.HasTagExact(InputTag) || 
		                Spec.Ability->GetAssetTags().HasTag(InputTag);

		if (!bMatches && MatchingAbilityTag.IsValid())
		{
			bMatches = Spec.DynamicAbilityTags.HasTag(MatchingAbilityTag) ||
			           Spec.Ability->GetAssetTags().HasTag(MatchingAbilityTag);
		}

		if (bMatches)
		{
			AbilitySpecInputReleased(Spec);
		}
	}
}
