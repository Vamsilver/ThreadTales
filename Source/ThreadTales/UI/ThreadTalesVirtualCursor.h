// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"
#include "ThreadTalesUIContext.h"

class UWidget;
class UWidgetTree;
class UThreadTalesLobbySlotWidget;

/**
 * Encapsulated 2D Virtual Cursor Engine for Gamepad UI interaction in ThreadTales.
 * Handles continuous analog physics, viewport clamping, friction slowdown, and geometric hit-testing.
 */
struct THREADTALES_API FThreadTalesVirtualCursor
{
	/** Screen coordinates */
	FVector2D Position = FVector2D(1440.0f, 540.0f);

	/** Movement speed in pixels / screen units per second */
	float Speed = 1200.0f;

	/** Analog stick deadzone */
	float DeadZone = 0.15f;

	/** Friction slowdown multiplier when hovering interactive buttons */
	float FrictionMultiplier = 0.45f;

	/** Whether the cursor is active */
	bool bIsActive = false;

	/** Currently hovered widget */
	TWeakObjectPtr<UWidget> HoveredWidget;

	/** Initializes cursor widget reference and visibility */
	void Init(UWidgetTree* WidgetTree, TObjectPtr<UWidget>& InOutCursorWidget);

	/** Updates position based on analog stick input and delta time. Returns true if moved. */
	bool UpdatePosition(const FVector2D& RawStick, const FGeometry& MyGeometry, float DeltaTime, FVector2D& OutVelocity);

	/** Checks if a specific widget is under cursor coordinates */
	static bool IsWidgetUnderCursor(UWidget* Widget, const FGeometry& MyGeometry, const FVector2D& ScreenPos);

	/** Applies screen position to widget (CanvasPanelSlot or RenderTranslation) */
	static void ApplyPositionToWidget(UWidget* CursorWidget, const FVector2D& ScreenPos);

	/** Hit-tests interactive widgets in Player 2 slot */
	static UWidget* HitTest(UThreadTalesLobbySlotWidget* Slot2, UWidget* BackButton, const FGeometry& MyGeometry, const FVector2D& ScreenPos);

	/** Executes semantic UI action on the hovered or fallback widget */
	static bool ExecuteP2Action(UThreadTalesLobbySlotWidget* Slot2, UWidget* BackButton, EThreadTalesUIAction Action, UWidget* CurrentHovered);
};
