// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesTraversalInputHelper.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "InputCoreTypes.h"

EThreadTalesLedgeJumpDirection FThreadTalesTraversalInputHelper::GetClimbingJumpDirection(const APlayerController* PC, bool& bOutIsSprint)
{
	bOutIsSprint = false;
	if (!PC)
	{
		return EThreadTalesLedgeJumpDirection::None;
	}

	const bool bSpacePressed = PC->IsInputKeyDown(EKeys::SpaceBar) || PC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom);
	bool bW = PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Up) || PC->IsInputKeyDown(EKeys::Up);
	bool bS = PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Down) || PC->IsInputKeyDown(EKeys::Down);
	bool bA = PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Left) || PC->IsInputKeyDown(EKeys::Left);
	bool bD = PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Right) || PC->IsInputKeyDown(EKeys::Right);

	// Also support analog left stick deflection above deadzone without masking digital keys
	float StickX = 0.0f;
	float StickY = 0.0f;
	PC->GetInputAnalogStickState(EControllerAnalogStick::CAS_LeftStick, StickX, StickY);
	if (StickY > 0.35f) bW = true;
	if (StickY < -0.35f) bS = true;
	if (StickX < -0.35f) bA = true;
	if (StickX > 0.35f) bD = true;

	// Space defaults to Up (climb up/hop up) ONLY if no other direction is pressed
	if (bSpacePressed && !bW && !bS && !bA && !bD)
	{
		bW = true;
	}

	bOutIsSprint = IsSprintHeld(PC);

	if (bW && bD) return EThreadTalesLedgeJumpDirection::UpRight;
	if (bW && bA) return EThreadTalesLedgeJumpDirection::UpLeft;
	if (bS && bD) return EThreadTalesLedgeJumpDirection::DownRight;
	if (bS && bA) return EThreadTalesLedgeJumpDirection::DownLeft;
	if (bW) return EThreadTalesLedgeJumpDirection::Up;
	if (bS) return EThreadTalesLedgeJumpDirection::Down;
	if (bD) return EThreadTalesLedgeJumpDirection::Right;
	if (bA) return EThreadTalesLedgeJumpDirection::Left;

	return EThreadTalesLedgeJumpDirection::None;
}

float FThreadTalesTraversalInputHelper::GetLateralInputSign(const APlayerController* PC)
{
	if (!PC)
	{
		return 0.0f;
	}

	if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Right) || PC->IsInputKeyDown(EKeys::Right))
	{
		return 1.0f;
	}
	if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Left) || PC->IsInputKeyDown(EKeys::Left))
	{
		return -1.0f;
	}

	float StickX = 0.0f;
	float StickY = 0.0f;
	PC->GetInputAnalogStickState(EControllerAnalogStick::CAS_LeftStick, StickX, StickY);
	if (StickX > 0.35f) return 1.0f;
	if (StickX < -0.35f) return -1.0f;

	return 0.0f;
}

bool FThreadTalesTraversalInputHelper::IsBackInputHeld(const APlayerController* PC)
{
	if (!PC)
	{
		return false;
	}

	if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Down) || PC->IsInputKeyDown(EKeys::Down))
	{
		return true;
	}

	float StickX = 0.0f;
	float StickY = 0.0f;
	PC->GetInputAnalogStickState(EControllerAnalogStick::CAS_LeftStick, StickX, StickY);
	return (StickY < -0.35f);
}

bool FThreadTalesTraversalInputHelper::IsJumpPressed(const APlayerController* PC)
{
	if (!PC)
	{
		return false;
	}

	return PC->IsInputKeyDown(EKeys::SpaceBar) || PC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom);
}

EThreadTalesLedgeJumpDirection FThreadTalesTraversalInputHelper::VectorToJumpDirection(const FVector2D& InputVector, float DeadZone)
{
	if (InputVector.SizeSquared() < (DeadZone * DeadZone))
	{
		return EThreadTalesLedgeJumpDirection::None;
	}

	// Atan2(Y, X) returns angle in radians [-PI, PI], where:
	// X = Right (+1) / Left (-1)
	// Y = Up/Forward (+1) / Down/Backward (-1)
	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Atan2(InputVector.Y, InputVector.X));

	// 8-directional angular classification (each sector is 45 degrees wide):
	// Right:      -22.5 to  +22.5
	// Up-Right:   +22.5 to  +67.5
	// Up:         +67.5 to +112.5
	// Up-Left:   +112.5 to +157.5
	// Left:      > +157.5 or < -157.5
	// Down-Left: -157.5 to -112.5
	// Down:      -112.5 to  -67.5
	// Down-Right: -67.5 to  -22.5

	if (AngleDegrees >= -22.5f && AngleDegrees <= 22.5f)
	{
		return EThreadTalesLedgeJumpDirection::Right;
	}
	else if (AngleDegrees > 22.5f && AngleDegrees <= 67.5f)
	{
		return EThreadTalesLedgeJumpDirection::UpRight;
	}
	else if (AngleDegrees > 67.5f && AngleDegrees <= 112.5f)
	{
		return EThreadTalesLedgeJumpDirection::Up;
	}
	else if (AngleDegrees > 112.5f && AngleDegrees <= 157.5f)
	{
		return EThreadTalesLedgeJumpDirection::UpLeft;
	}
	else if (AngleDegrees > 157.5f || AngleDegrees < -157.5f)
	{
		return EThreadTalesLedgeJumpDirection::Left;
	}
	else if (AngleDegrees >= -157.5f && AngleDegrees < -112.5f)
	{
		return EThreadTalesLedgeJumpDirection::DownLeft;
	}
	else if (AngleDegrees >= -112.5f && AngleDegrees < -67.5f)
	{
		return EThreadTalesLedgeJumpDirection::Down;
	}
	else // AngleDegrees >= -67.5f && AngleDegrees < -22.5f
	{
		return EThreadTalesLedgeJumpDirection::DownRight;
	}
}

