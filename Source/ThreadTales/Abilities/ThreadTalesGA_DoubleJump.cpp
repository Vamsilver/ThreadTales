// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesGA_DoubleJump.h"
#include "ThreadTales.h"
#include "ThreadTalesGameplayTags.h"
#include "Character/ThreadTalesCharacter.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

UThreadTalesGA_DoubleJump::UThreadTalesGA_DoubleJump()
{
	AbilityName = TEXT("Double Jump");
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	DoubleJumpZVelocity = 500.0f;

	SetAssetTags(FGameplayTagContainer(ThreadTalesGameplayTags::TAG_Ability_Movement_DoubleJump));
	ActivationOwnedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Movement_DoubleJumping);
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

	// Get montage, sound, VFX, and velocity from this ability, or fall back to character settings
	UAnimMontage* MontageToPlay = DoubleJumpMontage ? DoubleJumpMontage.Get() : (TTChar ? TTChar->DoubleJumpMontage.Get() : nullptr);
	USoundBase* SoundToPlay = DoubleJumpSound ? DoubleJumpSound.Get() : (TTChar ? TTChar->DoubleJumpSound.Get() : nullptr);
	UNiagaraSystem* EffectToPlay = DoubleJumpEffect ? DoubleJumpEffect.Get() : (TTChar ? TTChar->DoubleJumpEffect.Get() : nullptr);
	
	float EffectiveZVelocity = DoubleJumpZVelocity;
	if (TTChar && TTChar->DoubleJumpZVelocity > 0.0f)
	{
		EffectiveZVelocity = TTChar->DoubleJumpZVelocity;
	}

	// Apply vertical jump velocity
	if (UCharacterMovementComponent* CMC = Character->GetCharacterMovement())
	{
		FVector CurrentVelocity = CMC->Velocity;
		CurrentVelocity.Z = EffectiveZVelocity;
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

	UE_LOG(LogThreadTales, Display, TEXT("[DoubleJump GA] %s activated Double Jump (Velocity.Z: %.1f, Montage: %s)"),
		*Character->GetName(), EffectiveZVelocity, *GetNameSafe(MontageToPlay));

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
