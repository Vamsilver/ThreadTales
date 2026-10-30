// Copyright ThreadTales. All Rights Reserved.

#include "Abilities/ThreadTalesGA_Interact.h"
#include "ThreadTales.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesInteractionDetectorComponent.h"
#include "Combat/ThreadTalesInteractableComponent.h"
#include "Data/ThreadTalesInteractionConfig.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "UI/ThreadTalesInteractionPromptWidget.h"
#include "MotionWarpingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Ability_Interact, "Ability.Interact");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Interacting, "State.Interacting");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Interact_Trigger, "Event.Interact.Trigger");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Interact_Motion_Begin, "Event.Interact.Motion.Begin");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Interact_Motion_End, "Event.Interact.Motion.End");

UThreadTalesGA_Interact::UThreadTalesGA_Interact()
{
	AbilityName = TEXT("Contextual Interaction");
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	SetAssetTags(FGameplayTagContainer(TAG_Ability_Interact));
	ActivationOwnedTags.AddTag(TAG_State_Interacting);
}

bool UThreadTalesGA_Interact::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;

	AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	UThreadTalesInteractionDetectorComponent* Detector = TTChar ? TTChar->GetInteractionDetectorComponent() : nullptr;
	UThreadTalesInteractableComponent* TargetInteractable = Detector ? Detector->GetCurrentInteractable() : nullptr;
	return TargetInteractable && TargetInteractable->CanInteract(TTChar);
}

void UThreadTalesGA_Interact::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	UThreadTalesInteractionDetectorComponent* Detector = TTChar ? TTChar->GetInteractionDetectorComponent() : nullptr;
	ActiveInteractable = Detector ? Detector->GetCurrentInteractable() : nullptr;
	if (!TTChar || !ActiveInteractable.IsValid() || !ActiveInteractable->CanInteract(TTChar))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return;
	}
	if (ActiveInteractable->TryRejectLockedInteraction(TTChar))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false); return;
	}
	if (!ActiveInteractable->TriggerInteraction(TTChar))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return;
	}

	const UThreadTalesInteractionConfig* Config = ActiveInteractable->GetConfig();
	if (!Config)
	{
		ActiveInteractable->FinishInteraction(TTChar);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false); return;
	}

	if (Config->InputType == EThreadTalesInteractionInputType::Hold)
	{
		// UActorComponent: initialize hold duration timer and wait for release
		if (UCharacterMovementComponent* MoveComp = TTChar->GetCharacterMovement()) MoveComp->StopMovementImmediately();

		bIsHolding = true;
		HoldElapsedTime = 0.0f;
		WaitInputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, false);
		if (WaitInputReleaseTask)
		{
			WaitInputReleaseTask->OnRelease.AddDynamic(this, &UThreadTalesGA_Interact::OnHoldInputReleased);
			WaitInputReleaseTask->ReadyForActivation();
		}

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(HoldTimerHandle, this, &UThreadTalesGA_Interact::UpdateHoldProgress, 0.02f, true);
		}
		return;
	}

	bIsHolding = false;
	ExecuteInteractionAction(TTChar, Config);
}

void UThreadTalesGA_Interact::SetWalkGaitForInteraction(AThreadTalesCharacter* TTChar, bool bWalk)
{
	// UActorComponent: toggle GASPALS WantsToWalk flag in CharacterInputState struct
	if (!TTChar) return;
	if (bWalk) TTChar->StopSprinting();

	if (const FStructProperty* StructProp = CastField<FStructProperty>(TTChar->GetClass()->FindPropertyByName(TEXT("CharacterInputState"))))
	{
		if (void* StructAddr = StructProp->ContainerPtrToValuePtr<void>(TTChar))
		{
			for (TFieldIterator<FBoolProperty> It(StructProp->Struct); It; ++It)
			{
				if (It->GetName().Contains(TEXT("WantsToWalk"))) It->SetPropertyValue_InContainer(StructAddr, bWalk);
				else if (bWalk && It->GetName().Contains(TEXT("WantsToSprint"))) It->SetPropertyValue_InContainer(StructAddr, false);
			}
			if (!TTChar->HasAuthority())
			{
				if (UFunction* UpdateFunc = TTChar->FindFunction(TEXT("UpdateInputState_Server"))) TTChar->ProcessEvent(UpdateFunc, StructAddr);
			}
		}
	}
}

