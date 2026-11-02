// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ThreadTalesLobbyGameMode.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnThreadTalesLobbyUpdated, int32, CurrentPlayers, bool, bCanStartGame);

/**
 * Lobby Game Mode for pre-game character selection and co-op coordination.
 * Manages player readiness, mutual role exclusivity, and seamless level travel.
 */
UCLASS()
class THREADTALES_API AThreadTalesLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AThreadTalesLobbyGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Called by the Host to travel to the gameplay map when all conditions are satisfied */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Lobby")
	bool StartGame();

	/** Evaluates if the match can be launched (2 players, valid opposite roles, both ready) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Lobby")
	bool CanStartGame() const;

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Lobby|Events")
	FOnThreadTalesLobbyUpdated OnLobbyUpdated;

	/** Destination gameplay level URL */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Lobby")
	FString GameplayLevelPath;

private:
	void NotifyLobbyStateChanged();
};
