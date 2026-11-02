// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesMenuWidget.h"
#include "ThreadTalesMenuInputProcessor.h"
#include "ThreadTalesUIInputSubsystem.h"
#include "ThreadTalesUIFocusUtils.h"
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Core/ThreadTalesSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"

UThreadTalesMenuWidget::UThreadTalesMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UThreadTalesMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	bHasInitialFocus = false;
	bWasVisible = (GetVisibility() == ESlateVisibility::Visible || GetVisibility() == ESlateVisibility::SelfHitTestInvisible);

	if (FSlateApplication::IsInitialized() && !MenuInputProcessor.IsValid())
	{
		MenuInputProcessor = MakeShared<FThreadTalesMenuInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(MenuInputProcessor);
	}

	UGameInstance* GI = GetGameInstance();
	if (GI)
	{
		if (UThreadTalesUIInputSubsystem* UIInputSubsystem = GI->GetSubsystem<UThreadTalesUIInputSubsystem>())
		{
			UIInputSubsystem->PushContext(this);
		}
	}

	UThreadTalesSessionSubsystem* SessionSubsystem = GI ? GI->GetSubsystem<UThreadTalesSessionSubsystem>() : nullptr;
	if (SessionSubsystem)
	{
		bWasInLobby = (SessionSubsystem->IsLocalCoopActive() || SessionSubsystem->IsOnlineSessionActive());
		SessionSubsystem->OnLocalCoopStateChanged.RemoveDynamic(this, &UThreadTalesMenuWidget::HandleLocalCoopStateChanged);
		SessionSubsystem->OnLocalCoopStateChanged.AddDynamic(this, &UThreadTalesMenuWidget::HandleLocalCoopStateChanged);
		SessionSubsystem->OnDestroySessionComplete.RemoveDynamic(this, &UThreadTalesMenuWidget::HandleDestroySessionComplete);
		SessionSubsystem->OnDestroySessionComplete.AddDynamic(this, &UThreadTalesMenuWidget::HandleDestroySessionComplete);

		if (SessionSubsystem->IsLocalCoopActive())
		{
			BP_OnTransitionToLobby(true);
		}
		else if (SessionSubsystem->IsOnlineSessionActive())
		{
			BP_OnTransitionToLobby(false);
		}
	}
	else
	{
		bWasInLobby = false;
	}

	RefreshMenuButtons();
	RestoreMenuFocus();
}

void UThreadTalesMenuWidget::NativeDestruct()
{
	for (UThreadTalesButton* Btn : MenuButtons)
	{
		if (Btn)
		{
			Btn->OnButtonHovered.RemoveDynamic(this, &UThreadTalesMenuWidget::HandleButtonHovered);
			Btn->OnActionTriggered.RemoveDynamic(this, &UThreadTalesMenuWidget::HandleButtonActionTriggered);
		}
	}
	MenuButtons.Empty();

	if (FSlateApplication::IsInitialized() && MenuInputProcessor.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(MenuInputProcessor);
		MenuInputProcessor.Reset();
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UThreadTalesUIInputSubsystem* UIInputSubsystem = GI->GetSubsystem<UThreadTalesUIInputSubsystem>())
		{
			UIInputSubsystem->PopContext(this);
		}

		if (UThreadTalesSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UThreadTalesSessionSubsystem>())
		{
			SessionSubsystem->OnLocalCoopStateChanged.RemoveDynamic(this, &UThreadTalesMenuWidget::HandleLocalCoopStateChanged);
			SessionSubsystem->OnDestroySessionComplete.RemoveDynamic(this, &UThreadTalesMenuWidget::HandleDestroySessionComplete);
		}
	}

	bHasInitialFocus = false;
	bWasVisible = false;
	bWasInLobby = false;
	Super::NativeDestruct();
}

void UThreadTalesMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UGameInstance* GI = GetGameInstance();
	if (GI)
	{
		if (UThreadTalesUIInputSubsystem* UIInputSubsystem = GI->GetSubsystem<UThreadTalesUIInputSubsystem>())
		{
			UIInputSubsystem->PushContext(this);
		}
	}

	UThreadTalesSessionSubsystem* SessionSubsystem = GI ? GI->GetSubsystem<UThreadTalesSessionSubsystem>() : nullptr;
	const bool bIsInLobby = SessionSubsystem && (SessionSubsystem->IsLocalCoopActive() || SessionSubsystem->IsOnlineSessionActive());
	const bool bIsCurrentlyVisible = (GetVisibility() == ESlateVisibility::Visible || GetVisibility() == ESlateVisibility::SelfHitTestInvisible);

	if ((!bHasInitialFocus || (!bIsInLobby && bWasInLobby) || (bIsCurrentlyVisible && !bWasVisible)) && bIsCurrentlyVisible && !bIsInLobby)
	{
		RestoreMenuFocus();
		bHasInitialFocus = true;
	}

	bWasVisible = bIsCurrentlyVisible;
	bWasInLobby = bIsInLobby;
}

void UThreadTalesMenuWidget::RestoreMenuFocus()
{
	FocusDefaultButton();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UThreadTalesMenuWidget::FocusDefaultButton));
	}
}

void UThreadTalesMenuWidget::HandleLocalCoopStateChanged(bool bIsActive)
{
	if (!bIsActive)
	{
		RestoreMenuFocus();
	}
}

void UThreadTalesMenuWidget::HandleDestroySessionComplete(bool bWasSuccessful)
{
	RestoreMenuFocus();
}

bool UThreadTalesMenuWidget::HandleUINavigation(int32 PlayerSlotIndex, EThreadTalesNavDirection Direction)
{
	if (Direction == EThreadTalesNavDirection::Up)
	{
		NavigateMenu(-1);
		return true;
	}
	if (Direction == EThreadTalesNavDirection::Down)
	{
		NavigateMenu(1);
		return true;
	}
	return false;
}

bool UThreadTalesMenuWidget::HandleUIAction(int32 PlayerSlotIndex, EThreadTalesUIAction Action)
{
	if (Action == EThreadTalesUIAction::Accept)
	{
		TriggerSelectedButtonAction();
		return true;
	}
	if (Action == EThreadTalesUIAction::Cancel)
	{
		QuitGame();
		return true;
	}
	return false;
}

bool UThreadTalesMenuWidget::HandleAnalogNavigation(int32 PlayerSlotIndex, const FVector2D& AxisValue)
{
	if (FMath::Abs(AxisValue.Y) >= 0.55f)
	{
		NavigateMenu(AxisValue.Y > 0.0f ? -1 : 1);
		return true;
	}
	return false;
}

void UThreadTalesMenuWidget::OnUIContextActivated()
{
	RestoreMenuFocus();
}

void UThreadTalesMenuWidget::OnUIContextDeactivated()
{
}