void UThreadTalesGA_Interact::ExecuteInteractionAction(AThreadTalesCharacter* TTChar, const UThreadTalesInteractionConfig* Config)
{
	if (!TTChar || !Config || !ActiveInteractable.IsValid())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return;
	}

	// If animation is disabled for this object (e.g. instant lever/switch), trigger action directly without approach or anim montage
	if (!Config->bAnimateCharacter)
	{
		if (ActiveInteractable.IsValid())
		{
			ActiveInteractable->TriggerCosmeticAction();
			ActiveInteractable->FinishInteraction(TTChar);
		}
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	const FTransform TargetTransform = ActiveInteractable->GetInteractionTransform();
	const FVector TargetLoc = TargetTransform.GetLocation();
	const FVector CharLoc = TTChar->GetActorLocation();
	const float Dist2D = FVector::Dist2D(CharLoc, TargetLoc);
	const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(TTChar->GetActorRotation().Yaw, TargetTransform.Rotator().Yaw));

	// AAA Adaptive Reach Check: If player is within reach distance and facing the interaction point, skip walk phase
	bool bWithinAdaptiveReach = false;
	if (Config->bEnableAdaptiveReach)
	{
		const FVector ToTarget2D = (TargetLoc - CharLoc).GetSafeNormal2D();
		const FVector CharForward2D = TTChar->GetActorForwardVector().GetSafeNormal2D();
		const float AngleToTarget = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(CharForward2D, ToTarget2D), -1.0f, 1.0f)));
		if (Dist2D <= Config->AdaptiveReachDistance && AngleToTarget <= Config->AdaptiveReachAngle && YawDelta <= Config->AdaptiveReachAngle)
		{
			bWithinAdaptiveReach = true;
		}
	}

	if (!bWithinAdaptiveReach && (Dist2D > 12.0f || YawDelta > 12.0f))
	{
		if (UWorld* World = GetWorld())
		{
			SetWalkGaitForInteraction(TTChar, true);
			bIsApproaching = true;
			ApproachElapsedTime = 0.0f;
			LastApproachFrame = 0;
			ApproachMoveDir = (TargetLoc - CharLoc).GetSafeNormal2D();
			if (ApproachMoveDir.IsNearlyZero()) ApproachMoveDir = TargetTransform.GetRotation().GetForwardVector().GetSafeNormal2D();

			if (const AActor* PropActor = ActiveInteractable->GetOwner())
			{
				const FVector CenterLoc = PropActor->GetActorLocation();
				const FVector CenterToTarget = (TargetLoc - CenterLoc).GetSafeNormal2D();
				const FVector CenterToChar = (CharLoc - CenterLoc).GetSafeNormal2D();
				const FVector Tangent(-CenterToChar.Y, CenterToChar.X, 0.0f);
				ApproachTangentSign = (FVector::DotProduct(Tangent, CenterToTarget) >= -0.05f) ? 1.0f : -1.0f;
			}
			World->GetTimerManager().SetTimer(ApproachTimerHandle, this, &UThreadTalesGA_Interact::UpdateApproachMovement, 0.002f, true);
			return;
		}
	}

	PlayInteractionMontage(TTChar, Config);
}

void UThreadTalesGA_Interact::UpdateApproachMovement()
{
	// UActorComponent: steer character smoothly every frame in Walk gait without fighting CMC rotation
	if (LastApproachFrame == GFrameCounter) return;
	LastApproachFrame = GFrameCounter;

	AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo());
	if (!ActiveInteractable.IsValid() || !TTChar || TTChar->IsStaggered())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	UWorld* World = GetWorld();
	const float DeltaTime = FMath::Clamp(World ? World->GetDeltaSeconds() : 0.016f, 0.002f, 0.05f);
	ApproachElapsedTime += DeltaTime;

	const UThreadTalesInteractionConfig* Config = ActiveInteractable->GetConfig();
	const FTransform TargetTransform = ActiveInteractable->GetInteractionTransform();
	const FVector TargetLoc = TargetTransform.GetLocation();
	const FRotator TargetRot = TargetTransform.Rotator();
	const FVector CharLoc = TTChar->GetActorLocation();
	const float Dist2D = FVector::Dist2D(CharLoc, TargetLoc);
	const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(TTChar->GetActorRotation().Yaw, TargetRot.Yaw));

	if (Dist2D > 16.0f && ApproachElapsedTime < 2.0f)
	{
		FVector DesiredDir = (TargetLoc - CharLoc).GetSafeNormal2D();
		if (const AActor* PropActor = ActiveInteractable->GetOwner())
		{
			const FVector CenterLoc = PropActor->GetActorLocation();
			const FVector CenterToTarget = (TargetLoc - CenterLoc).GetSafeNormal2D();
			const FVector CenterToChar = (CharLoc - CenterLoc).GetSafeNormal2D();
			const float SideDot = FVector::DotProduct(CenterToChar, CenterToTarget);
			const float TangentAlpha = FMath::GetMappedRangeValueClamped(FVector2D(0.55f, -0.15f), FVector2D(0.0f, 1.0f), SideDot);
			if (!CenterToTarget.IsNearlyZero() && TangentAlpha > KINDA_SMALL_NUMBER)
			{
				const FVector Tangent = FVector(-CenterToChar.Y, CenterToChar.X, 0.0f) * ApproachTangentSign;
				DesiredDir = FMath::Lerp(DesiredDir, (DesiredDir * 0.35f + Tangent * 0.90f).GetSafeNormal2D(), TangentAlpha).GetSafeNormal2D();
			}
		}

		const float AlignAlpha = FMath::GetMappedRangeValueClamped(FVector2D(30.0f, 16.0f), FVector2D(0.0f, 0.65f), Dist2D);
		DesiredDir = FMath::Lerp(DesiredDir, TargetRot.Vector().GetSafeNormal2D(), AlignAlpha).GetSafeNormal2D();
		ApproachMoveDir = FMath::VInterpNormalRotationTo(ApproachMoveDir, DesiredDir, DeltaTime, 240.0f);
		TTChar->AddMovementInput(ApproachMoveDir, 1.0f);
		return;
	}

	bIsApproaching = false;
	if (World) World->GetTimerManager().ClearTimer(ApproachTimerHandle);
	SetWalkGaitForInteraction(TTChar, false);
	PlayInteractionMontage(TTChar, Config);
}

