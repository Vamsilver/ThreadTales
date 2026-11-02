// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/ThreadTalesPlayerState.h"
#include "ThreadTalesLobbySlotWidget.generated.h"

class UButton;
class UTextBlock;
class APlayerController;
class AThreadTalesPlayerState;
enum class EThreadTalesButtonAction : uint8;

/**
 * Interactive elements available inside WBP_LobbySlot.
 */
UENUM(BlueprintType)
enum class EThreadTalesSlotActiveElement : uint8
{
	RoleCarousel UMETA(DisplayName = "Role Carousel"),
	ReadyButton  UMETA(DisplayName = "Ready Button")
};

/**
 * Base C++ class for WBP_LobbySlot (Player 1 or Player 2 Slot).
 * Isolates input focus, role selection, and ready state per player controller.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class THREADTALES_API UThreadTalesLobbySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UThreadTalesLobbySlotWidget(const FObjectInitializer& ObjectInitializer);

	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;
	//~ End UUserWidget Interface

	/** Returns currently active/highlighted interactive element */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|LobbySlot")
	EThreadTalesSlotActiveElement GetActiveElement() const { return ActiveElement; }

	/** Explicitly sets active highlighted element */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|LobbySlot")
	void SetActiveElement(EThreadTalesSlotActiveElement NewElement);

	/** Navigates active element up/down */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|LobbySlot")
	void NavigateActiveElement(bool bUp);

	/** Returns the child widget that should receive initial user focus */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|LobbySlot")
	UWidget* GetDesiredFocusWidget() const;

	/** Configures which player slot this widget controls (0 = P1 / Host, 1 = P2 / Guest) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|LobbySlot")
	void InitializeSlot(int32 InSlotIndex);

	/** Selects a character role for the assigned player state */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|LobbySlot")
	void SelectRole(EThreadTalesCharacterRole NewRole);

	/** Cycles to the opposite role (Swordsman <-> Archer) automatically handling local swap */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|LobbySlot")
	void CycleRole();

	/** Toggles player readiness */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|LobbySlot")
	void ToggleReady();

	/** Gets the current role selected by this slot's player */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|LobbySlot")
	EThreadTalesCharacterRole GetAssignedRole() const;

	/** Returns true if this slot's player is marked ready */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|LobbySlot")
	bool IsReady() const;

	/** Returns true if a valid player controller and state exist for this slot */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|LobbySlot")
	bool IsPlayerConnected() const;

	/** Returns true if the requested role is already taken by the other player */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|LobbySlot")
	bool IsRoleTakenByOther(EThreadTalesCharacterRole Role) const;

	/** Focuses the default button for this player controller */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|LobbySlot")
	void FocusSlotDefaultButton();

	/** Forces state refresh and triggers BP_OnSlotStateUpdated */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|LobbySlot")
	void RefreshSlotState();

	/** Returns the button or widget for Ready state */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|LobbySlot")
	UWidget* GetReadyButtonWidget() const;

	/** Returns previous role carousel button or widget */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|LobbySlot")
	UWidget* GetPrevRoleButton() const;

	/** Returns next role carousel button or widget */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|LobbySlot")
	UWidget* GetNextRoleButton() const;

	/** Returns Swordsman selection button */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|LobbySlot")
	UButton* GetSelectSwordsmanButton() const { return SelectSwordsmanButton; }

	/** Returns Archer selection button */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|LobbySlot")
	UButton* GetSelectArcherButton() const { return SelectArcherButton; }

	/** Assigned slot index: 0 = P1, 1 = P2 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|LobbySlot")
	int32 PlayerSlotIndex = 0;

	/** Button to pick Swordsman (Male) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|LobbySlot")
	TObjectPtr<UButton> SelectSwordsmanButton;

	/** Button to pick Archer (Female) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|LobbySlot")
	TObjectPtr<UButton> SelectArcherButton;

	/** Button to toggle ready state */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|LobbySlot")
	TObjectPtr<UButton> ReadyButton;

	/** Carousel button: cycle to previous character role */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|LobbySlot")
	TObjectPtr<UWidget> Btn_PrevRole;

	/** Carousel button: cycle to next character role */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|LobbySlot")
	TObjectPtr<UWidget> Btn_NextRole;

	/** Ready button (supports UButton or WBP_MenuButton / UWidget) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|LobbySlot")
	TObjectPtr<UWidget> Btn_Ready;

	/** Direct designer binding for previous role button in WBP_LobbySlot */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|LobbySlot")
	TObjectPtr<UWidget> PrevRole_MenuButton;

	/** Direct designer binding for next role button in WBP_LobbySlot */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|LobbySlot")
	TObjectPtr<UWidget> NextRole_MenuButton;

	/** Direct designer binding for ready button in WBP_LobbySlot */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|LobbySlot")
	TObjectPtr<UWidget> Ready_MenuButton;

	/** Text block displaying player role name (SWORDSMAN / ARCHER) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|LobbySlot")
	TObjectPtr<UTextBlock> Txt_RoleName;

	/** Text block displaying player title */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|LobbySlot")
	TObjectPtr<UTextBlock> Txt_PlayerTitle;

	/** Text block displaying ready status */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|LobbySlot")
	TObjectPtr<UTextBlock> Txt_Status;

	/** Active input device state for this slot */
	UPROPERTY(BlueprintReadOnly, Category = "ThreadTales|LobbySlot")
	bool bIsGamepadActive = false;

	/** Currently active/highlighted interactive element in this slot */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|LobbySlot")
	EThreadTalesSlotActiveElement ActiveElement = EThreadTalesSlotActiveElement::RoleCarousel;

protected:
	/** Broadcasts to WBP whenever player presence, role, or readiness changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|LobbySlot")
	void BP_OnSlotStateUpdated(bool bConnected, const FString& PlayerName, EThreadTalesCharacterRole CurrentRole, bool bIsPlayerReady, bool bSwordsmanTakenByOther, bool bArcherTakenByOther);

	/** Broadcasts to WBP when input method changes for prompt icon switching */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|LobbySlot")
	void BP_OnSlotInputMethodChanged(bool bNewIsGamepad);

	/** Broadcasts to WBP when active element changes to update visual focus/hover */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|LobbySlot")
	void BP_OnSlotActiveElementChanged(EThreadTalesSlotActiveElement NewActiveElement);

	UFUNCTION()
	void HandleSelectSwordsmanClicked();

	UFUNCTION()
	void HandleSelectArcherClicked();

	UFUNCTION()
	void HandleReadyClicked();

	UFUNCTION()
	void HandlePrevRoleClicked();

	UFUNCTION()
	void HandleNextRoleClicked();

	UFUNCTION()
	void HandlePrevRoleActionTriggered(EThreadTalesButtonAction Action);

	UFUNCTION()
	void HandleNextRoleActionTriggered(EThreadTalesButtonAction Action);

	UFUNCTION()
	void HandleReadyActionTriggered(EThreadTalesButtonAction Action);

	APlayerController* GetAssignedPlayerController() const;
	AThreadTalesPlayerState* GetAssignedPlayerState() const;

	// Cached state to prevent redundant delegate calls
	EThreadTalesCharacterRole LastCachedRole = EThreadTalesCharacterRole::None;
	bool bLastCachedReady = false;
	bool bLastCachedConnected = false;
};
