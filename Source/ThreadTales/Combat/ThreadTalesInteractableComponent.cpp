// Copyright ThreadTales. All Rights Reserved.
// UActorComponent implementation: ThreadTalesInteractableComponent with smooth cosmetic lid rotation and floating prompt UI

#include "ThreadTalesInteractableComponent.h"
#include "Data/ThreadTalesInteractionConfig.h"
#include "Combat/ThreadTalesTriggerableInterface.h"
#include "UI/ThreadTalesInteractionPromptWidget.h"
#include "Components/WidgetComponent.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"

UThreadTalesInteractableComponent::UThreadTalesInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = false;
	SetIsReplicatedByDefault(true);
}

void UThreadTalesInteractableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UThreadTalesInteractableComponent, bIsActivated);
	DOREPLIFETIME(UThreadTalesInteractableComponent, bIsBusy);
	DOREPLIFETIME(UThreadTalesInteractableComponent, bHasBeenTriggered);
}

void UThreadTalesInteractableComponent::BeginPlay()
{
	Super::BeginPlay();
	if (TargetActorToTrigger) TargetActorsToTrigger.AddUnique(TargetActorToTrigger);
	CacheInteractionPoint(); CacheAnimatedComponent(); CacheHandTargetComponent(); SetupPromptWidget();
}

void UThreadTalesInteractableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(LidInterpolationTimerHandle); World->GetTimerManager().ClearTimer(AutoResetTimerHandle); }
	UpdatePromptVisibility(false);
	Super::EndPlay(EndPlayReason);
}

void UThreadTalesInteractableComponent::OnRep_IsActivated()
{
	StartLidAnimation(!bIsActivated);
	if (bIsFocused && !bIsBusy) UpdatePromptVisibility(true);
}

void UThreadTalesInteractableComponent::OnRep_IsBusy()
{
	const bool bShouldShow = bIsFocused && !bIsBusy && (!bHasBeenTriggered || IsToggleMode() || (InteractionConfig && !InteractionConfig->bOneShot));
	UpdatePromptVisibility(bShouldShow);
}

void UThreadTalesInteractableComponent::CacheInteractionPoint()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;
	if (!InteractionPointComponentName.IsNone())
	{
		for (UActorComponent* Comp : OwnerActor->GetComponents())
		{
			if (Comp && Comp->GetFName() == InteractionPointComponentName)
			{
				CachedInteractionPoint = Cast<USceneComponent>(Comp);
				return;
			}
		}
	}
	CachedInteractionPoint = OwnerActor->FindComponentByClass<UArrowComponent>() ? Cast<USceneComponent>(OwnerActor->FindComponentByClass<UArrowComponent>()) : OwnerActor->GetRootComponent();
}

void UThreadTalesInteractableComponent::CacheAnimatedComponent()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || AnimatedComponentName.IsNone()) return;
	for (UActorComponent* Comp : OwnerActor->GetComponents())
	{
		if (Comp && Comp->GetFName() == AnimatedComponentName)
		{
			if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
			{
				CachedAnimatedComponent = SceneComp;
				InitialLidRotation = SceneComp->GetRelativeRotation();
				TargetLidRotation = InitialLidRotation + OpenRotationOffset;
				InitialLidLocation = SceneComp->GetRelativeLocation();
				TargetLidLocation = InitialLidLocation + OpenTranslationOffset;
				return;
			}
		}
	}
}

void UThreadTalesInteractableComponent::CacheHandTargetComponent()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;
	for (UActorComponent* Comp : OwnerActor->GetComponents())
	{
		if (USceneComponent* SC = Cast<USceneComponent>(Comp))
		{
			if (SC->GetFName() == FName("HandTarget") || SC->ComponentHasTag(FName("HandTarget")))
			{
				CachedHandTargetComponent = SC; return;
			}
		}
	}
	const FName TargetName = HandTargetComponentName.IsNone() ? FName("HandTarget") : HandTargetComponentName;
	if (TargetName != FName("LidPivot") && TargetName != AnimatedComponentName)
	{
		for (UActorComponent* Comp : OwnerActor->GetComponents())
		{
			if (USceneComponent* SC = Cast<USceneComponent>(Comp))
			{
				if (SC->GetFName() == TargetName || SC->ComponentHasTag(TargetName))
				{
					CachedHandTargetComponent = SC; return;
				}
			}
		}
	}
	if (USceneComponent* AnimComp = CachedAnimatedComponent.Get())
	{
		TArray<USceneComponent*> Children;
		AnimComp->GetChildrenComponents(true, Children);
		for (USceneComponent* Child : Children)
		{
			if (Child && (Child->GetFName() == FName("HandTarget") || Child->ComponentHasTag(FName("HandTarget")) || Cast<UStaticMeshComponent>(Child)))
			{
				CachedHandTargetComponent = Child; return;
			}
		}
	}
}

