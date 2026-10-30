// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ThreadTalesHUDWidget.generated.h"

class UThreadTalesStatBarWidget;
class UThreadTalesBowReticleWidget;
class AThreadTalesCharacter;
class UVerticalBox;
class UTextBlock;
class UCanvasPanel;

/**
 * Primary Combat HUD Widget for ThreadTales.
 * Displays Souls-like Health, Stamina, and Poise bars in the top-left corner
 * along with a compact 2P Co-op Partner frame.
 */
UCLASS(BlueprintType, Blueprintable)
class THREADTALES_API UThreadTalesHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UThreadTalesHUDWidget(const FObjectInitializer& ObjectInitializer);

	/** Binds this HUD to the local player character and initializes stat bars */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI")
	void InitializeForCharacter(AThreadTalesCharacter* InCharacter);

	/** Binds the compact co-op frame to the teammate character */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI")
	void SetPartnerCharacter(AThreadTalesCharacter* InPartner);

	/** Returns the currently bound local player character */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	AThreadTalesCharacter* GetBoundCharacter() const { return BoundCharacter.Get(); }

	/** Returns the currently bound co-op partner character */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	AThreadTalesCharacter* GetBoundPartner() const { return BoundPartner.Get(); }

	/** Returns the local player's Stamina bar widget */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	UThreadTalesStatBarWidget* GetPlayerStaminaBar() const { return PlayerStaminaBar; }

	/** Returns the local player's Bow Reticle widget */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	UThreadTalesBowReticleWidget* GetBowReticleWidget() const { return BowReticleWidget; }

	/** Designer presentation hook: Called when Sekiro Posture breaks into staggered vulnerability or recovers */
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat HUD|Events",
		meta = (ToolTip = "Вызывается при изменении состояния ошеломления (пробитии стойки). Используйте для запуска UMG-анимации тряски экрана, виньетки или вспышки."))
	void OnStaggerStateChanged(bool bIsStaggered);

	/** Designer presentation hook: Called when player health enters or exits critical threshold (<= 25%) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat HUD|Events",
		meta = (ToolTip = "Вызывается при падении здоровья ниже 25% (или восстановлении выше). Используйте для пульсирующей красной рамки."))
	void OnCriticalHealthWarning(bool bIsCritical);

	/** Designer presentation hook: Called when a 2P co-op partner is discovered or lost */
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat HUD|Events",
		meta = (ToolTip = "Вызывается при обнаружении или смене напарника в 2P Co-op."))
	void OnPartnerAssigned(AThreadTalesCharacter* PartnerCharacter);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Local Player Callbacks
	UFUNCTION()
	void HandlePlayerHealthChanged(float NewHealth, float MaxHealth);

	UFUNCTION()
	void HandlePlayerStaminaChanged(float NewStamina, float MaxStamina);

	UFUNCTION()
	void HandlePlayerPoiseChanged(float NewPoise, float MaxPoise);

	UFUNCTION()
	void HandlePlayerStaggered(AActor* AttackerActor);

	UFUNCTION()
	void HandlePlayerDeath(AActor* DeadActor, AActor* KillerActor);

	// Co-op Partner Callbacks
	UFUNCTION()
	void HandlePartnerHealthChanged(float NewHealth, float MaxHealth);

	UFUNCTION()
	void HandlePartnerStaminaChanged(float NewStamina, float MaxStamina);

	UFUNCTION()
	void HandlePartnerDeath(AActor* DeadActor, AActor* KillerActor);

	void UnbindPlayerCallbacks();
	void UnbindPartnerCallbacks();
	void TryAutoDiscoverPartner();

	void BuildPlayerCluster(class UWidgetTree* InTree);
	void BuildPartnerCluster(class UWidgetTree* InTree);

public:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|UI")
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "ThreadTales|UI")
	TObjectPtr<UVerticalBox> PlayerClusterBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "ThreadTales|UI")
	TObjectPtr<UThreadTalesStatBarWidget> PlayerHealthBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "ThreadTales|UI")
	TObjectPtr<UThreadTalesStatBarWidget> PlayerStaminaBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "ThreadTales|UI")
	TObjectPtr<UThreadTalesStatBarWidget> PlayerPoiseBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "ThreadTales|UI")
	TObjectPtr<UTextBlock> StaggerStatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "ThreadTales|UI")
	TObjectPtr<UVerticalBox> PartnerClusterBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "ThreadTales|UI")
	TObjectPtr<UTextBlock> PartnerNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "ThreadTales|UI")
	TObjectPtr<UThreadTalesStatBarWidget> PartnerHealthBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "ThreadTales|UI")
	TObjectPtr<UThreadTalesStatBarWidget> PartnerStaminaBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "ThreadTales|UI")
	TObjectPtr<UThreadTalesBowReticleWidget> BowReticleWidget;


private:
	TWeakObjectPtr<AThreadTalesCharacter> BoundCharacter;
	TWeakObjectPtr<AThreadTalesCharacter> BoundPartner;

	float PartnerDiscoveryTimer;
	float StaggerBannerRemainingTime;
};
