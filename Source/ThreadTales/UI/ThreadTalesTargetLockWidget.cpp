// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ThreadTalesTargetLockWidget.h"

UThreadTalesTargetLockWidget::UThreadTalesTargetLockWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bIsLocked(false)
{
}

void UThreadTalesTargetLockWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Zero translation offset: UWidgetComponent in Screen space handles 100% of positioning
	SetRenderTranslation(FVector2D::ZeroVector);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UThreadTalesTargetLockWidget::NotifyTargetLocked(AActor* InTarget, AActor* InTargetingActor)
{
	LockedTargetActor = InTarget;
	TargetingActor = InTargetingActor;
	bIsLocked = true;

	SetRenderTranslation(FVector2D::ZeroVector);
	SetVisibility(ESlateVisibility::HitTestInvisible);

	OnTargetLocked(InTarget, InTargetingActor);
	OnTargetLockAcquired(InTarget);
}

void UThreadTalesTargetLockWidget::NotifyTargetUnlocked()
{
	bIsLocked = false;
	LockedTargetActor = nullptr;
	TargetingActor = nullptr;

	OnTargetUnlocked();
	OnTargetLockReleased();
}
