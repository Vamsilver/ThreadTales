#include "Abilities/ThreadTalesAbilitySystemComponent.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
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
	else if (InputTag.MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Ability_Ranged_Fire))
	{
		return ThreadTalesGameplayTags::TAG_Ability_Ranged_Fire;
	}
	else if (InputTag.MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Ability_Evade))
	{
		return ThreadTalesGameplayTags::TAG_Ability_Evade;
	}
	else if (InputTag.MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Ability_Block))
	{
		return ThreadTalesGameplayTags::TAG_Ability_Block;
	}
	else if (InputTag.MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Ability_Jump))
	{
		return ThreadTalesGameplayTags::TAG_Ability_Movement_DoubleJump;
	}
	else if (InputTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(FName("InputTag.Ability.Interact"))))
	{
		return FGameplayTag::RequestGameplayTag(FName("Ability.Interact"));
	}

	return FGameplayTag::EmptyTag;
}

void UThreadTalesAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	LastPressedInputTag = InputTag;

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

	// Pass 1: If any matching ability is already active, dispatch input to it and do NOT activate new abilities!
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.Ability)
		{
			continue;
		}

		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		bool bMatches = Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag) || 
		                Spec.Ability->GetAssetTags().HasTag(InputTag) ||
		                Spec.Ability->AbilityTags.HasTag(InputTag);

		if (!bMatches && MatchingAbilityTag.IsValid())
		{
			bMatches = Spec.GetDynamicSpecSourceTags().HasTagExact(MatchingAbilityTag) ||
			           Spec.Ability->GetAssetTags().HasTagExact(MatchingAbilityTag) ||
			           Spec.Ability->AbilityTags.HasTagExact(MatchingAbilityTag);
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		if (bMatches && Spec.IsActive())
		{
			Spec.InputPressed = true;
			AbilitySpecInputPressed(Spec);
			return; // Handled by active ability (e.g. combo input or charged attack)
		}
	}

	// Pass 2: No matching ability is active. Activate only the first matching ability!
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.Ability)
		{
			continue;
		}

		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		bool bMatches = Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag) || 
		                Spec.Ability->GetAssetTags().HasTag(InputTag) ||
		                Spec.Ability->AbilityTags.HasTag(InputTag);

		if (!bMatches && MatchingAbilityTag.IsValid())
		{
			bMatches = Spec.GetDynamicSpecSourceTags().HasTagExact(MatchingAbilityTag) ||
			           Spec.Ability->GetAssetTags().HasTagExact(MatchingAbilityTag) ||
			           Spec.Ability->AbilityTags.HasTagExact(MatchingAbilityTag);
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		if (bMatches)
		{
			Spec.InputPressed = true;
			UE_LOG(LogThreadTales, Display, TEXT("[ASC] Found matching ability '%s' for input '%s'! Attempting TryActivateAbility..."),
				*Spec.Ability->GetName(), *InputTag.ToString());

			if (TryActivateAbility(Spec.Handle))
			{
				UE_LOG(LogThreadTales, Display, TEXT("[ASC] TryActivateAbility SUCCEEDED for '%s'!"), *Spec.Ability->GetName());
				return; // Successfully activated, exit immediately!
			}
			else
			{
				Spec.InputPressed = false;
				UE_LOG(LogThreadTales, Warning, TEXT("[ASC] TryActivateAbility FAILED for '%s'! (CanActivateAbility returned false)"), *Spec.Ability->GetName());
			}
		}
	}

	UE_LOG(LogThreadTales, Warning, TEXT("[ASC] No matching ability found or activated for InputTag '%s' (MatchingAbilityTag: '%s', Activatable Count: %d)"),
		*InputTag.ToString(), *MatchingAbilityTag.ToString(), ActivatableAbilities.Items.Num());
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

		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		bool bMatches = Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag) || 
		                Spec.Ability->GetAssetTags().HasTag(InputTag) ||
		                Spec.Ability->AbilityTags.HasTag(InputTag);

		if (!bMatches && MatchingAbilityTag.IsValid())
		{
			bMatches = Spec.GetDynamicSpecSourceTags().HasTagExact(MatchingAbilityTag) ||
			           Spec.Ability->GetAssetTags().HasTagExact(MatchingAbilityTag) ||
			           Spec.Ability->AbilityTags.HasTagExact(MatchingAbilityTag);
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		if (bMatches)
		{
			Spec.InputPressed = false;
			if (Spec.IsActive())
			{
				AbilitySpecInputReleased(Spec);
			}
		}
	}
}

