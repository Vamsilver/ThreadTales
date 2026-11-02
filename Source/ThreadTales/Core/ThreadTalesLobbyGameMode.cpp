// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesLobbyGameMode.h"
#include "ThreadTalesPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"

AThreadTalesLobbyGameMode::AThreadTalesLobbyGameMode()
	: GameplayLevelPath(TEXT("/Game/LvL/LvL_Prototype/LvL_Blocking?listen"))
{
	DefaultPawnClass = nullptr;
	PlayerStateClass = AThreadTalesPlayerState::StaticClass();
	bUseSeamlessTravel = true;
}

void AThreadTalesLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	NotifyLobbyStateChanged();
}

void AThreadTalesLobbyGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	if (Exiting)
	{
		AThreadTalesPlayerState* PS = Exiting->GetPlayerState<AThreadTalesPlayerState>();
		if (PS)
		{
			PS->Server_SelectRole(EThreadTalesCharacterRole::None);
			PS->Server_SetReady(false);
		}
	}

	NotifyLobbyStateChanged();
}

bool AThreadTalesLobbyGameMode::StartGame()
{
	if (!HasAuthority())
	{
		return false;
	}

	if (!CanStartGame())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return World->ServerTravel(GameplayLevelPath, true);
}

bool AThreadTalesLobbyGameMode::CanStartGame() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	AGameStateBase* GS = World->GetGameState();
	if (!GS || GS->PlayerArray.Num() != 2)
	{
		return false;
	}

	AThreadTalesPlayerState* P1State = Cast<AThreadTalesPlayerState>(GS->PlayerArray[0]);
	AThreadTalesPlayerState* P2State = Cast<AThreadTalesPlayerState>(GS->PlayerArray[1]);

	if (!P1State || !P2State)
	{
		return false;
	}

	if (!P1State->IsReady() || !P2State->IsReady())
	{
		return false;
	}

	const EThreadTalesCharacterRole Role1 = P1State->GetSelectedRole();
	const EThreadTalesCharacterRole Role2 = P2State->GetSelectedRole();

	if (Role1 == EThreadTalesCharacterRole::None || Role2 == EThreadTalesCharacterRole::None)
	{
		return false;
	}

	return (Role1 != Role2);
}

void AThreadTalesLobbyGameMode::NotifyLobbyStateChanged()
{
	UWorld* World = GetWorld();
	const int32 NumPlayers = (World && World->GetGameState()) ? World->GetGameState()->PlayerArray.Num() : 0;
	OnLobbyUpdated.Broadcast(NumPlayers, CanStartGame());
}
