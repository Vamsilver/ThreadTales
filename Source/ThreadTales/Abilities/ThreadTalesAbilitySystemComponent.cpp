// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesAbilitySystemComponent.h"

UThreadTalesAbilitySystemComponent::UThreadTalesAbilitySystemComponent()
{
	// Allows this component to be replicated across the network
	SetIsReplicated(true);
}

void UThreadTalesAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
}