FVector2D FThreadTalesTraversalInputHelper::GetMovementInputVector(const APlayerController* PC, const ACharacter* Character)
{
	if (!PC)
	{
		return FVector2D::ZeroVector;
	}

	FVector2D Result = FVector2D::ZeroVector;

	// 1. Check Gamepad analog Left Stick for true proportional precision
	float StickX = 0.0f;
	float StickY = 0.0f;
	PC->GetInputAnalogStickState(EControllerAnalogStick::CAS_LeftStick, StickX, StickY);
	if (FMath::Abs(StickX) > 0.1f || FMath::Abs(StickY) > 0.1f)
	{
		Result = FVector2D(StickX, StickY);
	}
	else
	{
		// 2. Keyboard fallback (WASD and standard directional keys)
		float Fwd = 0.0f;
		float Right = 0.0f;

		if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Up) || PC->IsInputKeyDown(EKeys::Up))
		{
			Fwd += 1.0f;
		}
		if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Down) || PC->IsInputKeyDown(EKeys::Down))
		{
			Fwd -= 1.0f;
		}
		if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Right) || PC->IsInputKeyDown(EKeys::Right))
		{
			Right += 1.0f;
		}
		if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Left) || PC->IsInputKeyDown(EKeys::Left))
		{
			Right -= 1.0f;
		}

		Result = FVector2D(Right, Fwd);
	}

	return Result.GetClampedToMaxSize(1.0f);
}

bool FThreadTalesTraversalInputHelper::IsSprintHeld(const APlayerController* PC)
{
	if (!PC)
	{
		return false;
	}

	return PC->IsInputKeyDown(EKeys::LeftShift) ||
	       PC->IsInputKeyDown(EKeys::RightShift) ||
	       PC->IsInputKeyDown(EKeys::Gamepad_LeftThumbstick) ||
	       PC->IsInputKeyDown(EKeys::Gamepad_LeftShoulder);
}

FVector FThreadTalesTraversalInputHelper::GetClimbingIntentWorldVector(
	const APlayerController* PC,
	const FVector& WallNormal,
	const FVector& WallRight,
	bool bBackJumpIntent)
{
	const FVector Up = FVector::UpVector;
	const FVector SafeWallNormal = WallNormal.GetSafeNormal();
	const FVector SafeWallRight = WallRight.GetSafeNormal();

	if (!PC)
	{
		return Up;
	}

	const FVector2D Input2D = GetMovementInputVector(PC, nullptr);
	const bool bHasInput = (Input2D.SizeSquared() > 0.04f);

	if (!bHasInput)
	{
		// Neutral input: pressing Space or attempting grab defaults to upward intent
		return Up;
	}

	// Lateral component along wall face: X is Right (+1) / Left (-1)
	const FVector LateralComponent = SafeWallRight * Input2D.X;

	// Vertical component: Y is Forward/Up (+1) / Backward/Down (-1)
	const FVector VerticalComponent = Up * Input2D.Y;

	// If player is pulling backward (Y < -0.1f) and bBackJumpIntent is explicitly active,
	// blend outward away from wall along SafeWallNormal into open 3D space
	if (Input2D.Y < -0.1f && bBackJumpIntent)
	{
		const float BackAmount = FMath::Abs(Input2D.Y);
		const FVector OutwardComponent = SafeWallNormal * BackAmount;
		const FVector Combined = LateralComponent + OutwardComponent + (Up * (Input2D.Y * 0.4f));
		return Combined.GetSafeNormal();
	}

	const FVector WorldIntent = LateralComponent + VerticalComponent;
	return WorldIntent.IsNearlyZero() ? Up : WorldIntent.GetSafeNormal();
}
