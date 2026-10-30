// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ThreadTalesHUDWidget.h"
#include "UI/ThreadTalesStatBarWidget.h"
#include "UI/ThreadTalesBowReticleWidget.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
#include "Characters/ThreadTalesDummy.h"
#include "Combat/ThreadTalesWeapon_Bow.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Abilities/ThreadTalesPostureConfig.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "EngineUtils.h"

UThreadTalesHUDWidget::UThreadTalesHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PartnerDiscoveryTimer = 0.0f;
	StaggerBannerRemainingTime = 0.0f;
}

static UHorizontalBox* CreateLabeledBarRow(UWidgetTree* Tree, FName RowName, const FString& LabelStr, FLinearColor LabelColor, UThreadTalesStatBarWidget* BarWidget)
{
	UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), RowName); // [PROG-UI-FALLBACK-OK]
	USizeBox* LabelBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass()); // [PROG-UI-FALLBACK-OK]
	LabelBox->SetWidthOverride(48.0f);
	if (UHorizontalBoxSlot* LabelBoxSlot = Row->AddChildToHorizontalBox(LabelBox))
	{
		LabelBoxSlot->SetVerticalAlignment(VAlign_Center);
		LabelBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
	}

	UTextBlock* LabelText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(FText::FromString(LabelStr));
	FSlateFontInfo FontInfo = LabelText->GetFont();
	FontInfo.Size = 9;
	LabelText->SetFont(FontInfo);
	LabelText->SetColorAndOpacity(FSlateColor(LabelColor));
	LabelText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	LabelText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
	LabelBox->AddChild(LabelText);

	if (UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(BarWidget))
	{
		BarSlot->SetVerticalAlignment(VAlign_Center);
	}
	return Row;
}

void UThreadTalesHUDWidget::BuildPlayerCluster(UWidgetTree* InTree)
{
	if (!InTree || !RootCanvas) { return; }

	PlayerClusterBox = InTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PlayerClusterBox")); // [PROG-UI-FALLBACK-OK]
	if (UCanvasPanelSlot* PlayerSlot = RootCanvas->AddChildToCanvas(PlayerClusterBox))
	{
		PlayerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
		PlayerSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		PlayerSlot->SetPosition(FVector2D(36.0f, 32.0f));
		PlayerSlot->SetAutoSize(true);
	}

	const FLinearColor DarkBg(0.04f, 0.04f, 0.06f, 0.90f);

	// Health Bar (Crimson Red + Warm Amber Ghost)
	PlayerHealthBar = InTree->ConstructWidget<UThreadTalesStatBarWidget>(UThreadTalesStatBarWidget::StaticClass(), TEXT("PlayerHealthBar"));
	PlayerHealthBar->ConfigureBarStyle(FLinearColor(0.72f, 0.07f, 0.07f, 1.0f), FLinearColor(1.0f, 0.65f, 0.15f, 0.95f), DarkBg, 340.0f, 20.0f, true);
	UHorizontalBox* HpRow = CreateLabeledBarRow(InTree, TEXT("HpRow"), TEXT("HP"), FLinearColor(0.95f, 0.35f, 0.35f, 0.95f), PlayerHealthBar);
	if (UVerticalBoxSlot* VSlot = PlayerClusterBox->AddChildToVerticalBox(HpRow)) { VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f)); }

	// Stamina Bar (Emerald Green + Pale Lime Ghost)
	PlayerStaminaBar = InTree->ConstructWidget<UThreadTalesStatBarWidget>(UThreadTalesStatBarWidget::StaticClass(), TEXT("PlayerStaminaBar"));
	PlayerStaminaBar->ConfigureBarStyle(FLinearColor(0.16f, 0.58f, 0.22f, 1.0f), FLinearColor(0.75f, 0.95f, 0.65f, 0.90f), DarkBg, 280.0f, 14.0f, false);
	UHorizontalBox* StmRow = CreateLabeledBarRow(InTree, TEXT("StmRow"), TEXT("STM"), FLinearColor(0.45f, 0.88f, 0.50f, 0.95f), PlayerStaminaBar);
	if (UVerticalBoxSlot* VSlot = PlayerClusterBox->AddChildToVerticalBox(StmRow)) { VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f)); }

	// Posture / Concentration Bar (Sekiro Amber-Gold Fill 0% -> 100% + Crimson-Orange Ghost)
	PlayerPoiseBar = InTree->ConstructWidget<UThreadTalesStatBarWidget>(UThreadTalesStatBarWidget::StaticClass(), TEXT("PlayerPoiseBar"));
	PlayerPoiseBar->ConfigureBarStyle(FLinearColor(0.95f, 0.58f, 0.10f, 1.0f), FLinearColor(1.0f, 0.25f, 0.10f, 0.85f), DarkBg, 240.0f, 12.0f, false);
	PlayerPoiseBar->SetInvertFillPercent(true);
	UHorizontalBox* PoiseRow = CreateLabeledBarRow(InTree, TEXT("PoiseRow"), TEXT("POSTURE"), FLinearColor(0.98f, 0.72f, 0.22f, 0.95f), PlayerPoiseBar);
	if (UVerticalBoxSlot* VSlot = PlayerClusterBox->AddChildToVerticalBox(PoiseRow)) { VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f)); }

	// Stagger Warning Text
	StaggerStatusText = InTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StaggerStatusText"));
	StaggerStatusText->SetText(FText::FromString(TEXT("POSTURE BROKEN — STAGGERED!")));
	FSlateFontInfo StaggerFont = StaggerStatusText->GetFont();
	StaggerFont.Size = 10;
	StaggerStatusText->SetFont(StaggerFont);
	StaggerStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.22f, 0.22f, 1.0f)));
	StaggerStatusText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	StaggerStatusText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.95f));
	StaggerStatusText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* VSlot = PlayerClusterBox->AddChildToVerticalBox(StaggerStatusText)) { VSlot->SetPadding(FMargin(54.0f, 2.0f, 0.0f, 0.0f)); }
}

