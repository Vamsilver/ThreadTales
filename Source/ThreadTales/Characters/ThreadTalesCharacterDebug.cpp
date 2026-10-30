// Copyright ThreadTales. All Rights Reserved.

#include "Characters/ThreadTalesCharacterDebug.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
#include "ThreadTales.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"

#include "GameFramework/CharacterMovementComponent.h"

void UThreadTalesCharacterDebug::SwitchCharacter(AThreadTalesCharacter* SourceCharacter, TSubclassOf<AThreadTalesCharacter> NewCharacterClass)
{
#if !UE_BUILD_SHIPPING
	if (!SourceCharacter)
	{
		UE_LOG(LogThreadTales, Warning, TEXT("[Debug] SwitchCharacter failed: SourceCharacter is null!"));
		return;
	}

	if (!NewCharacterClass)
	{
		UE_LOG(LogThreadTales, Warning, TEXT("[Debug] SwitchCharacter failed: NewCharacterClass is null!"));
		return;
	}

	// AAA Co-op Invariant: If client initiates character switch, route through authoritative Server RPC
	if (!SourceCharacter->HasAuthority())
	{
		SourceCharacter->ServerDebugSwitchCharacter(NewCharacterClass);
		return;
	}

	if (SourceCharacter->GetClass() == NewCharacterClass)
	{
		UE_LOG(LogThreadTales, Log, TEXT("[Debug] Already playing as %s!"), *NewCharacterClass->GetName());
		return;
	}

	AController* SavedController = SourceCharacter->GetController();
	if (!SavedController)
	{
		UE_LOG(LogThreadTales, Warning, TEXT("[Debug] SwitchCharacter failed: Character has no controller!"));
		return;
	}

	UWorld* World = SourceCharacter->GetWorld();
	if (!World)
	{
		return;
	}

	const FTransform SpawnTransform = SourceCharacter->GetActorTransform();
	const FRotator ControlRot = SavedController->GetControlRotation();

	// Spawn new character at the exact same transform BEFORE unpossessing to prevent orphaned controller softlock
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AThreadTalesCharacter* NewChar = World->SpawnActor<AThreadTalesCharacter>(NewCharacterClass, SpawnTransform, SpawnParams);
	if (!NewChar)
	{
		UE_LOG(LogThreadTales, Error, TEXT("[Debug] SwitchCharacter failed: Could not spawn actor of class %s! Aborting to prevent player softlock."), *NewCharacterClass->GetName());
		return;
	}

	// Bilateral collision suppression to prevent capsule penetration ejection/jitter during possession
	SourceCharacter->SetActorEnableCollision(false);
	SourceCharacter->MoveIgnoreActorAdd(NewChar);
	NewChar->MoveIgnoreActorAdd(SourceCharacter);

	// Transfer kinetic state and movement mode
	if (UCharacterMovementComponent* SrcMove = SourceCharacter->GetCharacterMovement())
	{
		if (UCharacterMovementComponent* NewMove = NewChar->GetCharacterMovement())
		{
			NewMove->Velocity = SrcMove->Velocity;
			NewMove->SetMovementMode(SrcMove->MovementMode);
		}
	}

	// Safely unpossess and possess new character
	SavedController->UnPossess();

	SavedController->Possess(NewChar);
	SavedController->SetControlRotation(ControlRot);
	UE_LOG(LogThreadTales, Display, TEXT("[Debug] Successfully switched character to: %s"), *NewCharacterClass->GetName());

	// Clean up old character's equipment so weapon is destroyed
	if (UThreadTalesEquipmentComponent* EquipComp = SourceCharacter->GetEquipmentComponent())
	{
		EquipComp->UnequipWeapon();
	}

	// Destroy old character safely
	SourceCharacter->Destroy();
#endif
}

TSubclassOf<AThreadTalesCharacter> UThreadTalesCharacterDebug::CachedDebugMaleClass = nullptr;
TSubclassOf<AThreadTalesCharacter> UThreadTalesCharacterDebug::CachedDebugFemaleClass = nullptr;

TSubclassOf<AThreadTalesCharacter> UThreadTalesCharacterDebug::GetMaleClass()
{
	return CachedDebugMaleClass;
}

TSubclassOf<AThreadTalesCharacter> UThreadTalesCharacterDebug::GetFemaleClass()
{
	return CachedDebugFemaleClass;
}

void UThreadTalesCharacterDebug::SetDebugClasses(TSubclassOf<AThreadTalesCharacter> InMaleClass, TSubclassOf<AThreadTalesCharacter> InFemaleClass)
{
	CachedDebugMaleClass = InMaleClass;
	CachedDebugFemaleClass = InFemaleClass;
}

void UThreadTalesCharacterDebug::SwitchToMale(AThreadTalesCharacter* SourceCharacter)
{
#if !UE_BUILD_SHIPPING
	if (!SourceCharacter) return;
	TSubclassOf<AThreadTalesCharacter> MaleClass = GetMaleClass();
	if (MaleClass)
	{
		SwitchCharacter(SourceCharacter, MaleClass);
	}
	else
	{
		UE_LOG(LogThreadTales, Warning, TEXT("[Debug] SwitchToMale: Male class not configured."));
	}
#endif
}

void UThreadTalesCharacterDebug::SwitchToFemale(AThreadTalesCharacter* SourceCharacter)
{
#if !UE_BUILD_SHIPPING
	if (!SourceCharacter) return;
	TSubclassOf<AThreadTalesCharacter> FemaleClass = GetFemaleClass();
	if (FemaleClass)
	{
		SwitchCharacter(SourceCharacter, FemaleClass);
	}
	else
	{
		UE_LOG(LogThreadTales, Warning, TEXT("[Debug] SwitchToFemale: Female class not configured."));
	}
#endif
}

void UThreadTalesCharacterDebug::ToggleGender(AThreadTalesCharacter* SourceCharacter)
{
#if !UE_BUILD_SHIPPING
	if (!SourceCharacter) return;

	if (SourceCharacter->GetAlternateCharacterClass())
	{
		SwitchCharacter(SourceCharacter, SourceCharacter->GetAlternateCharacterClass());
		return;
	}

	if (CachedDebugMaleClass && CachedDebugFemaleClass)
	{
		const bool bIsCurrentlyMale = SourceCharacter->IsA(CachedDebugMaleClass);
		SwitchCharacter(SourceCharacter, bIsCurrentlyMale ? CachedDebugFemaleClass : CachedDebugMaleClass);
		return;
	}

	UE_LOG(LogThreadTales, Warning, TEXT("[Debug] ToggleGender: AlternateCharacterClass not set on %s. Please assign it in Blueprint Details panel."), *GetNameSafe(SourceCharacter));
#endif
}
