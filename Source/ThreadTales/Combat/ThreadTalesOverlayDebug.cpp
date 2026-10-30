// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ThreadTalesOverlayDebug.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
#include "Combat/ThreadTalesTargetLockComponent.h"
#include "Combat/ThreadTalesWeapon.h"
#include "Combat/ThreadTalesWeapon_Bow.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "DrawDebugHelpers.h"
#include "AbilitySystemComponent.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Characters/ThreadTalesTraversalInputHelper.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"
#include "ThreadTales.h"

static TAutoConsoleVariable<int32> CVarOverlayDebug(
	TEXT("tt.Combat.OverlayDebug"),
	0,
	TEXT("Log and visualize weapon overlay diagnostics (Hand IK distance, curves, gait, grip delta). 0 = off, 1 = on."),
	ECVF_Cheat);

static void DrawOverlayDebugVisuals(
	UWorld* World,
	USkeletalMeshComponent* MeshComp,
	const FVector& HandRLoc,
	const FVector& HandLLoc,
	const FRotator& RotR,
	const FRotator& RotL,
	const FVector& Spine01Loc,
	float DistR_Spine01,
	float DistL_Spine01,
	float BodyClearance,
	bool bWristFlipped,
	bool bBodyClipping,
	float HandIK_L)
{
	if (!World || !MeshComp)
	{
		return;
	}

	FColor LineColor = (bWristFlipped || bBodyClipping) ? FColor::Red : ((HandIK_L < 0.5f) ? FColor::Orange : FColor::Green);
	DrawDebugLine(World, HandRLoc, HandLLoc, LineColor, false, -1.0f, 0, 1.5f);
	DrawDebugPoint(World, HandRLoc, 6.0f, FColor::Cyan, false, -1.0f);
	DrawDebugPoint(World, HandLLoc, 6.0f, bWristFlipped ? FColor::Red : FColor::Magenta, false, -1.0f);
	DrawDebugCoordinateSystem(World, HandRLoc, RotR, 8.0f, false, -1.0f, 0, 1.0f);
	DrawDebugCoordinateSystem(World, HandLLoc, RotL, 8.0f, false, -1.0f, 0, 1.0f);

	const FVector ClosestHand = (DistR_Spine01 < DistL_Spine01) ? HandRLoc : HandLLoc;
	const FColor ClearanceColor = bBodyClipping ? FColor::Red : (BodyClearance < 22.0f ? FColor::Yellow : FColor::Green);
	DrawDebugLine(World, ClosestHand, Spine01Loc, ClearanceColor, false, -1.0f, 0, 1.2f);
	DrawDebugPoint(World, Spine01Loc, 5.0f, FColor::Yellow, false, -1.0f);
}

