// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_EarlyLocomotionCancel.h"
#include "ThreadTales.h"
#include "Abilities/ThreadTalesComboAbilityInterface.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Characters/ThreadTalesTraversalInputHelper.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

UAnimNotifyState_EarlyLocomotionCancel::UAnimNotifyState_EarlyLocomotionCancel()
{
	BlendOutTime = 0.35f;
}

static IThreadTalesComboAbilityInterface* FindActiveComboAbility(USkeletalMeshComponent* MeshComp)
{
	if (!MeshComp) return nullptr;

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor) return nullptr;

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(OwnerActor);
	if (!ASI) return nullptr;

	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
	if (!ASC) return nullptr;

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.IsActive())
		{
			if (IThreadTalesComboAbilityInterface* ComboAbility = Cast<IThreadTalesComboAbilityInterface>(Spec.GetPrimaryInstance()))
			{
				return ComboAbility;
			}

			for (UGameplayAbility* Inst : Spec.GetAbilityInstances())
			{
				if (IThreadTalesComboAbilityInterface* ComboAbility = Cast<IThreadTalesComboAbilityInterface>(Inst))
				{
					return ComboAbility;
				}
			}
		}
	}

	return nullptr;
}

void UAnimNotifyState_EarlyLocomotionCancel::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !MeshComp->GetWorld() || !MeshComp->GetWorld()->IsGameWorld())
	{
		return;
	}

	CheckAndCancelToLocomotion(MeshComp, Animation);
}

void UAnimNotifyState_EarlyLocomotionCancel::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp || !MeshComp->GetWorld() || !MeshComp->GetWorld()->IsGameWorld())
	{
		return;
	}

	CheckAndCancelToLocomotion(MeshComp, Animation);
}

void UAnimNotifyState_EarlyLocomotionCancel::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UAnimNotifyState_EarlyLocomotionCancel::GetNotifyName_Implementation() const
{
	return TEXT("Early Locomotion Cancel");
}

bool UAnimNotifyState_EarlyLocomotionCancel::CheckAndCancelToLocomotion(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation) const
{
	if (!MeshComp)
	{
		return false;
	}

	ACharacter* Character = Cast<ACharacter>(MeshComp->GetOwner());
	if (!Character)
	{
		return false;
	}

	AController* Controller = Character->GetController();
	FVector MoveInput = Character->GetLastMovementInputVector();
	if (MoveInput.IsNearlyZero() && Character->GetCharacterMovement())
	{
		MoveInput = Character->GetCharacterMovement()->GetCurrentAcceleration();
	}

	if (MoveInput.IsNearlyZero())
	{
		if (const APlayerController* PC = Cast<APlayerController>(Controller))
		{
			const FVector2D Input2D = FThreadTalesTraversalInputHelper::GetMovementInputVector(PC, Character);
			if (Input2D.SizeSquared() > 0.04f)
			{
				const FRotator CamRot = PC->GetControlRotation();
				const FRotator YawRot(0.0f, CamRot.Yaw, 0.0f);
				const FVector CamFwd = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
				const FVector CamRight = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
				MoveInput = (CamFwd * Input2D.Y + CamRight * Input2D.X).GetSafeNormal();
			}
		}
	}

	if (MoveInput.IsNearlyZero())
	{
		return false;
	}

	// Stop montage smoothly with full designer-specified blend out
	if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
	{
		UAnimMontage* Montage = Cast<UAnimMontage>(Animation);
		AnimInst->Montage_Stop(BlendOutTime, Montage);
	}

	// Movement intent confirmed: notify ability to cleanly transition to locomotion
	if (IThreadTalesComboAbilityInterface* ComboAbility = FindActiveComboAbility(MeshComp))
	{
		ComboAbility->TryCancelToLocomotion();
	}

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[EarlyLocomotionCancel] Breaking out of '%s' into Locomotion!"),
		*GetNameSafe(Animation));

	return true;
}
