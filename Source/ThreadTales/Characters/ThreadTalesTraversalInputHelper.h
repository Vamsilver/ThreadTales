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
	static EThreadTalesLedgeJumpDirection GetClimbingJumpDirection(const APlayerController* PC, bool& bOutIsSprint, const ACharacter* Character = nullptr);

	/**
	 * Returns 1.0f for right lateral input, -1.0f for left lateral input, or 0.0f if neutral.
	 * Used for smooth chained shimmying and lateral continuous ledge transitions.
	 */
	static float GetLateralInputSign(const APlayerController* PC, const ACharacter* Character = nullptr);

	/**
	 * Checks if backward/down input is held (for Wall Eject / Back Jump from ledge).
	 * If WallNormal is supplied, also evaluates camera-forward orientation facing away from wall with W.
	 */
	static bool IsBackInputHeld(const APlayerController* PC, const ACharacter* Character = nullptr, const FVector* WallNormal = nullptr);

	/**
	 * Queries whether jump input (Space / Gamepad Face Button Bottom / IA_Jump) is currently held by the player.
	 */
	static bool IsJumpPressed(const APlayerController* PC, const ACharacter* Character = nullptr);

	/**
	 * Queries whether sprint input (Shift / Gamepad Sprint / IA_Sprint) is currently held by the player.
	 */
	static bool IsSprintHeld(const APlayerController* PC, const ACharacter* Character = nullptr);

	/**
	 * Translates a 2D movement input vector (X = Right / Lateral, Y = Forward / Up)
	 * into one of the 8 discrete ledge jump directions using angular sector classification.
	 */
	static EThreadTalesLedgeJumpDirection VectorToJumpDirection(const FVector2D& InputVector, float DeadZone = 0.25f);

	/**
	 * Reads the current 2D movement input vector from the local Player Controller (Enhanced Input IA_Move with keyboard/gamepad fallback).
	 */
	static FVector2D GetMovementInputVector(const APlayerController* PC, const ACharacter* Character = nullptr);

	/**
	 * Computes a normalized 3D world direction vector representing the player's climbing jump intent,
	 * taking into account wall orientation (WallNormal, WallRight), WASD/Stick/Enhanced Input, and back-jump modifiers.
	 *
	 * @param PC Local player controller to query input from
	 * @param WallNormal Outward normal of the current wall face
	 * @param WallRight Right tangent vector along the wall face
	 * @param bBackJumpIntent If true, bias intent vector away from wall towards opposing walls
	 * @param Character Optional character reference for input config lookup
	 * @return Normalized 3D world direction vector (or FVector::UpVector if neutral Space is pressed)
	 */
	static FVector GetClimbingIntentWorldVector(
		const APlayerController* PC,
		const FVector& WallNormal,
		const FVector& WallRight,
		bool bBackJumpIntent = false,
		const ACharacter* Character = nullptr
	);
};