void FThreadTalesOverlayDebug::UpdateOverlayDebug(
	AThreadTalesCharacter* Character, float DeltaSeconds, float& InOutTelemetryTimer,
	float& InOutLastHandsDistance, FRotator& InOutLastHandRRot, FRotator& InOutLastHandLRot)
{
	if (!Character || !((CVarOverlayDebug.GetValueOnGameThread() > 0) || Character->bEnableOverlayDebugTelemetry))
	{
		return;
	}

	InOutTelemetryTimer += DeltaSeconds;
	const bool bShouldLog = (InOutTelemetryTimer >= 0.25f);

	USkeletalMeshComponent* MeshComp = Character->GetMesh();
	UAnimInstance* AnimInst = MeshComp ? MeshComp->GetAnimInstance() : nullptr;

	const FVector HandRLoc = (MeshComp && MeshComp->DoesSocketExist(TEXT("hand_r"))) ? MeshComp->GetSocketLocation(TEXT("hand_r")) : FVector::ZeroVector;
	const FVector HandLLoc = (MeshComp && MeshComp->DoesSocketExist(TEXT("hand_l"))) ? MeshComp->GetSocketLocation(TEXT("hand_l")) : FVector::ZeroVector;
	const float HandsDist = FVector::Dist(HandRLoc, HandLLoc);
	const float DistDelta = (InOutLastHandsDistance > 0.0f) ? (HandsDist - InOutLastHandsDistance) : 0.0f;
	InOutLastHandsDistance = HandsDist;

	const FVector PelvisLoc = (MeshComp && MeshComp->DoesSocketExist(TEXT("pelvis"))) ? MeshComp->GetSocketLocation(TEXT("pelvis")) : FVector::ZeroVector;
	const FVector Spine01Loc = (MeshComp && MeshComp->DoesSocketExist(TEXT("spine_01"))) ? MeshComp->GetSocketLocation(TEXT("spine_01")) : FVector::ZeroVector;
	const FVector Spine02Loc = (MeshComp && MeshComp->DoesSocketExist(TEXT("spine_02"))) ? MeshComp->GetSocketLocation(TEXT("spine_02")) : FVector::ZeroVector;

	const float DistR_Spine01 = FVector::Dist(HandRLoc, Spine01Loc);
	const float DistL_Spine01 = FVector::Dist(HandLLoc, Spine01Loc);
	const float DistR_Pelvis = FVector::Dist(HandRLoc, PelvisLoc);
	const float DistL_Pelvis = FVector::Dist(HandLLoc, PelvisLoc);
	const float MinDistSpine01 = FMath::Min(DistR_Spine01, DistL_Spine01);
	const float MinDistPelvis = FMath::Min(DistR_Pelvis, DistL_Pelvis);
	const float MinDistSpine02 = FMath::Min(FVector::Dist(HandRLoc, Spine02Loc), FVector::Dist(HandLLoc, Spine02Loc));
	const float BodyClearance = FMath::Min(MinDistSpine01, FMath::Min(MinDistPelvis, MinDistSpine02));

	const FTransform HandRComp = (MeshComp && MeshComp->DoesSocketExist(TEXT("hand_r"))) ? MeshComp->GetSocketTransform(TEXT("hand_r"), RTS_Component) : FTransform::Identity;
	const FTransform HandLComp = (MeshComp && MeshComp->DoesSocketExist(TEXT("hand_l"))) ? MeshComp->GetSocketTransform(TEXT("hand_l"), RTS_Component) : FTransform::Identity;
	const FVector Spine01Pos = (MeshComp && MeshComp->DoesSocketExist(TEXT("spine_01"))) ? MeshComp->GetSocketTransform(TEXT("spine_01"), RTS_Component).GetLocation() : FVector::ZeroVector;
	const FVector HandRPos = HandRComp.GetLocation();
	const FVector HandLPos = HandLComp.GetLocation();
	const float FwdOffsetR = HandRPos.X - Spine01Pos.X;
	const float FwdOffsetL = HandLPos.X - Spine01Pos.X;

	const FRotator RotR = HandRComp.Rotator();
	const FRotator RotL = HandLComp.Rotator();
	const float FwdAlign = FVector::DotProduct(HandRComp.GetUnitAxis(EAxis::X), HandLComp.GetUnitAxis(EAxis::X));
	const float UpAlign = FVector::DotProduct(HandRComp.GetUnitAxis(EAxis::Z), HandLComp.GetUnitAxis(EAxis::Z));

	const float Speed = Character->GetVelocity().Size2D();
	const uint8 PoseVal = Character->GetOverlayPose();
	const bool bInCombat = Character->GetEquipmentComponent() && Character->GetEquipmentComponent()->IsInCombatStance();
	AThreadTalesWeapon* CurWeapon = Character->GetEquipmentComponent() ? Character->GetEquipmentComponent()->GetCurrentWeapon() : nullptr;

	const bool bMontagePlaying = (AnimInst && AnimInst->IsAnyMontagePlaying());
	const bool bIsSprint = (Speed > 600.0f);
	const bool bWristFlipped = (!bMontagePlaying && PoseVal == 11 && !bIsSprint) && ((HandsDist < 8.0f) || (HandsDist > 25.0f) || (FwdAlign > 0.3f));
	const float ActiveClearance = bIsSprint ? FMath::Min(DistR_Spine01, DistR_Pelvis) : BodyClearance;
	const bool bBodyClipping = (!bMontagePlaying && PoseVal == 11 && ActiveClearance < 18.0f);

	const float HandIK_L = AnimInst ? AnimInst->GetCurveValue(TEXT("Enable_HandIK_L")) : 0.0f;
	const float ArmR_LS = AnimInst ? AnimInst->GetCurveValue(TEXT("Layering_Arm_R_LS")) : 0.0f;
	const float ArmL_LS = AnimInst ? AnimInst->GetCurveValue(TEXT("Layering_Arm_L_LS")) : 0.0f;
	const float Spine = AnimInst ? AnimInst->GetCurveValue(TEXT("Layering_Spine")) : 0.0f;
	const float SpineAdd = AnimInst ? AnimInst->GetCurveValue(TEXT("Layering_Spine_Add")) : 0.0f;
	const float SpineRot = AnimInst ? AnimInst->GetCurveValue(TEXT("Enable_SpineRotation")) : 0.0f;
	const TCHAR* GaitStr = (Speed > 600.0f) ? TEXT("Sprint") : ((Speed > 300.0f) ? TEXT("Run") : ((Speed > 10.0f) ? TEXT("Walk") : TEXT("Idle")));

	const float AngularSpeedR = (DeltaSeconds > 0.0001f && !InOutLastHandRRot.IsZero()) ? (FMath::RadiansToDegrees(RotR.Quaternion().AngularDistance(InOutLastHandRRot.Quaternion())) / DeltaSeconds) : 0.0f;
	const float AngularSpeedL = (DeltaSeconds > 0.0001f && !InOutLastHandRRot.IsZero()) ? (FMath::RadiansToDegrees(RotL.Quaternion().AngularDistance(InOutLastHandLRot.Quaternion())) / DeltaSeconds) : 0.0f;
	InOutLastHandRRot = RotR;
	InOutLastHandLRot = RotL;

	const float GripVelocity = (DeltaSeconds > 0.0001f && InOutLastHandsDistance > 0.0f) ? (FMath::Abs(DistDelta) / DeltaSeconds) : 0.0f;
	const bool bKinematicJerk = (!bMontagePlaying && (AngularSpeedR > 650.0f || AngularSpeedL > 650.0f || (PoseVal == 11 && GripVelocity > 100.0f)));

	if (bShouldLog)
	{
		InOutTelemetryTimer = 0.0f;
		const float ActorYaw = Character->GetActorRotation().Yaw;
		const float CtrlYaw = Character->GetController() ? Character->GetController()->GetControlRotation().Yaw : ActorYaw;
		const float RootCSYaw = (MeshComp && MeshComp->DoesSocketExist(TEXT("root"))) ? MeshComp->GetSocketTransform(TEXT("root"), RTS_Component).Rotator().Yaw : 0.0f;
		UE_LOG(LogThreadTalesCombat, Display,
			TEXT("[Overlay Telemetry] %s | Pose: %d | Combat: %d | Gait: %s (%.1f cm/s) | Yaw(Act/Ctrl/RootCS): %.1f/%.1f/%.1f | HandsDist: %.2f cm (d: %+.2f, %.0f cm/s) | BodyClearance: %.1f cm (Spine: %.1f, Pelvis: %.1f, FwdOff: R=%+.1f/L=%+.1f) %s | HandPos(R): (X=%.1f, Y=%.1f, Z=%.1f) | HandPos(L): (X=%.1f, Y=%.1f, Z=%.1f) | Rot(R): (P=%.0f, Y=%.0f, R=%.0f, w=%.0f d/s) | Rot(L): (P=%.0f, Y=%.0f, R=%.0f, w=%.0f d/s) | Align(Fwd/Up): %+.2f/%+.2f %s %s | Curves: [IK_L: %.2f | ArmLS(R/L): %.2f/%.2f | Spine: %.2f (Add: %.2f, Rot: %.2f)] | Weapon: %s"),
			*GetNameSafe(Character), PoseVal, bInCombat ? 1 : 0, GaitStr, Speed, ActorYaw, CtrlYaw, RootCSYaw, HandsDist, DistDelta, GripVelocity, BodyClearance, MinDistSpine01, MinDistPelvis,
			FwdOffsetR, FwdOffsetL, bBodyClipping ? TEXT("*** [BODY CLIPPING (< 18cm)] ***") : TEXT("[OK]"),
			HandRPos.X, HandRPos.Y, HandRPos.Z, HandLPos.X, HandLPos.Y, HandLPos.Z,
			RotR.Pitch, RotR.Yaw, RotR.Roll, AngularSpeedR, RotL.Pitch, RotL.Yaw, RotL.Roll, AngularSpeedL,
			FwdAlign, UpAlign,
			bWristFlipped ? TEXT("*** [WRIST FLIPPED] ***") : TEXT("[OK]"),
			bKinematicJerk ? TEXT("*** [JERK/SNAP DETECTED] ***") : TEXT(""),
			HandIK_L, ArmR_LS, ArmL_LS, Spine, SpineAdd, SpineRot, *GetNameSafe(CurWeapon));
	}

	DrawOverlayDebugVisuals(Character->GetWorld(), MeshComp, HandRLoc, HandLLoc, RotR, RotL, Spine01Loc, DistR_Spine01, DistL_Spine01, BodyClearance, bWristFlipped, bBodyClipping, HandIK_L);
}