void UThreadTalesInteractableComponent::SetupPromptWidget()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;
	if (!PromptWidgetComponent) PromptWidgetComponent = OwnerActor->FindComponentByClass<UWidgetComponent>();
	if (!PromptWidgetComponent)
	{
		PromptWidgetComponent = NewObject<UWidgetComponent>(OwnerActor, TEXT("InteractionPromptWidgetComp"));
		if (PromptWidgetComponent) { PromptWidgetComponent->SetupAttachment(OwnerActor->GetRootComponent()); PromptWidgetComponent->RegisterComponent(); }
	}
	if (PromptWidgetComponent)
	{
		PromptWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
		PromptWidgetComponent->SetDrawAtDesiredSize(true);
		PromptWidgetComponent->SetVisibility(false);
		if (PromptWidgetClass && !PromptWidgetComponent->GetWidgetClass()) PromptWidgetComponent->SetWidgetClass(PromptWidgetClass);
	}
}

void UThreadTalesInteractableComponent::UpdatePromptVisibility(bool bVisible)
{
	if (!PromptWidgetComponent) return;
	if (bVisible)
	{
		PromptWidgetComponent->SetWorldLocation(GetPromptLocation());
		PromptWidgetComponent->SetVisibility(true);
		PromptWidgetComponent->InitWidget();
		if (UThreadTalesInteractionPromptWidget* PromptWidget = GetPromptWidget())
		{
			const bool bLocked = IsLockedForCharacter(CachedFocusedCharacter.Get());
			const FText CustomText = (!bLocked && IsToggleMode() && bIsActivated) ? (!ToggledPromptText.IsEmpty() ? ToggledPromptText : (InteractionConfig ? InteractionConfig->ToggledPromptText : FText::GetEmpty())) : FText::GetEmpty();
			PromptWidget->ShowPrompt(InteractionConfig, bLocked, CustomText);
		}
	}
	else
	{
		if (UThreadTalesInteractionPromptWidget* PromptWidget = GetPromptWidget()) PromptWidget->HidePrompt();
		PromptWidgetComponent->SetVisibility(false);
	}
}

FVector UThreadTalesInteractableComponent::GetPromptLocation() const
{
	if (CachedAnimatedComponent.IsValid()) return CachedAnimatedComponent->GetComponentTransform().TransformPosition(PromptWidgetRelativeOffset);
	if (const AActor* Owner = GetOwner()) return Owner->GetActorTransform().TransformPosition(PromptWidgetRelativeOffset);
	return GetInteractionTransform().TransformPosition(PromptWidgetRelativeOffset);
}

UThreadTalesInteractionPromptWidget* UThreadTalesInteractableComponent::GetPromptWidget() const
{
	return PromptWidgetComponent ? Cast<UThreadTalesInteractionPromptWidget>(PromptWidgetComponent->GetUserWidgetObject()) : nullptr;
}

bool UThreadTalesInteractableComponent::IsLockedForCharacter(const ACharacter* Character) const
{
	if (bHasBeenTriggered || !InteractionConfig || !InteractionConfig->RequiredTag.IsValid()) return false;
	const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Character);
	const UAbilitySystemComponent* ASC = ASI ? ASI->GetAbilitySystemComponent() : nullptr;
	return ASC ? !ASC->HasMatchingGameplayTag(InteractionConfig->RequiredTag) : true;
}

