// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "ThreadTalesAbilitySystemComponent.generated.h"

/**
 * Base Ability System Component for the ThreadTales project.
 * Handles ability execution, tag management, and replication.
 */
UCLASS()
class THREADTALES_API UThreadTalesAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UThreadTalesAbilitySystemComponent();

	/** Called when the component is initialized on the owning actor */
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
};