void UThreadTalesHUDWidget::BuildPartnerCluster(UWidgetTree* InTree)
{
	if (!InTree || !RootCanvas) { return; }

	const FLinearColor DarkBg(0.04f, 0.04f, 0.06f, 0.90f);
	PartnerClusterBox = InTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PartnerClusterBox")); // [PROG-UI-FALLBACK-OK]
	PartnerClusterBox->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* PartnerSlot = RootCanvas->AddChildToCanvas(PartnerClusterBox))
	{
		PartnerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
		PartnerSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		PartnerSlot->SetPosition(FVector2D(36.0f, 126.0f));
		PartnerSlot->SetAutoSize(true);
	}

	PartnerNameText = InTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PartnerNameText"));
	PartnerNameText->SetText(FText::FromString(TEXT("CO-OP ALLY (P2)")));
	FSlateFontInfo PartnerFont = PartnerNameText->GetFont();
	PartnerFont.Size = 9;
	PartnerNameText->SetFont(PartnerFont);
	PartnerNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.82f, 0.42f, 0.95f)));
	PartnerNameText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	PartnerNameText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
	if (UVerticalBoxSlot* VSlot = PartnerClusterBox->AddChildToVerticalBox(PartnerNameText)) { VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f)); }

	PartnerHealthBar = InTree->ConstructWidget<UThreadTalesStatBarWidget>(UThreadTalesStatBarWidget::StaticClass(), TEXT("PartnerHealthBar"));
	PartnerHealthBar->ConfigureBarStyle(FLinearColor(0.68f, 0.12f, 0.12f, 0.95f), FLinearColor(1.0f, 0.65f, 0.15f, 0.90f), DarkBg, 200.0f, 11.0f, false);
	if (UVerticalBoxSlot* VSlot = PartnerClusterBox->AddChildToVerticalBox(PartnerHealthBar)) { VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f)); }

	PartnerStaminaBar = InTree->ConstructWidget<UThreadTalesStatBarWidget>(UThreadTalesStatBarWidget::StaticClass(), TEXT("PartnerStaminaBar"));
	PartnerStaminaBar->ConfigureBarStyle(FLinearColor(0.16f, 0.55f, 0.22f, 0.95f), FLinearColor(0.75f, 0.95f, 0.65f, 0.85f), DarkBg, 160.0f, 7.0f, false);
	if (UVerticalBoxSlot* VSlot = PartnerClusterBox->AddChildToVerticalBox(PartnerStaminaBar)) { VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 0.0f)); }
}

TSharedRef<SWidget> UThreadTalesHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas")); // [PROG-UI-FALLBACK-OK]
		WidgetTree->RootWidget = RootCanvas;

		BuildPlayerCluster(WidgetTree);
		BuildPartnerCluster(WidgetTree);

		// Central Dynamic Bow Reticle
		BowReticleWidget = WidgetTree->ConstructWidget<UThreadTalesBowReticleWidget>(UThreadTalesBowReticleWidget::StaticClass(), TEXT("BowReticleWidget"));
		if (UCanvasPanelSlot* ReticleSlot = RootCanvas->AddChildToCanvas(BowReticleWidget))
		{
			ReticleSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			ReticleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ReticleSlot->SetPosition(FVector2D::ZeroVector);
			ReticleSlot->SetAutoSize(true);
		}

	}

	return Super::RebuildWidget();
}

void UThreadTalesHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!BoundCharacter.IsValid())
	{
		if (APawn* OwningPawn = GetOwningPlayerPawn())
		{
			if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(OwningPawn))
			{
				InitializeForCharacter(TTChar);
			}
		}
	}

	TryAutoDiscoverPartner();
}

void UThreadTalesHUDWidget::NativeDestruct()
{
	UnbindPlayerCallbacks();
	UnbindPartnerCallbacks();
	Super::NativeDestruct();
}

void UThreadTalesHUDWidget::InitializeForCharacter(AThreadTalesCharacter* InCharacter)
{
	UnbindPlayerCallbacks();
	BoundCharacter = InCharacter;
	if (!InCharacter) { return; }

	InCharacter->OnHealthChanged.AddDynamic(this, &UThreadTalesHUDWidget::HandlePlayerHealthChanged);
	InCharacter->OnStaminaChanged.AddDynamic(this, &UThreadTalesHUDWidget::HandlePlayerStaminaChanged);
	InCharacter->OnPoiseChanged.AddDynamic(this, &UThreadTalesHUDWidget::HandlePlayerPoiseChanged);
	InCharacter->OnStaggered.AddDynamic(this, &UThreadTalesHUDWidget::HandlePlayerStaggered);
	InCharacter->OnCharacterDeath.AddDynamic(this, &UThreadTalesHUDWidget::HandlePlayerDeath);

	if (PlayerHealthBar) { PlayerHealthBar->SetStatValue(InCharacter->GetHealth(), InCharacter->GetMaxHealth(), true); }
	if (PlayerStaminaBar) { PlayerStaminaBar->SetStatValue(InCharacter->GetStamina(), InCharacter->GetMaxStamina(), true); }
	if (PlayerPoiseBar) { PlayerPoiseBar->SetStatValue(InCharacter->GetPoise(), InCharacter->GetMaxPoise(), true); }

	if (BowReticleWidget)
	{
		AThreadTalesWeapon_Bow* EquippedBow = nullptr;
		if (UThreadTalesEquipmentComponent* EquipComp = InCharacter->GetEquipmentComponent())
		{
			EquippedBow = Cast<AThreadTalesWeapon_Bow>(EquipComp->GetCurrentWeapon());
		}
		BowReticleWidget->InitializeForBow(EquippedBow, InCharacter);
	}

	TryAutoDiscoverPartner();
}

void UThreadTalesHUDWidget::SetPartnerCharacter(AThreadTalesCharacter* InPartner)
{
	if (BoundPartner.Get() == InPartner) { return; }

	UnbindPartnerCallbacks();
	BoundPartner = InPartner;

	if (!InPartner || InPartner == BoundCharacter.Get())
	{
		if (PartnerClusterBox) { PartnerClusterBox->SetVisibility(ESlateVisibility::Collapsed); }
		return;
	}

	if (PartnerClusterBox) { PartnerClusterBox->SetVisibility(ESlateVisibility::HitTestInvisible); }

	if (PartnerNameText)
	{
		const FString ClassName = InPartner->GetClass()->GetName();
		const FString DisplayRole = ClassName.Contains(TEXT("Female")) ? TEXT("ALLY: HERO FEMALE") : TEXT("ALLY: HERO MALE");
		PartnerNameText->SetText(FText::FromString(DisplayRole));
	}

	InPartner->OnHealthChanged.AddDynamic(this, &UThreadTalesHUDWidget::HandlePartnerHealthChanged);
	InPartner->OnStaminaChanged.AddDynamic(this, &UThreadTalesHUDWidget::HandlePartnerStaminaChanged);
	InPartner->OnCharacterDeath.AddDynamic(this, &UThreadTalesHUDWidget::HandlePartnerDeath);

	if (PartnerHealthBar) { PartnerHealthBar->SetStatValue(InPartner->GetHealth(), InPartner->GetMaxHealth(), true); }
	if (PartnerStaminaBar) { PartnerStaminaBar->SetStatValue(InPartner->GetStamina(), InPartner->GetMaxStamina(), true); }

	OnPartnerAssigned(InPartner);
}