bool UThreadTalesInteractableComponent::TryRejectLockedInteraction(ACharacter* Character)
{
	if (!IsLockedForCharacter(Character)) return false;
	if (InteractionConfig)
	{
		if (USoundBase* LockedSnd = InteractionConfig->LockedSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySoundAtLocation(this, LockedSnd, GetInteractionLocation());
		}
	}
	if (UThreadTalesInteractionPromptWidget* PromptWidget = GetPromptWidget()) { PromptWidget->ShowPrompt(InteractionConfig, true); PromptWidget->NotifyLockedInteractionDenied(); }
	return true;
}

bool UThreadTalesInteractableComponent::CanInteract(const ACharacter* Character) const
{
	if (!Character || bIsBusy) return false;
	if (bHasBeenTriggered && !IsToggleMode() && InteractionConfig && InteractionConfig->bOneShot) return false;
	const FVector TargetLoc = GetInteractionLocation();
	const FVector CharLoc = Character->GetActorLocation();
	const float RadiusSq = FMath::Square(InteractionRadius > 0.0f ? InteractionRadius : 200.0f);
	if (FVector::DistSquared2D(TargetLoc, CharLoc) > RadiusSq) return false;
	const FVector CharFacing = Character->GetActorForwardVector();
	const FVector DirToTarget = (TargetLoc - CharLoc).GetSafeNormal2D();
	const float Dot = FVector::DotProduct(CharFacing, DirToTarget.IsNearlyZero() ? CharFacing : DirToTarget);
	return Dot >= FMath::Cos(FMath::DegreesToRadians(InteractionAngleDegrees * 0.5f));
}

bool UThreadTalesInteractableComponent::TriggerInteraction(ACharacter* Character)
{
	if (!CanInteract(Character) || IsLockedForCharacter(Character)) return false;
	bIsBusy = true;
	bCosmeticActionExecutedThisInteraction = false;
	if (!InteractionConfig || InteractionConfig->InputType != EThreadTalesInteractionInputType::Hold) UpdatePromptVisibility(false);
	OnInteractionStarted.Broadcast(Character, this);
	return true;
}

void UThreadTalesInteractableComponent::FinishInteraction(ACharacter* Character)
{
	bIsBusy = false;
	UpdatePromptVisibility(false);
	if (!bHasBeenTriggered && InteractionConfig && InteractionConfig->RequiredTag.IsValid() && InteractionConfig->bConsumeRequiredTag && Character)
	{
		if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Character))
		{
			if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent()) ASC->RemoveLooseGameplayTag(InteractionConfig->RequiredTag);
		}
	}

	if (IsToggleMode())
	{
		bIsActivated = !bIsActivated;
		bHasBeenTriggered = true;
		if (bIsActivated) { if (!bCosmeticActionExecutedThisInteraction) StartLidAnimation(false); }
		else
		{
			if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(AutoResetTimerHandle);
			if (!bCosmeticActionExecutedThisInteraction)
			{
				StartLidAnimation(true);
				USoundBase* SoundToPlay = ResetSound ? ResetSound.Get() : (InteractionConfig ? InteractionConfig->DefaultResetSound.LoadSynchronous() : nullptr);
				if (SoundToPlay) UGameplayStatics::PlaySoundAtLocation(this, SoundToPlay, GetInteractionLocation());
			}
		}
	}
	else
	{
		bIsActivated = true;
		bHasBeenTriggered = true;
		if (!bCosmeticActionExecutedThisInteraction) StartLidAnimation(false);
	}

	if (bIsActivated && bAutoReset)
	{
		if (UWorld* World = GetWorld()) World->GetTimerManager().SetTimer(AutoResetTimerHandle, this, &UThreadTalesInteractableComponent::ResetInteractionState, FMath::Max(AutoResetDelay, 0.1f), false);
	}

	bCosmeticActionExecutedThisInteraction = false;
	OnInteractionCompleted.Broadcast(Character, this);
	NotifyLinkedTriggers(Character, bIsActivated);
	if (bIsFocused && !bIsBusy) UpdatePromptVisibility(true);
}

