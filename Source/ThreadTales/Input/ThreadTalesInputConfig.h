// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ThreadTalesInputConfig.generated.h"

class UInputAction;

/**
 * Struct linking an Enhanced Input Action to a Gameplay Tag.
 */
USTRUCT(BlueprintType)
struct FThreadTalesInputAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

/**
 * Data Asset that stores input configuration and maps Input Actions to Gameplay Tags.
 */
UCLASS(BlueprintType, Const)
class THREADTALES_API UThreadTalesInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UThreadTalesInputConfig();

	/** Returns the InputAction mapped to the specified native tag */
	const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

	/** Returns the InputAction mapped to the specified ability tag */
	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

	/** List of input actions used for native pawn/character actions (Move, Look) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native", meta = (TitleProperty = "InputAction"))
	TArray<FThreadTalesInputAction> NativeInputActions;

	/** List of input actions that trigger Gameplay Abilities via Input Tags */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities", meta = (TitleProperty = "InputAction"))
	TArray<FThreadTalesInputAction> AbilityInputActions;
};
