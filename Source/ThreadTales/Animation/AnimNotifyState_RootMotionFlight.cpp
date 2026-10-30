// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_RootMotionFlight.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "ThreadTales.h"

UAnimNotifyState_RootMotionFlight::UAnimNotifyState_RootMotionFlight()
{
}

void UAnimNotifyState_RootMotionFlight::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !MeshComp->GetWorld() || !MeshComp->GetWorld()->IsGameWorld()) return;

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor) return;

	if (ACharacter* Character = Cast<ACharacter>(OwnerActor))
	{
		if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			MoveComp->SetMovementMode(MOVE_Flying);
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Flight Notify] '%s': BEGIN Root Motion Flight on %s (Mode -> MOVE_Flying)"),
				*Character->GetName(), *GetNameSafe(Animation));
		}
	}
}

void UAnimNotifyState_RootMotionFlight::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetWorld() || !MeshComp->GetWorld()->IsGameWorld()) return;

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor) return;

	if (ACharacter* Character = Cast<ACharacter>(OwnerActor))
	{
		if (bMaintainFlightOnEnd)
		{
			// Sustain MOVE_Flying for flight transitions (e.g. Ladybug Takeoff into sustained AirHover)
			return;
		}

		if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			bool bIsGrounded = MoveComp->CurrentFloor.IsWalkableFloor();
			if (!bIsGrounded && bSafeGroundCheckOnEnd && Character->GetWorld())
			{
				const float CapsuleHalf = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 60.0f;
				const FVector Start = Character->GetActorLocation();
				const FVector End = Start - FVector(0.0f, 0.0f, CapsuleHalf + 40.0f);
				FHitResult FloorHit;
				FCollisionQueryParams Params(TEXT("RootMotionFlightGroundTrace"), false, Character);
				Params.AddIgnoredActor(Character);
				if (Character->GetWorld()->LineTraceSingleByChannel(FloorHit, Start, End, ECC_Visibility, Params))
				{
					bIsGrounded = true;
				}
			}

			if (!bSafeGroundCheckOnEnd || bIsGrounded)
			{
				MoveComp->SetMovementMode(MOVE_Walking);
			}
			else
			{
				MoveComp->SetMovementMode(MOVE_Falling);
			}

			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Flight Notify] '%s': END Root Motion Flight on %s (Mode -> %d)"),
				*Character->GetName(), *GetNameSafe(Animation), (int32)MoveComp->MovementMode);
		}
	}
}

FString UAnimNotifyState_RootMotionFlight::GetNotifyName_Implementation() const
{
	return TEXT("Root Motion Flight");
}
