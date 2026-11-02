// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesUIInputSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Core/ThreadTalesSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UThreadTalesUIInputSubsystem::UThreadTalesUIInputSubsystem()
	: Slot0Device(EThreadTalesInputDeviceType::KeyboardMouse)
	, Slot1Device(EThreadTalesInputDeviceType::Gamepad)
{
}

void UThreadTalesUIInputSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (FSlateApplication::IsInitialized() && !CentralInputProcessor.IsValid())
	{
		CentralInputProcessor = MakeShared<FThreadTalesCentralInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(CentralInputProcessor);
	}
}

void UThreadTalesUIInputSubsystem::Deinitialize()
{
	if (FSlateApplication::IsInitialized() && CentralInputProcessor.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(CentralInputProcessor);
		CentralInputProcessor.Reset();
	}

	ClearContextStack();
	Super::Deinitialize();
}

void UThreadTalesUIInputSubsystem::PushContext(TScriptInterface<IThreadTalesUIContext> Context)
{
	if (!Context.GetObject() || !Context.GetInterface())
	{
		return;
	}

	ContextStack.Remove(Context);

	if (ContextStack.Num() > 0)
	{
		TScriptInterface<IThreadTalesUIContext> PreviousTop = ContextStack.Last();
		if (PreviousTop.GetInterface())
		{
			PreviousTop->OnUIContextDeactivated();
		}
	}

	ContextStack.Add(Context);
	Context->OnUIContextActivated();
}

void UThreadTalesUIInputSubsystem::PopContext(TScriptInterface<IThreadTalesUIContext> Context)
{
	if (ContextStack.Num() == 0)
	{
		return;
	}

	if (!Context.GetObject())
	{
		TScriptInterface<IThreadTalesUIContext> Popped = ContextStack.Pop();
		if (Popped.GetInterface())
		{
			Popped->OnUIContextDeactivated();
		}
	}
	else
	{
		const int32 Index = ContextStack.Find(Context);
		if (Index != INDEX_NONE)
		{
			ContextStack.RemoveAt(Index);
			if (Context.GetInterface())
			{
				Context->OnUIContextDeactivated();
			}
		}
	}

	if (ContextStack.Num() > 0)
	{
		TScriptInterface<IThreadTalesUIContext> NewTop = ContextStack.Last();
		if (NewTop.GetInterface())
		{
			NewTop->OnUIContextActivated();
		}
	}
}

TScriptInterface<IThreadTalesUIContext> UThreadTalesUIInputSubsystem::GetCurrentContext() const
{
	for (int32 i = ContextStack.Num() - 1; i >= 0; --i)
	{
		const TScriptInterface<IThreadTalesUIContext>& Candidate = ContextStack[i];
		if (Candidate.GetObject() && Candidate.GetInterface())
		{
			if (Candidate->IsUIContextActive())
			{
				return Candidate;
			}
		}
	}
	return nullptr;
}

void UThreadTalesUIInputSubsystem::ClearContextStack()
{
	for (int32 i = ContextStack.Num() - 1; i >= 0; --i)
	{
		if (ContextStack[i].GetInterface())
		{
			ContextStack[i]->OnUIContextDeactivated();
		}
	}
	ContextStack.Empty();
}

int32 UThreadTalesUIInputSubsystem::ResolvePlayerSlotForInput(int32 UserIndex, const FKey& Key, bool bIsGamepad) const
{
	if (!bIsGamepad)
	{
		return 0;
	}

	if (UserIndex == 1)
	{
		return 1;
	}

	// UserIndex == 0 Gamepad evaluation
	const UGameInstance* GI = GetGameInstance();
	const UThreadTalesSessionSubsystem* SessionSubsystem = GI ? GI->GetSubsystem<UThreadTalesSessionSubsystem>() : nullptr;
	const bool bIsLocalCoop = SessionSubsystem && SessionSubsystem->IsLocalCoopActive();

	if (bIsLocalCoop)
	{
		const UGameMapsSettings* MapsSettings = GetDefault<UGameMapsSettings>();
		const bool bOffset = MapsSettings ? MapsSettings->bOffsetPlayerGamepadIds : false;

		if (!bOffset)
		{
			// If P1 is using KBM, dedicate first gamepad to P2 (Slot 1) for couch co-op
			if (Slot0Device == EThreadTalesInputDeviceType::KeyboardMouse)
			{
				return 1;
			}
		}
	}

	return 0;
}

