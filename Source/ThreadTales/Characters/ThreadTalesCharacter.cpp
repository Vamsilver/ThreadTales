// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesCharacter.h"
#include "Abilities/ThreadTalesAbilitySystemComponent.h"
#include "ThreadTales.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Core/ThreadTalesPlayerController.h"
#include "Characters/ThreadTalesCharacterDebug.h"
#include "Characters/ThreadTalesInteractionDetectorComponent.h"
#include "Characters/ThreadTalesDoubleJumpHandler.h"
#include "Characters/ThreadTalesAbilityInputDispatcher.h"
#include "Characters/ThreadTalesTraversalComponent.h"
#include "Characters/ThreadTalesCharacterStateConfig.h"
#include "Characters/ThreadTalesTraversalInputHelper.h"
#include "Combat/ThreadTalesTargetLockComponent.h"
#include "Combat/ThreadTalesOverlayDebug.h"
#include "Input/ThreadTalesInputConfig.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "MotionWarpingComponent.h"
#include "Engine/Engine.h"

AThreadTalesCharacter::AThreadTalesCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	JumpMaxCount = 2;

	TraversalComponent = CreateDefaultSubobject<UThreadTalesTraversalComponent>(TEXT("TraversalComponent"));
	TargetLockComponent = CreateDefaultSubobject<UThreadTalesTargetLockComponent>(TEXT("TargetLockComponent"));
	InteractionDetectorComponent = CreateDefaultSubobject<UThreadTalesInteractionDetectorComponent>(TEXT("InteractionDetectorComponent"));
	DoubleJumpHandler = CreateDefaultSubobject<UThreadTalesDoubleJumpHandler>(TEXT("DoubleJumpHandler"));
	AbilityInputDispatcher = CreateDefaultSubobject<UThreadTalesAbilityInputDispatcher>(TEXT("AbilityInputDispatcher"));
	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
}

void AThreadTalesCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (!MotionWarpingComponent || MotionWarpingComponent->GetOwner() != this)
	{
		MotionWarpingComponent = FindComponentByClass<UMotionWarpingComponent>();
	}

	if (!TargetLockComponent || TargetLockComponent->GetOwner() != this)
	{
		TargetLockComponent = FindComponentByClass<UThreadTalesTargetLockComponent>();
	}
	if (!TraversalComponent || TraversalComponent->GetOwner() != this)
	{
		TraversalComponent = FindComponentByClass<UThreadTalesTraversalComponent>();
	}
	if (!InteractionDetectorComponent || InteractionDetectorComponent->GetOwner() != this)
	{
		InteractionDetectorComponent = FindComponentByClass<UThreadTalesInteractionDetectorComponent>();
	}
	if (!DoubleJumpHandler || DoubleJumpHandler->GetOwner() != this)
	{
		DoubleJumpHandler = FindComponentByClass<UThreadTalesDoubleJumpHandler>();
	}
	if (!AbilityInputDispatcher || AbilityInputDispatcher->GetOwner() != this)
	{
		AbilityInputDispatcher = FindComponentByClass<UThreadTalesAbilityInputDispatcher>();
	}

	JumpMaxCount = DoubleJumpHandler ? DoubleJumpHandler->GetMaxJumpCount() : 2;

	UE_LOG(LogThreadTales, Warning, TEXT("[PlayerCharacter] %s BeginPlay (JumpMaxCount=%d)"), *GetName(), JumpMaxCount);
}

void AThreadTalesCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		const UInputAction* CrouchAction = nullptr;
		if (InputConfig)
		{
			CrouchAction = InputConfig->FindAbilityInputActionForTag(ThreadTalesGameplayTags::TAG_Input_Crouch, false);
			if (!CrouchAction)
			{
				CrouchAction = InputConfig->FindNativeInputActionForTag(ThreadTalesGameplayTags::TAG_Input_Crouch, false);
			}
		}
		if (!CrouchAction)
		{
			CrouchAction = FindObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Crouch.IA_Crouch"));
			if (!CrouchAction)
			{
				CrouchAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Crouch.IA_Crouch"));
			}
			if (!CrouchAction)
			{
				CrouchAction = FindObject<UInputAction>(nullptr, TEXT("/GASPALS/Input/IA_Crouch.IA_Crouch"));
			}
		}

		if (CrouchAction)
		{
			EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &AThreadTalesCharacter::RequestCrouchOrClimbDown);
		}

		// Target Switch Input Action (supports Right Stick flick / Scroll wheel)
		const UInputAction* SwitchAction = TargetSwitchAction;
		if (!SwitchAction && InputConfig)
		{
			SwitchAction = InputConfig->FindAbilityInputActionForTag(ThreadTalesGameplayTags::TAG_Input_Ability_TargetSwitch, false);
			if (!SwitchAction)
			{
				SwitchAction = InputConfig->FindNativeInputActionForTag(ThreadTalesGameplayTags::TAG_Input_Ability_TargetSwitch, false);
			}
		}

		if (SwitchAction)
		{
			EnhancedInputComponent->BindAction(SwitchAction, ETriggerEvent::Triggered, this, &AThreadTalesCharacter::Input_TargetSwitch);
		}
	}

	if (AbilityInputDispatcher && InputConfig)
	{
		AbilityInputDispatcher->BindAbilityActions(PlayerInputComponent, InputConfig, AbilityBindHandles);
		UE_LOG(LogThreadTales, Display, TEXT("[PlayerCharacter] SetupPlayerInputComponent successfully bound Ability Inputs for '%s'"), *GetName());
	}
}

