#include "Abilities/ThreadTalesGA_DoubleJump.h"
#include "ThreadTales.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Characters/ThreadTalesCharacter.h"
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
}

EDoubleJumpDirection UThreadTalesGA_DoubleJump::CalculateDoubleJumpDirection(float& OutAngle) const
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		OutAngle = 0.0f;
		return EDoubleJumpDirection::Neutral;
	}

	FVector InputVector = Character->GetLastMovementInputVector();
	if (InputVector.IsNearlyZero() && Character->GetCharacterMovement())
	{
		InputVector = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal();
	}

	// If no movement input is pressed, return Neutral (in-place jump)
	if (InputVector.IsNearlyZero())
	{
		OutAngle = 0.0f;
		return EDoubleJumpDirection::Neutral;
	}

	AController* Controller = Character->GetController();
	FRotator CameraRotation = Controller ? Controller->GetControlRotation() : Character->GetActorRotation();

	// Calculate angle between Movement Input and Camera Heading (-180..+180)
	FRotator InputRot = InputVector.Rotation();
	FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(InputRot, CameraRotation);
	float Angle = DeltaRot.Yaw;
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

	auto GetEffectiveMontage = [](const TObjectPtr<UAnimMontage>& AbilityMontage, const TObjectPtr<UAnimMontage>& CharacterMontage) -> UAnimMontage*
	{
		if (AbilityMontage) return AbilityMontage.Get();
		if (CharacterMontage) return CharacterMontage.Get();
		return nullptr;
	};

	switch (Direction)
	{
	case EDoubleJumpDirection::Neutral:
	{
		UAnimMontage* M = GetEffectiveMontage(DoubleJumpNeutralMontage, TTChar ? TTChar->DoubleJumpNeutralMontage : nullptr);
		return M;
	}
	case EDoubleJumpDirection::Forward:
	{
		UAnimMontage* M = GetEffectiveMontage(DoubleJumpForwardMontage, TTChar ? TTChar->DoubleJumpForwardMontage : nullptr);
		return M;
	}
	case EDoubleJumpDirection::Backward:
	{
		UAnimMontage* M = GetEffectiveMontage(DoubleJumpBackwardMontage, TTChar ? TTChar->DoubleJumpBackwardMontage : nullptr);
		return M;
	}
	case EDoubleJumpDirection::Left:
	{
		UAnimMontage* M = GetEffectiveMontage(DoubleJumpLeftMontage, TTChar ? TTChar->DoubleJumpLeftMontage : nullptr);
		return M;
	}
	case EDoubleJumpDirection::Right:
	{
		UAnimMontage* M = GetEffectiveMontage(DoubleJumpRightMontage, TTChar ? TTChar->DoubleJumpRightMontage : nullptr);
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

	float Angle = 0.0f;
	EDoubleJumpDirection Direction = CalculateDoubleJumpDirection(Angle);
	UAnimMontage* MontageToPlay = GetMontageForDirection(Direction);

	USoundBase* SoundToPlay = DoubleJumpSound ? DoubleJumpSound.Get() : (TTChar ? TTChar->DoubleJumpSound.Get() : nullptr);
	UNiagaraSystem* EffectToPlay = DoubleJumpEffect ? DoubleJumpEffect.Get() : (TTChar ? TTChar->DoubleJumpEffect.Get() : nullptr);
	
	float EffectiveZVelocity = DoubleJumpZVelocity;

	// Lock CMC auto-rotation to movement so character orientation is not twisted mid-air during the flip
	if (UCharacterMovementComponent* CMC = Character->GetCharacterMovement())
	{
		bSavedOrientRotationToMovement = CMC->bOrientRotationToMovement;
		bSavedUseControllerDesiredRotation = CMC->bUseControllerDesiredRotation;
		CMC->bOrientRotationToMovement = false;
		CMC->bUseControllerDesiredRotation = false;
	}

	// Align character rotation to camera view for directional jumps
	AController* Controller = Character->GetController();
	if (Controller && Direction != EDoubleJumpDirection::Neutral)
	{
		Character->SetActorRotation(FRotator(0.0f, Controller->GetControlRotation().Yaw, 0.0f));
	}

	// Apply jump impulse (vertical and horizontal)
	if (UCharacterMovementComponent* CMC = Character->GetCharacterMovement())
	{
		FVector CurrentVelocity = CMC->Velocity;
		CurrentVelocity.Z = EffectiveZVelocity;

		if (Direction != EDoubleJumpDirection::Neutral)
		{
			FVector InputVector = Character->GetLastMovementInputVector();
			if (!InputVector.IsNearlyZero())
			{
				InputVector.Normalize();
				CurrentVelocity.X = InputVector.X * DoubleJumpHorizontalVelocity;
				CurrentVelocity.Y = InputVector.Y * DoubleJumpHorizontalVelocity;
			}
		}

		CMC->Velocity = CurrentVelocity;
	}

	// Play optional sound
	if (SoundToPlay)
	{
		UGameplayStatics::PlaySoundAtLocation(Character, SoundToPlay, Character->GetActorLocation());
	}

	// Spawn optional Niagara VFX at feet
	if (EffectToPlay)
	{
		const float CapsuleHalfHeight = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f;
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(Character->GetWorld(), EffectToPlay, Character->GetActorLocation() - FVector(0.0f, 0.0f, CapsuleHalfHeight));
	}

	UE_LOG(LogThreadTales, Display, TEXT("[DoubleJump GA] %s activated Double Jump in direction %d (Angle: %.1f deg, Velocity.Z: %.1f, Montage: %s)"),
		*Character->GetName(), static_cast<int32>(Direction), Angle, EffectiveZVelocity, *GetNameSafe(MontageToPlay));

	// Play montage if assigned
	if (MontageToPlay)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay, 1.0f, NAME_None, true, 1.0f);
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
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		}
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}

	// Failsafe: Ensure ability ends even if montage delegates fail
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FailsafeTimerHandle, [this]()
		{
			if (IsActive())
			{
				UE_LOG(LogThreadTales, Warning, TEXT("[DoubleJump] Failsafe timer triggered EndAbility!"));
				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			}
		}, 1.5f, false);
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


