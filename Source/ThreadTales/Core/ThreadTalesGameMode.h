// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ThreadTalesGameMode.generated.h"

/**
 * Main gameplay GameMode for ThreadTales (2P Co-op Action-Adventure).
 * Integrates seamless travel, role-based pawn spawning (BP_HeroMale vs BP_HeroFemale),
 * and supports both Steam Online Co-op and Local Split-Screen.
 */
UCLASS()
class THREADTALES_API AThreadTalesGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AThreadTalesGameMode();

	//~ Begin AGameModeBase Interface
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual void GenericPlayerInitialization(AController* C) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	//~ End AGameModeBase Interface

	/** Class for Swordsman role (Player 1 / Male) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Classes")
	TSubclassOf<APawn> HeroMaleClass;

	/** Class for Archer role (Player 2 / Female) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Classes")
	TSubclassOf<APawn> HeroFemaleClass;
};
