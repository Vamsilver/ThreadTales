// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ThreadTalesButton.generated.h"

/**
 * Semantic actions that can be triggered by a ThreadTales interactive button.
 * Eliminates all string parsing and hardcoded button names in C++.
 */
UENUM(BlueprintType)
enum class EThreadTalesButtonAction : uint8
{
	None,
	StartLocalCoop,    // Start Couch Split-Screen / Local Co-op
	HostSteamGame,     // Host Online Steam Session
	FindSteamGame,     // Search & Join Steam Sessions
	QuitGame,          // Safely Quit Application
	ToggleReady,       // Toggle Player Readiness in Lobby
	StartMatch,        // Launch Match from Lobby
	Back               // Return to Previous Screen
};

/**
 * Visual states for interactive buttons, managing deterministic visual transitions.
 */
UENUM(BlueprintType)
enum class EThreadTalesButtonVisualState : uint8
{
	Normal,            // Default resting state
	Hovered,           // Mouse cursor over button
	Focused,           // Highlighted via Virtual Navigation (plays FocusAnim)
	Pressed,           // Clicked or activated via Accept button
	Disabled           // Inactive / grayed out, skipped during navigation
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThreadTalesButtonActionTriggered, EThreadTalesButtonAction, Action);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThreadTalesButtonHoverStateChanged, class UThreadTalesButton*, Button);

/**
 * Base C++ class for all interactive UI buttons (e.g. WBP_MenuButton).
 * Provides a clean visual state machine and strongly-typed action dispatching.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class THREADTALES_API UThreadTalesButton : public UUserWidget
{
	GENERATED_BODY()

public:
	UThreadTalesButton(const FObjectInitializer& ObjectInitializer);

	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	//~ End UUserWidget Interface

	/** Changes visual state and fires BP_OnVisualStateChanged */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Button")
	void SetButtonVisualState(EThreadTalesButtonVisualState NewState);

	/** Returns current visual state */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Button")
	EThreadTalesButtonVisualState GetButtonVisualState() const { return CurrentVisualState; }

	/** Executes the button's assigned action */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Button")
	void TriggerAction();

	/** Assigned semantic action configured in UMG Details */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Button", meta = (AllowPrivateAccess = "true"))
	EThreadTalesButtonAction ActionType = EThreadTalesButtonAction::None;

	/** Fired when the button is activated */
	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Button|Events")
	FOnThreadTalesButtonActionTriggered OnActionTriggered;

	/** Fired when mouse cursor hovers over the button */
	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Button|Events")
	FOnThreadTalesButtonHoverStateChanged OnButtonHovered;

protected:
	/** Current visual state */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Button", meta = (AllowPrivateAccess = "true"))
	EThreadTalesButtonVisualState CurrentVisualState = EThreadTalesButtonVisualState::Normal;

	/** Cosmetic presentation hook for UMG animations (e.g. FocusAnim, Hover sound) */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|Button|Visual")
	void BP_OnVisualStateChanged(EThreadTalesButtonVisualState OldState, EThreadTalesButtonVisualState NewState);
};
