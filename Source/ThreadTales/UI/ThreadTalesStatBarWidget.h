// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ThreadTalesStatBarWidget.generated.h"

class UProgressBar;
class UImage;
class UTextBlock;
class USizeBox;
class UBorder;

/**
 * Souls-like Stat Bar Widget supporting instant main bar updates,
 * delayed ghost-damage catch-up interpolation, and critical state flashing.
 */
UCLASS(BlueprintType, Blueprintable)
class THREADTALES_API UThreadTalesStatBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UThreadTalesStatBarWidget(const FObjectInitializer& ObjectInitializer);

	/** Configures visual style, dimensions, and colors of the bar */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI")
	void ConfigureBarStyle(
		FLinearColor InMainColor,
		FLinearColor InGhostColor,
		FLinearColor InBackgroundColor,
		float InWidth = 300.0f,
		float InHeight = 18.0f,
		bool bInShowNumericText = false);

	/** Updates the bar with new current and maximum values */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI")
	void SetStatValue(float NewValue, float NewMaxValue, bool bInstant = false);

	/** Triggers a temporary highlight/warning flash on the bar (e.g., on poise break or low stamina) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI")
	void TriggerFlash(FLinearColor FlashColor, float Duration = 0.45f);

	/** Enables or disables continuous pulsing flash (e.g., while staggered) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI")
	void SetContinuousFlash(bool bEnable, FLinearColor FlashColor = FLinearColor(1.0f, 0.2f, 0.2f, 0.65f));

	/** When true (Sekiro Posture mode), bar fills from 0% (full Poise=100) to 100% (broken Poise=0) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UI")
	void SetInvertFillPercent(bool bInvert);

	/** Returns the current target percentage [0..1] */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	float GetTargetPercent() const { return TargetPercent; }

	/** Returns the current ghost percentage [0..1] */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UI")
	float GetGhostPercent() const { return GhostPercent; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void SynchronizeProperties() override;

	void ApplyStyleToWidgets();
	void UpdateNumericText();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|UI")
	TObjectPtr<USizeBox> BarSizeBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|UI")
	TObjectPtr<UBorder> OuterBorder;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|UI")
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|UI")
	TObjectPtr<UProgressBar> GhostProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|UI")
	TObjectPtr<UProgressBar> MainProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|UI")
	TObjectPtr<UImage> FlashOverlayImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "ThreadTales|UI")
	TObjectPtr<UTextBlock> ValueText;

	/** Internal fallback colors & dimensions used strictly for C++ procedural widget construction */
	FLinearColor BackgroundBarColor;
	FLinearColor BorderColor;
	bool bApplyProceduralStyle;

public:
	/** Width override in pixels for this bar instance in UMG (0 = auto / layout determined) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Bar|Dimensions",
		meta = (ClampMin = "0.0", ToolTip = "Ширина полоски в пикселях. 0 = автоматический размер контейнера."))
	float BarWidth;

	/** Height override in pixels for this bar instance in UMG (0 = auto / layout determined) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Bar|Dimensions",
		meta = (ClampMin = "0.0", ToolTip = "Высота полоски в пикселях. 0 = автоматический размер контейнера."))
	float BarHeight;

	/** Primary fill color of the foreground bar (e.g. Crimson Red for HP, Amber Gold for Poise) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Bar|Colors",
		meta = (ToolTip = "Основной цвет полоски (например, красный для HP, янтарно-золотой для стойки Секиро)."))
	FLinearColor MainBarColor;

	/** Color of the trailing ghost damage catch-up bar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Bar|Colors",
		meta = (ToolTip = "Цвет полоски-призрака запаздывающего урона."))
	FLinearColor GhostBarColor;

	/** Hold time in seconds before the ghost bar begins shrinking toward the main bar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Timing", meta = (ClampMin = "0.0"))
	float GhostHoldDelay;

	/** Interpolation speed for the ghost bar catch-up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Timing", meta = (ClampMin = "0.5"))
	float GhostInterpSpeed;

	/** Whether to display numeric "Current / Max" text on the bar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Style")
	bool bShowNumericText;

	/** When true, bar fills from 0% (at MaxValue) up to 100% (at 0 Value), used for Sekiro Posture tension */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Bar|Style",
		meta = (ToolTip = "Инвертированный режим накопления (0% при максимуме, 100% при нуле). Используется для шкалы стойки Секиро (Posture)."))
	bool bInvertFillPercent;



private:
	float CurrentValue;
	float MaxValue;
	float TargetPercent;
	float GhostPercent;
	float GhostDelayRemaining;

	bool bIsFlashing;
	bool bContinuousFlash;
	float FlashTimeRemaining;
	float FlashTotalDuration;
	float FlashPulsePhase;
	FLinearColor CurrentFlashColor;
};