bool FThreadTalesOverlayDebug::CanAimOrBlock(const AThreadTalesCharacter* Character)
{
	if (!Character || !Character->IsAlive())
	{
		return false;
	}

	if (!Character->IsInCombatStance())
	{
		return false;
	}

	if (Character->IsSprinting())
	{
		return false;
	}

	const APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (PC && FThreadTalesTraversalInputHelper::IsSprintHeld(PC, Character))
	{
		return false;
	}

	if (Character->IsEvading() || Character->IsStaggered() || Character->IsClimbing())
	{
		return false;
	}

	if (const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
	{
		if (ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered) ||
			ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_PostureBroken) ||
			ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Evading))
		{
			return false;
		}
	}

	return true;
}

void FThreadTalesOverlayDebug::UpdateAimAndBlockState(AThreadTalesCharacter* Character, float DeltaSeconds)
{
	if (!Character)
	{
		return;
	}

	const bool bAllowedToAimOrBlock = CanAimOrBlock(Character);
	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();

	if (ASC && ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Blocking))
	{
		if (!bAllowedToAimOrBlock)
		{
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Block] %s: State invalid for block — cancelling block ability."), *GetNameSafe(Character));
			FGameplayTagContainer BlockContainer;
			BlockContainer.AddTag(ThreadTalesGameplayTags::TAG_Ability_Block);
			ASC->CancelAbilities(&BlockContainer);
		}
	}

	// If not allowed to aim or block, ensure GASPALS aim zoom and strafe are strictly suppressed
	if (!bAllowedToAimOrBlock)
	{
		bool bNeedsAimReset = false;
		if (FBoolProperty* AimProp = CastField<FBoolProperty>(Character->GetClass()->FindPropertyByName(TEXT("bIsAimInputDown"))))
		{
			if (AimProp->GetPropertyValue_InContainer(Character))
			{
				bNeedsAimReset = true;
			}
		}

		if (!bNeedsAimReset)
		{
			if (FProperty* InputStateProp = Character->GetClass()->FindPropertyByName(TEXT("CharacterInputState")))
			{
				if (const FStructProperty* StructProp = CastField<FStructProperty>(InputStateProp))
				{
					if (void* StructAddr = StructProp->ContainerPtrToValuePtr<void>(Character))
					{
						for (TFieldIterator<FBoolProperty> It(StructProp->Struct); It; ++It)
						{
							if (It->GetName().Contains(TEXT("WantsToAim")) && It->GetPropertyValue_InContainer(StructAddr))
							{
								bNeedsAimReset = true;
								break;
							}
						}
					}
				}
			}
		}

		if (bNeedsAimReset)
		{
			Character->SetAimInputDown(false);
		}

		bool bNeedsStrafeReset = false;
		const UThreadTalesTargetLockComponent* TargetLock = Character->GetTargetLockComponent();
		const bool bIsTargetLocked = TargetLock && TargetLock->IsTargetLocked();

		// Never reset strafe while Target Lock is active
		if (!bIsTargetLocked)
		{
			if (FBoolProperty* StrafeProp = CastField<FBoolProperty>(Character->GetClass()->FindPropertyByName(TEXT("bIsStrafeInputDown"))))
			{
				if (StrafeProp->GetPropertyValue_InContainer(Character))
				{
					bNeedsStrafeReset = true;
				}
			}

			if (!bNeedsStrafeReset)
			{
				if (FProperty* InputStateProp = Character->GetClass()->FindPropertyByName(TEXT("CharacterInputState")))
				{
					if (const FStructProperty* StructProp = CastField<FStructProperty>(InputStateProp))
					{
						if (void* StructAddr = StructProp->ContainerPtrToValuePtr<void>(Character))
						{
							for (TFieldIterator<FBoolProperty> It(StructProp->Struct); It; ++It)
							{
								if (It->GetName().Contains(TEXT("WantsToStrafe")) && It->GetPropertyValue_InContainer(StructAddr))
								{
									bNeedsStrafeReset = true;
									break;
								}
							}
						}
					}
				}
			}

			if (bNeedsStrafeReset)
			{
				Character->SetStrafeInputDown(false);
			}
		}
	}
}

