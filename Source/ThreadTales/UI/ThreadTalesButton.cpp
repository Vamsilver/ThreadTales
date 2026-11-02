// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesButton.h"

UThreadTalesButton::UThreadTalesButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ActionType(EThreadTalesButtonAction::None)
	, CurrentVisualState(EThreadTalesButtonVisualState::Normal)
{
	SetIsFocusable(true);
}

void UThreadTalesButton::NativeConstruct()
{
	Super::NativeConstruct();
	SetButtonVisualState(CurrentVisualState);
}

void UThreadTalesButton::SetButtonVisualState(EThreadTalesButtonVisualState NewState)
{
	if (CurrentVisualState == NewState)
	{
		return;
	}

	const EThreadTalesButtonVisualState OldState = CurrentVisualState;
	CurrentVisualState = NewState;
	BP_OnVisualStateChanged(OldState, NewState);
}

void UThreadTalesButton::TriggerAction()
{
	if (CurrentVisualState == EThreadTalesButtonVisualState::Disabled || !GetIsEnabled())
	{
		return;
	}

	SetButtonVisualState(EThreadTalesButtonVisualState::Pressed);
	OnActionTriggered.Broadcast(ActionType);
}

void UThreadTalesButton::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	if (CurrentVisualState != EThreadTalesButtonVisualState::Disabled)
	{
		SetButtonVisualState(EThreadTalesButtonVisualState::Hovered);
		OnButtonHovered.Broadcast(this);
	}
}

void UThreadTalesButton::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	if (CurrentVisualState == EThreadTalesButtonVisualState::Hovered)
	{
		SetButtonVisualState(EThreadTalesButtonVisualState::Normal);
	}
}

FReply UThreadTalesButton::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && CurrentVisualState != EThreadTalesButtonVisualState::Disabled)
	{
		SetButtonVisualState(EThreadTalesButtonVisualState::Pressed);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UThreadTalesButton::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && CurrentVisualState != EThreadTalesButtonVisualState::Disabled)
	{
		const bool bIsUnderCursor = InGeometry.IsUnderLocation(InMouseEvent.GetScreenSpacePosition());
		SetButtonVisualState(bIsUnderCursor ? EThreadTalesButtonVisualState::Hovered : EThreadTalesButtonVisualState::Normal);

		if (bIsUnderCursor)
		{
			TriggerAction();
		}
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}
