// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesCombatLocomotionHandler.h"
#include "ThreadTales.h"
#include "Characters/ThreadTalesBaseCharacter.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Combat/ThreadTalesTargetLockComponent.h"
#include "Combat/ThreadTalesOverlayDebug.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"

UThreadTalesCombatLocomotionHandler::UThreadTalesCombatLocomotionHandler()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

AThreadTalesBaseCharacter* UThreadTalesCombatLocomotionHandler::GetOwningCharacter() const
{
	return Cast<AThreadTalesBaseCharacter>(GetOwner());
}

uint8 UThreadTalesCombatLocomotionHandler::GetOverlayPoseValueByName(FName PoseName)
{
	if (PoseName.IsNone() || PoseName == FName("Default") || PoseName == FName("None"))
	{
		return 0; // Default
	}

	if (PoseName == FName("Bow") || PoseName == FName("bow"))
	{
		return 4; // Bow
	}

	if (PoseName == FName("Katana") || PoseName == FName("katana"))
	{
		return 11; // Katana
	}

	static UEnum* CachedOverlayPoseEnum = nullptr;
	if (!CachedOverlayPoseEnum)
	{
		CachedOverlayPoseEnum = LoadObject<UEnum>(nullptr, TEXT("/GASPALS/OverlaySystem/Blueprints/Enum_OverlayPose.Enum_OverlayPose"));
	}

	if (CachedOverlayPoseEnum)
	{
		const FString TargetNameStr = PoseName.ToString();
		for (int32 i = 0; i < CachedOverlayPoseEnum->NumEnums(); ++i)
		{
			if (CachedOverlayPoseEnum->GetDisplayNameTextByIndex(i).ToString().Equals(TargetNameStr, ESearchCase::IgnoreCase) ||
				CachedOverlayPoseEnum->GetNameStringByIndex(i).Equals(TargetNameStr, ESearchCase::IgnoreCase))
			{
				return static_cast<uint8>(CachedOverlayPoseEnum->GetValueByIndex(i));
			}
		}
	}

	return 0;
}

uint8 UThreadTalesCombatLocomotionHandler::GetOverlayPose() const
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return 0;
	}

	FProperty* OverlayProp = Character->GetClass()->FindPropertyByName(TEXT("OverlayPose"));
	if (OverlayProp)
	{
		const uint8* PropAddr = OverlayProp->ContainerPtrToValuePtr<uint8>(Character);
		if (PropAddr)
		{
			return *PropAddr;
		}
	}
	return 0;
}

void UThreadTalesCombatLocomotionHandler::SetOverlayPose(uint8 NewOverlayPose)
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return;
	}

	if (!Character->HasAuthority())
	{
		ServerSetOverlayPose(NewOverlayPose);
	}

	// 1. Locate property "OverlayPose" on character (defined in CBP_SandboxCharacter)
	FProperty* OverlayProp = Character->GetClass()->FindPropertyByName(TEXT("OverlayPose"));
	if (OverlayProp)
	{
		uint8* PropAddr = OverlayProp->ContainerPtrToValuePtr<uint8>(Character);
		if (PropAddr)
		{
			const uint8 OldVal = *PropAddr;
			*PropAddr = NewOverlayPose;
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Overlay] %s OverlayPose updated: %d -> %d"),
				*GetNameSafe(Character), OldVal, NewOverlayPose);
		}
	}

	// 2. Trigger UpdateOverlayPose function on CBP_SandboxCharacter
	UFunction* UpdateFunc = Character->GetClass()->FindFunctionByName(TEXT("UpdateOverlayPose"));
	if (UpdateFunc)
	{
		Character->ProcessEvent(UpdateFunc, nullptr);
	}
	else
	{
		UFunction* OnRepFunc = Character->GetClass()->FindFunctionByName(TEXT("OnRep_OverlayPose"));
		if (OnRepFunc)
		{
			Character->ProcessEvent(OnRepFunc, nullptr);
		}
	}

	// 3. Clear legacy spawned GASPALS bow skeletal mesh
	for (UActorComponent* Comp : Character->GetComponents())
	{
		if (Comp && Comp->GetName().Contains(TEXT("OverlaySkeletalMesh")))
		{
			if (USkeletalMeshComponent* SkMesh = Cast<USkeletalMeshComponent>(Comp))
			{
				if (SkMesh->GetSkeletalMeshAsset() && SkMesh->GetSkeletalMeshAsset()->GetName().Contains(TEXT("Bow")))
				{
					SkMesh->SetSkeletalMeshAsset(nullptr);
					SkMesh->SetVisibility(false);
					SkMesh->SetHiddenInGame(true);
				}
			}
		}
	}

	// 4. Robust socket docking: ensure HeldObjectRoot is never stranded at world origin
	if (AThreadTalesCharacter* PlayerChar = Cast<AThreadTalesCharacter>(Character))
	{
		FThreadTalesOverlayDebug::DockHeldObjectRoot(PlayerChar, NewOverlayPose);
	}
}

