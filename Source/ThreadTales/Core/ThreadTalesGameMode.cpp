// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesGameMode.h"
#include "ThreadTalesPlayerState.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerController.h"

AThreadTalesGameMode::AThreadTalesGameMode()
{
	bUseSeamlessTravel = true;
	PlayerStateClass = AThreadTalesPlayerState::StaticClass();

	static ConstructorHelpers::FClassFinder<APawn> MalePawnFinder(TEXT("/Game/Blueprints/Characters/BP_HeroMale.BP_HeroMale_C"));
	if (MalePawnFinder.Succeeded())
	{
		HeroMaleClass = MalePawnFinder.Class;
		DefaultPawnClass = HeroMaleClass;
	}

	static ConstructorHelpers::FClassFinder<APawn> FemalePawnFinder(TEXT("/Game/Blueprints/Characters/BP_HeroFemale.BP_HeroFemale_C"));
	if (FemalePawnFinder.Succeeded())
	{
		HeroFemaleClass = FemalePawnFinder.Class;
	}
}

UClass* AThreadTalesGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (InController)
	{
		AThreadTalesPlayerState* TTPS = InController->GetPlayerState<AThreadTalesPlayerState>();
		if (TTPS)
		{
			const EThreadTalesCharacterRole SelectedCharRole = TTPS->GetSelectedRole();
			if (SelectedCharRole == EThreadTalesCharacterRole::Archer_Female && HeroFemaleClass)
			{
				return HeroFemaleClass.Get();
			}
			if (SelectedCharRole == EThreadTalesCharacterRole::Swordsman_Male && HeroMaleClass)
			{
				return HeroMaleClass.Get();
			}
		}

		// Fallback for standalone/direct PIE testing without lobby selection
		APlayerController* PC = Cast<APlayerController>(InController);
		if (PC)
		{
			ULocalPlayer* LP = PC->GetLocalPlayer();
			const bool bIsFirstPlayer = LP ? (LP->GetControllerId() == 0) : (GetWorld() && InController == GetWorld()->GetFirstPlayerController());
			if (bIsFirstPlayer)
			{
				if (HeroMaleClass)
				{
					return HeroMaleClass.Get();
				}
			}
			else
			{
				if (HeroFemaleClass)
				{
					return HeroFemaleClass.Get();
				}
			}
		}
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void AThreadTalesGameMode::GenericPlayerInitialization(AController* C)
{
	Super::GenericPlayerInitialization(C);

	APlayerController* PC = Cast<APlayerController>(C);
	if (PC && PC->IsLocalPlayerController())
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(false);
	}
}

void AThreadTalesGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	APlayerController* PC = Cast<APlayerController>(NewPlayer);
	if (PC && PC->IsLocalPlayerController())
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(false);
	}
}
