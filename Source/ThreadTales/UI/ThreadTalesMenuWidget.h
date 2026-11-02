// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ThreadTalesUIContext.h"
#include "ThreadTalesButton.h"
#include "ThreadTalesMenuWidget.generated.h"

class FThreadTalesMenuInputProcessor;

/**
 * Base C++ class for WBP_MainMenu.
 * Implements AAA Virtual Navigation: flat button array, deterministic index traversal,
 * persistent focus across mouse clicks, and screen stack transition to Lobby.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class THREADTALES_API UThreadTalesMenuWidget : public UUserWidget, public IThreadTalesUIContext
{
	GENERATED_BODY()

public:
	UThreadTalesMenuWidget(const FObjectInitializer& ObjectInitializer);

	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	//~ Begin IThreadTalesUIContext Interface
	virtual bool HandleUINavigation(int32 PlayerSlotIndex, EThreadTalesNavDirection Direction) override;
	virtual bool HandleUIAction(int32 PlayerSlotIndex, EThreadTalesUIAction Action) override;
	virtual bool HandleAnalogNavigation(int32 PlayerSlotIndex, const FVector2D& AxisValue) override;
	virtual void OnUIContextActivated() override;
	virtual void OnUIContextDeactivated() override;
	virtual bool IsUIContextActive() const override;
	//~ End IThreadTalesUIContext Interface

	/** Discovers all UThreadTalesButton instances in the widget tree and initializes navigation */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Menu")
	void RefreshMenuButtons();

	/** Sets explicit active button index and updates visual states */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Menu")
	void SetSelectedButtonIndex(int32 NewIndex);

	/** Navigates menu buttons by relative offset (+1 next, -1 previous) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Menu")
	void NavigateMenu(int32 Direction);

	/** Executes action of the currently selected button */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Menu")
	void TriggerSelectedButtonAction();

	/** Dispatches semantic button action */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Menu")
	void HandleButtonAction(EThreadTalesButtonAction Action);

	/** Transitions from Main Menu to Lobby screen, removing self from Viewport */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Menu")
	void TransitionToLobbyScreen(bool bIsLocalCoop);

	/** Focuses the default button for the primary controller */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Menu")
	void FocusDefaultButton();

	/** Sets explicit focus and visual highlight to a target menu button */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Menu")
	void SetFocusedMenuButton(UWidget* Button);

	/** Activates the currently focused or active button */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Menu")
	bool ActivateFocusedOrSelectedButton();

	/** Restores menu focus to default button immediately and on next tick */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Menu")
	void RestoreMenuFocus();

	/** Guarantees global Slate focus across all connected devices and users */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Menu")
	void SetGlobalUserFocus(UWidget* TargetWidget);

	/** Gets the primary default menu button */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Menu")
	UWidget* GetDefaultMenuButton() const;

	/** Finds the first focusable interactive widget in the menu */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Menu")
	UWidget* FindFirstFocusableWidget() const;

	/** Returns all ordered interactive buttons in the main menu */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Menu")
	TArray<UWidget*> GetMainMenuButtons() const;

	/** Initiates Local Split-Screen Co-op and transitions to Lobby */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Menu")
	virtual void StartLocalCoopGame();

	/** Hosts a Steam Online Session and travels to Lobby */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Menu")
	virtual void HostSteamGame();

	/** Quits the game application safely */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Menu")
	virtual void QuitGame();

	/** Target lobby widget class to spawn when entering lobby (Screen Stack pattern) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Menu", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> LobbyMenuWidgetClass;

	/** Ordered flat array of interactive UThreadTalesButton instances */
	UPROPERTY(BlueprintReadOnly, Category = "ThreadTales|Menu", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UThreadTalesButton>> MenuButtons;

	/** Index of currently selected button in MenuButtons */
	UPROPERTY(BlueprintReadOnly, Category = "ThreadTales|Menu", meta = (AllowPrivateAccess = "true"))
	int32 SelectedButtonIndex = 0;

	/** Currently focused or selected button, persisting through mouse clicks */
	UPROPERTY(BlueprintReadOnly, Category = "ThreadTales|Menu", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> CurrentFocusedButton;

	//~ Backward-compatible bind widget properties
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "ThreadTales|Menu")
	TObjectPtr<UWidget> SplitButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "ThreadTales|Menu")
	TObjectPtr<UWidget> LocalMenuButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "ThreadTales|Menu")
	TObjectPtr<UWidget> SteamButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "ThreadTales|Menu")
	TObjectPtr<UWidget> SteamMenuButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "ThreadTales|Menu")
	TObjectPtr<UWidget> FindButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "ThreadTales|Menu")
	TObjectPtr<UWidget> FindMenuButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "ThreadTales|Menu")
	TObjectPtr<UWidget> QuitMenuButton;

protected:
	bool bHasInitialFocus = false;
	bool bWasVisible = false;
	bool bWasInLobby = false;

	/** Dedicated Slate Input Pre-Processor for menu gamepad navigation and A button activation */
	TSharedPtr<FThreadTalesMenuInputProcessor> MenuInputProcessor;

	UFUNCTION()
	void HandleButtonHovered(UThreadTalesButton* HoveredButton);

	UFUNCTION()
	void HandleButtonActionTriggered(EThreadTalesButtonAction Action);

	UFUNCTION()
	void HandleLocalCoopStateChanged(bool bIsActive);

	UFUNCTION()
	void HandleDestroySessionComplete(bool bWasSuccessful);

	/** Called when transitioning from Menu to Lobby (bIsLocalCoop true if split-screen, false if Steam host) */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|Menu")
	void BP_OnTransitionToLobby(bool bIsLocalCoop);

	/** Called when a menu button receives focus / selection (for custom blueprint visuals) */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|Menu")
	void BP_OnMenuButtonSelected(UWidget* SelectedButton);

	friend class FThreadTalesMenuInputProcessor;
};