bool UThreadTalesMenuWidget::IsUIContextActive() const
{
	UGameInstance* GI = GetGameInstance();
	UThreadTalesSessionSubsystem* SessionSubsystem = GI ? GI->GetSubsystem<UThreadTalesSessionSubsystem>() : nullptr;
	const bool bIsInLobby = SessionSubsystem && (SessionSubsystem->IsLocalCoopActive() || SessionSubsystem->IsOnlineSessionActive());
	const bool bIsCurrentlyVisible = (GetVisibility() == ESlateVisibility::Visible || GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
	return bIsCurrentlyVisible && !bIsInLobby;
}

void UThreadTalesMenuWidget::RefreshMenuButtons()
{
	for (UThreadTalesButton* Btn : MenuButtons)
	{
		if (Btn)
		{
			Btn->OnButtonHovered.RemoveDynamic(this, &UThreadTalesMenuWidget::HandleButtonHovered);
			Btn->OnActionTriggered.RemoveDynamic(this, &UThreadTalesMenuWidget::HandleButtonActionTriggered);
		}
	}
	MenuButtons.Empty();

	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			if (UThreadTalesButton* TTBtn = Cast<UThreadTalesButton>(Widget))
			{
				if (TTBtn->ActionType == EThreadTalesButtonAction::None)
				{
					const FString BtnName = TTBtn->GetName();
					if (BtnName.Contains(TEXT("Local")) || BtnName.Contains(TEXT("Split")))
					{
						TTBtn->ActionType = EThreadTalesButtonAction::StartLocalCoop;
					}
					else if (BtnName.Contains(TEXT("Steam")) || BtnName.Contains(TEXT("Host")))
					{
						TTBtn->ActionType = EThreadTalesButtonAction::HostSteamGame;
					}
					else if (BtnName.Contains(TEXT("Find")) || BtnName.Contains(TEXT("Join")))
					{
						TTBtn->ActionType = EThreadTalesButtonAction::FindSteamGame;
					}
					else if (BtnName.Contains(TEXT("Quit")) || BtnName.Contains(TEXT("Exit")))
					{
						TTBtn->ActionType = EThreadTalesButtonAction::QuitGame;
					}
				}
				MenuButtons.Add(TTBtn);
				TTBtn->OnButtonHovered.AddDynamic(this, &UThreadTalesMenuWidget::HandleButtonHovered);
				TTBtn->OnActionTriggered.AddDynamic(this, &UThreadTalesMenuWidget::HandleButtonActionTriggered);
			}
		});
	}

	if (MenuButtons.Num() > 0)
	{
		SelectedButtonIndex = FMath::Clamp(SelectedButtonIndex, 0, MenuButtons.Num() - 1);
		for (int32 i = 0; i < MenuButtons.Num(); ++i)
		{
			if (MenuButtons[i])
			{
				const EThreadTalesButtonVisualState VisualState = (i == SelectedButtonIndex)
					? EThreadTalesButtonVisualState::Focused
					: EThreadTalesButtonVisualState::Normal;
				MenuButtons[i]->SetButtonVisualState(VisualState);
			}
		}

		if (MenuButtons.IsValidIndex(SelectedButtonIndex) && MenuButtons[SelectedButtonIndex])
		{
			CurrentFocusedButton = MenuButtons[SelectedButtonIndex];
			SetGlobalUserFocus(MenuButtons[SelectedButtonIndex]);
			BP_OnMenuButtonSelected(MenuButtons[SelectedButtonIndex]);
		}
	}
}

void UThreadTalesMenuWidget::SetSelectedButtonIndex(int32 NewIndex)
{
	if (MenuButtons.Num() == 0)
	{
		return;
	}

	if (MenuButtons.IsValidIndex(SelectedButtonIndex) && MenuButtons[SelectedButtonIndex])
	{
		MenuButtons[SelectedButtonIndex]->SetButtonVisualState(EThreadTalesButtonVisualState::Normal);
	}

	SelectedButtonIndex = FMath::Clamp(NewIndex, 0, MenuButtons.Num() - 1);
	if (UThreadTalesButton* TargetBtn = MenuButtons[SelectedButtonIndex])
	{
		TargetBtn->SetButtonVisualState(EThreadTalesButtonVisualState::Focused);
		CurrentFocusedButton = TargetBtn;
		SetGlobalUserFocus(TargetBtn);
		BP_OnMenuButtonSelected(TargetBtn);
	}
}

void UThreadTalesMenuWidget::TriggerSelectedButtonAction()
{
	if (MenuButtons.IsValidIndex(SelectedButtonIndex) && MenuButtons[SelectedButtonIndex])
	{
		MenuButtons[SelectedButtonIndex]->TriggerAction();
	}
	else
	{
		ActivateFocusedOrSelectedButton();
	}
}

