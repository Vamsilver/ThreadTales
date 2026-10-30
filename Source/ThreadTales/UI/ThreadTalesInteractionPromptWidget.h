// Copyright ThreadTales. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ThreadTalesInteractionPromptWidget.generated.h"

class UThreadTalesInteractionConfig;

/**
 * Designer-friendly base UserWidget for contextual world interaction prompts.
 * Designed for Screen-Space UWidgetComponents floating above interactable objects.
 * Level & UI Designers can implement custom animations (Fade In/Out), icons, and styles in UMG.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class THREADTALES_API UThreadTalesInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Sets prompt data and triggers designer presentation hooks */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI|Interaction")
	virtual void ShowPrompt(const UThreadTalesInteractionConfig* Config, bool bIsLocked = false, FText CustomActionText = FText::GetEmpty());

	/** Hides prompt and triggers designer fade-out hooks */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI|Interaction")
	virtual void HidePrompt();

	/** Updates hold progress percentage (0.0 to 1.0) for circular/radial indicators */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI|Interaction")
	virtual void UpdateHoldProgress(float ProgressPercent);

	/** Triggers visual rejection feedback (shake/flash) when player presses Interact on a locked object */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI|Interaction")
	virtual void NotifyLockedInteractionDenied();

	/** Action text shown to player (e.g. "Open Chest", "Inspect", "Talk") */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI|Interaction")
	FText GetActionText() const { return CurrentActionText; }

	/** Keyboard/gamepad key glyph text (e.g. "[E]", "(X)") */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI|Interaction")
	FText GetKeyText() const { return CurrentKeyText; }

	/** Is the interaction currently locked (e.g. requires key/tag) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI|Interaction")
	bool IsLocked() const { return bCurrentlyLocked; }

	/** Current hold progress (0.0f - 1.0f) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI|Interaction")
	float GetHoldProgress() const { return CurrentHoldProgress; }

protected:
	/**
	 * Designer Hook: Fired when prompt is activated.
	 * Implement in UMG Event Graph to play Fade In animations, scale pop-ins, or audio cues.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|UI|Interaction|Events", meta = (DisplayName = "On Prompt Shown"))
	void BP_OnPromptShown(const FText& ActionText, const FText& KeyText, bool bLocked);

	/**
	 * Designer Hook: Fired when player leaves interaction range.
	 * Implement in UMG Event Graph to play Fade Out animations.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|UI|Interaction|Events", meta = (DisplayName = "On Prompt Hidden"))
	void BP_OnPromptHidden();

	/**
	 * Designer Hook: Fired continuously while key is held down.
	 * Implement in UMG Event Graph to update progress bars or fill materials.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|UI|Interaction|Events", meta = (DisplayName = "On Hold Progress Updated"))
	void BP_OnHoldProgressUpdated(float ProgressPercent);

	/**
	 * Designer Hook: Fired when player attempts to interact while locked (missing RequiredTag).
	 * Implement in UMG Event Graph to play red flash / shake animation on the prompt widget.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|UI|Interaction|Events", meta = (DisplayName = "On Prompt Locked Denied"))
	void BP_OnPromptLockedDenied();

	/** Cached text displayed on the prompt */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|UI|Interaction|State")
	FText CurrentActionText;

	/** Key label text displayed in the button frame (default: "E") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|UI|Interaction|State")
	FText CurrentKeyText = FText::FromString(TEXT("E"));

	/** Optional bound text block automatically updated by ShowPrompt */
	UPROPERTY(BlueprintReadOnly, Category = "ThreadTales|UI|Interaction", meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> ActionTextBlock = nullptr;

	/** Whether the prompt represents a locked action */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|UI|Interaction|State")
	bool bCurrentlyLocked = false;

	/** Hold progress from 0.0 to 1.0 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|UI|Interaction|State")
	float CurrentHoldProgress = 0.0f;
};
