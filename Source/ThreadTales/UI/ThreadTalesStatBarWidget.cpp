// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ThreadTalesStatBarWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"

UThreadTalesStatBarWidget::UThreadTalesStatBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MainBarColor = FLinearColor(0.72f, 0.08f, 0.08f, 1.0f);       // Crimson Red
	GhostBarColor = FLinearColor(1.0f, 0.65f, 0.15f, 0.95f);      // Warm Amber
	BackgroundBarColor = FLinearColor(0.04f, 0.04f, 0.06f, 0.90f); // Deep Charcoal
	BorderColor = FLinearColor(0.28f, 0.28f, 0.32f, 0.85f);        // Metallic Trim

	BarWidth = 140.0f;
	BarHeight = 10.0f;

	GhostHoldDelay = 0.45f;
	GhostInterpSpeed = 2.2f;
	bShowNumericText = false;
	bInvertFillPercent = false;
	bApplyProceduralStyle = false;

	CurrentValue = 100.0f;
	MaxValue = 100.0f;
	TargetPercent = 1.0f;
	GhostPercent = 1.0f;
	GhostDelayRemaining = 0.0f;

	bIsFlashing = false;
	bContinuousFlash = false;
	FlashTimeRemaining = 0.0f;
	FlashTotalDuration = 0.45f;
	FlashPulsePhase = 0.0f;
	CurrentFlashColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.0f);
}

void UThreadTalesStatBarWidget::SetInvertFillPercent(bool bInvert)
{
	bInvertFillPercent = bInvert;
	SetStatValue(CurrentValue, MaxValue, true);
}

TSharedRef<SWidget> UThreadTalesStatBarWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BarSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarSizeBox")); // [PROG-UI-FALLBACK-OK]
		WidgetTree->RootWidget = BarSizeBox;

		OuterBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OuterBorder")); // [PROG-UI-FALLBACK-OK]
		OuterBorder->SetPadding(FMargin(1.5f));
		BarSizeBox->AddChild(OuterBorder);

		UOverlay* BarOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("BarOverlay")); // [PROG-UI-FALLBACK-OK]
		OuterBorder->AddChild(BarOverlay);

		// Layer 0: Dark Background
		BackgroundImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BackgroundImage"));
		if (UOverlaySlot* BgSlot = BarOverlay->AddChildToOverlay(BackgroundImage))
		{
			BgSlot->SetHorizontalAlignment(HAlign_Fill);
			BgSlot->SetVerticalAlignment(VAlign_Fill);
		}

		// Layer 1: Ghost Catch-up Bar
		GhostProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("GhostProgressBar"));
		if (UOverlaySlot* GhostSlot = BarOverlay->AddChildToOverlay(GhostProgressBar))
		{
			GhostSlot->SetHorizontalAlignment(HAlign_Fill);
			GhostSlot->SetVerticalAlignment(VAlign_Fill);
		}

		// Layer 2: Main Foreground Bar
		MainProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("MainProgressBar"));
		if (UOverlaySlot* MainSlot = BarOverlay->AddChildToOverlay(MainProgressBar))
		{
			MainSlot->SetHorizontalAlignment(HAlign_Fill);
			MainSlot->SetVerticalAlignment(VAlign_Fill);
		}

		// Layer 3: Flash Overlay
		FlashOverlayImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FlashOverlayImage"));
		if (UOverlaySlot* FlashSlot = BarOverlay->AddChildToOverlay(FlashOverlayImage))
		{
			FlashSlot->SetHorizontalAlignment(HAlign_Fill);
			FlashSlot->SetVerticalAlignment(VAlign_Fill);
		}

		// Layer 4: Optional Value Text
		ValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ValueText"));
		if (UOverlaySlot* TextSlot = BarOverlay->AddChildToOverlay(ValueText))
		{
			TextSlot->SetHorizontalAlignment(HAlign_Right);
			TextSlot->SetVerticalAlignment(VAlign_Center);
			TextSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		}

		bApplyProceduralStyle = true;
		ApplyStyleToWidgets();
	}

	return Super::RebuildWidget();
}

void UThreadTalesStatBarWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyStyleToWidgets();
}

void UThreadTalesStatBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyStyleToWidgets();
	UpdateNumericText();
}

void UThreadTalesStatBarWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (BarSizeBox)
	{
		if (BarWidth > 0.0f)
		{
			BarSizeBox->SetWidthOverride(BarWidth);
		}
		else
		{
			BarSizeBox->ClearWidthOverride();
		}

		if (BarHeight > 0.0f)
		{
			BarSizeBox->SetHeightOverride(BarHeight);
		}
		else
		{
			BarSizeBox->ClearHeightOverride();
		}
	}

	if (MainProgressBar && MainBarColor.A > 0.0f)
	{
		MainProgressBar->SetFillColorAndOpacity(MainBarColor);
	}

	if (GhostProgressBar && GhostBarColor.A > 0.0f)
	{
		GhostProgressBar->SetFillColorAndOpacity(GhostBarColor);
	}
}

void UThreadTalesStatBarWidget::ConfigureBarStyle(
	FLinearColor InMainColor,
	FLinearColor InGhostColor,
	FLinearColor InBackgroundColor,
	float InWidth,
	float InHeight,
	bool bInShowNumericText)
{
	MainBarColor = InMainColor;
	GhostBarColor = InGhostColor;
	BackgroundBarColor = InBackgroundColor;
	BarWidth = InWidth;
	BarHeight = InHeight;
	bShowNumericText = bInShowNumericText;

	ApplyStyleToWidgets();
	UpdateNumericText();
}

void UThreadTalesStatBarWidget::ApplyStyleToWidgets()
{
	if (BarSizeBox)
	{
		if (BarWidth > 0.0f)
		{
			BarSizeBox->SetWidthOverride(BarWidth);
		}
		else
		{
			BarSizeBox->ClearWidthOverride();
		}

		if (BarHeight > 0.0f)
		{
			BarSizeBox->SetHeightOverride(BarHeight);
		}
		else
		{
			BarSizeBox->ClearHeightOverride();
		}
	}

	if (MainProgressBar && MainBarColor.A > 0.0f)
	{
		MainProgressBar->SetFillColorAndOpacity(MainBarColor);
	}

	if (GhostProgressBar && GhostBarColor.A > 0.0f)
	{
		GhostProgressBar->SetFillColorAndOpacity(GhostBarColor);
	}

	if (bApplyProceduralStyle)
	{
		if (OuterBorder)
		{
			OuterBorder->SetBrushColor(BorderColor);
		}

		if (BackgroundImage)
		{
			BackgroundImage->SetColorAndOpacity(BackgroundBarColor);
		}

		// Configure flat, transparent-background progress bar style so GhostProgressBar shines through behind MainProgressBar
		FProgressBarStyle CleanBarStyle;
		CleanBarStyle.BackgroundImage.DrawAs = ESlateBrushDrawType::NoDrawType;
		CleanBarStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor::Transparent);
		CleanBarStyle.FillImage.DrawAs = ESlateBrushDrawType::Box;
		CleanBarStyle.FillImage.TintColor = FSlateColor(FLinearColor::White);

		if (GhostProgressBar)
		{
			GhostProgressBar->SetWidgetStyle(CleanBarStyle);
			GhostProgressBar->SetFillColorAndOpacity(GhostBarColor);
		}

		if (MainProgressBar)
		{
			MainProgressBar->SetWidgetStyle(CleanBarStyle);
			MainProgressBar->SetFillColorAndOpacity(MainBarColor);
		}

		if (FlashOverlayImage)
		{
			FlashOverlayImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f));
		}

		if (ValueText)
		{
			ValueText->SetVisibility(bShowNumericText ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			FSlateFontInfo FontInfo = ValueText->GetFont();
			FontInfo.Size = FMath::Clamp(FMath::RoundToInt(BarHeight * 0.58f), 8, 14);
			ValueText->SetFont(FontInfo);
			ValueText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.95f, 0.95f, 0.92f)));
			ValueText->SetShadowOffset(FVector2D(1.0f, 1.0f));
			ValueText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
		}
	}

	// Always sync progress bar values regardless of procedural style mode
	if (GhostProgressBar)
	{
		GhostProgressBar->SetPercent(GhostPercent);
	}

	if (MainProgressBar)
	{
		MainProgressBar->SetPercent(TargetPercent);
	}
}

