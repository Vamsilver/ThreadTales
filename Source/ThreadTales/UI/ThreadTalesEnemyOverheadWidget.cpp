// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ThreadTalesEnemyOverheadWidget.h"
#include "UI/ThreadTalesStatBarWidget.h"
#include "Characters/ThreadTalesBaseCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Characters/ThreadTalesEnemyCharacter.h"
#include "AI/ThreadTalesEnemyConfig.h"

UThreadTalesEnemyOverheadWidget::UThreadTalesEnemyOverheadWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CombatVisibilityDuration = 4.5f;
	FadeOutDuration = 0.5f;
	EnemyDisplayName = FText::FromString(TEXT("Самурай"));

	bAlwaysVisible = false;
	bStartVisible = true;

	bIsTargetLocked = false;
	bIsDead = false;
	bWasStaggered = false;
	CombatVisibilityTimer = 0.0f;
	CurrentOpacity = 0.0f;
	bLastVisibilityState = false;
	PreviousHealthValue = -1.0f;
}

TSharedRef<SWidget> UThreadTalesEnemyOverheadWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OverheadRootBox")); // [PROG-UI-FALLBACK-OK]
		WidgetTree->RootWidget = RootBox;

		// 1. Enemy Name Text
		EnemyNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EnemyNameText")); // [PROG-UI-FALLBACK-OK]
		EnemyNameText->SetText(EnemyDisplayName);
		FSlateFontInfo FontInfo = EnemyNameText->GetFont();
		FontInfo.Size = 10;
		EnemyNameText->SetFont(FontInfo);
		EnemyNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.92f, 0.94f, 0.95f)));
		EnemyNameText->SetShadowOffset(FVector2D(1.0f, 1.0f));
		EnemyNameText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
		if (UVerticalBoxSlot* TextSlot = RootBox->AddChildToVerticalBox(EnemyNameText))
		{
			TextSlot->SetHorizontalAlignment(HAlign_Center);
			TextSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		}

		// 3. Health Bar
		HealthBar = WidgetTree->ConstructWidget<UThreadTalesStatBarWidget>(UThreadTalesStatBarWidget::StaticClass(), TEXT("HealthBar")); // [PROG-UI-FALLBACK-OK]
		if (UVerticalBoxSlot* HealthSlot = RootBox->AddChildToVerticalBox(HealthBar))
		{
			HealthSlot->SetHorizontalAlignment(HAlign_Center);
			HealthSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		}

		// 4. Poise Bar
		PoiseBar = WidgetTree->ConstructWidget<UThreadTalesStatBarWidget>(UThreadTalesStatBarWidget::StaticClass(), TEXT("PoiseBar")); // [PROG-UI-FALLBACK-OK]
		if (UVerticalBoxSlot* PoiseSlot = RootBox->AddChildToVerticalBox(PoiseBar))
		{
			PoiseSlot->SetHorizontalAlignment(HAlign_Center);
			PoiseSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
		}
	}

	return Super::RebuildWidget();
}

void UThreadTalesEnemyOverheadWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Pure presentation: preserve designer UMG sizes, colors, and styles!
	// Only activate Sekiro posture inversion logic on PoiseBar
	if (PoiseBar)
	{
		PoiseBar->SetInvertFillPercent(true);
	}

	// Defensive legacy dot collapse
	if (UWidget* OldDot = GetWidgetFromName(TEXT("LockOnDotImage")))
	{
		OldDot->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Defensive fallback bindings if designer named widgets differently
	if (!HealthBar)
	{
		HealthBar = Cast<UThreadTalesStatBarWidget>(GetWidgetFromName(TEXT("PlayerHealthBar")));
		if (!HealthBar)
		{
			HealthBar = Cast<UThreadTalesStatBarWidget>(GetWidgetFromName(TEXT("EnemyHealthBar")));
		}
	}
	if (!PoiseBar)
	{
		PoiseBar = Cast<UThreadTalesStatBarWidget>(GetWidgetFromName(TEXT("PostureBar")));
	}
	if (!EnemyNameText)
	{
		EnemyNameText = Cast<UTextBlock>(GetWidgetFromName(TEXT("NameText")));
	}

	if (EnemyNameText && !EnemyDisplayName.IsEmpty())
	{
		EnemyNameText->SetText(EnemyDisplayName);
	}

	if (bAlwaysVisible || bStartVisible)
	{
		CombatVisibilityTimer = CombatVisibilityDuration;
		CurrentOpacity = 1.0f;
		SetRenderOpacity(1.0f);
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		CurrentOpacity = 0.0f;
		SetRenderOpacity(0.0f);
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UThreadTalesEnemyOverheadWidget::NativeDestruct()
{
	UnbindCharacterCallbacks();
	Super::NativeDestruct();
}

void UThreadTalesEnemyOverheadWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateVisibilityState(InDeltaTime);

	if (AThreadTalesBaseCharacter* Char = BoundCharacter.Get())
	{
		const bool bCurrentlyStaggered = Char->IsStaggered();
		if (bWasStaggered && !bCurrentlyStaggered)
		{
			if (PoiseBar)
			{
				PoiseBar->SetContinuousFlash(false);
			}
		}
		bWasStaggered = bCurrentlyStaggered;
	}
}

void UThreadTalesEnemyOverheadWidget::InitializeForCharacter(AThreadTalesBaseCharacter* InCharacter)
{
	if (BoundCharacter.Get() == InCharacter && InCharacter != nullptr)
	{
		return;
	}

	UnbindCharacterCallbacks();
	BoundCharacter = InCharacter;

	if (!InCharacter)
	{
		SetRenderOpacity(0.0f);
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(InCharacter))
	{
		if (const UThreadTalesEnemyConfig* Config = EnemyChar->GetEnemyConfig())
		{
			if (!Config->EnemyDisplayName.IsEmpty())
			{
				SetEnemyDisplayName(Config->EnemyDisplayName);
			}
		}
	}

	PreviousHealthValue = InCharacter->GetHealth();

	InCharacter->OnHealthChanged.AddDynamic(this, &UThreadTalesEnemyOverheadWidget::HandleHealthChanged);
	InCharacter->OnPoiseChanged.AddDynamic(this, &UThreadTalesEnemyOverheadWidget::HandlePoiseChanged);
	InCharacter->OnStaggered.AddDynamic(this, &UThreadTalesEnemyOverheadWidget::HandleStaggered);
	InCharacter->OnCharacterDeath.AddDynamic(this, &UThreadTalesEnemyOverheadWidget::HandleCharacterDeath);

	if (HealthBar)
	{
		HealthBar->SetStatValue(InCharacter->GetHealth(), InCharacter->GetMaxHealth(), true);
	}

	if (PoiseBar)
	{
		PoiseBar->SetStatValue(InCharacter->GetPoise(), InCharacter->GetMaxPoise(), true);
	}

	if (!InCharacter->IsAlive())
	{
		HandleDeath();
	}
}

void UThreadTalesEnemyOverheadWidget::SetTargetLocked(bool bInLocked)
{
	bIsTargetLocked = bInLocked;

	if (bIsTargetLocked && !bIsDead)
	{
		SetVisibility(ESlateVisibility::HitTestInvisible);
		RefreshCombatVisibility();
	}
}

void UThreadTalesEnemyOverheadWidget::SetEnemyDisplayName(const FText& InName)
{
	EnemyDisplayName = InName;
	if (EnemyNameText)
	{
		EnemyNameText->SetText(EnemyDisplayName);
	}
}

void UThreadTalesEnemyOverheadWidget::HandleDeath()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	bIsTargetLocked = false;
	CombatVisibilityTimer = 0.0f;

	if (CurrentOpacity <= 0.01f)
	{
		CurrentOpacity = 0.0f;
		SetRenderOpacity(0.0f);
		SetVisibility(ESlateVisibility::Collapsed);
	}

	UnbindCharacterCallbacks();
}