void UThreadTalesMenuWidget::HandleButtonAction(EThreadTalesButtonAction Action)
{
	switch (Action)
	{
	case EThreadTalesButtonAction::StartLocalCoop: StartLocalCoopGame(); break;
	case EThreadTalesButtonAction::HostSteamGame:
	case EThreadTalesButtonAction::FindSteamGame: HostSteamGame(); break;
	case EThreadTalesButtonAction::QuitGame:
	case EThreadTalesButtonAction::Back: QuitGame(); break;
	default: break;
	}
}

void UThreadTalesMenuWidget::HandleButtonActionTriggered(EThreadTalesButtonAction Action)
{
	HandleButtonAction(Action);
}

void UThreadTalesMenuWidget::HandleButtonHovered(UThreadTalesButton* HoveredButton)
{
	if (!HoveredButton || MenuButtons.Num() == 0)
	{
		return;
	}

	const int32 FoundIdx = MenuButtons.Find(HoveredButton);
	if (FoundIdx != INDEX_NONE && FoundIdx != SelectedButtonIndex)
	{
		SetSelectedButtonIndex(FoundIdx);
	}
}

void UThreadTalesMenuWidget::TransitionToLobbyScreen(bool bIsLocalCoop)
{
	if (!LobbyMenuWidgetClass)
	{
		LobbyMenuWidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UI/WBP_LobbyMenu.WBP_LobbyMenu_C"));
	}

	if (LobbyMenuWidgetClass)
	{
		APlayerController* PC = GetOwningPlayer();
		if (!PC && GetWorld())
		{
			PC = GetWorld()->GetFirstPlayerController();
		}

		UUserWidget* LobbyWidget = nullptr;
		if (PC)
		{
			LobbyWidget = CreateWidget<UUserWidget>(PC, LobbyMenuWidgetClass);
		}
		else if (UGameInstance* GI = GetGameInstance())
		{
			LobbyWidget = CreateWidget<UUserWidget>(GI, LobbyMenuWidgetClass);
		}

		if (LobbyWidget)
		{
			LobbyWidget->AddToViewport(0);
		}
		BP_OnTransitionToLobby(bIsLocalCoop);
		RemoveFromParent();
		return;
	}

	BP_OnTransitionToLobby(bIsLocalCoop);
}

void UThreadTalesMenuWidget::StartLocalCoopGame()
{
	UGameInstance* GI = GetGameInstance();
	if (GI)
	{
		UThreadTalesSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UThreadTalesSessionSubsystem>();
		if (SessionSubsystem)
		{
			SessionSubsystem->StartLocalCoop();
		}
	}
	TransitionToLobbyScreen(true);
}

void UThreadTalesMenuWidget::HostSteamGame()
{
	UGameInstance* GI = GetGameInstance();
	if (GI)
	{
		UThreadTalesSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UThreadTalesSessionSubsystem>();
		if (SessionSubsystem)
		{
			SessionSubsystem->HostOnlineSession();
		}
	}
	TransitionToLobbyScreen(false);
}