void UThreadTalesGA_Interact::PlayInteractionMontage(AThreadTalesCharacter* TTChar, const UThreadTalesInteractionConfig* Config)
{
	// UActorComponent: proceed with motion warping and montage playback without hard camera snaps
	if (!TTChar || !Config)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	SetWalkGaitForInteraction(TTChar, false);
	if (UCharacterMovementComponent* MoveComp = TTChar->GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
	}

	ConfigureMotionWarping(TTChar, Config);

	UAnimMontage* MontageToPlay = Config->CharacterMontage.LoadSynchronous();
	if (ActiveInteractable.IsValid() && ActiveInteractable->IsActivated())
	{
		if (UAnimMontage* ToggledMontage = Config->ToggledCharacterMontage.LoadSynchronous())
		{
			MontageToPlay = ToggledMontage;
		}
	}

	if (MontageToPlay)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay, 1.0f, NAME_None, false, 1.0f);
		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UThreadTalesGA_Interact::OnMontageCompleted);
			MontageTask->OnBlendOut.AddDynamic(this, &UThreadTalesGA_Interact::OnMontageBlendOut);
			MontageTask->OnInterrupted.AddDynamic(this, &UThreadTalesGA_Interact::OnMontageInterrupted);
			MontageTask->OnCancelled.AddDynamic(this, &UThreadTalesGA_Interact::OnMontageCancelled);
			MontageTask->ReadyForActivation();

			WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TAG_Event_Interact_Trigger, nullptr, true, true);
			if (WaitEventTask)
			{
				WaitEventTask->EventReceived.AddDynamic(this, &UThreadTalesGA_Interact::OnInteractEventReceived);
				WaitEventTask->ReadyForActivation();
			}

			WaitEndEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TAG_Event_Interact_Motion_End, nullptr, true, true);
			if (WaitEndEventTask)
			{
				WaitEndEventTask->EventReceived.AddDynamic(this, &UThreadTalesGA_Interact::OnInteractMotionEndReceived);
				WaitEndEventTask->ReadyForActivation();
			}
			return;
		}
	}

	if (ActiveInteractable.IsValid()) ActiveInteractable->FinishInteraction(TTChar);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UThreadTalesGA_Interact::ConfigureMotionWarping(AThreadTalesCharacter* TTChar, const UThreadTalesInteractionConfig* Config)
{
	if (!TTChar || !Config || !ActiveInteractable.IsValid())
	{
		return;
	}

	UMotionWarpingComponent* WarpingComp = TTChar->FindComponentByClass<UMotionWarpingComponent>();
	if (!WarpingComp)
	{
		WarpingComp = NewObject<UMotionWarpingComponent>(TTChar, TEXT("MotionWarpingComp"));
		if (WarpingComp)
		{
			WarpingComp->RegisterComponent();
		}
	}

	if (WarpingComp)
	{
		const FTransform TargetTransform = ActiveInteractable->GetInteractionTransform();
		const FName TargetName = Config->WarpTargetName.IsNone() ? FName("InteractionTarget") : Config->WarpTargetName;
		WarpingComp->AddOrUpdateWarpTargetFromTransform(TargetName, TargetTransform);

		if (!Config->HandWarpTargetName.IsNone())
		{
			const FTransform HandTransform = ActiveInteractable->GetHandInteractionTransform();
			WarpingComp->AddOrUpdateWarpTargetFromTransform(Config->HandWarpTargetName, HandTransform);
		}
	}
}