void UThreadTalesHUDWidget::UnbindPlayerCallbacks()
{
	if (AThreadTalesCharacter* Char = BoundCharacter.Get())
	{
		Char->OnHealthChanged.RemoveDynamic(this, &UThreadTalesHUDWidget::HandlePlayerHealthChanged);
		Char->OnStaminaChanged.RemoveDynamic(this, &UThreadTalesHUDWidget::HandlePlayerStaminaChanged);
		Char->OnPoiseChanged.RemoveDynamic(this, &UThreadTalesHUDWidget::HandlePlayerPoiseChanged);
		Char->OnStaggered.RemoveDynamic(this, &UThreadTalesHUDWidget::HandlePlayerStaggered);
		Char->OnCharacterDeath.RemoveDynamic(this, &UThreadTalesHUDWidget::HandlePlayerDeath);
	}
	BoundCharacter.Reset();
}

void UThreadTalesHUDWidget::UnbindPartnerCallbacks()
{
	if (AThreadTalesCharacter* Partner = BoundPartner.Get())
	{
		Partner->OnHealthChanged.RemoveDynamic(this, &UThreadTalesHUDWidget::HandlePartnerHealthChanged);
		Partner->OnStaminaChanged.RemoveDynamic(this, &UThreadTalesHUDWidget::HandlePartnerStaminaChanged);
		Partner->OnCharacterDeath.RemoveDynamic(this, &UThreadTalesHUDWidget::HandlePartnerDeath);
	}
	BoundPartner.Reset();
}

void UThreadTalesHUDWidget::TryAutoDiscoverPartner()
{
	if (BoundPartner.IsValid() || !BoundCharacter.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AThreadTalesCharacter> It(World); It; ++It)
	{
		AThreadTalesCharacter* Candidate = *It;
		if (!Candidate || Candidate == BoundCharacter.Get())
		{
			continue;
		}
		// Exclude training dummies from co-op partner frame
		if (Candidate->IsA(AThreadTalesDummy::StaticClass()))
		{
			continue;
		}
		// Only bind player-controlled or co-op hero characters
		if (Candidate->IsPlayerControlled() || Candidate->GetClass()->GetName().Contains(TEXT("Hero")))
		{
			SetPartnerCharacter(Candidate);
			break;
		}
	}
}

void UThreadTalesHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Re-bind if owning player pawn changed (e.g. after respawn)
	if (!BoundCharacter.IsValid())
	{
		if (APawn* OwningPawn = GetOwningPlayerPawn())
		{
			if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(OwningPawn))
			{
				InitializeForCharacter(TTChar);
			}
		}
	}

	// Periodically check for co-op teammate if not yet bound
	if (!BoundPartner.IsValid())
	{
		PartnerDiscoveryTimer -= InDeltaTime;
		if (PartnerDiscoveryTimer <= 0.0f)
		{
			PartnerDiscoveryTimer = 1.0f;
			TryAutoDiscoverPartner();
		}
	}

	// Update Stagger Banner countdown
	if (StaggerBannerRemainingTime > 0.0f)
	{
		StaggerBannerRemainingTime -= InDeltaTime;
		if (StaggerBannerRemainingTime <= 0.0f)
		{
			if (StaggerStatusText)
			{
				StaggerStatusText->SetVisibility(ESlateVisibility::Collapsed);
			}
			if (PlayerPoiseBar)
			{
				PlayerPoiseBar->SetContinuousFlash(false);
				if (AThreadTalesCharacter* Char = BoundCharacter.Get())
				{
					PlayerPoiseBar->SetStatValue(Char->GetPoise(), Char->GetMaxPoise(), false);
				}
				else
				{
					PlayerPoiseBar->SetStatValue(100.0f, 100.0f, false);
				}
			}
			OnStaggerStateChanged(false);
		}
	}

	// Update Bow Reticle visibility and binding
	if (BowReticleWidget)
	{
		AThreadTalesCharacter* LocalChar = BoundCharacter.Get();
		bool bIsAimingWithBow = false;
		AThreadTalesWeapon_Bow* EquippedBow = nullptr;

		if (LocalChar)
		{
			if (UThreadTalesEquipmentComponent* EquipComp = LocalChar->GetEquipmentComponent())
			{
				EquippedBow = Cast<AThreadTalesWeapon_Bow>(EquipComp->GetCurrentWeapon());
			}

			if (EquippedBow)
			{
				BowReticleWidget->InitializeForBow(EquippedBow, LocalChar);
				if (UAbilitySystemComponent* ASC = LocalChar->GetAbilitySystemComponent())
				{
					bIsAimingWithBow = ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Aiming);
				}
			}
			else
			{
				BowReticleWidget->InitializeForBow(nullptr, LocalChar);
			}
		}

		BowReticleWidget->SetReticleVisible(bIsAimingWithBow, true);
	}
}

