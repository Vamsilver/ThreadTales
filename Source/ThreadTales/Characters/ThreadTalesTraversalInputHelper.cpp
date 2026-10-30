// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesTraversalInputHelper.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "InputCoreTypes.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Input/ThreadTalesInputConfig.h"
#include "Core/ThreadTalesGameplayTags.h"

EThreadTalesLedgeJumpDirection FThreadTalesTraversalInputHelper::GetClimbingJumpDirection(const APlayerController* PC, bool& bOutIsSprint, const ACharacter* Character)
{
	bOutIsSprint = false;
	if (!PC)
	{
		return EThreadTalesLedgeJumpDirection::None;
	}

	const bool bSpacePressed = IsJumpPressed(PC, Character);
	const FVector2D Input2D = GetMovementInputVector(PC, Character);

	bool bW = (Input2D.Y > 0.35f);
	bool bS = (Input2D.Y < -0.35f);
	bool bA = (Input2D.X < -0.35f);
	bool bD = (Input2D.X > 0.35f);

	// Space defaults to Up (climb up/hop up) ONLY if no other direction is pressed
	if (bSpacePressed && !bW && !bS && !bA && !bD)
	{
		bW = true;
	}

	bOutIsSprint = IsSprintHeld(PC, Character);

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

float FThreadTalesTraversalInputHelper::GetLateralInputSign(const APlayerController* PC, const ACharacter* Character)
{
	if (!PC)
	{
		return 0.0f;
	}

	const FVector2D Input2D = GetMovementInputVector(PC, Character);
	if (Input2D.X > 0.25f)
	{
		return 1.0f;
	}
	if (Input2D.X < -0.25f)
	{
		return -1.0f;
	}

	return 0.0f;
}

bool FThreadTalesTraversalInputHelper::IsBackInputHeld(const APlayerController* PC, const ACharacter* Character, const FVector* WallNormal)
{
	if (!PC)
	{
		return false;
	}

	const FVector2D Input2D = GetMovementInputVector(PC, Character);
	if (Input2D.Y < -0.35f)
	{
		return true;
	}

	// Camera-aware outward intent: if camera is facing away from the wall into open space/room
	// and player presses W / Forward input, player is steering outward away from the wall!
	if (WallNormal && Input2D.Y > 0.35f)
	{
		const FRotator ControlRot = PC->GetControlRotation();
		const FVector CamFwd = FRotationMatrix(ControlRot).GetUnitAxis(EAxis::X);
		const FVector CamFwd2D = FVector(CamFwd.X, CamFwd.Y, 0.0f).GetSafeNormal();
		const FVector WallNorm2D = FVector(WallNormal->X, WallNormal->Y, 0.0f).GetSafeNormal();

		if (FVector::DotProduct(CamFwd2D, WallNorm2D) > 0.2f)
		{
			return true;
		}
	}

	return false;
}

bool FThreadTalesTraversalInputHelper::IsJumpPressed(const APlayerController* PC, const ACharacter* Character)
{
	if (!PC)
	{
		return false;
	}

	if (const UEnhancedPlayerInput* EnhancedPlayerInput = Cast<UEnhancedPlayerInput>(PC->PlayerInput))
	{
		const UInputAction* JumpAction = nullptr;
		const ACharacter* Char = Character ? Character : Cast<ACharacter>(PC->GetPawn());
		const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(Char);
		if (TTChar && TTChar->GetInputConfig())
		{
			JumpAction = TTChar->GetInputConfig()->FindAbilityInputActionForTag(ThreadTalesGameplayTags::TAG_Input_Ability_Jump, false);
		}
		if (!JumpAction)
		{
			static const UInputAction* CachedJumpAction = nullptr;
			if (!CachedJumpAction)
			{
				CachedJumpAction = FindObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Jump.IA_Jump"));
				if (!CachedJumpAction)
				{
					CachedJumpAction = FindObject<UInputAction>(nullptr, TEXT("/GASPALS/Input/IA_Jump.IA_Jump"));
				}
			}
			JumpAction = CachedJumpAction;
		}

		if (JumpAction && EnhancedPlayerInput->GetActionValue(JumpAction).Get<bool>())
		{
			return true;
		}
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

	// 1. Primary: Read from Enhanced Input subsystem / UEnhancedPlayerInput
	if (const UEnhancedPlayerInput* EnhancedPlayerInput = Cast<UEnhancedPlayerInput>(PC->PlayerInput))
	{
		const UInputAction* MoveAction = nullptr;

		// Check Character's InputConfig for TAG_Input_Move
		const ACharacter* Char = Character ? Character : Cast<ACharacter>(PC->GetPawn());
		const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(Char);
		if (TTChar && TTChar->GetInputConfig())
		{
			MoveAction = TTChar->GetInputConfig()->FindNativeInputActionForTag(ThreadTalesGameplayTags::TAG_Input_Move, false);
		}

		// Fallback: find from known asset paths
		if (!MoveAction)
		{
			static const UInputAction* CachedMoveAction = nullptr;
			if (!CachedMoveAction)
			{
				CachedMoveAction = FindObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
				if (!CachedMoveAction)
				{
					CachedMoveAction = FindObject<UInputAction>(nullptr, TEXT("/GASPALS/Input/IA_Move.IA_Move"));
				}
			}
			MoveAction = CachedMoveAction;
		}

		if (MoveAction)
		{
			const FInputActionValue ActionVal = EnhancedPlayerInput->GetActionValue(MoveAction);
			const FVector2D Move2D = ActionVal.Get<FVector2D>();
			if (Move2D.SizeSquared() > 0.01f)
			{
				return Move2D.GetClampedToMaxSize(1.0f);
			}
		}
	}

	// 2. Hardware fallback: keyboard check (WASD & arrow keys)
	float Fwd = 0.0f;
	float Right = 0.0f;

	if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up))
	{
		Fwd += 1.0f;
	}
	if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down))
	{
		Fwd -= 1.0f;
	}
	if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right))
	{
		Right += 1.0f;
	}
	if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left))
	{
		Right -= 1.0f;
	}

	if (Fwd != 0.0f || Right != 0.0f)
	{
		return FVector2D(Right, Fwd).GetClampedToMaxSize(1.0f);
	}

	// 3. Hardware fallback: Gamepad analog Left Stick and D-Pad check
	float StickX = 0.0f;
	float StickY = 0.0f;
	PC->GetInputAnalogStickState(EControllerAnalogStick::CAS_LeftStick, StickX, StickY);
	if (PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Right) || PC->IsInputKeyDown(EKeys::Gamepad_DPad_Right)) StickX = 1.0f;
	if (PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Left)  || PC->IsInputKeyDown(EKeys::Gamepad_DPad_Left))  StickX = -1.0f;
	if (PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Up)    || PC->IsInputKeyDown(EKeys::Gamepad_DPad_Up))    StickY = 1.0f;
	if (PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Down)  || PC->IsInputKeyDown(EKeys::Gamepad_DPad_Down))  StickY = -1.0f;

	if (FMath::Abs(StickX) > 0.15f || FMath::Abs(StickY) > 0.15f)
	{
		return FVector2D(StickX, StickY).GetClampedToMaxSize(1.0f);
	}

	return FVector2D::ZeroVector;
}

