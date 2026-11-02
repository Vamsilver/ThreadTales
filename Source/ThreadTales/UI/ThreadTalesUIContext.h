// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ThreadTalesUIContext.generated.h"

/**
 * Direction for discrete UI navigation events.
 */
UENUM(BlueprintType)
enum class EThreadTalesNavDirection : uint8
{
	Up,
	Down,
	Left,
	Right
};

/**
 * High-level semantic actions for UI interaction.
 */
UENUM(BlueprintType)
enum class EThreadTalesUIAction : uint8
{
	Accept,
	Cancel,
	ToggleReady,
	StartMatch,
	Special
};

/**
 * Physical input device category.
 */
UENUM(BlueprintType)
enum class EThreadTalesInputDeviceType : uint8
{
	KeyboardMouse,
	Gamepad
};

UINTERFACE(MinimalAPI, BlueprintType)
class UThreadTalesUIContext : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface implemented by widgets or objects serving as active UI navigation contexts.
 */
class THREADTALES_API IThreadTalesUIContext
{
	GENERATED_BODY()

public:
	/** Handles discrete navigational impulses (D-pad, Left stick click, Arrow keys, WASD) */
	virtual bool HandleUINavigation(int32 PlayerSlotIndex, EThreadTalesNavDirection Direction) { return false; }

	/** Handles semantic action buttons (Accept/A, Cancel/B, Ready/Y, Start/Options) */
	virtual bool HandleUIAction(int32 PlayerSlotIndex, EThreadTalesUIAction Action) { return false; }

	/** Handles continuous analog stick input with internal thresholding */
	virtual bool HandleAnalogNavigation(int32 PlayerSlotIndex, const FVector2D& AxisValue) { return false; }

	/** Notifies the context when it becomes active in the subsystem stack */
	virtual void OnUIContextActivated() {}

	/** Notifies the context when it is popped or superseded */
	virtual void OnUIContextDeactivated() {}

	/** Returns true if this context is eligible to receive input */
	virtual bool IsUIContextActive() const { return true; }
};
