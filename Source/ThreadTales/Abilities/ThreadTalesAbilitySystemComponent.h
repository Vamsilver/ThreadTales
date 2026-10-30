// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "ThreadTalesAbilitySystemComponent.generated.h"

/**
 * Base Ability System Component for the ThreadTales project.
 * Handles ability execution, tag-based input dispatching, and replication.
 */
UCLASS()
class THREADTALES_API UThreadTalesAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UThreadTalesAbilitySystemComponent();

	/** Called when the component is initialized on the owning actor */
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	/** Dispatches input pressed event to matching abilities (activates or notifies WaitInputPress) */
	void AbilityInputTagPressed(const FGameplayTag& InputTag);

	/** Dispatches input released event to matching abilities (notifies WaitInputRelease) */
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	/** Helper to map InputTag to corresponding Ability Tag if DynamicAbilityTags are not explicitly set */
	static FGameplayTag GetAbilityTagForInputTag(const FGameplayTag& InputTag);

	/** Returns the most recently dispatched input tag */
	const FGameplayTag& GetLastPressedInputTag() const { return LastPressedInputTag; }

private:
	FGameplayTag LastPressedInputTag;
};