void UThreadTalesStatBarWidget::SetStatValue(float NewValue, float NewMaxValue, bool bInstant)
{
	CurrentValue = FMath::Max(0.0f, NewValue);
	MaxValue = FMath::Max(1.0f, NewMaxValue);
	const float RawRatio = FMath::Clamp(CurrentValue / MaxValue, 0.0f, 1.0f);
	const float NewPercent = bInvertFillPercent ? (1.0f - RawRatio) : RawRatio;

	if (bInstant)
	{
		TargetPercent = NewPercent;
		GhostPercent = NewPercent;
		GhostDelayRemaining = 0.0f;
	}
	else
	{
		if (NewPercent < TargetPercent)
		{
			// Bar fill decreased (e.g. HP/Stamina consumed, or inverted Posture cooling down):
			// If ghost was already equal to target, reset the hold delay
			if (GhostPercent <= TargetPercent + KINDA_SMALL_NUMBER)
			{
				GhostPercent = TargetPercent;
				GhostDelayRemaining = GhostHoldDelay;
			}
			else
			{
				GhostDelayRemaining = FMath::Max(GhostDelayRemaining, GhostHoldDelay * 0.6f);
			}
			TargetPercent = NewPercent;
		}
		else
		{
			// Bar fill increased (e.g. HP/Stamina regen, or inverted Posture building up on block/hit):
			// Both bars advance together
			TargetPercent = NewPercent;
			if (GhostPercent < TargetPercent)
			{
				GhostPercent = TargetPercent;
				GhostDelayRemaining = 0.0f;
			}
		}
	}

	if (MainProgressBar)
	{
		MainProgressBar->SetPercent(TargetPercent);
	}
	if (GhostProgressBar)
	{
		GhostProgressBar->SetPercent(GhostPercent);
	}

	UpdateNumericText();
}

void UThreadTalesStatBarWidget::UpdateNumericText()
{
	if (ValueText && bShowNumericText)
	{
		const float DisplayedValue = bInvertFillPercent ? FMath::Max(0.0f, MaxValue - CurrentValue) : CurrentValue;
		ValueText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), DisplayedValue, MaxValue)));
	}
}

void UThreadTalesStatBarWidget::TriggerFlash(FLinearColor FlashColor, float Duration)
{
	bIsFlashing = true;
	CurrentFlashColor = FlashColor;
	FlashTotalDuration = FMath::Max(0.05f, Duration);
	FlashTimeRemaining = FlashTotalDuration;
}

void UThreadTalesStatBarWidget::SetContinuousFlash(bool bEnable, FLinearColor FlashColor)
{
	bContinuousFlash = bEnable;
	if (bEnable)
	{
		CurrentFlashColor = FlashColor;
		FlashPulsePhase = 0.0f;
	}
	else if (!bIsFlashing && FlashOverlayImage)
	{
		FlashOverlayImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f));
	}
}

void UThreadTalesStatBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 1. Update Ghost Catch-up Interpolation
	if (GhostPercent > TargetPercent)
	{
		if (GhostDelayRemaining > 0.0f)
		{
			GhostDelayRemaining -= InDeltaTime;
		}
		else
		{
			GhostPercent = FMath::FInterpConstantTo(GhostPercent, TargetPercent, InDeltaTime, GhostInterpSpeed * 0.35f);
			GhostPercent = FMath::FInterpTo(GhostPercent, TargetPercent, InDeltaTime, GhostInterpSpeed * 2.5f);
			if (FMath::Abs(GhostPercent - TargetPercent) < 0.003f)
			{
				GhostPercent = TargetPercent;
			}

			if (GhostProgressBar)
			{
				GhostProgressBar->SetPercent(GhostPercent);
			}
		}
	}

	// 2. Update Flash Overlay
	if (FlashOverlayImage)
	{
		if (bContinuousFlash)
		{
			FlashPulsePhase += InDeltaTime * 8.0f;
			const float PulseAlpha = (FMath::Sin(FlashPulsePhase) * 0.5f + 0.5f) * CurrentFlashColor.A;
			FLinearColor OutColor = CurrentFlashColor;
			OutColor.A = PulseAlpha;
			FlashOverlayImage->SetColorAndOpacity(OutColor);
		}
		else if (bIsFlashing)
		{
			FlashTimeRemaining -= InDeltaTime;
			if (FlashTimeRemaining <= 0.0f)
			{
				bIsFlashing = false;
				FlashOverlayImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f));
			}
			else
			{
				const float NormalizedAlpha = FMath::Clamp(FlashTimeRemaining / FlashTotalDuration, 0.0f, 1.0f);
				FLinearColor OutColor = CurrentFlashColor;
				OutColor.A = CurrentFlashColor.A * NormalizedAlpha;
				FlashOverlayImage->SetColorAndOpacity(OutColor);
			}
		}
	}
}
