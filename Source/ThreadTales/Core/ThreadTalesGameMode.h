// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ThreadTalesGameMode.generated.h"

class AThreadTalesSpawnPoint;

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
	virtual void BeginPlay() override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual void GenericPlayerInitialization(AController* C) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	//~ End AGameModeBase Interface

	/** Returns the currently active team spawn point */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Spawn")
	AThreadTalesSpawnPoint* GetActiveSpawnPoint() const;

	/** Sets the active team spawn point, deactivating previous point and updating all PlayerControllers */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Spawn")
	void SetActiveSpawnPoint(AThreadTalesSpawnPoint* NewSpawnPoint);

	/** Automatically activates the initial spawn point on level start if none is active */
	UPROPERTY(EditDefaultsOnly, Category = "ThreadTales|Spawn")
	bool bAutoActivateInitialSpawnPoint = true;

	/** Class for Swordsman role (Player 1 / Male) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Classes")
	TSubclassOf<APawn> HeroMaleClass;

	/** Class for Archer role (Player 2 / Female) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Classes")
	TSubclassOf<APawn> HeroFemaleClass;

protected:
	/** Tracks the active team spawn point for 2P Co-op */
	UPROPERTY(Transient)
	TWeakObjectPtr<AThreadTalesSpawnPoint> ActiveSpawnPoint;
};
