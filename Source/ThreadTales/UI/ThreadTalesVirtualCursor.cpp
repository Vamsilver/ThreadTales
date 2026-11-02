// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesVirtualCursor.h"
#include "ThreadTalesLobbySlotWidget.h"
#include "ThreadTalesButton.h"
#include "Components/Widget.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"

void FThreadTalesVirtualCursor::Init(UWidgetTree* WidgetTree, TObjectPtr<UWidget>& InOutCursorWidget)
{
	bIsActive = true;
	if (!InOutCursorWidget && WidgetTree)
	{
		for (const TCHAR* Name : { TEXT("VirtualCursor_P2"), TEXT("VirtualCursor"), TEXT("GamepadCursor"), TEXT("Img_VirtualCursor") })
		{
			if ((InOutCursorWidget = WidgetTree->FindWidget(FName(Name)))) break;
		}
	}
	if (InOutCursorWidget)
	{
		InOutCursorWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

bool FThreadTalesVirtualCursor::UpdatePosition(const FVector2D& RawStick, const FGeometry& MyGeometry, float DeltaTime, FVector2D& OutVelocity)
{
	OutVelocity = FVector2D::ZeroVector;
	const float StickMag = RawStick.Size();

	if (StickMag <= DeadZone)
	{
		return false;
	}

	const float NormalizedMag = FMath::Clamp((StickMag - DeadZone) / (1.0f - DeadZone), 0.0f, 1.0f);
	const FVector2D Dir = RawStick / StickMag;
	OutVelocity = FVector2D(Dir.X, -Dir.Y) * (NormalizedMag * Speed);

	if (HoveredWidget.IsValid())
	{
		OutVelocity *= FrictionMultiplier;
	}

	Position += OutVelocity * DeltaTime;

	const FVector2D ViewportSize = MyGeometry.GetLocalSize();
	const float MaxX = (ViewportSize.X > 100.0f) ? ViewportSize.X - 16.0f : 1904.0f;
	const float MaxY = (ViewportSize.Y > 100.0f) ? ViewportSize.Y - 16.0f : 1064.0f;

	Position.X = FMath::Clamp(Position.X, 16.0f, MaxX);
	Position.Y = FMath::Clamp(Position.Y, 16.0f, MaxY);

	return true;
}

bool FThreadTalesVirtualCursor::IsWidgetUnderCursor(UWidget* Widget, const FGeometry& MyGeometry, const FVector2D& ScreenPos)
{
	if (!Widget || !Widget->IsVisible() || !Widget->GetIsEnabled())
	{
		return false;
	}

	const FGeometry& WidgetGeometry = Widget->GetCachedGeometry();
	const FVector2D AbsoluteCursorPos = MyGeometry.LocalToAbsolute(ScreenPos);
	return WidgetGeometry.IsUnderLocation(AbsoluteCursorPos);
}

void FThreadTalesVirtualCursor::ApplyPositionToWidget(UWidget* CursorWidget, const FVector2D& ScreenPos)
{
	if (!CursorWidget)
	{
		return;
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CursorWidget->Slot))
	{
		CanvasSlot->SetPosition(ScreenPos);
	}
	else
	{
		CursorWidget->SetRenderTranslation(ScreenPos);
	}
}

UWidget* FThreadTalesVirtualCursor::HitTest(UThreadTalesLobbySlotWidget* Slot2, UWidget* BackButton, const FGeometry& MyGeometry, const FVector2D& ScreenPos)
{
	if (!Slot2)
	{
		return nullptr;
	}

	if (UWidget* W = Slot2->GetReadyButtonWidget())
	{
		if (IsWidgetUnderCursor(W, MyGeometry, ScreenPos)) return W;
	}
	if (UWidget* W = Slot2->GetPrevRoleButton())
	{
		if (IsWidgetUnderCursor(W, MyGeometry, ScreenPos)) return W;
	}
	if (UWidget* W = Slot2->GetNextRoleButton())
	{
		if (IsWidgetUnderCursor(W, MyGeometry, ScreenPos)) return W;
	}
	if (UWidget* W = Slot2->GetSelectSwordsmanButton())
	{
		if (IsWidgetUnderCursor(W, MyGeometry, ScreenPos)) return W;
	}
	if (UWidget* W = Slot2->GetSelectArcherButton())
	{
		if (IsWidgetUnderCursor(W, MyGeometry, ScreenPos)) return W;
	}
	if (BackButton && IsWidgetUnderCursor(BackButton, MyGeometry, ScreenPos))
	{
		return BackButton;
	}

	return nullptr;
}

bool FThreadTalesVirtualCursor::ExecuteP2Action(UThreadTalesLobbySlotWidget* Slot2, UWidget* BackButton, EThreadTalesUIAction Action, UWidget* CurrentHovered)
{
	if (!Slot2)
	{
		return false;
	}

	if (Action == EThreadTalesUIAction::Accept)
	{
		if (CurrentHovered)
		{
			if (CurrentHovered == Slot2->GetReadyButtonWidget())
			{
				if (UThreadTalesButton* TTBtn = Cast<UThreadTalesButton>(CurrentHovered)) TTBtn->SetButtonVisualState(EThreadTalesButtonVisualState::Pressed);
				Slot2->ToggleReady();
				return true;
			}
			if (CurrentHovered == Slot2->GetPrevRoleButton() || CurrentHovered == Slot2->GetNextRoleButton())
			{
				if (!Slot2->IsReady())
				{
					if (UThreadTalesButton* TTBtn = Cast<UThreadTalesButton>(CurrentHovered)) TTBtn->SetButtonVisualState(EThreadTalesButtonVisualState::Pressed);
					Slot2->CycleRole();
				}
				return true;
			}
			if (CurrentHovered == Slot2->GetSelectSwordsmanButton())
			{
				if (!Slot2->IsReady()) Slot2->SelectRole(EThreadTalesCharacterRole::Swordsman_Male);
				return true;
			}
			if (CurrentHovered == Slot2->GetSelectArcherButton())
			{
				if (!Slot2->IsReady()) Slot2->SelectRole(EThreadTalesCharacterRole::Archer_Female);
				return true;
			}
			if (CurrentHovered == BackButton)
			{
				if (Slot2->IsReady()) { Slot2->ToggleReady(); return true; }
			}
		}

		// Fallback: If not hovering specific button, 'A' toggles ready on P2 slot
		Slot2->ToggleReady();
		return true;
	}

	if (Action == EThreadTalesUIAction::Cancel)
	{
		if (Slot2->IsReady())
		{
			Slot2->ToggleReady();
			return true;
		}
		return false;
	}

	if (Action == EThreadTalesUIAction::ToggleReady)
	{
		Slot2->ToggleReady();
		return true;
	}

	return false;
}
