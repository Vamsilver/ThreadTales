// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesCharacter.h"
#include "ThreadTales.h"
#include "ThreadTalesAbilitySystemComponent.h"
#include "ThreadTalesAttributeSet.h"
#include "ThreadTalesEquipmentComponent.h"

AThreadTalesCharacter::AThreadTalesCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	MaxHealth = 100.0f;

	// Create ability system component, and set it to be explicitly replicated
	AbilitySystemComponent = CreateDefaultSubobject<UThreadTalesAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// Create the attribute set, this replicates by default
	AttributeSet = CreateDefaultSubobject<UThreadTalesAttributeSet>(TEXT("AttributeSet"));

	// Create equipment manager
	EquipmentComponent = CreateDefaultSubobject<UThreadTalesEquipmentComponent>(TEXT("EquipmentComponent"));
}

UAbilitySystemComponent* AThreadTalesCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AThreadTalesCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Server-side initialization
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void AThreadTalesCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	// Client-side initialization
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void AThreadTalesCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AThreadTalesCharacter::ActivateAbilityByTag(FGameplayTag ContainerTag)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(ContainerTag));
	}
}