void UThreadTalesMenuWidget::QuitGame()
{
	UKismetSystemLibrary::QuitGame(GetWorld(), GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UThreadTalesMenuWidget::SetGlobalUserFocus(UWidget* TargetWidget)
{
	if (!TargetWidget) return;
	APlayerController* PC = GetOwningPlayer();
	if (!PC && GetWorld()) PC = GetWorld()->GetFirstPlayerController();
	if (PC)
	{
		TargetWidget->SetUserFocus(PC);
		TargetWidget->SetFocus();
	}
	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<SWidget> SlateWidget = TargetWidget->GetCachedWidget();
		if (!SlateWidget.IsValid()) SlateWidget = TargetWidget->TakeWidget();
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

UWidget* UThreadTalesMenuWidget::GetDefaultMenuButton() const
{
	if (LocalMenuButton && LocalMenuButton->GetIsEnabled() && LocalMenuButton->IsVisible()) return LocalMenuButton.Get();
	if (SplitButton && SplitButton->GetIsEnabled() && SplitButton->IsVisible()) return SplitButton.Get();
	if (WidgetTree)
	{
		for (const FName& Name : { FName("LocalMenuButton"), FName("Btn_SplitScreen"), FName("SplitButton") })
		{
			if (UWidget* Btn = WidgetTree->FindWidget(Name))
			{
				if (Btn->GetIsEnabled() && Btn->IsVisible()) return Btn;
			}
		}
	}
	return MenuButtons.Num() > 0 ? MenuButtons[0].Get() : nullptr;
}

TArray<UWidget*> UThreadTalesMenuWidget::GetMainMenuButtons() const
{
	TArray<UWidget*> Result;
	auto AddIfValid = [&Result, this](UWidget* Btn, const FName& FallbackName)
	{
		UWidget* Candidate = Btn ? Btn : (WidgetTree ? WidgetTree->FindWidget(FallbackName) : nullptr);
		if (Candidate && Candidate->GetIsEnabled() && Candidate->IsVisible())
		{
			Result.AddUnique(Candidate);
		}
	};

	AddIfValid(LocalMenuButton.Get(), FName("LocalMenuButton"));
	AddIfValid(SplitButton.Get(), FName("Btn_SplitScreen"));
	AddIfValid(SteamMenuButton.Get(), FName("Btn_SteamOnline"));
	AddIfValid(QuitMenuButton.Get(), FName("Btn_Quit"));

	for (UThreadTalesButton* Btn : MenuButtons)
	{
		if (Btn) Result.AddUnique(Btn);
	}
	return Result;
}

void UThreadTalesMenuWidget::NavigateMenu(int32 Direction)
{
	if (MenuButtons.Num() > 0)
	{
		const int32 NextIndex = (SelectedButtonIndex + Direction + MenuButtons.Num()) % MenuButtons.Num();
		SetSelectedButtonIndex(NextIndex);
	}
}

void UThreadTalesMenuWidget::SetFocusedMenuButton(UWidget* Button)
{
	if (UThreadTalesButton* TTBtn = Cast<UThreadTalesButton>(Button))
	{
		const int32 Idx = MenuButtons.Find(TTBtn);
		if (Idx != INDEX_NONE)
		{
			SetSelectedButtonIndex(Idx);
			return;
		}
	}
	CurrentFocusedButton = Button;
	APlayerController* PC = GetOwningPlayer();
	if (!PC && GetWorld()) PC = GetWorld()->GetFirstPlayerController();
	Button->SetFocus();
	if (PC) Button->SetUserFocus(PC);
	SetGlobalUserFocus(Button);
	BP_OnMenuButtonSelected(Button);
}

bool UThreadTalesMenuWidget::ActivateFocusedOrSelectedButton()
{
	if (MenuButtons.IsValidIndex(SelectedButtonIndex) && MenuButtons[SelectedButtonIndex])
	{
		MenuButtons[SelectedButtonIndex]->TriggerAction();
		return true;
	}
	TriggerSelectedButtonAction();
	return true;
}

UWidget* UThreadTalesMenuWidget::FindFirstFocusableWidget() const
{
	return ThreadTalesUIFocusUtils::FindFirstFocusableWidget(WidgetTree, GetDefaultMenuButton());
}

void UThreadTalesMenuWidget::FocusDefaultButton()
{
	UWorld* World = GetWorld();
	APlayerController* PC = GetOwningPlayer();
	if (!PC && World) PC = World->GetFirstPlayerController();
	if (!PC) return;

	if (MenuButtons.Num() > 0)
	{
		SetSelectedButtonIndex(SelectedButtonIndex);
		return;
	}

	UWidget* TargetWidget = GetDefaultMenuButton();
	if (!TargetWidget) TargetWidget = FindFirstFocusableWidget();

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	if (TargetWidget)
	{
		InputMode.SetWidgetToFocus(TargetWidget->TakeWidget());
	}
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;

	if (TargetWidget)
	{
		TargetWidget->SetUserFocus(PC);
		TargetWidget->SetFocus();
		SetFocusedMenuButton(TargetWidget);
	}
}
