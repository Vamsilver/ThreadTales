// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UThreadTalesClimbingConfig;

/**
 * Dedicated handler for camera behavior and alignment during character climbing and traversal.
 * Handles synchronized camera rotation during corner transitions and respects player mouse overrides.
 */
class THREADTALES_API FThreadTalesClimbingCameraHandler
{
public:
	FThreadTalesClimbingCameraHandler();

	/**
	 * Initiates synchronized camera tracking for a corner wrapping transition.
	 * Calculates the delta yaw from the character's turn and sets up synchronous interpolation.
	 *
	 * @param PC The player controller to rotate
	 * @param StartCharacterYaw Initial yaw of the character before the turn
	 * @param TargetCharacterYaw Final yaw of the character on the new wall
	 * @param TransitionDuration Total duration of the transition
	 * @param Config Climbing configuration containing camera turn parameters
	 */
	void StartCornerTurn(
		APlayerController* PC,
		float StartCharacterYaw,
		float TargetCharacterYaw,
		float TransitionDuration,
		const UThreadTalesClimbingConfig* Config
	);

	/**
	 * Updates camera rotation each tick while in corner transition.
	 * Interpolates camera yaw in lockstep with the character's active turning phase.
	 *
	 * @param DeltaTime Frame delta time
	 * @param TransitionTimer Current elapsed time of the transition
	 * @param PC The player controller
	 * @param Config Climbing configuration
	 */
	void Tick(
		float DeltaTime,
		float TransitionTimer,
		APlayerController* PC,
		const UThreadTalesClimbingConfig* Config
	);

	/**
	 * Resets active camera turn alignment.
	 * Called when the transition ends, when the player moves the mouse, or when climbing is interrupted.
	 */
	void Reset();

	/** Returns true if camera alignment is currently actively rotating the camera */
	bool IsActive() const { return bIsActive; }

private:
	/** True if camera is currently tracking an active corner turn */
	bool bIsActive;

	/** Starting camera yaw at the moment the corner transition began */
	float StartCameraYaw;

	/** Target camera yaw on the new wall */
	float TargetCameraYaw;

	/** Duration of the active turning phase in seconds (corresponds to active turn fraction of kinematics) */
	float ActiveTurnDuration;

	/** Tracks player mouse input delta to detect manual intervention */
	float LastControlRotationYaw;
};
