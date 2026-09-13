// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesClimbingCameraHandler.h"
#include "Characters/ThreadTalesClimbingConfig.h"
#include "GameFramework/PlayerController.h"

FThreadTalesClimbingCameraHandler::FThreadTalesClimbingCameraHandler()
	: bIsActive(false)
	, StartCameraYaw(0.0f)
	, TargetCameraYaw(0.0f)
	, ActiveTurnDuration(0.55f)
	, LastControlRotationYaw(0.0f)
{
}

void FThreadTalesClimbingCameraHandler::StartCornerTurn(
	APlayerController* PC,
	float StartCharacterYaw,
	float TargetCharacterYaw,
	float TransitionDuration,
	const UThreadTalesClimbingConfig* Config)
{
	if (!PC || (Config && !Config->bEnableCornerCameraTurn))
	{
		Reset();
		return;
	}

	const FRotator CurrentControlRot = PC->GetControlRotation();
	StartCameraYaw = CurrentControlRot.Yaw;

	// Calculate the angular delta of the character's corner turn (typically +90 or -90 degrees)
	const float CharacterDeltaYaw = FRotator::NormalizeAxis(TargetCharacterYaw - StartCharacterYaw);

	// The camera target yaw shifts by the exact same angular delta, preserving the player's viewing offset relative to the wall
	TargetCameraYaw = FRotator::NormalizeAxis(StartCameraYaw + CharacterDeltaYaw);

	// The active turning phase in corner kinematics takes ~0.55s
	ActiveTurnDuration = FMath::Clamp(0.55f, 0.1f, FMath::Max(TransitionDuration, 0.1f));

	LastControlRotationYaw = StartCameraYaw;
	bIsActive = true;
}

void FThreadTalesClimbingCameraHandler::Tick(
	float DeltaTime,
	float TransitionTimer,
	APlayerController* PC,
	const UThreadTalesClimbingConfig* Config)
{
	if (!bIsActive || !PC)
	{
		return;
	}

	const FRotator CurrentControlRot = PC->GetControlRotation();

	// If player significantly moves mouse during auto-turn, yield control immediately to respect player intent
	const float PlayerMouseDeltaYaw = FMath::Abs(FRotator::NormalizeAxis(CurrentControlRot.Yaw - LastControlRotationYaw));
	if (PlayerMouseDeltaYaw > 1.5f)
	{
		Reset();
		return;
	}

	// Calculate progress along the active turn phase [0.0, 1.0]
	const float TurnFraction = FMath::Clamp(TransitionTimer / FMath::Max(ActiveTurnDuration, 0.001f), 0.0f, 1.0f);
	const float SmoothT = FMath::SmoothStep(0.0f, 1.0f, TurnFraction);

	// Compute synchronous desired yaw along the shortest angular distance
	const float DeltaYaw = FRotator::NormalizeAxis(TargetCameraYaw - StartCameraYaw);
	const float DesiredYaw = FRotator::NormalizeAxis(StartCameraYaw + (DeltaYaw * SmoothT));

	// Fine smoothing interpolation to eliminate any frame-rate micro-stutter
	const float InterpSpeed = (Config ? Config->CornerCameraTurnInterpSpeed : 5.0f) * 2.0f;
	const float FinalYaw = FMath::RInterpTo(
		CurrentControlRot,
		FRotator(CurrentControlRot.Pitch, DesiredYaw, CurrentControlRot.Roll),
		DeltaTime,
		InterpSpeed
	).Yaw;

	PC->SetControlRotation(FRotator(CurrentControlRot.Pitch, FinalYaw, CurrentControlRot.Roll));
	LastControlRotationYaw = FinalYaw;

	// Automatically complete alignment once the active turn phase ends and camera is settled
	if (TurnFraction >= 1.0f && FMath::Abs(FRotator::NormalizeAxis(FinalYaw - TargetCameraYaw)) < 0.5f)
	{
		Reset();
	}
}

void FThreadTalesClimbingCameraHandler::Reset()
{
	bIsActive = false;
	StartCameraYaw = 0.0f;
	TargetCameraYaw = 0.0f;
	LastControlRotationYaw = 0.0f;
}
