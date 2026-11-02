// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesMenuInputProcessor.h"
#include "ThreadTalesMenuWidget.h"
#include "Core/ThreadTalesSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"

FThreadTalesMenuInputProcessor::FThreadTalesMenuInputProcessor(UThreadTalesMenuWidget* InOwner)
	: OwnerMenuWidget(InOwner)
{
}

bool FThreadTalesMenuInputProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	UThreadTalesMenuWidget* Menu = OwnerMenuWidget.Get();
	if (!Menu || !Menu->GetWorld() || !Menu->IsVisible() || !Menu->GetIsEnabled())
	{
		return false;
	}

	if (UGameInstance* GI = Menu->GetGameInstance())
	{
		if (UThreadTalesSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UThreadTalesSessionSubsystem>())
		{
			if (SessionSubsystem->IsLocalCoopActive() || SessionSubsystem->IsOnlineSessionActive())
			{
				return false;
			}
		}
	}

	const FKey Key = InKeyEvent.GetKey();

	// 1. Activation: Gamepad FaceButton Bottom (A / Cross), Enter, SpaceBar, Virtual Gamepad Accept
	if (Key == EKeys::Gamepad_FaceButton_Bottom || Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Virtual_Gamepad_Accept)
	{
		return Menu->ActivateFocusedOrSelectedButton();
	}

	// 2. Navigation Down (Next button)
	if (Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down || Key == EKeys::Down || Key == EKeys::S)
	{
		Menu->NavigateMenu(1);
		return true;
	}

	// 3. Navigation Up (Previous button)
	if (Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up || Key == EKeys::Up || Key == EKeys::W)
	{
		Menu->NavigateMenu(-1);
		return true;
	}

	return false;
}

bool FThreadTalesMenuInputProcessor::HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent)
{
	UThreadTalesMenuWidget* Menu = OwnerMenuWidget.Get();
	if (!Menu || !Menu->GetWorld() || !Menu->IsVisible() || !Menu->GetIsEnabled())
	{
		return false;
	}

	if (UGameInstance* GI = Menu->GetGameInstance())
	{
		if (UThreadTalesSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UThreadTalesSessionSubsystem>())
		{
			if (SessionSubsystem->IsLocalCoopActive() || SessionSubsystem->IsOnlineSessionActive())
			{
				return false;
			}
		}
	}

	const FKey Key = InAnalogInputEvent.GetKey();
	if (Key == EKeys::Gamepad_LeftY)
	{
		const float AxisVal = InAnalogInputEvent.GetAnalogValue();
		const double CurrentTime = FPlatformTime::Seconds();
		if (FMath::Abs(AxisVal) >= 0.65f)
		{
			if (CurrentTime - LastAnalogNavTime >= 0.25)
			{
				LastAnalogNavTime = CurrentTime;
				Menu->NavigateMenu(AxisVal > 0.0f ? -1 : 1);
				return true;
			}
		}
	}

	return false;
}