void UThreadTalesGA_Interact::OnInteractEventReceived(FGameplayEventData Payload)
{
	if (ActiveInteractable.IsValid())
	{
		ActiveInteractable->TriggerCosmeticAction();
	}
}

void UThreadTalesGA_Interact::OnInteractMotionEndReceived(FGameplayEventData Payload)
{
}

void UThreadTalesGA_Interact::OnMontageCompleted()
{
	AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo());
	if (ActiveInteractable.IsValid() && TTChar) ActiveInteractable->FinishInteraction(TTChar);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UThreadTalesGA_Interact::OnMontageBlendOut() { OnMontageCompleted(); }
void UThreadTalesGA_Interact::OnMontageInterrupted() { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); }
void UThreadTalesGA_Interact::OnMontageCancelled() { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); }

void UThreadTalesGA_Interact::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);
	if (bIsHolding) OnHoldInputReleased(HoldElapsedTime);
}

void UThreadTalesGA_Interact::OnHoldInputReleased(float TimeHeld)
{
	// UActorComponent: abort hold interaction on key release
	if (!bIsHolding) return;
	bIsHolding = false;

	AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo());
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(HoldTimerHandle);

	if (ActiveInteractable.IsValid())
	{
		if (UThreadTalesInteractionPromptWidget* Prompt = ActiveInteractable->GetPromptWidget()) Prompt->UpdateHoldProgress(0.0f);
		ActiveInteractable->CancelInteraction(TTChar);
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UThreadTalesGA_Interact::UpdateHoldProgress()
{
	// UActorComponent: advance hold progress timer and verify distance/stagger
	AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo());
	if (!ActiveInteractable.IsValid() || !TTChar || TTChar->IsStaggered() || FVector::DistSquared2D(ActiveInteractable->GetInteractionLocation(), TTChar->GetActorLocation()) > FMath::Square(260.0f))
	{
		OnHoldInputReleased(HoldElapsedTime);
		return;
	}

	const UThreadTalesInteractionConfig* Config = ActiveInteractable->GetConfig();
	const float Duration = Config ? FMath::Max(Config->HoldDuration, 0.1f) : 1.0f;
	HoldElapsedTime += 0.02f;
	const float Progress = FMath::Clamp(HoldElapsedTime / Duration, 0.0f, 1.0f);

	if (UThreadTalesInteractionPromptWidget* Prompt = ActiveInteractable->GetPromptWidget()) Prompt->UpdateHoldProgress(Progress);

	if (Progress >= 1.0f)
	{
		bIsHolding = false;
		if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(HoldTimerHandle);
		if (WaitInputReleaseTask) { WaitInputReleaseTask->EndTask(); WaitInputReleaseTask = nullptr; }
		if (UThreadTalesInteractionPromptWidget* Prompt = ActiveInteractable->GetPromptWidget()) Prompt->UpdateHoldProgress(0.0f);
		ExecuteInteractionAction(TTChar, Config);
	}
}

void UThreadTalesGA_Interact::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	bIsHolding = false;
	bIsApproaching = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HoldTimerHandle);
		World->GetTimerManager().ClearTimer(ApproachTimerHandle);
	}

	if (WaitInputReleaseTask) { WaitInputReleaseTask->EndTask(); WaitInputReleaseTask = nullptr; }
	if (MontageTask) { MontageTask->EndTask(); MontageTask = nullptr; }
	if (WaitEventTask) { WaitEventTask->EndTask(); WaitEventTask = nullptr; }
	if (WaitEndEventTask) { WaitEndEventTask->EndTask(); WaitEndEventTask = nullptr; }

	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		SetWalkGaitForInteraction(TTChar, false);
		if (bWasCancelled && ActiveInteractable.IsValid()) ActiveInteractable->CancelInteraction(TTChar);
		if (ActiveInteractable.IsValid())
		{
			if (UThreadTalesInteractionPromptWidget* Prompt = ActiveInteractable->GetPromptWidget()) Prompt->UpdateHoldProgress(0.0f);
		}

		if (UMotionWarpingComponent* WarpingComp = TTChar->FindComponentByClass<UMotionWarpingComponent>())
		{
			if (ActiveInteractable.IsValid() && ActiveInteractable->GetConfig())
			{
				const FName TargetName = ActiveInteractable->GetConfig()->WarpTargetName.IsNone() ? FName("InteractionTarget") : ActiveInteractable->GetConfig()->WarpTargetName;
				WarpingComp->RemoveWarpTarget(TargetName);
				if (!ActiveInteractable->GetConfig()->HandWarpTargetName.IsNone()) WarpingComp->RemoveWarpTarget(ActiveInteractable->GetConfig()->HandWarpTargetName);
			}
		}
	}

	ActiveInteractable = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
