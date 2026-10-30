// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ThreadTalesCharacterStateConfig.generated.h"

class ACharacter;
class UAbilitySystemComponent;

/**
 * Rule for contextually redirecting an ability input tag based on character state and active tags.
 */
USTRUCT(BlueprintType)
struct FThreadTalesInputRedirectRule
{
	GENERATED_BODY()

	/** The input tag triggered by the player (e.g. InputTag.Ability.Movement.Jump) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Redirect", meta = (Categories = "InputTag"))
	FGameplayTag SourceInputTag;

	/** The replacement input tag to dispatch instead (e.g. InputTag.Ability.Combat.Evade) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Redirect", meta = (Categories = "InputTag"))
	FGameplayTag TargetInputTag;

	/** All of these tags must be present on the character / ASC for redirection to apply (e.g. State.Combat.InCombat) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Redirect")
	FGameplayTagContainer RequiredTags;

	/** If any of these tags are present, redirection is blocked */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Redirect")
	FGameplayTagContainer BlockedTags;

	/** If true, redirection only occurs when character is on ground (IsMovingOnGround() == true) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Redirect")
	bool bRequiresGrounded = true;

	/** If true, redirection only occurs when character is airborne (IsFalling() == true) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Redirect")
	bool bRequiresInAir = false;
};

/**
 * Data Asset governing character state configuration:
 * - Blocked traversal and parkour gameplay tags (e.g. while in combat)
 * - Dynamic input redirection rules (e.g. Space -> Evade when in combat on ground)
 */
UCLASS(BlueprintType, Const)
class THREADTALES_API UThreadTalesCharacterStateConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UThreadTalesCharacterStateConfig();

	/** Tags that block ledge climbing and traversal when present on character ASC */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State|Traversal")
	FGameplayTagContainer BlockedTraversalTags;

	/** Tags that block GASPALS horizontal parkour (vault, hurdle, mantle) when present on character ASC */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State|Traversal")
	FGameplayTagContainer BlockedParkourTags;

	/** List of rules evaluated sequentially to redirect input tags */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State|Input", meta = (TitleProperty = "SourceInputTag"))
	TArray<FThreadTalesInputRedirectRule> InputRedirectRules;

	/**
	 * Checks if an incoming input tag should be redirected based on active character state and ASC tags.
	 * @param Character The character providing locomotion/movement state.
	 * @param ASC The ability system component providing gameplay tags.
	 * @param InInputTag The incoming input tag.
	 * @param OutRedirectedTag The redirected tag if matching rule found.
	 * @return True if a redirection rule was applied.
	 */
	bool ResolveInputRedirect(const ACharacter* Character, const UAbilitySystemComponent* ASC, const FGameplayTag& InInputTag, FGameplayTag& OutRedirectedTag) const;

	/** Returns true if character has any tags blocking vertical traversal / climbing */
	bool ShouldBlockTraversal(const UAbilitySystemComponent* ASC, const ACharacter* Character = nullptr) const;

	/** Returns true if character has any tags blocking horizontal parkour */
	bool ShouldBlockParkour(const UAbilitySystemComponent* ASC, const ACharacter* Character = nullptr) const;
};
