// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ThreadTalesBowReticleWidget.generated.h"

class UCanvasPanel;
class USizeBox;
class UImage;
class UTextBlock;
class AThreadTalesWeapon_Bow;
class AThreadTalesCharacter;

/**
 * Dynamic Archery Crosshair & Reticle UMG Widget for ThreadTales.
 * Implements pure C++ Controller / UMG View architecture with BindWidgetOptional.
 * Displays:
 * - Central pinpoint dot / crosshair
 * - Dynamic charge brackets (Left/Right, Top/Bottom) converging as string tension charges (0% -> 100%)
 * - 4-tier visual charge stage feedback (LV1 = White, LV2 = Amber Gold, LV3 = Crimson Pulse, LV4 = Radiant Violet / Overcharge)
 * - Directional Hit Marker (X-cross) flashing upon confirmed enemy strike (white for normal, gold/red for critical/high tier)
 */
UCLASS(BlueprintType, Blueprintable)
class THREADTALES_API UThreadTalesBowReticleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UThreadTalesBowReticleWidget(const FObjectInitializer& ObjectInitializer);

	/** Sets or updates the active bow weapon and character reference */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI|Reticle")
	void InitializeForBow(AThreadTalesWeapon_Bow* InBow, AThreadTalesCharacter* InCharacter);

	/** Updates the dynamic draw ratio (0.0 to 1.0) and charge tier (1 to 4) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI|Reticle")
	void UpdateChargeState(float InDrawRatio, int32 InChargeTier);

	/** Triggers an animated hit marker flash upon striking an enemy */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI|Reticle")
	void TriggerHitMarker(bool bCriticalOrHighTier = false);

	/** Shows or hides reticle with smooth fading or instant visibility */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI|Reticle")
	void SetReticleVisible(bool bVisible, bool bAnimate = true);

	/** Returns whether reticle is currently visible */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI|Reticle")
	bool IsReticleVisible() const { return bIsReticleVisible; }

	/** Current charge tier (1 = LV1, 2 = LV2, 3 = LV3, 4 = LV4) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI|Reticle")
	int32 GetCurrentChargeTier() const { return CurrentChargeTier; }

	/** Current draw ratio (0.0 to 1.0) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI|Reticle")
	float GetCurrentDrawRatio() const { return CurrentDrawRatio; }

	// ==========================================
	// DESIGNER COSMETIC HOOKS (AAA 3-Tier Standard)
	// ==========================================

	/** Blueprint implementable event fired when charge tier changes (for sound effects, particles, screen shakes) */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|UI|Reticle|Events",
		meta = (ToolTip = "Вызывается при переходе на новый уровень натяжения (LV1-LV4). Используется для звуков, частиц или анимаций."))
	void OnChargeTierChanged(int32 NewTier, int32 PreviousTier);

	/** Blueprint implementable event fired on hit marker trigger */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|UI|Reticle|Events",
		meta = (ToolTip = "Вызывается при попадании стрелы во врага. Используется для UMG-анимации хитмаркера."))
	void OnHitMarkerTriggered(bool bCritical);

	/** Blueprint implementable event fired when reticle visibility state changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|UI|Reticle|Events",
		meta = (ToolTip = "Вызывается при включении или выключении режима прицеливания."))
	void OnReticleVisibilityChanged(bool bNewVisible);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void UpdateBracketPositions(float InDrawRatio);
	void UpdateTierVisuals(int32 InChargeTier);
	void UpdateHitMarker(float InDeltaTime);
	void UpdateReticleFading(float InDeltaTime);

protected:
	// ==========================================
	// UMG WIDGET BINDINGS (BindWidgetOptional)
	// ==========================================

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|UI|Reticle")
	TObjectPtr<UCanvasPanel> ReticleRootCanvas;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|UI|Reticle")
	TObjectPtr<UImage> CenterDotImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|UI|Reticle")
	TObjectPtr<UImage> BracketLeftImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|UI|Reticle")
	TObjectPtr<UImage> BracketRightImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|UI|Reticle")
	TObjectPtr<UImage> BracketTopImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|UI|Reticle")
	TObjectPtr<UImage> BracketBottomImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|UI|Reticle")
	TObjectPtr<UTextBlock> TierIndicatorText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|UI|Reticle")
	TObjectPtr<UImage> HitMarkerImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|UI|Reticle")
	TObjectPtr<UImage> HitMarkerLine1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|UI|Reticle")
	TObjectPtr<UImage> HitMarkerLine2;

public:
	// ==========================================
	// CONFIGURATION & STYLING (Editable Defaults)
	// ==========================================

	/** Spread distance of horizontal brackets at 0% draw (rest) in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Reticle|Brackets", meta = (ClampMin = "10.0", ClampMax = "200.0"))
	float MaxBracketSpread = 72.0f;

	/** Spread distance of horizontal brackets at 100% draw (fully converged) in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Reticle|Brackets", meta = (ClampMin = "2.0", ClampMax = "50.0"))
	float MinBracketSpread = 16.0f;

	/** Spread distance of vertical brackets at 0% draw (rest) in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Reticle|Brackets", meta = (ClampMin = "10.0", ClampMax = "200.0"))
	float MaxVerticalSpread = 48.0f;

	/** Spread distance of vertical brackets at 100% draw in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Reticle|Brackets", meta = (ClampMin = "2.0", ClampMax = "50.0"))
	float MinVerticalSpread = 12.0f;

	/** Color for Level 1 charge (Quick Shot) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Reticle|Colors")
	FLinearColor ColorTier1 = FLinearColor(0.92f, 0.92f, 0.95f, 0.85f); // Soft White

	/** Color for Level 2 charge (Full Draw) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Reticle|Colors")
	FLinearColor ColorTier2 = FLinearColor(1.0f, 0.78f, 0.18f, 0.95f); // Amber Gold

	/** Color for Level 3 charge (Piercing Overcharge / Sweet-Spot) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Reticle|Colors")
	FLinearColor ColorTier3 = FLinearColor(1.0f, 0.22f, 0.15f, 1.0f); // Crimson Red

	/** Color for Level 4 charge (Radiant / Max Tension) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Reticle|Colors")
	FLinearColor ColorTier4 = FLinearColor(0.85f, 0.35f, 1.0f, 1.0f); // Radiant Violet

	/** Hit marker flash duration in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Reticle|HitMarker", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float HitMarkerDuration = 0.25f;

	/** Whether to apply procedural C++ shapes and layout fallback if no UMG blueprint hierarchy exists */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Reticle|Style")
	bool bApplyProceduralStyle = true;

	/** Whether 2D screen crosshair is disabled in favor of 3D physical trajectory arc */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Reticle|Style", meta = (ToolTip = "Disables 2D screen reticle in favor of 3D physical trajectory arc."))
	bool bDisableScreen2DReticle = true;

private:
	TWeakObjectPtr<AThreadTalesWeapon_Bow> BoundBow;
	TWeakObjectPtr<AThreadTalesCharacter> BoundCharacter;

	float CurrentDrawRatio = 0.0f;
	int32 CurrentChargeTier = 1;
	bool bIsReticleVisible = false;

	float CurrentOpacity = 0.0f;
	float TargetOpacity = 0.0f;
	float FadeSpeed = 8.0f;

	float HitMarkerRemainingTime = 0.0f;
	bool bHitMarkerCritical = false;

	float PulsePhase = 0.0f;
};
