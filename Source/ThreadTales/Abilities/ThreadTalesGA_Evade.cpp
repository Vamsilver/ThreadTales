#include "Abilities/ThreadTalesGA_Evade.h"
#include "ThreadTales.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetMathLibrary.h"

UThreadTalesGA_Evade::UThreadTalesGA_Evade()
{
	AbilityName = TEXT("Evade / Dodge");
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	SetAssetTags(FGameplayTagContainer(ThreadTalesGameplayTags::TAG_Ability_Evade));
	ActivationOwnedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Evading);
	ActivationOwnedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Invulnerable);

	// Evade cancels melee attacks and aiming
	CancelAbilitiesWithTag.AddTag(ThreadTalesGameplayTags::TAG_Ability_Melee);
	CancelAbilitiesWithTag.AddTag(ThreadTalesGameplayTags::TAG_Ability_Ranged);

	// Evade blocks attack actions while rolling/evading
	BlockAbilitiesWithTag = CancelAbilitiesWithTag;
}

EEvadeDirection UThreadTalesGA_Evade::CalculateEvadeDirection(float& OutAngle) const
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		OutAngle = 180.0f;
		return EEvadeDirection::Backward;
	}

	AController* Controller = Character->GetController();
	FRotator CameraRotation = Controller ? Controller->GetControlRotation() : Character->GetActorRotation();

	FVector InputVector = Character->GetLastMovementInputVector();
	if (InputVector.IsNearlyZero() && Character->GetCharacterMovement())
	{
		InputVector = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal();
	}

	// If no movement input, default to backward dodge / step back
	if (InputVector.IsNearlyZero())
	{
		OutAngle = 180.0f;
		return EEvadeDirection::Backward;
	}

	// Calculate angle between Movement Input and Camera Heading (-180..+180)
	FRotator InputRot = InputVector.Rotation();
	FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(InputRot, CameraRotation);
	float Angle = DeltaRot.Yaw;
	OutAngle = Angle;

	if (Angle >= -22.5f && Angle < 22.5f)
	{
		return EEvadeDirection::Forward;
	}
	else if (Angle >= 22.5f && Angle < 67.5f)
	{
		return EEvadeDirection::ForwardRight;
	}
	else if (Angle >= 67.5f && Angle < 112.5f)
	{
		return EEvadeDirection::Right;
	}
	else if (Angle >= 112.5f && Angle < 157.5f)
	{
		return EEvadeDirection::BackwardRight;
	}
	else if (Angle >= -67.5f && Angle < -22.5f)
	{
		return EEvadeDirection::ForwardLeft;
	}
	else if (Angle >= -112.5f && Angle < -67.5f)
	{
		return EEvadeDirection::Left;
	}
	else if (Angle >= -157.5f && Angle < -112.5f)
	{
		return EEvadeDirection::BackwardLeft;
	}
	else
	{
		return EEvadeDirection::Backward;
	}
}

UAnimMontage* UThreadTalesGA_Evade::GetMontageForDirection(EEvadeDirection Direction) const
{
	switch (Direction)
	{
	case EEvadeDirection::Forward:
		return EvadeForwardMontage ? EvadeForwardMontage : DefaultDodgeMontage;
	case EEvadeDirection::ForwardRight:
		return EvadeForwardRightMontage ? EvadeForwardRightMontage : (EvadeRightMontage ? EvadeRightMontage : DefaultDodgeMontage);
	case EEvadeDirection::Right:
		return EvadeRightMontage ? EvadeRightMontage : DefaultDodgeMontage;
	case EEvadeDirection::BackwardRight:
		return EvadeBackwardRightMontage ? EvadeBackwardRightMontage : (EvadeBackwardMontage ? EvadeBackwardMontage : DefaultDodgeMontage);
	case EEvadeDirection::Backward:
		return EvadeBackwardMontage ? EvadeBackwardMontage : DefaultDodgeMontage;
	case EEvadeDirection::BackwardLeft:
		return EvadeBackwardLeftMontage ? EvadeBackwardLeftMontage : (EvadeBackwardMontage ? EvadeBackwardMontage : DefaultDodgeMontage);
	case EEvadeDirection::Left:
		return EvadeLeftMontage ? EvadeLeftMontage : DefaultDodgeMontage;
	case EEvadeDirection::ForwardLeft:
		return EvadeForwardLeftMontage ? EvadeForwardLeftMontage : (EvadeLeftMontage ? EvadeLeftMontage : DefaultDodgeMontage);
	default:
		return DefaultDodgeMontage;
	}
}

bool UThreadTalesGA_Evade::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (Character && Character->GetCharacterMovement())
	{
		// Ground evade should NOT activate if falling
		if (Character->GetCharacterMovement()->IsFalling())
		{
			return false;
		}
	}
	return true;
}

void UThreadTalesGA_Evade::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

	float Angle = 0.0f;
	EEvadeDirection Direction = CalculateEvadeDirection(Angle);
	UAnimMontage* MontageToPlay = GetMontageForDirection(Direction);

	AController* Controller = Character->GetController();
	if (Controller)
	{
		// Orient character to face camera forward so 8-way strafe root motion moves in the exact relative camera direction
		Character->SetActorRotation(FRotator(0.0f, Controller->GetControlRotation().Yaw, 0.0f));
	}

	// Clear leftover walking/running velocity so Root Motion takes 100% clean authority
	if (UCharacterMovementComponent* CMC = Character->GetCharacterMovement())
	{
		CMC->StopMovementImmediately();
	}

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Evade GA] Evading in direction %d (Angle: %.1f deg) with Montage: %s"),
		static_cast<int32>(Direction), Angle, *GetNameSafe(MontageToPlay));

	if (MontageToPlay)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay, 1.0f, NAME_None, true, 1.0f);
		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UThreadTalesGA_Evade::OnMontageCompleted);
			MontageTask->OnInterrupted.AddDynamic(this, &UThreadTalesGA_Evade::OnMontageInterrupted);
			MontageTask->OnCancelled.AddDynamic(this, &UThreadTalesGA_Evade::OnMontageCancelled);
			MontageTask->ReadyForActivation();
		}
	}
	else
	{
		UE_LOG(LogThreadTalesCombat, Warning, TEXT("[Evade GA] No montage assigned for Evade direction %d!"), static_cast<int32>(Direction));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}

	// Automatically check input based on montage length and configured percentages
	float CancelPercentage = DefaultEarlyCancelPercentage;
	if (MontageCancelPercentages.Contains(MontageToPlay))
	{
		CancelPercentage = MontageCancelPercentages[MontageToPlay];
	}
	
	float CheckTime = MontageToPlay->GetPlayLength() * FMath::Clamp(CancelPercentage, 0.0f, 1.0f);
	
	class UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, CheckTime);
	if (DelayTask)
	{
		DelayTask->OnFinish.AddDynamic(this, &UThreadTalesGA_Evade::OnCheckInputTimerFinished);
		DelayTask->ReadyForActivation();
	}
}

void UThreadTalesGA_Evade::OnCheckInputTimerFinished()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (Character && Character->GetCharacterMovement())
	{
		// If player is NOT pressing movement keys (acceleration is nearly zero)
		if (Character->GetCharacterMovement()->GetCurrentAcceleration().IsNearlyZero())
		{
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Evade GA] Movement input released. Cancelling evade montage early to return to Idle."));
			MontageStop(0.2f); // Smooth blend out
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
		else
		{
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Evade GA] Movement input detected. Allowing dodge to seamlessly transition into run."));
		}
	}
}

void UThreadTalesGA_Evade::OnMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UThreadTalesGA_Evade::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UThreadTalesGA_Evade::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
