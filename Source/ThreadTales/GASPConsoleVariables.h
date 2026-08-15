// Copyright Epic Games, Inc. All Rights Reserved.
// Console variables required by Game Animation Sample Blueprints.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

/**
 * Registers all console variables (CVars) that the Game Animation Sample
 * Blueprints expect to exist. Declaring them as TAutoConsoleVariable prevents
 * "Failed to find console variable" and performance warnings.
 *
 * Include this header in exactly ONE .cpp file (ThreadTales.cpp).
 */
namespace GASPConsoleVariables
{
	// ── Debug Drawing ──────────────────────────────────────────────────
	static TAutoConsoleVariable<int32> CVarDrawCharacterDebugShapes(
		TEXT("DDCvar.DrawCharacterDebugShapes"),
		0,
		TEXT("Draw character debug shapes (capsule, arrows, etc). 0 = off, 1 = on."),
		ECVF_Cheat);

	static TAutoConsoleVariable<int32> CVarDrawCharacterDebugStates(
		TEXT("DDCvar.DrawCharacterDebugStates"),
		0,
		TEXT("Draw character state info on screen. 0 = off, 1 = on."),
		ECVF_Cheat);

	static TAutoConsoleVariable<int32> CVarDrawCharacterDebugGraphs(
		TEXT("DDCvar.DrawCharacterDebugGraphs"),
		0,
		TEXT("Draw character debug graphs. 0 = off, 1 = on."),
		ECVF_Cheat);

	// ── Root Motion ────────────────────────────────────────────────────
	static TAutoConsoleVariable<float> CVarOffsetRootBoneTranslationRadius(
		TEXT("DDCvar.OffsetRootBone.TranslationRadius"),
		0.0f,
		TEXT("Radius for root bone translation offset."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarAttributeBasedRootMotion(
		TEXT("DDCVar.AttributeBasedRootMotion.Enable"),
		0,
		TEXT("Enable attribute based root motion."),
		ECVF_Default);

	// ── Animation System ───────────────────────────────────────────────
	static TAutoConsoleVariable<int32> CVarThreadSafeAnimationUpdate(
		TEXT("DDCVar.ThreadSafeAnimationUpdate.Enable"),
		1,
		TEXT("Enable thread-safe animation update. 0 = off, 1 = on."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarExperimentalStateMachine(
		TEXT("DDCVar.ExperimentalStateMachine.Enable"),
		0,
		TEXT("Enable the experimental state machine locomotion. 0 = off (Motion Matching), 1 = on."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarExperimentalStateMachineDebug(
		TEXT("DDCVar.ExperimentalStateMachine.Debug"),
		0,
		TEXT("Enable debugging for experimental state machine."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarLocomotionSetupCMC(
		TEXT("DDCVar.LocomotionSetupCMC"),
		1,
		TEXT("Locomotion setup for CMC character. 0 = Mover, 1 = CMC."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarLocomotionSetupMover(
		TEXT("DDCvar.LocomotionSetupMover"),
		1,
		TEXT("Locomotion setup for Mover character."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarDefaultAnimSet(
		TEXT("DDCVar.DefaultAnimSet"),
		0,
		TEXT("Default animation set."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarUseUnrealAnimationFramework(
		TEXT("DDCvar.UseUnrealAnimationFramework"),
		0,
		TEXT("Use Unreal Animation Framework."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarFootPlacementMode(
		TEXT("DDCvar.FootPlacementMode"),
		1,
		TEXT("0 == off, 1 == foot placement node, 2 == biped control rig"),
		ECVF_Default);

	// ── Camera ─────────────────────────────────────────────────────────
	static TAutoConsoleVariable<int32> CVarCameraStyle(
		TEXT("DDCvar.CameraStyle"),
		0,
		TEXT("Camera style index. 0 = default third person, 1 = exploration, 2 = combat."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarNewGameplayCameraSystem(
		TEXT("DDCVar.NewGameplayCameraSystem.Enable"),
		1,
		TEXT("Enable new gameplay camera system. 0 = off, 1 = on."),
		ECVF_Default);

	// ── Control / Input Styles ─────────────────────────────────────────
	static TAutoConsoleVariable<int32> CVarStrafeStyle(
		TEXT("DDCvar.StrafeStyle"),
		1,
		TEXT("Strafe style index."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarAimStyle(
		TEXT("DDCvar.AimStyle"),
		0,
		TEXT("Aim style index."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarControlStyle(
		TEXT("DDCvar.ControlStyle"),
		0,
		TEXT("Control style index."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarAnalogInputStyle(
		TEXT("DDCvar.AnalogInputStyle"),
		0,
		TEXT("Analog input style index."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarPawnClass(
		TEXT("DDCvar.PawnClass"),
		-1,
		TEXT("Pawn class index."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarVisualOverride(
		TEXT("DDCvar.VisualOverride"),
		-1,
		TEXT("Visual override index."),
		ECVF_Default);

	// ── Audio Debug ────────────────────────────────────────────────────
	static TAutoConsoleVariable<int32> CVarDrawVisLogShapesForFoleySounds(
		TEXT("DDCvar.DrawVisLogShapesForFoleySounds"),
		0,
		TEXT("Draw visual log shapes for foley sounds. 0 = off, 1 = on."),
		ECVF_Cheat);

	// ── Motion Matching ────────────────────────────────────────────────
	static TAutoConsoleVariable<int32> CVarMMDatabaseLOD(
		TEXT("DDCvar.MMDatabaseLOD"),
		0,
		TEXT("Motion Matching database LOD level. 0 = full quality."),
		ECVF_Default);

	// ── Traversal Debug ────────────────────────────────────────────────
	static TAutoConsoleVariable<int32> CVarTraversalDrawDebugLevel(
		TEXT("DDCvar.Traversal.DrawDebugLevel"),
		0,
		TEXT("Draw debug for traversal. 0 = off, 1 = low, 2 = high."),
		ECVF_Cheat);

	static TAutoConsoleVariable<float> CVarTraversalDrawDebugDuration(
		TEXT("DDCvar.Traversal.DrawDebugDuration"),
		2.0f,
		TEXT("Duration for traversal debug drawing."),
		ECVF_Cheat);
}
