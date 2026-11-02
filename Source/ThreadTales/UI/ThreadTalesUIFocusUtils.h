// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"

namespace ThreadTalesUIFocusUtils
{
	inline bool IsWidgetFocusable(const UWidget* Widget)
	{
		if (!Widget || !Widget->GetIsEnabled() || !Widget->IsVisible())
		{
			return false;
		}
		if (const UUserWidget* UW = Cast<UUserWidget>(Widget))
		{
			return true; // Visible and enabled UserWidgets (like WBP_MenuButton or WBP_LobbySlot) are focusable
		}
		if (const UButton* Btn = Cast<UButton>(Widget))
		{
			return Btn->GetIsFocusable();
		}
		TSharedPtr<SWidget> SlateWidget = Widget->GetCachedWidget();
		return SlateWidget.IsValid() && SlateWidget->SupportsKeyboardFocus();
	}

	inline void SetUserFocusExplicit(UWidget* TargetWidget, APlayerController* PC)
	{
		if (!TargetWidget || !PC)
		{
			return;
		}

		if (UUserWidget* UW = Cast<UUserWidget>(TargetWidget))
		{
			UW->SetIsFocusable(true);
		}

		TSharedPtr<SWidget> SlateWidget = TargetWidget->GetCachedWidget();
		if (!SlateWidget.IsValid())
		{
			SlateWidget = TargetWidget->TakeWidget();
		}

		if (SlateWidget.IsValid())
		{
			TargetWidget->SetUserFocus(PC);
			TargetWidget->SetFocus();

			if (ULocalPlayer* LP = PC->GetLocalPlayer())
			{
				const int32 UserIndex = LP->GetControllerId();
				if (FSlateApplication::IsInitialized())
				{
					FSlateApplication::Get().SetUserFocus(UserIndex, SlateWidget, EFocusCause::SetDirectly);
					if (TSharedPtr<FSlateUser> SlateUser = FSlateApplication::Get().GetUser(UserIndex))
					{
						SlateUser->SetFocus(SlateWidget.ToSharedRef(), EFocusCause::SetDirectly);
					}
				}
			}
		}
	}

	inline void SetGlobalUserFocus(UWidget* TargetWidget, APlayerController* PC = nullptr)
	{
		if (!TargetWidget)
		{
			return;
		}

		if (PC)
		{
			TargetWidget->SetUserFocus(PC);
			TargetWidget->SetFocus();
		}

		if (FSlateApplication::IsInitialized())
		{
			TSharedPtr<SWidget> SlateWidget = TargetWidget->GetCachedWidget();
			if (!SlateWidget.IsValid())
			{
				SlateWidget = TargetWidget->TakeWidget();
			}

			if (SlateWidget.IsValid())
			{
				FSlateApplication::Get().ForEachUser([&SlateWidget](FSlateUser& User)
				{
					User.SetFocus(SlateWidget.ToSharedRef(), EFocusCause::SetDirectly);
					FSlateApplication::Get().SetUserFocus(User.GetUserIndex(), SlateWidget, EFocusCause::SetDirectly);
				});
				FSlateApplication::Get().SetAllUserFocus(SlateWidget, EFocusCause::SetDirectly);
			}
		}
	}

	inline UWidget* FindFirstFocusableWidget(const UWidgetTree* WidgetTree, UWidget* DefaultBtn = nullptr)
	{
		if (DefaultBtn)
		{
			return DefaultBtn;
		}
		if (!WidgetTree)
		{
			return nullptr;
		}

		TArray<TWeakObjectPtr<UWidget>> Candidates;
		WidgetTree->ForEachWidget([&Candidates](UWidget* Candidate)
		{
			if (!Candidate)
			{
				return;
			}
			const FString Name = Candidate->GetName();
			if (Name.Contains(TEXT("Lobby")) || Name.Contains(TEXT("Switcher")) || Name.Contains(TEXT("Canvas")))
			{
				return;
			}
			if (IsWidgetFocusable(Candidate))
			{
				Candidates.Add(Candidate);
			}
		});

		if (Candidates.Num() == 0)
		{
			return nullptr;
		}

		UWidget* BestWidget = nullptr;
		FVector2D BestPos(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
		bool bFoundValidGeometry = false;

		for (const TWeakObjectPtr<UWidget>& CandidatePtr : Candidates)
		{
			UWidget* Candidate = CandidatePtr.Get();
			if (!Candidate)
			{
				continue;
			}
			const FGeometry& Geom = Candidate->GetCachedGeometry();
			if (Geom.GetLocalSize().X > 0.0f && Geom.GetLocalSize().Y > 0.0f)
			{
				const FVector2D Pos = Geom.GetAbsolutePosition();
				if (!bFoundValidGeometry || Pos.Y < (BestPos.Y - 10.0f) || (FMath::IsNearlyEqual(Pos.Y, BestPos.Y, 10.0f) && Pos.X < BestPos.X))
				{
					BestPos = Pos;
					BestWidget = Candidate;
					bFoundValidGeometry = true;
				}
			}
		}

		return (bFoundValidGeometry && BestWidget) ? BestWidget : (Candidates[0].IsValid() ? Candidates[0].Get() : nullptr);
	}
}
