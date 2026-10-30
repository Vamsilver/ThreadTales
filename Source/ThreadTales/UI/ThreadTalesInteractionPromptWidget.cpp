// Copyright ThreadTales. All Rights Reserved.

#include "ThreadTalesInteractionPromptWidget.h"
#include "Data/ThreadTalesInteractionConfig.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"

void UThreadTalesInteractionPromptWidget::ShowPrompt(const UThreadTalesInteractionConfig* Config, bool bIsLocked, FText CustomActionText)
{
	bCurrentlyLocked = bIsLocked;
	CurrentHoldProgress = 0.0f;

	if (!CustomActionText.IsEmpty())
	{
		CurrentActionText = CustomActionText;
	}
	else if (Config)
	{
		CurrentActionText = bIsLocked ? Config->LockedPromptText : Config->PromptText;
	}
	else
	{
		CurrentActionText = FText::FromString(TEXT("Interact"));
	}

	UTextBlock* TargetText = ActionTextBlock;
	if (!TargetText && WidgetTree)
	{
		WidgetTree->ForEachWidget([&TargetText](UWidget* Widget)
		{
			if (!TargetText)
			{
				TargetText = Cast<UTextBlock>(Widget);
			}
		});
	}

	if (TargetText)
	{
		const FText DisplayText = CurrentKeyText.IsEmpty()
			? CurrentActionText
			: FText::Format(FText::FromString(TEXT("[{0}] {1}")), CurrentKeyText, CurrentActionText);
		TargetText->SetText(DisplayText);
	}

	BP_OnPromptShown(CurrentActionText, CurrentKeyText, bCurrentlyLocked);
}

void UThreadTalesInteractionPromptWidget::HidePrompt()
{
	CurrentHoldProgress = 0.0f;
	BP_OnPromptHidden();
}

void UThreadTalesInteractionPromptWidget::UpdateHoldProgress(float ProgressPercent)
{
	CurrentHoldProgress = FMath::Clamp(ProgressPercent, 0.0f, 1.0f);
	BP_OnHoldProgressUpdated(CurrentHoldProgress);
}

void UThreadTalesInteractionPromptWidget::NotifyLockedInteractionDenied()
{
	BP_OnPromptLockedDenied();
}