void UThreadTalesEnemyOverheadWidget::RefreshCombatVisibility()
{
	if (!bIsDead)
	{
		CombatVisibilityTimer = CombatVisibilityDuration;
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UThreadTalesEnemyOverheadWidget::HandleHealthChanged(float NewHealth, float MaxHealth)
{
	const float DamageAmount = (PreviousHealthValue > 0.0f && NewHealth < PreviousHealthValue) ? (PreviousHealthValue - NewHealth) : 0.0f;
	PreviousHealthValue = NewHealth;

	const float HealthPercent = (MaxHealth > 0.0f) ? FMath::Clamp(NewHealth / MaxHealth, 0.0f, 1.0f) : 0.0f;

	if (HealthBar)
	{
		HealthBar->SetStatValue(NewHealth, MaxHealth, false);
	}

	OnHealthChanged(NewHealth, MaxHealth, HealthPercent);
	OnDamageReceived(DamageAmount, NewHealth, MaxHealth);
	RefreshCombatVisibility();
}

void UThreadTalesEnemyOverheadWidget::HandlePoiseChanged(float NewPoise, float MaxPoise)
{
	const float PoisePercent = (MaxPoise > 0.0f) ? FMath::Clamp(NewPoise / MaxPoise, 0.0f, 1.0f) : 0.0f;

	if (PoiseBar)
	{
		PoiseBar->SetStatValue(NewPoise, MaxPoise, false);
	}

	OnPoiseChanged(NewPoise, MaxPoise, PoisePercent);
	RefreshCombatVisibility();
}

void UThreadTalesEnemyOverheadWidget::HandleStaggered(AActor* AttackerActor)
{
	bWasStaggered = true;
	if (PoiseBar)
	{
		PoiseBar->TriggerFlash(FLinearColor(1.0f, 0.2f, 0.2f, 1.0f), 1.25f);
		PoiseBar->SetContinuousFlash(true);
	}
	OnPostureBroken(AttackerActor);
	RefreshCombatVisibility();
}

void UThreadTalesEnemyOverheadWidget::HandleCharacterDeath(AActor* DeadActor, AActor* KillerActor)
{
	HandleDeath();
}

void UThreadTalesEnemyOverheadWidget::UnbindCharacterCallbacks()
{
	if (AThreadTalesBaseCharacter* Char = BoundCharacter.Get())
	{
		Char->OnHealthChanged.RemoveDynamic(this, &UThreadTalesEnemyOverheadWidget::HandleHealthChanged);
		Char->OnPoiseChanged.RemoveDynamic(this, &UThreadTalesEnemyOverheadWidget::HandlePoiseChanged);
		Char->OnStaggered.RemoveDynamic(this, &UThreadTalesEnemyOverheadWidget::HandleStaggered);
		Char->OnCharacterDeath.RemoveDynamic(this, &UThreadTalesEnemyOverheadWidget::HandleCharacterDeath);
	}
	BoundCharacter.Reset();
}

void UThreadTalesEnemyOverheadWidget::UpdateVisibilityState(float DeltaTime)
{
	if (bIsDead)
	{
		if (CurrentOpacity > 0.0f)
		{
			CurrentOpacity = FMath::FInterpTo(CurrentOpacity, 0.0f, DeltaTime, 10.0f);
			SetRenderOpacity(CurrentOpacity);
			if (CurrentOpacity <= 0.01f)
			{
				CurrentOpacity = 0.0f;
				SetRenderOpacity(0.0f);
				SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		return;
	}

	const bool bCurrentlyActive = !bIsDead && (bAlwaysVisible || bIsTargetLocked || (CombatVisibilityTimer > 0.0f));
	if (bCurrentlyActive != bLastVisibilityState)
	{
		bLastVisibilityState = bCurrentlyActive;
		OnCombatVisibilityChanged(bCurrentlyActive);
	}

	if (bAlwaysVisible || bIsTargetLocked)
	{
		CurrentOpacity = 1.0f;
		SetRenderOpacity(1.0f);
		SetVisibility(ESlateVisibility::HitTestInvisible);
		CombatVisibilityTimer = CombatVisibilityDuration;
		return;
	}

	if (CombatVisibilityTimer > 0.0f)
	{
		CombatVisibilityTimer -= DeltaTime;
		SetVisibility(ESlateVisibility::HitTestInvisible);

		if (CombatVisibilityTimer <= FadeOutDuration)
		{
			const float SafeFadeDuration = FMath::Max(FadeOutDuration, 0.01f);
			CurrentOpacity = FMath::Clamp(CombatVisibilityTimer / SafeFadeDuration, 0.0f, 1.0f);
			SetRenderOpacity(CurrentOpacity);
		}
		else
		{
			CurrentOpacity = 1.0f;
			SetRenderOpacity(1.0f);
		}
	}
	else
	{
		CurrentOpacity = 0.0f;
		SetRenderOpacity(0.0f);
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

float UThreadTalesEnemyOverheadWidget::GetHealthPercent() const
{
	if (BoundCharacter.IsValid() && BoundCharacter->GetMaxHealth() > 0.0f)
	{
		return FMath::Clamp(BoundCharacter->GetHealth() / BoundCharacter->GetMaxHealth(), 0.0f, 1.0f);
	}
	return 0.0f;
}

float UThreadTalesEnemyOverheadWidget::GetPoisePercent() const
{
	if (BoundCharacter.IsValid() && BoundCharacter->GetMaxPoise() > 0.0f)
	{
		return FMath::Clamp(BoundCharacter->GetPoise() / BoundCharacter->GetMaxPoise(), 0.0f, 1.0f);
	}
	return 0.0f;
}

float UThreadTalesEnemyOverheadWidget::GetHealth() const
{
	return BoundCharacter.IsValid() ? BoundCharacter->GetHealth() : 0.0f;
}

float UThreadTalesEnemyOverheadWidget::GetMaxHealth() const
{
	return BoundCharacter.IsValid() ? BoundCharacter->GetMaxHealth() : 0.0f;
}

float UThreadTalesEnemyOverheadWidget::GetPoise() const
{
	return BoundCharacter.IsValid() ? BoundCharacter->GetPoise() : 0.0f;
}

float UThreadTalesEnemyOverheadWidget::GetMaxPoise() const
{
	return BoundCharacter.IsValid() ? BoundCharacter->GetMaxPoise() : 0.0f;
}
