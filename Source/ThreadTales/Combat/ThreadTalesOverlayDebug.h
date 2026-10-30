// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class AThreadTalesCharacter;

/**
 * Diagnostic and telemetry utility for weapon overlay evaluation (Hand IK, spine/pelvis clearance, wrist flip).
 * Completely decoupled from AThreadTalesCharacter to eliminate God-Class bloat.
 */
class THREADTALES_API FThreadTalesOverlayDebug
{
public:
	/** Evaluates skeletal transforms, clearance, and emits debug visualization & logs if enabled */
	static void UpdateOverlayDebug(
		AThreadTalesCharacter* Character,
		float DeltaSeconds,
		float& InOutTelemetryTimer,
		float& InOutLastHandsDistance,
		FRotator& InOutLastHandRRot,
		FRotator& InOutLastHandLRot);

	/** Returns true if character is allowed to enter Aim or Block stance (not sprinting, in combat stance, not staggered/evading) */
	static bool CanAimOrBlock(const AThreadTalesCharacter* Character);

	/** Ensures GASPALS aim zoom, strafe, and smooth camera-aligned rotation are updated every tick */
	static void UpdateAimAndBlockState(AThreadTalesCharacter* Character, float DeltaSeconds = 0.0f);

	/** Robust socket docking: ensures HeldObjectRoot is attached to the appropriate socket (e.g. overlay_bow for Bow) */
	static void DockHeldObjectRoot(AThreadTalesCharacter* Character, uint8 NewOverlayPose);

	/** Resolves dynamic weapon input tags (e.g. converting melee input to ranged fire if bow is equipped) */
	static FGameplayTag ResolveWeaponInputTag(const AThreadTalesCharacter* Character, FGameplayTag InputTag);

	/** Ensures current equipped weapon's IMC is registered with player controller */
	static void RefreshWeaponMappingContext(const AThreadTalesCharacter* Character);
};
