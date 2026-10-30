// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ThreadTalesBowReticleWidget.h"
#include "Combat/ThreadTalesWeapon_Bow.h"
#include "Data/ThreadTalesBowConfig.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

UThreadTalesBowReticleWidget::UThreadTalesBowReticleWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MaxBracketSpread = 72.0f;
	MinBracketSpread = 16.0f;
	MaxVerticalSpread = 48.0f;
	MinVerticalSpread = 12.0f;

	ColorTier1 = FLinearColor(0.92f, 0.92f, 0.95f, 0.85f); // Soft White
	ColorTier2 = FLinearColor(1.0f, 0.78f, 0.18f, 0.95f);  // Amber Gold
	ColorTier3 = FLinearColor(1.0f, 0.22f, 0.15f, 1.0f);   // Crimson Red
	ColorTier4 = FLinearColor(0.85f, 0.35f, 1.0f, 1.0f);   // Radiant Violet

	HitMarkerDuration = 0.25f;
	bApplyProceduralStyle = true;
	bDisableScreen2DReticle = true;

	CurrentDrawRatio = 0.0f;
	CurrentChargeTier = 1;
	bIsReticleVisible = false;

	CurrentOpacity = 0.0f;
	TargetOpacity = 0.0f;
	FadeSpeed = 10.0f;

	HitMarkerRemainingTime = 0.0f;
	bHitMarkerCritical = false;
	PulsePhase = 0.0f;
}

TSharedRef<SWidget> UThreadTalesBowReticleWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		ReticleRootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ReticleRootCanvas")); // [PROG-UI-FALLBACK-OK]
		WidgetTree->RootWidget = ReticleRootCanvas;

		// 1. Center Pinpoint Dot (6x6 px)
		CenterDotImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CenterDotImage"));
		if (UCanvasPanelSlot* DotSlot = ReticleRootCanvas->AddChildToCanvas(CenterDotImage))
		{
			DotSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			DotSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			DotSlot->SetSize(FVector2D(6.0f, 6.0f));
			DotSlot->SetPosition(FVector2D::ZeroVector);
		}

		// 2. Left Bracket (Vertical Bar 3x16 px)
		BracketLeftImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BracketLeftImage"));
		if (UCanvasPanelSlot* LeftSlot = ReticleRootCanvas->AddChildToCanvas(BracketLeftImage))
		{
			LeftSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			LeftSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			LeftSlot->SetSize(FVector2D(3.0f, 16.0f));
			LeftSlot->SetPosition(FVector2D(-MaxBracketSpread, 0.0f));
		}

		// 3. Right Bracket (Vertical Bar 3x16 px)
		BracketRightImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BracketRightImage"));
		if (UCanvasPanelSlot* RightSlot = ReticleRootCanvas->AddChildToCanvas(BracketRightImage))
		{
			RightSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			RightSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			RightSlot->SetSize(FVector2D(3.0f, 16.0f));
			RightSlot->SetPosition(FVector2D(MaxBracketSpread, 0.0f));
		}

		// 4. Top Bracket (Horizontal Bar 16x3 px)
		BracketTopImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BracketTopImage"));
		if (UCanvasPanelSlot* TopSlot = ReticleRootCanvas->AddChildToCanvas(BracketTopImage))
		{
			TopSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			TopSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			TopSlot->SetSize(FVector2D(16.0f, 3.0f));
			TopSlot->SetPosition(FVector2D(0.0f, -MaxVerticalSpread));
		}

		// 5. Bottom Bracket (Horizontal Bar 16x3 px)
		BracketBottomImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BracketBottomImage"));
		if (UCanvasPanelSlot* BottomSlot = ReticleRootCanvas->AddChildToCanvas(BracketBottomImage))
		{
			BottomSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			BottomSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			BottomSlot->SetSize(FVector2D(16.0f, 3.0f));
			BottomSlot->SetPosition(FVector2D(0.0f, MaxVerticalSpread));
		}

		// 6. Tier Indicator Text ("LV1", "LV2", "LV3", "LV4")
		TierIndicatorText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TierIndicatorText"));
		TierIndicatorText->SetText(FText::FromString(TEXT("LV1")));
		FSlateFontInfo FontInfo = TierIndicatorText->GetFont();
		FontInfo.Size = 9;
		TierIndicatorText->SetFont(FontInfo);
		TierIndicatorText->SetColorAndOpacity(FSlateColor(ColorTier1));
		TierIndicatorText->SetShadowOffset(FVector2D(1.0f, 1.0f));
		TierIndicatorText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
		if (UCanvasPanelSlot* TextSlot = ReticleRootCanvas->AddChildToCanvas(TierIndicatorText))
		{
			TextSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			TextSlot->SetAlignment(FVector2D(0.5f, 0.0f));
			TextSlot->SetPosition(FVector2D(0.0f, MaxVerticalSpread + 8.0f));
			TextSlot->SetAutoSize(true);
		}

		// 7. Hit Marker - Stylized diagonal cross lines (18x2 px each, rotated +/-45 deg)
		HitMarkerLine1 = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("HitMarkerLine1"));
		HitMarkerLine1->SetVisibility(ESlateVisibility::Collapsed);
		HitMarkerLine1->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		HitMarkerLine1->SetRenderTransformAngle(45.0f);
		if (UCanvasPanelSlot* HitSlot1 = ReticleRootCanvas->AddChildToCanvas(HitMarkerLine1))
		{
			HitSlot1->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			HitSlot1->SetAlignment(FVector2D(0.5f, 0.5f));
			HitSlot1->SetSize(FVector2D(18.0f, 2.5f));
			HitSlot1->SetPosition(FVector2D::ZeroVector);
		}

		HitMarkerLine2 = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("HitMarkerLine2"));
		HitMarkerLine2->SetVisibility(ESlateVisibility::Collapsed);
		HitMarkerLine2->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		HitMarkerLine2->SetRenderTransformAngle(-45.0f);
		if (UCanvasPanelSlot* HitSlot2 = ReticleRootCanvas->AddChildToCanvas(HitMarkerLine2))
		{
			HitSlot2->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			HitSlot2->SetAlignment(FVector2D(0.5f, 0.5f));
			HitSlot2->SetSize(FVector2D(18.0f, 2.5f));
			HitSlot2->SetPosition(FVector2D::ZeroVector);
		}
	}

	return Super::RebuildWidget();
}

void UThreadTalesBowReticleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetRenderOpacity(0.0f);
	CurrentOpacity = 0.0f;
	TargetOpacity = 0.0f;
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (bDisableScreen2DReticle)
	{
		if (CenterDotImage) { CenterDotImage->SetVisibility(ESlateVisibility::Collapsed); }
		if (BracketLeftImage) { BracketLeftImage->SetVisibility(ESlateVisibility::Collapsed); }
		if (BracketRightImage) { BracketRightImage->SetVisibility(ESlateVisibility::Collapsed); }
		if (BracketTopImage) { BracketTopImage->SetVisibility(ESlateVisibility::Collapsed); }
		if (BracketBottomImage) { BracketBottomImage->SetVisibility(ESlateVisibility::Collapsed); }
		if (TierIndicatorText) { TierIndicatorText->SetVisibility(ESlateVisibility::Collapsed); }
	}

	UpdateBracketPositions(0.0f);
	UpdateTierVisuals(1);
}

void UThreadTalesBowReticleWidget::NativeDestruct()
{
	BoundBow.Reset();
	BoundCharacter.Reset();
	Super::NativeDestruct();
}

void UThreadTalesBowReticleWidget::InitializeForBow(AThreadTalesWeapon_Bow* InBow, AThreadTalesCharacter* InCharacter)
{
	BoundBow = InBow;
	BoundCharacter = InCharacter;
}

void UThreadTalesBowReticleWidget::SetReticleVisible(bool bVisible, bool bAnimate)
{
	const bool bEffectiveVisible = bVisible && !bDisableScreen2DReticle;
	if (bIsReticleVisible == bEffectiveVisible)
	{
		return;
	}

	bIsReticleVisible = bEffectiveVisible;
	TargetOpacity = bEffectiveVisible ? 1.0f : 0.0f;

	if (!bAnimate)
	{
		CurrentOpacity = TargetOpacity;
		SetRenderOpacity(CurrentOpacity);
	}

	OnReticleVisibilityChanged(bEffectiveVisible);
}


