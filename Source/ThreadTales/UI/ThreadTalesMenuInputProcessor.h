// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "UObject/WeakObjectPtr.h"

class UThreadTalesMenuWidget;

/**
 * Slate Input Pre-Processor for ThreadTales Main Menu.
 * Intercepts Gamepad A (FaceButton_Bottom), Enter, SpaceBar, and D-Pad/Stick navigation
 * to guarantee 100% reliable button activation and visual highlight in Main Menu.
 */
class THREADTALES_API FThreadTalesMenuInputProcessor : public IInputProcessor
{
public:
	explicit FThreadTalesMenuInputProcessor(UThreadTalesMenuWidget* InOwner);
	virtual ~FThreadTalesMenuInputProcessor() override = default;

	//~ Begin IInputProcessor Interface
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return false; }
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return false; }
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return false; }
	//~ End IInputProcessor Interface

private:
	TWeakObjectPtr<UThreadTalesMenuWidget> OwnerMenuWidget;
	double LastAnalogNavTime = 0.0;
};
