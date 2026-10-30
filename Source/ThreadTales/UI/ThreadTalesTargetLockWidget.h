// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ThreadTalesTargetLockWidget.generated.h"

/**
 * Screen-space tactical combat target lock reticle widget for ThreadTales.
 * Hosted inside a UWidgetComponent (Space = Screen) attached directly to the target's torso/socket.
 * Pure presentation/events base class: visual styling, images, sizes, and animations
 * are authored 100% inside Widget Blueprint (WBP) by designers.
 */
UCLASS(BlueprintType, Blueprintable)
class THREADTALES_API UThreadTalesTargetLockWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UThreadTalesTargetLockWidget(const FObjectInitializer& ObjectInitializer);

	/** Notifies the widget that target lock has engaged */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI")
	void NotifyTargetLocked(AActor* InTarget, AActor* InTargetingActor);

	/** Notifies the widget that target lock has disengaged */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI")
	void NotifyTargetUnlocked();

	/** Returns the actor currently locked onto (the owner of the widget component) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	AActor* GetLockedTarget() const { return LockedTargetActor.Get(); }

	/** Returns the character that locked onto this target (e.g. player) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	AActor* GetTargetingActor() const { return TargetingActor.Get(); }

	/** Returns true if target lock is currently active */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	bool IsTargetLocked() const { return bIsLocked; }

	// ==========================================
	// BLUEPRINT DESIGNER HOOKS
	// ==========================================

	/** Called when locked onto this target. Use this in WBP to play animations (e.g. pulse, fade-in, sound) */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|UI|Events")
	void OnTargetLocked(AActor* TargetActor, AActor* InTargetingActor);

	/** Called when target lock is released. Use this in WBP to play exit animations or hide */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|UI|Events")
	void OnTargetUnlocked();

	/** Compatibility event for existing WBP nodes */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|UI|Events")
	void OnTargetLockAcquired(AActor* NewTarget);

	/** Compatibility event for existing WBP nodes */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|UI|Events")
	void OnTargetLockReleased();

	/** Compatibility event for target switching */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|UI|Events")
	void OnTargetSwitched(AActor* NewTarget);

protected:
	virtual void NativeConstruct() override;

private:
	UPROPERTY()
	TWeakObjectPtr<AActor> LockedTargetActor;

	UPROPERTY()
	TWeakObjectPtr<AActor> TargetingActor;

	UPROPERTY()
	bool bIsLocked;
};