void UThreadTalesBowReticleWidget::UpdateChargeState(float InDrawRatio, int32 InChargeTier)
{
	const float ClampedRatio = FMath::Clamp(InDrawRatio, 0.0f, 1.0f);
	const int32 ClampedTier = FMath::Clamp(InChargeTier, 1, 4);

	if (ClampedTier != CurrentChargeTier)
	{
		const int32 PrevTier = CurrentChargeTier;
		CurrentChargeTier = ClampedTier;
		UpdateTierVisuals(CurrentChargeTier);
		OnChargeTierChanged(CurrentChargeTier, PrevTier);
	}

	CurrentDrawRatio = ClampedRatio;
	UpdateBracketPositions(CurrentDrawRatio);
}

void UThreadTalesBowReticleWidget::TriggerHitMarker(bool bCriticalOrHighTier)
{
	bHitMarkerCritical = bCriticalOrHighTier;
	HitMarkerRemainingTime = HitMarkerDuration;

	const FLinearColor HitColor = bHitMarkerCritical
		? FLinearColor(1.0f, 0.2f, 0.15f, 1.0f)
		: FLinearColor(1.0f, 1.0f, 1.0f, 0.95f);

	if (HitMarkerImage)
	{
		HitMarkerImage->SetColorAndOpacity(HitColor);
		HitMarkerImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (HitMarkerLine1)
	{
		HitMarkerLine1->SetColorAndOpacity(HitColor);
		HitMarkerLine1->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (HitMarkerLine2)
	{
		HitMarkerLine2->SetColorAndOpacity(HitColor);
		HitMarkerLine2->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (bDisableScreen2DReticle)
	{
		SetRenderOpacity(1.0f);
	}

	OnHitMarkerTriggered(bCriticalOrHighTier);
}

void UThreadTalesBowReticleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 1. Smooth Fade Transition
	UpdateReticleFading(InDeltaTime);

	// 2. Poll Bow tension and charge tier synchronized with BowConfig & hold duration
	if (const AThreadTalesWeapon_Bow* Bow = BoundBow.Get())
	{
		const float DrawRatio = Bow->GetDrawRatio();
		int32 EvaluatedTier = 1;
		const UThreadTalesBowConfig* BowConfig = Bow->GetBowConfig();

		if (BowConfig)
		{
			const float HoldElapsed = Bow->GetDrawElapsed();
			EvaluatedTier = BowConfig->EvaluateChargeTier(HoldElapsed);
			if (HoldElapsed >= (BowConfig->LV3ChargeTimeSeconds * 1.5f))
			{
				EvaluatedTier = 4; // Radiant Overcharge threshold
			}
		}
		else
		{
			if (DrawRatio >= 0.95f) { EvaluatedTier = 3; }
			else if (DrawRatio >= 0.45f) { EvaluatedTier = 2; }
		}

		UpdateChargeState(DrawRatio, EvaluatedTier);
	}

	// 3. Sweet-spot Pulse at LV3/LV4
	if (CurrentChargeTier >= 3 && CurrentOpacity > 0.01f)
	{
		PulsePhase += InDeltaTime * 12.0f;
		const float PulseAlpha = 0.75f + 0.25f * FMath::Sin(PulsePhase);
		const FLinearColor BaseColor = (CurrentChargeTier == 4) ? ColorTier4 : ColorTier3;
		const FLinearColor PulsingColor = BaseColor * PulseAlpha;

		if (bApplyProceduralStyle)
		{
			if (CenterDotImage) { CenterDotImage->SetColorAndOpacity(PulsingColor); }
			if (BracketLeftImage) { BracketLeftImage->SetColorAndOpacity(PulsingColor); }
			if (BracketRightImage) { BracketRightImage->SetColorAndOpacity(PulsingColor); }
			if (BracketTopImage) { BracketTopImage->SetColorAndOpacity(PulsingColor); }
			if (BracketBottomImage) { BracketBottomImage->SetColorAndOpacity(PulsingColor); }
			if (TierIndicatorText) { TierIndicatorText->SetColorAndOpacity(FSlateColor(PulsingColor)); }
		}
	}

	// 4. Hit Marker Countdown
	UpdateHitMarker(InDeltaTime);
}

void UThreadTalesBowReticleWidget::UpdateReticleFading(float InDeltaTime)
{
	if (!FMath::IsNearlyEqual(CurrentOpacity, TargetOpacity, 0.01f))
	{
		CurrentOpacity = FMath::FInterpTo(CurrentOpacity, TargetOpacity, InDeltaTime, FadeSpeed);
		SetRenderOpacity(CurrentOpacity);
	}
	else if (CurrentOpacity != TargetOpacity)
	{
		CurrentOpacity = TargetOpacity;
		SetRenderOpacity(CurrentOpacity);
	}
}

void UThreadTalesBowReticleWidget::UpdateBracketPositions(float InDrawRatio)
{
	const float HorizontalSpread = FMath::Lerp(MaxBracketSpread, MinBracketSpread, InDrawRatio);
	const float VerticalSpread = FMath::Lerp(MaxVerticalSpread, MinVerticalSpread, InDrawRatio);

	if (BracketLeftImage)
	{
		if (UCanvasPanelSlot* LeftSlot = Cast<UCanvasPanelSlot>(BracketLeftImage->Slot))
		{
			LeftSlot->SetPosition(FVector2D(-HorizontalSpread, 0.0f));
		}
	}
	if (BracketRightImage)
	{
		if (UCanvasPanelSlot* RightSlot = Cast<UCanvasPanelSlot>(BracketRightImage->Slot))
		{
			RightSlot->SetPosition(FVector2D(HorizontalSpread, 0.0f));
		}
	}
	if (BracketTopImage)
	{
		if (UCanvasPanelSlot* TopSlot = Cast<UCanvasPanelSlot>(BracketTopImage->Slot))
		{
			TopSlot->SetPosition(FVector2D(0.0f, -VerticalSpread));
		}
	}
	if (BracketBottomImage)
	{
		if (UCanvasPanelSlot* BottomSlot = Cast<UCanvasPanelSlot>(BracketBottomImage->Slot))
		{
			BottomSlot->SetPosition(FVector2D(0.0f, VerticalSpread));
		}
	}
	if (TierIndicatorText)
	{
		if (UCanvasPanelSlot* TextSlot = Cast<UCanvasPanelSlot>(TierIndicatorText->Slot))
		{
			TextSlot->SetPosition(FVector2D(0.0f, VerticalSpread + 8.0f));
		}
	}
}

void UThreadTalesBowReticleWidget::UpdateTierVisuals(int32 InChargeTier)
{
	FLinearColor TargetColor = ColorTier1;
	FString TierString = TEXT("LV1");

	switch (InChargeTier)
	{
	case 2:
		TargetColor = ColorTier2;
		TierString = TEXT("LV2");
		break;
	case 3:
		TargetColor = ColorTier3;
		TierString = TEXT("LV3");
		break;
	case 4:
		TargetColor = ColorTier4;
		TierString = TEXT("LV4");
		break;
	default:
		TargetColor = ColorTier1;
		TierString = TEXT("LV1");
		break;
	}

	if (TierIndicatorText)
	{
		TierIndicatorText->SetText(FText::FromString(TierString));
		TierIndicatorText->SetColorAndOpacity(FSlateColor(TargetColor));
	}

	if (bApplyProceduralStyle)
	{
		if (CenterDotImage) { CenterDotImage->SetColorAndOpacity(TargetColor); }
		if (BracketLeftImage) { BracketLeftImage->SetColorAndOpacity(TargetColor); }
		if (BracketRightImage) { BracketRightImage->SetColorAndOpacity(TargetColor); }
		if (BracketTopImage) { BracketTopImage->SetColorAndOpacity(TargetColor); }
		if (BracketBottomImage) { BracketBottomImage->SetColorAndOpacity(TargetColor); }
	}
}

void UThreadTalesBowReticleWidget::UpdateHitMarker(float InDeltaTime)
{
	if (HitMarkerRemainingTime > 0.0f)
	{
		HitMarkerRemainingTime -= InDeltaTime;
		const float Alpha = FMath::Clamp(HitMarkerRemainingTime / FMath::Max(HitMarkerDuration, 0.05f), 0.0f, 1.0f);
		const FLinearColor HitCol = bHitMarkerCritical ? FLinearColor(1.0f, 0.2f, 0.15f, Alpha) : FLinearColor(1.0f, 1.0f, 1.0f, Alpha * 0.95f);

		if (HitMarkerRemainingTime <= 0.0f)
		{
			if (HitMarkerImage) { HitMarkerImage->SetVisibility(ESlateVisibility::Collapsed); }
			if (HitMarkerLine1) { HitMarkerLine1->SetVisibility(ESlateVisibility::Collapsed); }
			if (HitMarkerLine2) { HitMarkerLine2->SetVisibility(ESlateVisibility::Collapsed); }
			if (bDisableScreen2DReticle)
			{
				SetRenderOpacity(0.0f);
			}
		}
	}
}
