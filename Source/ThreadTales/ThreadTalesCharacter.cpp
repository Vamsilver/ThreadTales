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

void AThreadTalesCharacter::DebugSwitchCharacter(TSubclassOf<AThreadTalesCharacter> NewCharacterClass)
{
	if (!NewCharacterClass)
	{
		UE_LOG(LogThreadTales, Warning, TEXT("[Debug] DebugSwitchCharacter failed: NewCharacterClass is null!"));
		return;
	}

	if (GetClass() == NewCharacterClass)
	{
		UE_LOG(LogThreadTales, Log, TEXT("[Debug] Already playing as %s!"), *NewCharacterClass->GetName());
		return;
	}

	AController* SavedController = GetController();
	if (!SavedController)
	{
		UE_LOG(LogThreadTales, Warning, TEXT("[Debug] DebugSwitchCharacter failed: Character has no controller!"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	FTransform SpawnTransform = GetActorTransform();
	FRotator ControlRot = SavedController->GetControlRotation();

	// Preserve references to switch back
	TSubclassOf<AThreadTalesCharacter> SavedMaleClass = DebugMaleClass;
	TSubclassOf<AThreadTalesCharacter> SavedFemaleClass = DebugFemaleClass;

	// Unpossess current character
	SavedController->UnPossess();

	// Spawn new character at the exact same transform
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AThreadTalesCharacter* NewChar = World->SpawnActor<AThreadTalesCharacter>(NewCharacterClass, SpawnTransform, SpawnParams);
	if (NewChar)
	{
		// Pass debug references forward if not set on the new instance
		if (!NewChar->DebugMaleClass) NewChar->DebugMaleClass = SavedMaleClass;
		if (!NewChar->DebugFemaleClass) NewChar->DebugFemaleClass = SavedFemaleClass;

		SavedController->Possess(NewChar);
		SavedController->SetControlRotation(ControlRot);
		UE_LOG(LogThreadTales, Display, TEXT("[Debug] Successfully switched character to: %s"), *NewCharacterClass->GetName());
	}

	// Clean up old character's equipment so weapon is destroyed
	if (EquipmentComponent)
	{
		EquipmentComponent->UnequipWeapon();
	}

	// Destroy old character
	Destroy();
}

void AThreadTalesCharacter::DebugSwitchToMale()
{
	if (DebugMaleClass)
	{
		DebugSwitchCharacter(DebugMaleClass);
	}
	else
	{
		UE_LOG(LogThreadTales, Warning, TEXT("[Debug] DebugSwitchToMale: DebugMaleClass is not assigned on character!"));
	}
}

void AThreadTalesCharacter::DebugSwitchToFemale()
{
	if (DebugFemaleClass)
	{
		DebugSwitchCharacter(DebugFemaleClass);
	}
	else
	{
		UE_LOG(LogThreadTales, Warning, TEXT("[Debug] DebugSwitchToFemale: DebugFemaleClass is not assigned on character!"));
	}
}

void AThreadTalesCharacter::DebugToggleGender()
{
	if (DebugMaleClass && IsA(DebugMaleClass))
	{
		DebugSwitchToFemale();
	}
	else
	{
		DebugSwitchToMale();
	}
}
