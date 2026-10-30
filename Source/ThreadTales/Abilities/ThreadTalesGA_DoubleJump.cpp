#include "Abilities/ThreadTalesGA_DoubleJump.h"
#include "ThreadTales.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesStaminaComponent.h"
#include "Characters/ThreadTalesDoubleJumpHandler.h"
#include "Characters/ThreadTalesTraversalInputHelper.h"
#include "Combat/ThreadTalesTargetLockComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "NiagaraFunctionLibrary.h"

UThreadTalesGA_DoubleJump::UThreadTalesGA_DoubleJump()
{
	AbilityName = TEXT("Directional Double Jump");
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	DoubleJumpZVelocity = 500.0f;

	SetAssetTags(FGameplayTagContainer(ThreadTalesGameplayTags::TAG_Ability_Movement_DoubleJump));
	ActivationOwnedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Movement_DoubleJumping);
	ActivationBlockedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_InCombat);
}

bool UThreadTalesGA_DoubleJump::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!TTChar)
	{
		return false;
	}

	// Double jump is strictly forbidden when weapon is drawn in hand or combat/traversal is blocked
	if (TTChar->IsInCombatStance() || TTChar->IsTraversalBlocked())
	{
		return false;
	}

	const UCharacterMovementComponent* CMC = TTChar->GetCharacterMovement();
	if (!CMC || !CMC->IsFalling())
	{
		return false;
	}

	if (TTChar->IsClimbing())
	{
		return false;
	}

	const UThreadTalesDoubleJumpHandler* DJHandler = TTChar->GetDoubleJumpHandler();
	if (DJHandler && DJHandler->HasDoubleJumpedInAir())
	{
		return false;
	}

	const int32 EffectiveMaxJumps = DJHandler ? DJHandler->GetMaxJumpCount() : TTChar->JumpMaxCount;
	if (TTChar->JumpCurrentCount >= EffectiveMaxJumps)
	{
		return false;
	}

	if (!TTChar->CanSpendStamina())
	{
		return false;
	}

	return true;
}

bool UThreadTalesGA_DoubleJump::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		if (UThreadTalesStaminaComponent* StamComp = TTChar->GetStaminaComponent())
		{
			if (!StamComp->CanSpendStamina())
			{
				StamComp->NotifyInsufficientStamina();
				return false;
			}
			return true;
		}
		if (!TTChar->CanSpendStamina())
		{
			return false;
		}
		return true;
	}
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}

void UThreadTalesGA_DoubleJump::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		if (UThreadTalesStaminaComponent* StamComp = TTChar->GetStaminaComponent())
		{
			StamComp->TryConsumeStamina(StamComp->GetDoubleJumpStaminaCost(), true);
		}
	}
}

FVector UThreadTalesGA_DoubleJump::ComputeMovementIntent(const ACharacter* Character) const
{
	if (!Character) return FVector::ZeroVector;

	FVector MoveDir = FVector::ZeroVector;
	if (const APlayerController* PC = Cast<APlayerController>(Character->GetController()))
	{
		const FVector2D Input2D = FThreadTalesTraversalInputHelper::GetMovementInputVector(PC, Character);
		static constexpr float InputDeadzoneThresholdSq = 0.04f;
		if (Input2D.SizeSquared() > InputDeadzoneThresholdSq)
		{
			const FRotator CamRot = PC->GetControlRotation();
			const FRotator YawRot(0.0f, CamRot.Yaw, 0.0f);
			const FVector CamFwd = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
			const FVector CamRight = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
			MoveDir = (CamFwd * Input2D.Y + CamRight * Input2D.X).GetSafeNormal();
		}
	}

	if (MoveDir.IsNearlyZero())
	{
		if (!Character->GetPendingMovementInputVector().IsNearlyZero())
		{
			MoveDir = Character->GetPendingMovementInputVector().GetSafeNormal();
		}
		else if (!Character->GetLastMovementInputVector().IsNearlyZero())
		{
			MoveDir = Character->GetLastMovementInputVector().GetSafeNormal();
		}
		else if (Character->GetCharacterMovement() && !Character->GetCharacterMovement()->GetCurrentAcceleration().IsNearlyZero())
		{
			MoveDir = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal();
		}
	}
	return MoveDir;
}

EDoubleJumpDirection UThreadTalesGA_DoubleJump::CalculateDoubleJumpDirection(float& OutAngle) const
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		OutAngle = 0.0f;
		return EDoubleJumpDirection::Neutral;
	}

	const FVector InputVector = ComputeMovementIntent(Character);
	if (InputVector.IsNearlyZero())
	{
		OutAngle = 0.0f;
		return EDoubleJumpDirection::Neutral;
	}

	AController* Controller = Character->GetController();
	const FRotator CameraRotation = Controller ? Controller->GetControlRotation() : Character->GetActorRotation();

	const FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(InputVector.Rotation(), CameraRotation);
	const float Angle = DeltaRot.Yaw;
	OutAngle = Angle;

	if (Angle >= -45.0f && Angle < 45.0f)
	{
		return EDoubleJumpDirection::Forward;
	}
	else if (Angle >= 45.0f && Angle < 135.0f)
	{
		return EDoubleJumpDirection::Right;
	}
	else if (Angle >= -135.0f && Angle < -45.0f)
	{
		return EDoubleJumpDirection::Left;
	}
	else
	{
		return EDoubleJumpDirection::Backward;
	}
}

