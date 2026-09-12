// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/ThreadTalesTraversalTypes.h"

class APlayerController;
class ACharacter;

/**
 * Utility helper for translating analog/digital 2D movement inputs into discrete
 * 8-directional ledge jump directions, handling deadzones, and input sprint detection.
 */
class THREADTALES_API FThreadTalesTraversalInputHelper
{
public:
	/**
	 * Reads player controller input (both keyboard WASD/arrows and gamepad stick) and classifies
	 * it into discrete 8-directional ledge jump directions.
	 *
	 * @param PC The player controller to query
	 * @param bOutIsSprint Outputs whether sprint modifier is active
	 * @return Classified EThreadTalesLedgeJumpDirection
	 */
	static EThreadTalesLedgeJumpDirection GetClimbingJumpDirection(const APlayerController* PC, bool& bOutIsSprint);

	/**
	 * Returns 1.0f for right lateral input, -1.0f for left lateral input, or 0.0f if neutral.
	 * Used for smooth chained shimmying and lateral continuous ledge transitions.
	 */
	static float GetLateralInputSign(const APlayerController* PC);

	/**
	 * Checks if backward/down input is held (for Wall Eject / Back Jump from ledge).
	 */
	static bool IsBackInputHeld(const APlayerController* PC);

	/**
	 * Queries whether sprint input is currently held by the player.
	 */
	static bool IsSprintHeld(const APlayerController* PC);

	/**
	 * Translates a 2D movement input vector (X = Right / Lateral, Y = Forward / Up)
	 * into one of the 8 discrete ledge jump directions using angular sector classification.
	 */
	static EThreadTalesLedgeJumpDirection VectorToJumpDirection(const FVector2D& InputVector, float DeadZone = 0.25f);

	/**
	 * Reads the current 2D movement input vector from the local Player Controller.
	 */
	static FVector2D GetMovementInputVector(const APlayerController* PC, const ACharacter* Character);
};
