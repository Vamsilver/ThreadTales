// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ThreadTalesEnemyOverheadWidget.generated.h"

class UThreadTalesStatBarWidget;
class AThreadTalesBaseCharacter;
class UImage;
class UTextBlock;

/**
 * Floating overhead widget for enemies in ThreadTales.
 * Reuses UThreadTalesStatBarWidget to provide complete visual and functional parity
 * with player HUD bars (instant updates, ghost-damage catch-up, and Sekiro posture).
 * Manages tactical combat visibility with automatic fade-out and target lock indication.
 */
UCLASS(BlueprintType, Blueprintable)
class THREADTALES_API UThreadTalesEnemyOverheadWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UThreadTalesEnemyOverheadWidget(const FObjectInitializer& ObjectInitializer);

	/** Binds this overhead widget to an enemy character and hooks up attribute delegates */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI")
	void InitializeForCharacter(AThreadTalesBaseCharacter* InCharacter);

	/** Sets whether the owner enemy is currently locked onto by the player's TargetLockComponent */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI")
	void SetTargetLocked(bool bInLocked);

	/** Immediately triggers death hiding and unbinds delegates */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI")
	void HandleDeath();

	/** Refreshes combat visibility timer, keeping the bar visible */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI")
	void RefreshCombatVisibility();

	/** Sets or updates the displayed enemy name */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI")
	void SetEnemyDisplayName(const FText& InName);

	/** Returns the currently bound character */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	AThreadTalesBaseCharacter* GetBoundCharacter() const { return BoundCharacter.Get(); }

	/** Returns true if the widget is currently locked on */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	bool IsTargetLocked() const { return bIsTargetLocked; }

	/** Returns true if the widget is currently visible or fading */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	bool IsCombatVisible() const { return !bIsDead && (bAlwaysVisible || bIsTargetLocked || (CombatVisibilityTimer > 0.0f)); }

	/** Returns the Health bar sub-widget */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	UThreadTalesStatBarWidget* GetHealthBar() const { return HealthBar; }

	/** Returns the Poise / Posture bar sub-widget */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	UThreadTalesStatBarWidget* GetPoiseBar() const { return PoiseBar; }

	/** Returns current health percentage [0..1] */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	float GetHealthPercent() const;

	/** Returns current poise / posture percentage [0..1] */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	float GetPoisePercent() const;

	/** Returns current health value */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	float GetHealth() const;

	/** Returns maximum health value */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	float GetMaxHealth() const;

	/** Returns current poise value */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	float GetPoise() const;

	/** Returns maximum poise value */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	float GetMaxPoise() const;

	/** Designer presentation hook: Called when bound character health changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|UI|Events", meta = (DisplayName = "OnHealthChanged"))
	void OnHealthChanged(float NewHealth, float MaxHealth, float HealthPercent);

	/** Designer presentation hook: Called when bound character poise/posture changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|UI|Events", meta = (DisplayName = "OnPoiseChanged"))
	void OnPoiseChanged(float NewPoise, float MaxPoise, float PoisePercent);

	/** Designer presentation hook: Called when combat visibility state changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|UI|Events", meta = (DisplayName = "OnCombatVisibilityChanged"))
	void OnCombatVisibilityChanged(bool bIsVisible);

	/** Designer presentation hook: Called when bound enemy receives damage */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|UI|Events", meta = (DisplayName = "OnDamageReceived"))
	void OnDamageReceived(float DamageAmount, float CurrentHealth, float MaxHealth);

	/** Designer presentation hook: Called when bound enemy posture is broken (staggered) */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|UI|Events", meta = (DisplayName = "OnPostureBroken"))
	void OnPostureBroken(AActor* AttackerActor);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Character Attribute Callbacks
	UFUNCTION()
	void HandleHealthChanged(float NewHealth, float MaxHealth);

	UFUNCTION()
	void HandlePoiseChanged(float NewPoise, float MaxPoise);

	UFUNCTION()
	void HandleStaggered(AActor* AttackerActor);

	UFUNCTION()
	void HandleCharacterDeath(AActor* DeadActor, AActor* KillerActor);

	void UnbindCharacterCallbacks();
	void UpdateVisibilityState(float DeltaTime);

protected:
	/** Health bar displaying foreground damage and trailing ghost damage */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "ThreadTales|UI")
	TObjectPtr<UThreadTalesStatBarWidget> HealthBar;

	/** Poise / Posture bar displaying Sekiro-style tension accumulation */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "ThreadTales|UI")
	TObjectPtr<UThreadTalesStatBarWidget> PoiseBar;

	/** Optional enemy name or archetype title text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "ThreadTales|UI")
	TObjectPtr<UTextBlock> EnemyNameText;

public:
	/** Duration in seconds the widget remains visible after taking damage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Timing", meta = (ClampMin = "0.5"))
	float CombatVisibilityDuration;

	/** Duration in seconds over which the widget smoothly fades out when exiting combat */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Timing", meta = (ClampMin = "0.1"))
	float FadeOutDuration;

	/** Optional display name for the enemy */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Style")
	FText EnemyDisplayName;

	/** If true, the overhead bar stays visible permanently (for debugging / boss encounters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Visibility")
	bool bAlwaysVisible;

	/** If true, the bar starts visible on spawn and then fades out after CombatVisibilityDuration if no combat occurs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Visibility")
	bool bStartVisible;

private:
	TWeakObjectPtr<AThreadTalesBaseCharacter> BoundCharacter;

	bool bIsTargetLocked;
	bool bIsDead;
	bool bWasStaggered;
	float CombatVisibilityTimer;
	float CurrentOpacity;
	bool bLastVisibilityState;
	float PreviousHealthValue;
};