UAnimMontage* UThreadTalesGA_DoubleJump::GetMontageForDirection(EDoubleJumpDirection Direction) const
{
	AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo());
	UThreadTalesDoubleJumpHandler* DJHandler = TTChar ? TTChar->GetDoubleJumpHandler() : nullptr;

	auto GetEffectiveMontage = [](const TObjectPtr<UAnimMontage>& AbilityMontage, const TObjectPtr<UAnimMontage>& HandlerMontage) -> UAnimMontage*
	{
		if (AbilityMontage) return AbilityMontage.Get();
		if (HandlerMontage) return HandlerMontage.Get();
		return nullptr;
	};

	switch (Direction)
	{
	case EDoubleJumpDirection::Neutral:
	{
		UAnimMontage* M = GetEffectiveMontage(DoubleJumpNeutralMontage, DJHandler ? DJHandler->DoubleJumpNeutralMontage : nullptr);
		return M;
	}
	case EDoubleJumpDirection::Forward:
	{
		UAnimMontage* M = GetEffectiveMontage(DoubleJumpForwardMontage, DJHandler ? DJHandler->DoubleJumpForwardMontage : nullptr);
		return M;
	}
	case EDoubleJumpDirection::Backward:
	{
		UAnimMontage* M = GetEffectiveMontage(DoubleJumpBackwardMontage, DJHandler ? DJHandler->DoubleJumpBackwardMontage : nullptr);
		return M;
	}
	case EDoubleJumpDirection::Left:
	{
		UAnimMontage* M = GetEffectiveMontage(DoubleJumpLeftMontage, DJHandler ? DJHandler->DoubleJumpLeftMontage : nullptr);
		return M;
	}
	case EDoubleJumpDirection::Right:
	{
		UAnimMontage* M = GetEffectiveMontage(DoubleJumpRightMontage, DJHandler ? DJHandler->DoubleJumpRightMontage : nullptr);
		return M;
	}
	default:
		return nullptr;
	}
}