void AThreadTalesCharacter::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (AbilityInputDispatcher)
	{
		AbilityInputDispatcher->HandleInputTagPressed(InputTag);
	}
}

void AThreadTalesCharacter::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (AbilityInputDispatcher)
	{
		AbilityInputDispatcher->HandleInputTagReleased(InputTag);
	}
}

void AThreadTalesCharacter::Input_TargetSwitch(const FInputActionValue& Value)
{
	if (!TargetLockComponent || !TargetLockComponent->IsTargetLocked())
	{
		return;
	}

	float AxisX = 0.0f;
	if (Value.GetValueType() == EInputActionValueType::Axis1D)
	{
		AxisX = Value.Get<float>();
	}
	else if (Value.GetValueType() == EInputActionValueType::Axis2D)
	{
		AxisX = Value.Get<FVector2D>().X;
	}
	else if (Value.GetValueType() == EInputActionValueType::Boolean)
	{
		AxisX = Value.Get<bool>() ? 1.0f : -1.0f;
	}

	if (FMath::Abs(AxisX) > 0.25f)
	{
		TargetLockComponent->SwitchTarget(AxisX > 0.0f);
	}
}

bool AThreadTalesCharacter::IsClimbing() const
{
	return TraversalComponent && TraversalComponent->IsLedgeClimbing();
}

bool AThreadTalesCharacter::IsFreehangClimbing() const
{
	return TraversalComponent && TraversalComponent->IsLedgeClimbing() && TraversalComponent->IsFreehang();
}

bool AThreadTalesCharacter::IsRegrabCooldownActive() const
{
	return TraversalComponent && TraversalComponent->IsRegrabCooldownActive();
}

bool AThreadTalesCharacter::IsParkourBlocked() const
{
	if (IsClimbing() || IsRegrabCooldownActive() || MantleExitDebounceTimer > 0.0f)
	{
		return true;
	}

	const bool bCombatStance = IsInCombatStance();
	const bool bInCombatTag = AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_InCombat);
	if (bCombatStance || bInCombatTag)
	{
		return true;
	}

	if (StateConfig && StateConfig->ShouldBlockParkour(AbilitySystemComponent.Get(), this))
	{
		return true;
	}

	return false;
}

bool AThreadTalesCharacter::IsTraversalBlocked() const
{
	if (IsClimbing() || IsRegrabCooldownActive() || MantleExitDebounceTimer > 0.0f)
	{
		return true;
	}

	const bool bCombatStance = IsInCombatStance();
	const bool bInCombatTag = AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_InCombat);
	if (bCombatStance || bInCombatTag)
	{
		return true;
	}

	if (StateConfig && StateConfig->ShouldBlockTraversal(AbilitySystemComponent.Get(), this))
	{
		return true;
	}

	return false;
}

void AThreadTalesCharacter::RequestReleaseLedge()
{
	if (TraversalComponent && TraversalComponent->IsLedgeClimbing())
	{
		TraversalComponent->ReleaseLedge();
	}
}

bool AThreadTalesCharacter::TryClimbDown()
{
	return TraversalComponent ? TraversalComponent->TryClimbDown() : false;
}

void AThreadTalesCharacter::RequestCrouchOrClimbDown()
{
	if (TraversalComponent && TraversalComponent->HandleCrouchOrClimbDownInput())
	{
		return;
	}

	if (bIsCrouched)
	{
		UnCrouch();
	}
	else if (CanCrouch())
	{
		Crouch();
	}
}

void AThreadTalesCharacter::Crouch(bool bClientSimulation)
{
	if (!bClientSimulation && TraversalComponent && TraversalComponent->HandleCrouchOrClimbDownInput())
	{
		return;
	}

	Super::Crouch(bClientSimulation);
}

void AThreadTalesCharacter::UnCrouch(bool bClientSimulation)
{
	Super::UnCrouch(bClientSimulation);
}

bool AThreadTalesCharacter::TryGaspalsTraversal_Implementation()
{
	return false;
}

void AThreadTalesCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (DoubleJumpHandler)
	{
		DoubleJumpHandler->ResetDoubleJumpOnLanded();
	}
}

void AThreadTalesCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	if (PrevMovementMode == MOVE_Falling && GetCharacterMovement()->MovementMode != MOVE_Falling)
	{
		if (AbilitySystemComponent)
		{
			if (AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_HasAirDashed))
			{
				AbilitySystemComponent->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_HasAirDashed);
			}
		}

		if (DoubleJumpHandler)
		{
			DoubleJumpHandler->ResetDoubleJumpOnLanded();
		}
	}
}

void AThreadTalesCharacter::CheckJumpInput(float DeltaTime)
{
	if (JumpCurrentCount > 0 && (IsInCombatStance() || IsTraversalBlocked()))
	{
		return;
	}

	bool bWasFallingBeforeJump = GetCharacterMovement() && GetCharacterMovement()->IsFalling();

	if (bPressedJump && bWasFallingBeforeJump && JumpCurrentCount > 0)
	{
		bWasJumping = false;
	}

	int32 JumpCountBefore = JumpCurrentCount;

	Super::CheckJumpInput(DeltaTime);

	if (JumpCurrentCount > JumpCountBefore && bWasFallingBeforeJump && (DoubleJumpHandler ? !DoubleJumpHandler->HasDoubleJumpedInAir() : true))
	{
		if (DoubleJumpHandler)
		{
			DoubleJumpHandler->SetHasDoubleJumpedInAir(true);
		}
		OnDoubleJump();
	}
}

bool AThreadTalesCharacter::CanJumpInternal_Implementation() const
{
	if (JumpCurrentCount >= 1 && (IsInCombatStance() || IsTraversalBlocked()))
	{
		return false;
	}

	return Super::CanJumpInternal_Implementation();
}

void AThreadTalesCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	if (JumpCurrentCount == 1 && (DoubleJumpHandler ? !DoubleJumpHandler->HasDoubleJumpedInAir() : true))
	{
		TryConsumeStamina(GetJumpStaminaCost(), true);
	}

	UE_LOG(LogThreadTales, Display, TEXT("[Jump] %s OnJumped: JumpCurrentCount = %d / %d (Stamina: %.1f / %.1f)"), 
		*GetName(), JumpCurrentCount, JumpMaxCount, GetStamina(), GetMaxStamina());
}

void AThreadTalesCharacter::SetMaxJumpCount(int32 NewMaxJumpCount)
{
	if (DoubleJumpHandler)
	{
		DoubleJumpHandler->SetMaxJumpCount(NewMaxJumpCount);
	}
	JumpMaxCount = FMath::Max(1, NewMaxJumpCount);
}

bool AThreadTalesCharacter::IsDoubleJumping() const
{
	return DoubleJumpHandler ? DoubleJumpHandler->IsDoubleJumping() : (JumpCurrentCount > 1);
}

void AThreadTalesCharacter::OnDoubleJump()
{
	if (DoubleJumpHandler)
	{
		DoubleJumpHandler->OnDoubleJump();
	}
}

void AThreadTalesCharacter::PerformDoubleJump()
{
	if (DoubleJumpHandler)
	{
		DoubleJumpHandler->PerformDoubleJump();
	}
}

void AThreadTalesCharacter::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce)
{
	if (TraversalComponent && TraversalComponent->IsLedgeClimbing())
	{
		return;
	}

	Super::AddMovementInput(WorldDirection, ScaleValue, bForce);
}

void AThreadTalesCharacter::FaceRotation(FRotator NewControlRotation, float DeltaTime)
{
	if (TraversalComponent && TraversalComponent->IsLedgeClimbing())
	{
		return;
	}

	Super::FaceRotation(NewControlRotation, DeltaTime);
}

void AThreadTalesCharacter::Jump()
{
	if (TraversalComponent && TraversalComponent->IsLedgeClimbing())
	{
		TraversalComponent->HandleLedgeJumpInput(true);
		return;
	}

	// 1. Contextual Input Redirection (Data-Driven via StateConfig, e.g. Jump -> Evade when grounded in combat)
	if (StateConfig && AbilitySystemComponent)
	{
		FGameplayTag RedirectedTag = ThreadTalesGameplayTags::TAG_Input_Ability_Jump;
		if (StateConfig->ResolveInputRedirect(this, AbilitySystemComponent.Get(), ThreadTalesGameplayTags::TAG_Input_Ability_Jump, RedirectedTag))
		{
			UE_LOG(LogThreadTales, Display, TEXT("[Character] Direct Jump() redirected to '%s' (Owner: %s)"),
				*RedirectedTag.ToString(), *GetName());
			AbilitySystemComponent->AbilityInputTagPressed(RedirectedTag);
			return;
		}
	}

	if (DoubleJumpHandler && DoubleJumpHandler->TryTriggerInAirJump())
	{
		return;
	}

	if (!CanSpendStamina())
	{
		NotifyInsufficientStamina();
		return;
	}

	const bool bParkourBlocked = IsParkourBlocked();
	if (bEnableGaspalsParkour && !bParkourBlocked && GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround())
	{
		const bool bHasInput = GetLastMovementInputVector().SizeSquared() > 0.01f || GetVelocity().SizeSquared2D() > 10000.0f;
		if (bHasInput)
		{
			if (TryGaspalsTraversal())
			{
				TryConsumeStamina(GetJumpStaminaCost(), true);
				UE_LOG(LogThreadTales, Display, TEXT("[Traversal] %s TryGaspalsTraversal succeeded -> consumed Jump action"), *GetName());
				return;
			}
		}
	}

	Super::Jump();
}

