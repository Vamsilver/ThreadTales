// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ThreadTalesPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UThreadTalesHUDWidget;
class AThreadTalesCharacter;
class AThreadTalesSpawnPoint;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings and player Combat HUD
 */
UCLASS(abstract)
class AThreadTalesPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	AThreadTalesPlayerController();

	/** Ensures the local player Combat HUD widget is created and bound to the given character */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI")
	UThreadTalesHUDWidget* EnsurePlayerHUDCreated(AThreadTalesCharacter* InCharacter = nullptr);

	/** Returns the active Combat HUD widget instance */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	UThreadTalesHUDWidget* GetPlayerHUDWidget() const { return PlayerHUDWidget; }

	/** Available playable character classes for hot-swapping / toggle gender (Data-Driven, configured in BP_ThreadTalesPlayerController) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Characters")
	TArray<TSubclassOf<AThreadTalesCharacter>> PlayableCharacterClasses;

	/** Cycles to the next available playable character (Male -> Female -> ...) */
	UFUNCTION(Exec, BlueprintCallable, Category = "ThreadTales|Characters")
	void TogglePlayableCharacter();

	/** Returns the currently active bonfire spawn point, if any */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Respawn")
	AThreadTalesSpawnPoint* GetCurrentSpawnPoint() const { return CurrentSpawnPoint; }

	/** Sets the currently active bonfire spawn point */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Respawn")
	void SetCurrentSpawnPoint(AThreadTalesSpawnPoint* NewSpawnPoint);

	/** Returns the active spawn transform (from CurrentSpawnPoint, or default PlayerStart if none) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Respawn")
	FTransform GetActiveSpawnTransform() const;

protected:

	/** Currently assigned bonfire spawn point */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "ThreadTales|Respawn", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AThreadTalesSpawnPoint> CurrentSpawnPoint;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** Widget class to spawn for the Combat HUD (defaults to UThreadTalesHUDWidget::StaticClass()) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UThreadTalesHUDWidget> PlayerHUDWidgetClass;

	/** Active Combat HUD widget instance */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ThreadTales|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UThreadTalesHUDWidget> PlayerHUDWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Called when possessing a pawn */
	virtual void OnPossess(APawn* InPawn) override;

	/** Called when pawn changes on client */
	virtual void AcknowledgePossession(APawn* P) override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

};