void FThreadTalesOverlayDebug::DockHeldObjectRoot(AThreadTalesCharacter* Character, uint8 NewOverlayPose)
{
	if (!Character || !Character->GetMesh())
	{
		return;
	}

	for (UActorComponent* Comp : Character->GetComponents())
	{
		if (Comp && Comp->GetName().Contains(TEXT("HeldObjectRoot")))
		{
			if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
			{
				if (NewOverlayPose == 0) // Default / Sheathed
				{
					SceneComp->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
					SceneComp->SetVisibility(false, true);
				}
			}
			break;
		}
	}
}

FGameplayTag FThreadTalesOverlayDebug::ResolveWeaponInputTag(const AThreadTalesCharacter* Character, FGameplayTag InputTag)
{
	if (!Character)
	{
		return InputTag;
	}
	if (InputTag.MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Ability_Melee))
	{
		if (const UThreadTalesEquipmentComponent* EquipComp = Character->GetEquipmentComponent())
		{
			if (AThreadTalesWeapon* CurWeapon = EquipComp->GetCurrentWeapon())
			{
				if (CurWeapon->GetPrimaryAttackTag().MatchesTagExact(ThreadTalesGameplayTags::TAG_Ability_Ranged_Fire) ||
				    CurWeapon->GetOverlayPoseName() == FName("Bow") ||
				    CurWeapon->IsA<AThreadTalesWeapon_Bow>())
				{
					return ThreadTalesGameplayTags::TAG_Input_Ability_Ranged_Fire;
				}
			}
		}
	}
	else if (InputTag.MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Ability_Block))
	{
		if (const UThreadTalesEquipmentComponent* EquipComp = Character->GetEquipmentComponent())
		{
			if (AThreadTalesWeapon* CurWeapon = EquipComp->GetCurrentWeapon())
			{
				if (CurWeapon->GetPrimaryAttackTag().MatchesTagExact(ThreadTalesGameplayTags::TAG_Ability_Ranged_Fire) ||
				    CurWeapon->GetOverlayPoseName() == FName("Bow") ||
				    CurWeapon->IsA<AThreadTalesWeapon_Bow>())
				{
					return ThreadTalesGameplayTags::TAG_Input_Ability_Ranged;
				}
			}
		}
	}
	return InputTag;
}

void FThreadTalesOverlayDebug::RefreshWeaponMappingContext(const AThreadTalesCharacter* Character)
{
	if (Character)
	{
		if (const UThreadTalesEquipmentComponent* EquipComp = Character->GetEquipmentComponent())
		{
			if (AThreadTalesWeapon* CurWeapon = EquipComp->GetCurrentWeapon())
			{
				CurWeapon->AddWeaponMappingContext();
			}
		}
	}
}