void UThreadTalesInteractableComponent::CancelInteraction(ACharacter* Character)
{
	bIsBusy = false;
	if (bCosmeticActionExecutedThisInteraction || LidInterpolationTimerHandle.IsValid())
	{
		if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(LidInterpolationTimerHandle);
		bLidAnimationStarted = false;
		if (USceneComponent* AnimatedComp = CachedAnimatedComponent.Get()) AnimatedComp->SetRelativeRotation(bIsActivated ? TargetLidRotation : InitialLidRotation);
	}
	bCosmeticActionExecutedThisInteraction = false;
	UpdatePromptVisibility(bIsFocused && (!bHasBeenTriggered || IsToggleMode() || (InteractionConfig && !InteractionConfig->bOneShot)));
}

void UThreadTalesInteractableComponent::TriggerCosmeticAction()
{
	if (bCosmeticActionExecutedThisInteraction) return;
	bCosmeticActionExecutedThisInteraction = true;
	UpdatePromptVisibility(false);
	if (!bLidAnimationStarted) StartLidAnimation(IsToggleMode() ? bIsActivated : false);
}

void UThreadTalesInteractableComponent::StartLidAnimation(bool bReverse)
{
	bIsAnimatingReverse = bReverse;
	bLidAnimationStarted = true;
	if (!CachedAnimatedComponent.IsValid()) { CacheAnimatedComponent(); if (!CachedAnimatedComponent.IsValid()) { bLidAnimationStarted = false; return; } }
	UWorld* World = GetWorld();
	if (!World) { bLidAnimationStarted = false; return; }

	USoundBase* SoundToPlay = bReverse
		? (ResetSound ? ResetSound.Get() : (InteractionConfig ? InteractionConfig->DefaultResetSound.LoadSynchronous() : nullptr))
		: (OpenSound ? OpenSound.Get() : (InteractionConfig ? InteractionConfig->DefaultOpenSound.LoadSynchronous() : nullptr));
	if (SoundToPlay) UGameplayStatics::PlaySoundAtLocation(this, SoundToPlay, GetInteractionLocation());

	LidAnimElapsedTime = 0.0f;
	World->GetTimerManager().SetTimer(LidInterpolationTimerHandle, this, &UThreadTalesInteractableComponent::UpdateLidRotation, 0.016f, true);
}

void UThreadTalesInteractableComponent::UpdateLidRotation()
{
	USceneComponent* AnimatedComp = CachedAnimatedComponent.Get();
	UWorld* World = GetWorld();
	if (!AnimatedComp || !World)
	{
		if (World) World->GetTimerManager().ClearTimer(LidInterpolationTimerHandle);
		bLidAnimationStarted = false;
		return;
	}

	const float DeltaTime = FMath::Clamp(World->GetDeltaSeconds(), 0.001f, 0.1f);
	LidAnimElapsedTime += DeltaTime;

	const float TargetDuration = (bIsAnimatingReverse && CloseDuration > 0.0f) ? CloseDuration : OpenDuration;
	const float Duration = FMath::Max(TargetDuration, 0.05f);
	const float NormalizedTime = FMath::Clamp(LidAnimElapsedTime / Duration, 0.0f, 1.0f);

	float SmoothAlpha = 0.0f;
	if (KinematicsCurve)
	{
		SmoothAlpha = FMath::Clamp(KinematicsCurve->GetFloatValue(NormalizedTime), 0.0f, 1.0f);
	}
	else
	{
		SmoothAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, NormalizedTime, 2.0f);
	}

	const FRotator FromRot = bIsAnimatingReverse ? TargetLidRotation : InitialLidRotation;
	const FRotator ToRot = bIsAnimatingReverse ? InitialLidRotation : TargetLidRotation;
	AnimatedComp->SetRelativeRotation(FMath::Lerp(FromRot, ToRot, SmoothAlpha));

	const FVector FromLoc = bIsAnimatingReverse ? TargetLidLocation : InitialLidLocation;
	const FVector ToLoc = bIsAnimatingReverse ? InitialLidLocation : TargetLidLocation;
	AnimatedComp->SetRelativeLocation(FMath::Lerp(FromLoc, ToLoc, SmoothAlpha));

	if (NormalizedTime >= 1.0f)
	{
		AnimatedComp->SetRelativeRotation(ToRot);
		AnimatedComp->SetRelativeLocation(ToLoc);
		World->GetTimerManager().ClearTimer(LidInterpolationTimerHandle);
		bLidAnimationStarted = false;
	}
}