bool FThreadTalesTraversalInputHelper::IsSprintHeld(const APlayerController* PC, const ACharacter* Character)
{
	if (!PC)
	{
		return false;
	}

	if (const UEnhancedPlayerInput* EnhancedPlayerInput = Cast<UEnhancedPlayerInput>(PC->PlayerInput))
	{
		const UInputAction* SprintAction = nullptr;
		const ACharacter* Char = Character ? Character : Cast<ACharacter>(PC->GetPawn());
		const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(Char);
		if (TTChar && TTChar->GetInputConfig())
		{
			SprintAction = TTChar->GetInputConfig()->FindNativeInputActionForTag(ThreadTalesGameplayTags::TAG_Input_Sprint, false);
			if (!SprintAction)
			{
				SprintAction = TTChar->GetInputConfig()->FindAbilityInputActionForTag(ThreadTalesGameplayTags::TAG_Input_Sprint, false);
			}
		}
		if (!SprintAction)
		{
			static const UInputAction* CachedSprintAction = nullptr;
			if (!CachedSprintAction)
			{
				CachedSprintAction = FindObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Sprint.IA_Sprint"));
				if (!CachedSprintAction)
				{
					CachedSprintAction = FindObject<UInputAction>(nullptr, TEXT("/GASPALS/Input/IA_Sprint.IA_Sprint"));
				}
			}
			SprintAction = CachedSprintAction;
		}

		if (SprintAction && EnhancedPlayerInput->GetActionValue(SprintAction).Get<bool>())
		{
			return true;
		}
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
	bool bBackJumpIntent,
	const ACharacter* Character)
{
	const FVector Up = FVector::UpVector;
	const FVector SafeWallNormal = WallNormal.GetSafeNormal();
	const FVector SafeWallRight = WallRight.GetSafeNormal();

	if (!PC)
	{
		return Up;
	}

	const FVector2D Input2D = GetMovementInputVector(PC, Character);
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

	// If bBackJumpIntent is explicitly active (S held or camera looking away from wall with W held),
	// blend outward away from wall along SafeWallNormal into open 3D space
	if (bBackJumpIntent)
	{
		const float OutwardAmount = FMath::Max(FMath::Abs(Input2D.Y), 0.7f);
		const FVector OutwardComponent = SafeWallNormal * OutwardAmount;
		const FVector Combined = LateralComponent + OutwardComponent + (Up * 0.25f);
		return Combined.GetSafeNormal();
	}

	const FVector WorldIntent = LateralComponent + VerticalComponent;
	return WorldIntent.IsNearlyZero() ? Up : WorldIntent.GetSafeNormal();
}