void UThreadTalesHUDWidget::HandlePlayerHealthChanged(float NewHealth, float MaxHealth)
{
	if (PlayerHealthBar)
	{
		const float OldPercent = PlayerHealthBar->GetTargetPercent();
		PlayerHealthBar->SetStatValue(NewHealth, MaxHealth, false);
		if (MaxHealth > 0.0f && (NewHealth / MaxHealth) < OldPercent)
		{
			PlayerHealthBar->TriggerFlash(FLinearColor(1.0f, 0.85f, 0.85f, 0.55f), 0.25f);
		}
	}

	const bool bIsCritical = (MaxHealth > 0.0f) && ((NewHealth / MaxHealth) <= 0.25f);
	OnCriticalHealthWarning(bIsCritical);
}

void UThreadTalesHUDWidget::HandlePlayerStaminaChanged(float NewStamina, float MaxStamina)
{
	if (PlayerStaminaBar)
	{
		PlayerStaminaBar->SetStatValue(NewStamina, MaxStamina, false);
		if (MaxStamina > 0.0f && (NewStamina / MaxStamina) <= 0.15f)
		{
			PlayerStaminaBar->TriggerFlash(FLinearColor(1.0f, 0.35f, 0.1f, 0.65f), 0.35f);
		}
	}
}

void UThreadTalesHUDWidget::HandlePlayerPoiseChanged(float NewPoise, float MaxPoise)
{
	if (PlayerPoiseBar)
	{
		// While staggered from a Posture Break, keep the bar visually pinned at 100% fill until the stagger window ends
		if (StaggerBannerRemainingTime > 0.0f && NewPoise > 0.0f)
		{
			return;
		}

		const float OldPercent = PlayerPoiseBar->GetTargetPercent();
		PlayerPoiseBar->SetStatValue(NewPoise, MaxPoise, false);
		const float NewPercent = PlayerPoiseBar->GetTargetPercent();

		// Flash bright amber/gold when Posture buildup increases, or red when >= 75% full
		if (NewPercent > OldPercent + 0.01f)
		{
			const FLinearColor FlashCol = (NewPercent >= 0.75f)
				? FLinearColor(1.0f, 0.22f, 0.12f, 0.75f)
				: FLinearColor(1.0f, 0.85f, 0.35f, 0.55f);
			PlayerPoiseBar->TriggerFlash(FlashCol, 0.25f);
		}
	}
}

void UThreadTalesHUDWidget::HandlePlayerStaggered(AActor* AttackerActor)
{
	float Duration = 3.50f;
	if (AThreadTalesCharacter* Char = BoundCharacter.Get())
	{
		if (const UThreadTalesPostureConfig* Cfg = Char->GetPostureConfig())
		{
			Duration = Cfg->PostureBreakStunDuration;
		}
	}
	StaggerBannerRemainingTime = FMath::Max(1.25f, Duration);
	if (StaggerStatusText)
	{
		StaggerStatusText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (PlayerPoiseBar)
	{
		// Pin Posture bar at 100% fill (Poise = 0) and pulse crimson red while staggered
		PlayerPoiseBar->SetStatValue(0.0f, 100.0f, true);
		PlayerPoiseBar->SetContinuousFlash(true, FLinearColor(1.0f, 0.18f, 0.18f, 0.80f));
	}

	OnStaggerStateChanged(true);
}

void UThreadTalesHUDWidget::HandlePlayerDeath(AActor* DeadActor, AActor* KillerActor)
{
	if (PlayerHealthBar)
	{
		PlayerHealthBar->SetStatValue(0.0f, 100.0f, false);
		PlayerHealthBar->TriggerFlash(FLinearColor(1.0f, 0.0f, 0.0f, 0.85f), 0.8f);
	}
}

void UThreadTalesHUDWidget::HandlePartnerHealthChanged(float NewHealth, float MaxHealth)
{
	if (PartnerHealthBar) { PartnerHealthBar->SetStatValue(NewHealth, MaxHealth, false); }
}

void UThreadTalesHUDWidget::HandlePartnerStaminaChanged(float NewStamina, float MaxStamina)
{
	if (PartnerStaminaBar) { PartnerStaminaBar->SetStatValue(NewStamina, MaxStamina, false); }
}

void UThreadTalesHUDWidget::HandlePartnerDeath(AActor* DeadActor, AActor* KillerActor)
{
	if (PartnerHealthBar)
	{
		PartnerHealthBar->SetStatValue(0.0f, 100.0f, false);
		PartnerHealthBar->TriggerFlash(FLinearColor(1.0f, 0.0f, 0.0f, 0.85f), 0.8f);
	}
}