void UThreadTalesGA_DoubleJump::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(Character);
	UThreadTalesDoubleJumpHandler* DJHandler = TTChar ? TTChar->GetDoubleJumpHandler() : nullptr;
	if (DJHandler)
	{
		DJHandler->SetHasDoubleJumpedInAir(true);
	}
	if (TTChar)
	{
		const int32 MaxJumps = DJHandler ? DJHandler->GetMaxJumpCount() : 2;
		TTChar->JumpCurrentCount = FMath::Max(TTChar->JumpCurrentCount + 1, MaxJumps);
	}

	const bool bTargetLocked = TTChar && TTChar->GetTargetLockComponent() && TTChar->GetTargetLockComponent()->IsTargetLocked();

	const FVector MoveDir = ComputeMovementIntent(Character);
	const bool bHasMoveInput = !MoveDir.IsNearlyZero();

	float Angle = 0.0f;
	EDoubleJumpDirection Direction = EDoubleJumpDirection::Forward;
	UAnimMontage* MontageToPlay = nullptr;

	AController* Controller = Character->GetController();

	if (bTargetLocked)
	{
		// With Target Lock: 4-way directional double jump relative to locked target / camera view
		Direction = CalculateDoubleJumpDirection(Angle);
		MontageToPlay = GetMontageForDirection(Direction);
		if (Controller && Direction != EDoubleJumpDirection::Neutral)
		{
			Character->SetActorRotation(FRotator(0.0f, Controller->GetControlRotation().Yaw, 0.0f));
		}
	}
	else
	{
		// Without Target Lock (Free Locomotion):
		// Do NOT snap to camera! Align character by movement direction.
		// For the character, double jumping along movement intent is a natural forward flip.
		if (bHasMoveInput)
		{
			Direction = EDoubleJumpDirection::Forward;
			MontageToPlay = GetMontageForDirection(EDoubleJumpDirection::Forward);
			Character->SetActorRotation(FRotator(0.0f, MoveDir.Rotation().Yaw, 0.0f));
		}
		else
		{
			// Neutral double jump (no input): vertical flip in-place without changing rotation
			Direction = EDoubleJumpDirection::Neutral;
			MontageToPlay = GetMontageForDirection(EDoubleJumpDirection::Neutral);
		}
	}

	USoundBase* SoundToPlay = DoubleJumpSound ? DoubleJumpSound.Get() : (DJHandler ? DJHandler->DoubleJumpSound.Get() : nullptr);
	UNiagaraSystem* EffectToPlay = DoubleJumpEffect ? DoubleJumpEffect.Get() : (DJHandler ? DJHandler->DoubleJumpEffect.Get() : nullptr);
	
	const float EffectiveZVelocity = (DJHandler && DJHandler->GetDoubleJumpZVelocity() > 0.0f)
		? DJHandler->GetDoubleJumpZVelocity()
		: DoubleJumpZVelocity;

	const float EffectiveHorizontalVelocity = (DJHandler && DJHandler->GetDoubleJumpHorizontalVelocity() > 0.0f)
		? DJHandler->GetDoubleJumpHorizontalVelocity()
		: DoubleJumpHorizontalVelocity;

	// Lock CMC auto-rotation to movement so character orientation is not twisted mid-air during the flip
	if (UCharacterMovementComponent* CMC = Character->GetCharacterMovement())
	{
		bSavedOrientRotationToMovement = CMC->bOrientRotationToMovement;
		bSavedUseControllerDesiredRotation = CMC->bUseControllerDesiredRotation;
		CMC->bOrientRotationToMovement = false;
		CMC->bUseControllerDesiredRotation = false;
	}

	// Apply jump impulse (vertical and horizontal)
	ApplyDoubleJumpImpulse(Character, MoveDir, bHasMoveInput, EffectiveZVelocity, EffectiveHorizontalVelocity);

	// Play optional audio and VFX feedback
	PlayDoubleJumpFeedback(Character, SoundToPlay, EffectToPlay);

	UE_LOG(LogThreadTales, Display, TEXT("[DoubleJump GA] %s activated Double Jump in direction %d (Angle: %.1f deg, Velocity.Z: %.1f, Montage: %s)"),
		*Character->GetName(), static_cast<int32>(Direction), Angle, EffectiveZVelocity, *GetNameSafe(MontageToPlay));

	// Play montage if assigned
	if (MontageToPlay)
	{
		// bStopWhenAbilityEnds = false allows montage's natural BlendOut to play out smoothly into falling pose
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay, 1.0f, NAME_None, false, 1.0f);
		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UThreadTalesGA_DoubleJump::OnMontageCompleted);
			MontageTask->OnInterrupted.AddDynamic(this, &UThreadTalesGA_DoubleJump::OnMontageInterrupted);
			MontageTask->OnCancelled.AddDynamic(this, &UThreadTalesGA_DoubleJump::OnMontageCancelled);
			MontageTask->ReadyForActivation();
		}
		else
		{
			Character->PlayAnimMontage(MontageToPlay);
		}
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}

	// Failsafe: Ensure ability ends even if montage delegates fail
	if (UWorld* World = GetWorld())
	{
		FTimerDelegate FailsafeDelegate = FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (IsActive())
			{
				UE_LOG(LogThreadTales, Warning, TEXT("[DoubleJump] Failsafe timer triggered EndAbility!"));
				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			}
		});
		const float FailsafeTimeout = (MontageToPlay ? MontageToPlay->GetPlayLength() : 1.0f) + FailsafeDurationOffset;
		World->GetTimerManager().SetTimer(FailsafeTimerHandle, FailsafeDelegate, FailsafeTimeout, false);
	}
}

void UThreadTalesGA_DoubleJump::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FailsafeTimerHandle);
	}

	// Restore CMC rotation settings
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UCharacterMovementComponent* CMC = Character->GetCharacterMovement())
		{
			CMC->bOrientRotationToMovement = bSavedOrientRotationToMovement;
			CMC->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UThreadTalesGA_DoubleJump::OnMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UThreadTalesGA_DoubleJump::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UThreadTalesGA_DoubleJump::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UThreadTalesGA_DoubleJump::ApplyDoubleJumpImpulse(ACharacter* Character, const FVector& MoveDir, bool bHasMoveInput, float EffectiveZVelocity, float EffectiveHorizontalVelocity)
{
	if (!Character) return;

	if (UCharacterMovementComponent* CMC = Character->GetCharacterMovement())
	{
		FVector CurrentVelocity = CMC->Velocity;
		CurrentVelocity.Z = EffectiveZVelocity;

		if (bHasMoveInput)
		{
			CurrentVelocity.X = MoveDir.X * EffectiveHorizontalVelocity;
			CurrentVelocity.Y = MoveDir.Y * EffectiveHorizontalVelocity;
		}

		Character->LaunchCharacter(CurrentVelocity, true, true);
	}
}

void UThreadTalesGA_DoubleJump::PlayDoubleJumpFeedback(ACharacter* Character, USoundBase* Sound, UNiagaraSystem* Effect)
{
	if (!Character) return;

	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(Character, Sound, Character->GetActorLocation());
	}

	if (Effect)
	{
		const float CapsuleHalfHeight = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f;
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(Character->GetWorld(), Effect, Character->GetActorLocation() - FVector(0.0f, 0.0f, CapsuleHalfHeight));
	}
}