bool UThreadTalesUIInputSubsystem::DispatchNavigation(int32 PlayerSlotIndex, EThreadTalesNavDirection Direction)
{
	TScriptInterface<IThreadTalesUIContext> Current = GetCurrentContext();
	if (Current.GetInterface())
	{
		return Current->HandleUINavigation(PlayerSlotIndex, Direction);
	}
	return false;
}

bool UThreadTalesUIInputSubsystem::DispatchAction(int32 PlayerSlotIndex, EThreadTalesUIAction Action)
{
	TScriptInterface<IThreadTalesUIContext> Current = GetCurrentContext();
	if (Current.GetInterface())
	{
		return Current->HandleUIAction(PlayerSlotIndex, Action);
	}
	return false;
}

bool UThreadTalesUIInputSubsystem::DispatchAnalog(int32 PlayerSlotIndex, const FVector2D& AxisValue)
{
	TScriptInterface<IThreadTalesUIContext> Current = GetCurrentContext();
	if (Current.GetInterface())
	{
		return Current->HandleAnalogNavigation(PlayerSlotIndex, AxisValue);
	}
	return false;
}

FVector2D UThreadTalesUIInputSubsystem::GetPlayerLeftStickAxis(int32 PlayerSlotIndex) const
{
	const int32 SafeIndex = FMath::Clamp(PlayerSlotIndex, 0, 1);
	return LeftStickAxes[SafeIndex];
}

void UThreadTalesUIInputSubsystem::UpdatePlayerLeftStickAxis(int32 PlayerSlotIndex, const FVector2D& NewAxis)
{
	const int32 SafeIndex = FMath::Clamp(PlayerSlotIndex, 0, 1);
	LeftStickAxes[SafeIndex] = NewAxis;
}

EThreadTalesInputDeviceType UThreadTalesUIInputSubsystem::GetActiveDeviceForPlayer(int32 PlayerSlotIndex) const
{
	return (PlayerSlotIndex == 1) ? Slot1Device : Slot0Device;
}

void UThreadTalesUIInputSubsystem::SetPlayerDevice(int32 PlayerSlotIndex, EThreadTalesInputDeviceType DeviceType)
{
	EThreadTalesInputDeviceType& Target = (PlayerSlotIndex == 1) ? Slot1Device : Slot0Device;
	if (Target != DeviceType)
	{
		Target = DeviceType;
		OnInputDeviceChanged.Broadcast(PlayerSlotIndex, DeviceType);
	}
}

bool UThreadTalesUIInputSubsystem::IsInputProcessorActive() const
{
	return CentralInputProcessor.IsValid();
}

// ---------------------------------------------------------------------------
// FThreadTalesCentralInputProcessor Implementation
// ---------------------------------------------------------------------------

FThreadTalesCentralInputProcessor::FThreadTalesCentralInputProcessor(UThreadTalesUIInputSubsystem* InSubsystem)
	: Subsystem(InSubsystem)
{
}

bool FThreadTalesCentralInputProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	UThreadTalesUIInputSubsystem* Sub = Subsystem.Get();
	if (!Sub || !Sub->GetCurrentContext().GetObject())
	{
		return false;
	}

	const FKey Key = InKeyEvent.GetKey();
	const bool bIsGamepad = Key.IsGamepadKey();
	const int32 UserIndex = InKeyEvent.GetUserIndex();
	const int32 SlotIndex = Sub->ResolvePlayerSlotForInput(UserIndex, Key, bIsGamepad);

	// Update detected device
	const EThreadTalesInputDeviceType NewDevice = bIsGamepad ? EThreadTalesInputDeviceType::Gamepad : EThreadTalesInputDeviceType::KeyboardMouse;
	Sub->SetPlayerDevice(SlotIndex, NewDevice);

	// 1. Navigation translation (D-Pad and Arrow keys only; analog stick is reserved for free cursor / continuous input)
	if (Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Up || Key == EKeys::W)
	{
		return Sub->DispatchNavigation(SlotIndex, EThreadTalesNavDirection::Up);
	}
	if (Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Down || Key == EKeys::S)
	{
		return Sub->DispatchNavigation(SlotIndex, EThreadTalesNavDirection::Down);
	}
	if (Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Left || Key == EKeys::A)
	{
		return Sub->DispatchNavigation(SlotIndex, EThreadTalesNavDirection::Left);
	}
	if (Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Right || Key == EKeys::D)
	{
		return Sub->DispatchNavigation(SlotIndex, EThreadTalesNavDirection::Right);
	}

	// Consume synthetic LeftStick navigation keys generated by Slate to avoid focus wrestling
	if (Key == EKeys::Gamepad_LeftStick_Up || Key == EKeys::Gamepad_LeftStick_Down ||
	    Key == EKeys::Gamepad_LeftStick_Left || Key == EKeys::Gamepad_LeftStick_Right)
	{
		return true;
	}

	// 2. Action translation
	if (Key == EKeys::Gamepad_FaceButton_Bottom || Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Virtual_Gamepad_Accept)
	{
		return Sub->DispatchAction(SlotIndex, EThreadTalesUIAction::Accept);
	}
	if (Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Escape || Key == EKeys::BackSpace)
	{
		return Sub->DispatchAction(SlotIndex, EThreadTalesUIAction::Cancel);
	}
	if (Key == EKeys::Gamepad_FaceButton_Left || Key == EKeys::R)
	{
		return Sub->DispatchAction(SlotIndex, EThreadTalesUIAction::ToggleReady);
	}
	if (Key == EKeys::Gamepad_Special_Right || Key == EKeys::P)
	{
		return Sub->DispatchAction(SlotIndex, EThreadTalesUIAction::StartMatch);
	}

	return false;
}

bool FThreadTalesCentralInputProcessor::HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent)
{
	UThreadTalesUIInputSubsystem* Sub = Subsystem.Get();
	if (!Sub || !Sub->GetCurrentContext().GetObject())
	{
		return false;
	}

	const FKey Key = InAnalogInputEvent.GetKey();
	const int32 UserIndex = InAnalogInputEvent.GetUserIndex();
	const int32 SlotIndex = Sub->ResolvePlayerSlotForInput(UserIndex, Key, true);
	const int32 SafeIndex = FMath::Clamp(SlotIndex, 0, 1);

	const float AxisVal = InAnalogInputEvent.GetAnalogValue();
	const double CurrentTime = FPlatformTime::Seconds();
	const double RepeatDelay = 0.22;

	if (Key == EKeys::Gamepad_LeftY)
	{
		CurrentLeftStick[SafeIndex].Y = AxisVal;
		Sub->UpdatePlayerLeftStickAxis(SlotIndex, CurrentLeftStick[SafeIndex]);
		Sub->DispatchAnalog(SlotIndex, CurrentLeftStick[SafeIndex]);
		return true; // Consume event: continuous analog stick is processed by virtual cursor in Tick, prevent Slate navigation
	}
	else if (Key == EKeys::Gamepad_LeftX)
	{
		CurrentLeftStick[SafeIndex].X = AxisVal;
		Sub->UpdatePlayerLeftStickAxis(SlotIndex, CurrentLeftStick[SafeIndex]);
		Sub->DispatchAnalog(SlotIndex, CurrentLeftStick[SafeIndex]);
		return true; // Consume event: continuous analog stick is processed by virtual cursor in Tick, prevent Slate navigation
	}

	return false;
}

bool FThreadTalesCentralInputProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (UThreadTalesUIInputSubsystem* Sub = Subsystem.Get())
	{
		Sub->SetPlayerDevice(0, EThreadTalesInputDeviceType::KeyboardMouse);
	}
	return false;
}
