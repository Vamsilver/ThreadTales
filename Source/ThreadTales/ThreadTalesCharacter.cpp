// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesCharacter.h"
#include "ThreadTales.h"

AThreadTalesCharacter::AThreadTalesCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MaxHealth = 100.0f;
}

void AThreadTalesCharacter::BeginPlay()
{
	Super::BeginPlay();
}
