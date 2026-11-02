// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/ThreadTalesPlayerState.h"
#include "ThreadTalesUIContext.h"
#include "ThreadTalesVirtualCursor.h"
#include "ThreadTalesLobbyWidget.generated.h"

class UButton;
class UTextBlock;
class UThreadTalesLobbySlotWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnThreadTalesLobbyReturnToMenu);

/**
 * Base C++ class for WBP_LobbyMenu.
 * Coordinates character selection, readiness, match start, and navigation back to main menu.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class THREADTALES_API UThreadTalesLobbyWidget : public UUserWidget, public IThreadTalesUIContext
{
	GENERATED_BODY()

public:
	UThreadTalesLobbyWidget(const FObjectInitializer& ObjectInitializer);

	/** Event dispatcher fired when the player leaves the lobby back to main menu */
	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Lobby|Events")
	FOnThreadTalesLobbyReturnToMenu OnReturnToMainMenu;

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

	/** Finds the top-most enabled and focusable widget (e.g. first Button) */
	UWidget* FindFirstFocusableWidget() const;

	/** Guarantees global Slate focus across all connected devices and users */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Lobby")
	void SetGlobalUserFocus(UWidget* TargetWidget);

	/** Selects role for current player (in local co-op, automatically pairs with the other role for P2) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Lobby")
	void SelectRole(EThreadTalesCharacterRole NewRole);

	/** Explicitly selects a role for a specific player slot (0 = P1, 1 = P2) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Lobby")
	void SelectRoleForPlayer(int32 PlayerIndex, EThreadTalesCharacterRole NewRole);

	/** Gets formatted player name assigned to the specified character role */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Lobby")
	FText GetRoleOwnerName(EThreadTalesCharacterRole Role) const;

	/** Checks whether a role is already occupied by a player */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Lobby")
	bool IsRoleTaken(EThreadTalesCharacterRole Role) const;

	/** Gets current role of the player at PlayerIndex */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Lobby")
	EThreadTalesCharacterRole GetPlayerRole(int32 PlayerIndex) const;

	/** Forces state evaluation and triggers BP_OnLobbyStateUpdated */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Lobby")
	void ForceRefreshLobbyState();

	/** Requests the game mode to start match and travel to gameplay level */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Lobby")
	void RequestStartMatch();

	/** Leaves the lobby (cancels local split-screen or destroys online session) and returns to main menu */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Lobby")
	void LeaveLobby();

	/** Evaluates if all start conditions are satisfied (2 players, opposite roles, both ready) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Lobby")
	bool CanStartMatch() const;

	/** Returns true if currently running in local split-screen co-op */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Lobby")
	bool IsLocalCoopMatch() const;

	/** Returns true if currently running in an online session (Steam or LAN) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Lobby")
	bool IsOnlineMatch() const;

	/** Returns true if this client is the host of the lobby session */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Lobby")
	bool IsLobbyHost() const;

	/** Returns the slot index of the local player in GameState (0 = Host, 1 = Guest) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Lobby")
	int32 GetLocalPlayerIndex() const;

	/** Checks whether the player at PlayerIndex is marked ready */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Lobby")
	bool IsPlayerReady(int32 PlayerIndex) const;

	/** Slot widget for Player 1 (Host / Left panel, optional) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|Lobby")
	TObjectPtr<UThreadTalesLobbySlotWidget> PlayerSlot_1;

	/** Slot widget for Player 2 (Guest / Right panel, optional) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|Lobby")
	TObjectPtr<UThreadTalesLobbySlotWidget> PlayerSlot_2;

	/** Optional button reference for Sword character selection */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|Lobby")
	TObjectPtr<UButton> Button_Sword;

	/** Optional button reference for Bow character selection */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|Lobby")
	TObjectPtr<UButton> Button_Bow;

	/** Button to start the gameplay session */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|Lobby")
	TObjectPtr<UButton> StartMatchButton;

	/** Button to leave lobby back to main menu */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|Lobby")
	TObjectPtr<UButton> BackButton;

	/** Virtual cursor widget for Player 2 / Gamepad */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|Lobby|Cursor")
	TObjectPtr<UWidget> VirtualCursor_P2;

	/** Movement speed of the virtual cursor in pixels / screen units per second */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Lobby|Cursor")
	float VirtualCursorSpeed = 1200.0f;

	/** Analog stick deadzone (below which stick input is treated as zero) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Lobby|Cursor")
	float VirtualCursorDeadZone = 0.15f;

	/** Friction multiplier applied to cursor speed when hovering over an interactive widget (0.1 - 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Lobby|Cursor")
	float HoverFrictionMultiplier = 0.45f;

	/** Current screen coordinates of the virtual cursor */
	UPROPERTY(BlueprintReadOnly, Category = "ThreadTales|Lobby|Cursor")
	FVector2D VirtualCursorPosition = FVector2D(1440.0f, 540.0f);

	/** Returns true if the virtual cursor is active (e.g. Player 2 on Gamepad) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Lobby|Cursor")
	bool IsVirtualCursorActive() const { return VirtualCursor.bIsActive; }

	/** Explicitly sets virtual cursor screen position */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Lobby|Cursor")
	void SetVirtualCursorPosition(const FVector2D& NewPosition);

	/** Returns currently hovered interactive widget under the virtual cursor */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Lobby|Cursor")
	UWidget* GetVirtualCursorHoveredWidget() const { return VirtualCursor.HoveredWidget.Get(); }

	/** Target main menu widget class to spawn when leaving lobby (Screen Stack pattern) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Lobby", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> MainMenuWidgetClass;

	/** Explicitly sets widget focus for the owning player */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Lobby")
	void FocusDefaultButton();

protected:
	/** Broadcasts when the virtual cursor position changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|Lobby|Cursor")
	void BP_OnVirtualCursorMoved(const FVector2D& NewPosition, const FVector2D& Velocity);

	/** Broadcasts when hovered interactive element under virtual cursor changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|Lobby|Cursor")
	void BP_OnVirtualCursorHoverChanged(UWidget* HoveredWidget, bool bIsHovered);

	void UpdateVirtualCursor(const FGeometry& MyGeometry, float DeltaTime);
	UWidget* HitTestVirtualCursor(const FGeometry& MyGeometry, const FVector2D& ScreenPos);
	/** Broadcasts to WBP when player count, roles, or launch readiness change */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|Lobby")
	void BP_OnLobbyStateUpdated(int32 CurrentPlayers, bool bCanStartMatch);

	/** Broadcasts to WBP when match travel has been triggered */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|Lobby")
	void BP_OnMatchStarting();

	/** Broadcasts to WBP to animate or display the main menu */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|Lobby")
	void BP_OnReturnToMainMenu();

	UFUNCTION()
	void HandleStartMatchClicked();

	UFUNCTION()
	void HandleBackButtonClicked();

	UFUNCTION()
	void HandleGameModeLobbyUpdated(int32 CurrentPlayers, bool bCanStart);

	bool bHasInitialFocus = false;
	bool bLastCanStart = false;
	int32 LastPlayerCount = 0;
	EThreadTalesCharacterRole LastRole1 = EThreadTalesCharacterRole::None;
	EThreadTalesCharacterRole LastRole2 = EThreadTalesCharacterRole::None;
	bool bLastReady1 = false;
	bool bLastReady2 = false;

	/** Virtual cursor simulation engine */
	FThreadTalesVirtualCursor VirtualCursor;
};