void UThreadTalesCombatLocomotionHandler::ServerSetOverlayPose_Implementation(uint8 NewOverlayPose)
{
	SetOverlayPose(NewOverlayPose);
}

void UThreadTalesCombatLocomotionHandler::SetOverlayPoseByName(FName PoseName)
{
	const uint8 PoseVal = GetOverlayPoseValueByName(PoseName);
	SetOverlayPose(PoseVal);
}

void UThreadTalesCombatLocomotionHandler::SetAimInputDown(bool bAiming)
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return;
	}

	if (bAiming && !Character->CanAimOrBlock())
	{
		bAiming = false;
	}

	if (!Character->HasAuthority())
	{
		ServerSetAimInputDown(bAiming);
	}

	// 1. Locate bIsAimInputDown on CBP_SandboxCharacter via property reflection
	FBoolProperty* AimProp = CastField<FBoolProperty>(Character->GetClass()->FindPropertyByName(TEXT("bIsAimInputDown")));
	if (AimProp)
	{
		const bool bOldVal = AimProp->GetPropertyValue_InContainer(Character);
		if (bOldVal != bAiming)
		{
			AimProp->SetPropertyValue_InContainer(Character, bAiming);
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Block] %s bIsAimInputDown: %s -> %s"),
				*GetNameSafe(Character), bOldVal ? TEXT("true") : TEXT("false"), bAiming ? TEXT("true") : TEXT("false"));
		}
	}

	// 2. Locate CharacterInputState on CBP_SandboxCharacter and update WantsToAim
	bool bWantsStrafe = false;
	if (FProperty* InputStateProp = Character->GetClass()->FindPropertyByName(TEXT("CharacterInputState")))
	{
		if (const FStructProperty* StructProp = CastField<FStructProperty>(InputStateProp))
		{
			void* StructAddr = StructProp->ContainerPtrToValuePtr<void>(Character);
			if (StructAddr)
			{
				for (TFieldIterator<FBoolProperty> It(StructProp->Struct); It; ++It)
				{
					if (It->GetName().Contains(TEXT("WantsToAim")))
					{
						const bool bOldVal = It->GetPropertyValue_InContainer(StructAddr);
						if (bOldVal != bAiming)
						{
							It->SetPropertyValue_InContainer(StructAddr, bAiming);
						}
					}
					else if (It->GetName().Contains(TEXT("WantsToStrafe")))
					{
						bWantsStrafe = It->GetPropertyValue_InContainer(StructAddr);
					}
				}

				if (!Character->HasAuthority())
				{
					if (UFunction* UpdateFunc = Character->FindFunction(TEXT("UpdateInputState_Server")))
					{
						Character->ProcessEvent(UpdateFunc, StructAddr);
					}
				}
			}
		}
	}

	// 3. Immediately update CharacterMovementComponent rotation mode
	if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
	{
		const AThreadTalesCharacter* PlayerChar = Cast<AThreadTalesCharacter>(Character);
		const bool bTargetLocked = PlayerChar && PlayerChar->GetTargetLockComponent() && PlayerChar->GetTargetLockComponent()->IsTargetLocked();
		const bool bIsClimbing = PlayerChar && PlayerChar->IsClimbing();
		const bool bShouldStrafe = !bIsClimbing && (bAiming || bWantsStrafe || bTargetLocked);
		MoveComp->bUseControllerDesiredRotation = bShouldStrafe;
		MoveComp->bOrientRotationToMovement = !bShouldStrafe;
		MoveComp->RotationRate = bIsClimbing ? FRotator::ZeroRotator : FRotator(0.0f, 720.0f, 0.0f);
	}
}

