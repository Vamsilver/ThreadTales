// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ThreadTalesGameplayAbility.generated.h"

/**
 * Base class for all abilities in ThreadTales
 */
UCLASS()
class THREADTALES_API UThreadTalesGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UThreadTalesGameplayAbility();

	// Defines the type of ability (for UI or logic)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	FName AbilityName;
};