void AThreadTalesCharacter::StopJumping()
{
	if (StateConfig && AbilitySystemComponent)
	{
		FGameplayTag RedirectedTag = ThreadTalesGameplayTags::TAG_Input_Ability_Jump;
		if (StateConfig->ResolveInputRedirect(this, AbilitySystemComponent.Get(), ThreadTalesGameplayTags::TAG_Input_Ability_Jump, RedirectedTag))
		{
			AbilitySystemComponent->AbilityInputTagReleased(RedirectedTag);
			return;
		}
	}

	Super::StopJumping();
}

void AThreadTalesCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	FThreadTalesOverlayDebug::RefreshWeaponMappingContext(this);
}

void AThreadTalesCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	FThreadTalesOverlayDebug::RefreshWeaponMappingContext(this);
}

bool AThreadTalesCharacter::CanAimOrBlock() const
{
	return FThreadTalesOverlayDebug::CanAimOrBlock(this);
}

void AThreadTalesCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (MantleExitDebounceTimer > 0.0f)
	{
		MantleExitDebounceTimer = FMath::Max(0.0f, MantleExitDebounceTimer - DeltaSeconds);
	}

	if (IsLocallyControlled())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->ViewPitchMin = CameraPitchMin;
				PC->PlayerCameraManager->ViewPitchMax = CameraPitchMax;
			}
		}
	}

	FThreadTalesOverlayDebug::UpdateAimAndBlockState(this);
	FThreadTalesOverlayDebug::UpdateOverlayDebug(this, DeltaSeconds, OverlayTelemetryTimer, LastHandsDistance, LastHandRRot, LastHandLRot);
}

void AThreadTalesCharacter::SetInteractionHandIKTarget(const FVector& TargetLocation, float Alpha)
{
	if (InteractionDetectorComponent)
	{
		InteractionDetectorComponent->SetInteractionHandIKTarget(TargetLocation, Alpha);
	}
}

void AThreadTalesCharacter::ClearInteractionHandIK()
{
	if (InteractionDetectorComponent)
	{
		InteractionDetectorComponent->ClearInteractionHandIK();
	}
}

FVector AThreadTalesCharacter::GetInteractionHandIKLocation() const
{
	return InteractionDetectorComponent ? InteractionDetectorComponent->GetInteractionHandIKLocation() : FVector::ZeroVector;
}

float AThreadTalesCharacter::GetInteractionHandIKAlpha() const
{
	return InteractionDetectorComponent ? InteractionDetectorComponent->GetInteractionHandIKAlpha() : 0.0f;
}

bool AThreadTalesCharacter::IsInteractionHandIKEnabled() const
{
	return InteractionDetectorComponent && InteractionDetectorComponent->IsInteractionHandIKEnabled();
}

FName AThreadTalesCharacter::GetInteractionHandIKBoneName() const
{
	return InteractionDetectorComponent ? InteractionDetectorComponent->GetInteractionHandIKBoneName() : FName("hand_r");
}

void AThreadTalesCharacter::DebugSwitchCharacter(TSubclassOf<AThreadTalesCharacter> NewCharacterClass)
{
	UThreadTalesCharacterDebug::SwitchCharacter(this, NewCharacterClass);
}

void AThreadTalesCharacter::ServerDebugSwitchCharacter_Implementation(TSubclassOf<AThreadTalesCharacter> NewCharacterClass)
{
	UThreadTalesCharacterDebug::SwitchCharacter(this, NewCharacterClass);
}

void AThreadTalesCharacter::DebugSwitchToMale()
{
	UThreadTalesCharacterDebug::SwitchToMale(this);
}

void AThreadTalesCharacter::DebugSwitchToFemale()
{
	UThreadTalesCharacterDebug::SwitchToFemale(this);
}

void AThreadTalesCharacter::DebugToggleGender()
{
	if (AlternateCharacterClass)
	{
		UThreadTalesCharacterDebug::SwitchCharacter(this, AlternateCharacterClass);
	}
	else
	{
		UThreadTalesCharacterDebug::ToggleGender(this);
	}
}