void UThreadTalesInteractableComponent::ResetInteractionState()
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(AutoResetTimerHandle);
	if (bIsBusy || (!bIsActivated && !bLidAnimationStarted)) return;

	bIsActivated = false;
	StartLidAnimation(true);
	USoundBase* SoundToPlay = ResetSound ? ResetSound.Get() : (InteractionConfig ? InteractionConfig->DefaultResetSound.LoadSynchronous() : nullptr);
	if (SoundToPlay) UGameplayStatics::PlaySoundAtLocation(this, SoundToPlay, GetInteractionLocation());

	OnInteractionReset.Broadcast(nullptr, this);
	NotifyLinkedTriggers(GetOwner(), false);
	if (bIsFocused && !bIsBusy) UpdatePromptVisibility(true);
}

void UThreadTalesInteractableComponent::NotifyLinkedTriggers(AActor* InstigatorActor, bool bActiveState)
{
	auto TryExec = [InstigatorActor, bActiveState](AActor* Target)
	{
		if (Target && Target->GetClass()->ImplementsInterface(UThreadTalesTriggerableInterface::StaticClass()))
		{
			// Broadcast explicit bool state event
			IThreadTalesTriggerableInterface::Execute_ExecuteSetTriggerState(Target, bActiveState, InstigatorActor);

			if (bActiveState)
			{
				// Forward activation
				IThreadTalesTriggerableInterface::Execute_ExecuteTrigger(Target, InstigatorActor);
			}
			else
			{
				// Reverse / toggle-off reset
				IThreadTalesTriggerableInterface::Execute_ExecuteResetTrigger(Target, InstigatorActor);
			}
		}
	};
	TryExec(TargetActorToTrigger);
	for (AActor* ExtraTarget : TargetActorsToTrigger)
	{
		if (ExtraTarget != TargetActorToTrigger) TryExec(ExtraTarget);
	}
}

void UThreadTalesInteractableComponent::ToggleState(ACharacter* InstigatorCharacter) { FinishInteraction(InstigatorCharacter); }
FVector UThreadTalesInteractableComponent::GetInteractionLocation() const { return GetInteractionTransform().GetLocation(); }
FRotator UThreadTalesInteractableComponent::GetInteractionRotation() const { return GetInteractionTransform().Rotator(); }

FTransform UThreadTalesInteractableComponent::GetInteractionTransform() const
{
	if (CachedInteractionPoint.IsValid())
	{
		const FVector ArrowLoc = CachedInteractionPoint->GetComponentLocation();
		const FVector FacingDir = CachedInteractionPoint->GetForwardVector().GetSafeNormal2D();
		if (!FacingDir.IsNearlyZero())
		{
			return FTransform(FRotator(0.0f, FacingDir.Rotation().Yaw, 0.0f), ArrowLoc, FVector::OneVector);
		}
		return CachedInteractionPoint->GetComponentTransform();
	}
	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetActorTransform() : FTransform::Identity;
}

FTransform UThreadTalesInteractableComponent::GetHandInteractionTransform() const
{
	if (CachedHandTargetComponent.IsValid() && CachedHandTargetComponent != CachedAnimatedComponent) return CachedHandTargetComponent->GetComponentTransform();
	if (CachedAnimatedComponent.IsValid())
	{
		FTransform TopXform = CachedAnimatedComponent->GetComponentTransform();
		TopXform.SetLocation(TopXform.TransformPosition(FVector(0.0f, 0.0f, 50.0f)));
		return TopXform;
	}
	return GetInteractionTransform();
}

void UThreadTalesInteractableComponent::SetFocused(bool bInFocused, const ACharacter* FocusedCharacter)
{
	CachedFocusedCharacter = bInFocused ? FocusedCharacter : nullptr;
	if (bIsFocused == bInFocused) return;
	bIsFocused = bInFocused;
	UpdatePromptVisibility(bIsFocused && !bIsBusy && (!bHasBeenTriggered || IsToggleMode() || (InteractionConfig && !InteractionConfig->bOneShot)));
}

bool UThreadTalesInteractableComponent::IsToggleMode() const
{
	if (bToggleMode) return true;
	if (InteractionConfig && (InteractionConfig->bToggleMode || !InteractionConfig->ToggledCharacterMontage.IsNull())) return true;
	return false;
}
