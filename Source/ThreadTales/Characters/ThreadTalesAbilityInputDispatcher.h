// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ThreadTalesAbilityInputDispatcher.generated.h"

class AThreadTalesCharacter;
class UThreadTalesInputConfig;

/**
 * Component handling Enhanced Input to GAS GameplayTag translation,
 * state-driven input redirection (CharacterStateConfig), and contextual action routing.
 * Extracted from AThreadTalesCharacter to decouple input routing from the character pawn.
 */
UCLASS(ClassGroup=(ThreadTales), meta=(BlueprintSpawnableComponent, DisplayName="ThreadTales Ability Input Dispatcher"))
class THREADTALES_API UThreadTalesAbilityInputDispatcher : public UActorComponent
{
	GENERATED_BODY()

public:
	UThreadTalesAbilityInputDispatcher();

	/** Binds Input Actions from InputConfig to this dispatcher on the InputComponent */
	void BindAbilityActions(
		class UInputComponent* PlayerInputComponent,
		const UThreadTalesInputConfig* InputConfig,
		TArray<uint32>& OutBindHandles
	);

	/** Handles input tag pressed event */
	void HandleInputTagPressed(FGameplayTag InputTag);

	/** Handles input tag released event */
	void HandleInputTagReleased(FGameplayTag InputTag);

protected:
	/** Helper to obtain owning AThreadTalesCharacter */
	AThreadTalesCharacter* GetOwningCharacter() const;

	/** Tracks active input tag redirections between Pressed and Released events */
	TMap<FGameplayTag, FGameplayTag> ActiveInputRedirections;
};
