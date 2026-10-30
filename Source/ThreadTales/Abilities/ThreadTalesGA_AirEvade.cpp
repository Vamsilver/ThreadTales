// Copyright Epic Games, Inc. All Rights Reserved.

#include "Abilities/ThreadTalesGA_AirEvade.h"
#include "ThreadTales.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesStaminaComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "Kismet/KismetMathLibrary.h"

UThreadTalesGA_AirEvade::UThreadTalesGA_AirEvade()
{
	AbilityName = TEXT("Air Evade / Dash");
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	SetAssetTags(FGameplayTagContainer(ThreadTalesGameplayTags::TAG_Ability_Movement_AirEvade));
}

void UThreadTalesGA_AirEvade::PostInitProperties()
{
	Super::PostInitProperties();

	// Isolate Air Evade from the general Ability.Evade tag so it does not trigger during ground evades
	AbilityTags.RemoveTag(ThreadTalesGameplayTags::TAG_Ability_Evade);
	AbilityTags.AddTag(ThreadTalesGameplayTags::TAG_Ability_Movement_AirEvade);
}

bool UThreadTalesGA_AirEvade::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		if (UThreadTalesStaminaComponent* StamComp = TTChar->GetStaminaComponent())
		{
			if (!StamComp->CanSpendStamina())
			{
				if (TTChar->GetCharacterMovement() && TTChar->GetCharacterMovement()->IsFalling())
				{
					StamComp->NotifyInsufficientStamina();
				}
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

void UThreadTalesGA_AirEvade::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		if (UThreadTalesStaminaComponent* StamComp = TTChar->GetStaminaComponent())
		{
			StamComp->TryConsumeStamina(StamComp->GetAirEvadeStaminaCost(), true);
		}
	}
}

bool UThreadTalesGA_AirEvade::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (Character && Character->GetCharacterMovement())
	{
		// Only allow activation if we are falling
		if (!Character->GetCharacterMovement()->IsFalling())
		{
			return false;
		}
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC && ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_HasAirDashed))
	{
		// Already air dashed this jump
		return false;
	}

	return true;
}

void UThreadTalesGA_AirEvade::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Add the tag so we can't spam it in the air
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC)
	{
		ASC->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_HasAirDashed);
	}

	// Calculate direction based on input
	float Angle = 0.0f;
	EEvadeDirection Direction = CalculateEvadeDirection(Angle);
	UAnimMontage* MontageToPlay = GetMontageForDirection(Direction);

	// Get World Space dash direction
	FVector DashDir = Character->GetLastMovementInputVector();
	if (DashDir.IsNearlyZero())
	{
		// Default backward dodge if no input
		DashDir = -Character->GetActorForwardVector();
	}
	else
	{
		DashDir.Normalize();
	}

	// Play Montage
	if (MontageToPlay)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay, 1.0f, NAME_None, true, 1.0f);
		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UThreadTalesGA_AirEvade::OnMontageCompleted);
			MontageTask->OnInterrupted.AddDynamic(this, &UThreadTalesGA_AirEvade::OnMontageInterrupted);
			MontageTask->OnCancelled.AddDynamic(this, &UThreadTalesGA_AirEvade::OnMontageCancelled);
			MontageTask->ReadyForActivation();
		}
	}

	// Apply Air Dash Force (Overrides Gravity temporarily)
	DashTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this,
		NAME_None,
		DashDir,
		DashForce,
		DashDuration,
		true, // bIsAdditive
		nullptr, // Curve
		ERootMotionFinishVelocityMode::SetVelocity,
		DashDir * (DashForce * 0.5f), // Final velocity after dash
		0.0f,
		false
	);

	if (DashTask)
	{
		DashTask->OnFinish.AddDynamic(this, &UThreadTalesGA_AirEvade::OnDashFinished);
		DashTask->ReadyForActivation();
	}
	else
	{
		// Fallback if task fails to create
		Character->LaunchCharacter(DashDir * DashForce, true, true);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UThreadTalesGA_AirEvade::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		TTChar->ApplyStaminaRegenDelay(TTChar->GetStamina() <= 0.0f);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UThreadTalesGA_AirEvade::OnDashFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UThreadTalesGA_AirEvade::OnMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UThreadTalesGA_AirEvade::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UThreadTalesGA_AirEvade::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

EEvadeDirection UThreadTalesGA_AirEvade::CalculateEvadeDirection(float& OutAngle) const
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		OutAngle = 0.0f;
		return EEvadeDirection::Forward;
	}

	FVector InputVector = Character->GetLastMovementInputVector();
	if (InputVector.IsNearlyZero())
	{
		OutAngle = 180.0f;
		return EEvadeDirection::Backward;
	}

	FVector ForwardVector = Character->GetActorForwardVector();
	ForwardVector.Z = 0.0f;
	ForwardVector.Normalize();

	FVector InputDir = InputVector;
	InputDir.Z = 0.0f;
	InputDir.Normalize();

	float Dot = FVector::DotProduct(ForwardVector, InputDir);
	float Cross = FVector::CrossProduct(ForwardVector, InputDir).Z;
	OutAngle = FMath::RadiansToDegrees(FMath::Acos(Dot));

	if (Cross < 0.0f)
	{
		OutAngle = -OutAngle;
	}

	if (OutAngle > -22.5f && OutAngle <= 22.5f) return EEvadeDirection::Forward;
	if (OutAngle > 22.5f && OutAngle <= 67.5f) return EEvadeDirection::ForwardRight;
	if (OutAngle > 67.5f && OutAngle <= 112.5f) return EEvadeDirection::Right;
	if (OutAngle > 112.5f && OutAngle <= 157.5f) return EEvadeDirection::BackwardRight;
	if (OutAngle > 157.5f || OutAngle <= -157.5f) return EEvadeDirection::Backward;
	if (OutAngle > -157.5f && OutAngle <= -112.5f) return EEvadeDirection::BackwardLeft;
	if (OutAngle > -112.5f && OutAngle <= -67.5f) return EEvadeDirection::Left;
	if (OutAngle > -67.5f && OutAngle <= -22.5f) return EEvadeDirection::ForwardLeft;

	return EEvadeDirection::Forward;
}

UAnimMontage* UThreadTalesGA_AirEvade::GetMontageForDirection(EEvadeDirection Direction) const
{
	switch (Direction)
	{
	case EEvadeDirection::Forward:			return EvadeForwardMontage ? EvadeForwardMontage : DefaultDodgeMontage;
	case EEvadeDirection::ForwardRight:		return EvadeForwardRightMontage ? EvadeForwardRightMontage : DefaultDodgeMontage;
	case EEvadeDirection::Right:			return EvadeRightMontage ? EvadeRightMontage : DefaultDodgeMontage;
	case EEvadeDirection::BackwardRight:	return EvadeBackwardRightMontage ? EvadeBackwardRightMontage : DefaultDodgeMontage;
	case EEvadeDirection::Backward:			return EvadeBackwardMontage ? EvadeBackwardMontage : DefaultDodgeMontage;
	case EEvadeDirection::BackwardLeft:		return EvadeBackwardLeftMontage ? EvadeBackwardLeftMontage : DefaultDodgeMontage;
	case EEvadeDirection::Left:				return EvadeLeftMontage ? EvadeLeftMontage : DefaultDodgeMontage;
	case EEvadeDirection::ForwardLeft:		return EvadeForwardLeftMontage ? EvadeForwardLeftMontage : DefaultDodgeMontage;
	}
	return DefaultDodgeMontage;
}