void UThreadTalesCombatLocomotionHandler::ServerSetAimInputDown_Implementation(bool bAiming)
{
	SetAimInputDown(bAiming);
}

void UThreadTalesCombatLocomotionHandler::SetStrafeInputDown(bool bStrafing)
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return;
	}

	const AThreadTalesCharacter* PlayerChar = Cast<AThreadTalesCharacter>(Character);
	const bool bTargetLocked = PlayerChar && PlayerChar->GetTargetLockComponent() && PlayerChar->GetTargetLockComponent()->IsTargetLocked();
	const bool bEffectiveStrafe = bStrafing || bTargetLocked;

	if (!Character->HasAuthority())
	{
		ServerSetStrafeInputDown(bEffectiveStrafe);
	}

	// 1. Locate bIsStrafeInputDown on CBP_SandboxCharacter
	if (FBoolProperty* StrafeProp = CastField<FBoolProperty>(Character->GetClass()->FindPropertyByName(TEXT("bIsStrafeInputDown"))))
	{
		StrafeProp->SetPropertyValue_InContainer(Character, bEffectiveStrafe);
	}

	// 2. Locate CharacterInputState on CBP_SandboxCharacter and update WantsToStrafe
	bool bWantsAim = false;
	if (FProperty* InputStateProp = Character->GetClass()->FindPropertyByName(TEXT("CharacterInputState")))
	{
		if (const FStructProperty* StructProp = CastField<FStructProperty>(InputStateProp))
		{
			void* StructAddr = StructProp->ContainerPtrToValuePtr<void>(Character);
			if (StructAddr)
			{
				for (TFieldIterator<FBoolProperty> It(StructProp->Struct); It; ++It)
				{
					if (It->GetName().Contains(TEXT("WantsToStrafe")))
					{
						It->SetPropertyValue_InContainer(StructAddr, bEffectiveStrafe);
					}
					else if (It->GetName().Contains(TEXT("WantsToAim")))
					{
						bWantsAim = It->GetPropertyValue_InContainer(StructAddr);
					}
				}

				if (!Character->HasAuthority())
				{
					if (UFunction* UpdateFunc = Character->FindFunction(TEXT("UpdateInputState_Server")))
					{
						Character->ProcessEvent(UpdateFunc, StructAddr);
					}
				}
			}
		}
	}

	// 3. Update CharacterMovementComponent rotation mode
	if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
	{
		const bool bIsClimbing = PlayerChar && PlayerChar->IsClimbing();
		const bool bShouldStrafe = !bIsClimbing && (bEffectiveStrafe || bWantsAim);
		MoveComp->bUseControllerDesiredRotation = bShouldStrafe;
		MoveComp->bOrientRotationToMovement = !bShouldStrafe;
		MoveComp->RotationRate = bIsClimbing ? FRotator::ZeroRotator : FRotator(0.0f, 720.0f, 0.0f);
	}
}

void UThreadTalesCombatLocomotionHandler::ServerSetStrafeInputDown_Implementation(bool bStrafing)
{
	SetStrafeInputDown(bStrafing);
}
