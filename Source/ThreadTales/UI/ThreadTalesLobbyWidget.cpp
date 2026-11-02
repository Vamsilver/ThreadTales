// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesLobbyWidget.h"
#include "ThreadTalesLobbySlotWidget.h"
#include "ThreadTalesButton.h"
#include "ThreadTalesMenuWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetNavigation.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameStateBase.h"
#include "Core/ThreadTalesLobbyGameMode.h"
#include "Core/ThreadTalesSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "GameMapsSettings.h"
#include "TimerManager.h"
#include "ThreadTalesUIFocusUtils.h"
#include "ThreadTalesUIInputSubsystem.h"

UThreadTalesLobbyWidget::UThreadTalesLobbyWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UThreadTalesLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	bHasInitialFocus = false;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UThreadTalesUIInputSubsystem* UIInputSubsystem = GI->GetSubsystem<UThreadTalesUIInputSubsystem>())
		{
			UIInputSubsystem->PushContext(this);
		}
	}

	if (!PlayerSlot_1 && WidgetTree)
	{
		for (const TCHAR* N : { TEXT("PlayerSlot_1"), TEXT("WBP_LobbySlot_1"), TEXT("WBP_LobbySlot") })
			if ((PlayerSlot_1 = Cast<UThreadTalesLobbySlotWidget>(WidgetTree->FindWidget(FName(N))))) break;
	}

	if (!PlayerSlot_2 && WidgetTree)
	{
		for (const TCHAR* N : { TEXT("PlayerSlot_2"), TEXT("WBP_LobbySlot_2") })
			if ((PlayerSlot_2 = Cast<UThreadTalesLobbySlotWidget>(WidgetTree->FindWidget(FName(N))))) break;
	}

	if (PlayerSlot_1) PlayerSlot_1->InitializeSlot(0);
	if (PlayerSlot_2) PlayerSlot_2->InitializeSlot(1);

	VirtualCursor.Init(WidgetTree, VirtualCursor_P2);

	UWorld* World = GetWorld();
	if (World)
	{
		if (AThreadTalesLobbyGameMode* GM = Cast<AThreadTalesLobbyGameMode>(UGameplayStatics::GetGameMode(World)))
		{
			GM->OnLobbyUpdated.AddDynamic(this, &UThreadTalesLobbyWidget::HandleGameModeLobbyUpdated);
		}
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UThreadTalesLobbyWidget::FocusDefaultButton));
	}
	else FocusDefaultButton();

	ForceRefreshLobbyState();
}

void UThreadTalesLobbyWidget::SetGlobalUserFocus(UWidget* TargetWidget)
{
	ThreadTalesUIFocusUtils::SetGlobalUserFocus(TargetWidget, GetOwningPlayer());
}

void UThreadTalesLobbyWidget::FocusDefaultButton()
{
	UWorld* World = GetWorld();
	APlayerController* PC0 = World ? UGameplayStatics::GetPlayerController(World, 0) : GetOwningPlayer();
	APlayerController* PC1 = World ? UGameplayStatics::GetPlayerController(World, 1) : nullptr;

	UWidget* SwordBtn = PlayerSlot_1 ? PlayerSlot_1->GetDesiredFocusWidget() : (Button_Sword ? Button_Sword.Get() : FindFirstFocusableWidget());
	UWidget* BowBtn = PlayerSlot_2 ? PlayerSlot_2->GetDesiredFocusWidget() : (Button_Bow ? Button_Bow.Get() : nullptr);

	if (PC0)
	{
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		if (SwordBtn) InputMode.SetWidgetToFocus(SwordBtn->TakeWidget());
		PC0->SetInputMode(InputMode);
		PC0->bShowMouseCursor = true;
		if (SwordBtn)
		{
			SwordBtn->SetUserFocus(PC0);
			SwordBtn->SetFocus();
			ThreadTalesUIFocusUtils::SetUserFocusExplicit(SwordBtn, PC0);
			if (PlayerSlot_1) PlayerSlot_1->FocusSlotDefaultButton();
		}
	}
	if (PC1)
	{
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC1->SetInputMode(InputMode);
		if (BowBtn)
		{
			BowBtn->SetUserFocus(PC1);
			BowBtn->SetFocus();
		}
	}
}

UWidget* UThreadTalesLobbyWidget::FindFirstFocusableWidget() const
{
	if (!WidgetTree) return nullptr;
	if (Button_Sword && ThreadTalesUIFocusUtils::IsWidgetFocusable(Button_Sword)) return Button_Sword;
	if (PlayerSlot_1 && ThreadTalesUIFocusUtils::IsWidgetFocusable(PlayerSlot_1)) return PlayerSlot_1;
	UWidget* BackBtn = BackButton ? BackButton.Get() : WidgetTree->FindWidget(FName("Btn_Back"));
	if (BackBtn && ThreadTalesUIFocusUtils::IsWidgetFocusable(BackBtn)) return BackBtn;
	if (StartMatchButton && ThreadTalesUIFocusUtils::IsWidgetFocusable(StartMatchButton)) return StartMatchButton;
	return ThreadTalesUIFocusUtils::FindFirstFocusableWidget(WidgetTree);
}

void UThreadTalesLobbyWidget::NativeDestruct()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UThreadTalesUIInputSubsystem* UIInputSubsystem = GI->GetSubsystem<UThreadTalesUIInputSubsystem>())
		{
			UIInputSubsystem->PopContext(this);
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (AThreadTalesLobbyGameMode* GM = Cast<AThreadTalesLobbyGameMode>(UGameplayStatics::GetGameMode(World)))
		{
			GM->OnLobbyUpdated.RemoveAll(this);
		}
	}
	Super::NativeDestruct();
}

void UThreadTalesLobbyWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateVirtualCursor(MyGeometry, InDeltaTime);

	UWorld* World = GetWorld();
	AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	const int32 CurrentPlayers = GS ? GS->PlayerArray.Num() : 0;
	const bool bCanStart = CanStartMatch();

	AThreadTalesPlayerState* P1State = (GS && GS->PlayerArray.IsValidIndex(0)) ? Cast<AThreadTalesPlayerState>(GS->PlayerArray[0]) : nullptr;
	AThreadTalesPlayerState* P2State = (GS && GS->PlayerArray.IsValidIndex(1)) ? Cast<AThreadTalesPlayerState>(GS->PlayerArray[1]) : nullptr;

	const EThreadTalesCharacterRole Role1 = P1State ? P1State->GetSelectedRole() : EThreadTalesCharacterRole::None;
	const EThreadTalesCharacterRole Role2 = P2State ? P2State->GetSelectedRole() : EThreadTalesCharacterRole::None;
	const bool bReady1 = P1State ? P1State->IsReady() : false;
	const bool bReady2 = P2State ? P2State->IsReady() : false;

	const bool bIsCurrentlyVisible = (GetVisibility() == ESlateVisibility::Visible || GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
	if (!bHasInitialFocus && bIsCurrentlyVisible)
	{
		FocusDefaultButton();
		bHasInitialFocus = true;
	}

	if (bCanStart != bLastCanStart || CurrentPlayers != LastPlayerCount || 
	    Role1 != LastRole1 || Role2 != LastRole2 || bReady1 != bLastReady1 || bReady2 != bLastReady2)
	{
		const bool bPlayerCountChanged = (CurrentPlayers != LastPlayerCount);
		bLastCanStart = bCanStart;
		LastPlayerCount = CurrentPlayers;
		LastRole1 = Role1;
		LastRole2 = Role2;
		bLastReady1 = bReady1;
		bLastReady2 = bReady2;

		if (StartMatchButton) StartMatchButton->SetIsEnabled(bCanStart && IsLobbyHost());
		if (bPlayerCountChanged && CurrentPlayers >= 2)
		{
			if (PlayerSlot_2) PlayerSlot_2->InitializeSlot(1);
			FocusDefaultButton();
		}

		BP_OnLobbyStateUpdated(CurrentPlayers, bCanStart);
	}
}

void UThreadTalesLobbyWidget::SelectRole(EThreadTalesCharacterRole NewRole)
{
	SelectRoleForPlayer(GetLocalPlayerIndex(), NewRole);
}

void UThreadTalesLobbyWidget::SelectRoleForPlayer(int32 PlayerIndex, EThreadTalesCharacterRole NewRole)
{
	UWorld* World = GetWorld();
	if (IsLocalCoopMatch())
	{
		// Local mode: split between PC0 and PC1 on the same machine
		APlayerController* TargetPC = World ? UGameplayStatics::GetPlayerController(World, PlayerIndex) : nullptr;
		AThreadTalesPlayerState* TargetPS = TargetPC ? TargetPC->GetPlayerState<AThreadTalesPlayerState>() : nullptr;
		if (!TargetPS && World)
		{
			AGameStateBase* GS = World->GetGameState();
			if (GS && GS->PlayerArray.IsValidIndex(PlayerIndex))
			{
				TargetPS = Cast<AThreadTalesPlayerState>(GS->PlayerArray[PlayerIndex]);
			}
		}

		if (TargetPS)
		{
			if (TargetPS->GetSelectedRole() == NewRole)
			{
				TargetPS->Server_SelectRole(EThreadTalesCharacterRole::None);
				TargetPS->Server_SetReady(false);
			}
			else
			{
				// If the other local player had this role, clear it from them first to avoid deadlock
				const int32 OtherIndex = (PlayerIndex == 0) ? 1 : 0;
				APlayerController* OtherPC = World ? UGameplayStatics::GetPlayerController(World, OtherIndex) : nullptr;
				AThreadTalesPlayerState* OtherPS = OtherPC ? OtherPC->GetPlayerState<AThreadTalesPlayerState>() : nullptr;
				if (OtherPS && OtherPS->GetSelectedRole() == NewRole)
				{
					OtherPS->Server_SelectRole(EThreadTalesCharacterRole::None);
					OtherPS->Server_SetReady(false);
				}

				TargetPS->Server_SelectRole(NewRole);
				TargetPS->Server_SetReady(NewRole != EThreadTalesCharacterRole::None);
			}
		}
	}
	else
	{
		// Online mode: each player selects independently through local PC0 and replicates via PlayerState
		APlayerController* LocalPC = GetOwningPlayer();
		if (!LocalPC && World)
		{
			LocalPC = UGameplayStatics::GetPlayerController(World, 0);
		}

		AThreadTalesPlayerState* LocalPS = LocalPC ? LocalPC->GetPlayerState<AThreadTalesPlayerState>() : nullptr;
		const int32 LocalIndex = GetLocalPlayerIndex();

		// In online mode, ignore requests targeting the remote player's slot
		if (LocalPS && (PlayerIndex == LocalIndex || PlayerIndex == 0 || LocalIndex == INDEX_NONE))
		{
			if (LocalPS->GetSelectedRole() == NewRole)
			{
				LocalPS->Server_SelectRole(EThreadTalesCharacterRole::None);
				LocalPS->Server_SetReady(false);
			}
			else if (!LocalPS->IsRoleTakenByOther(NewRole))
			{
				LocalPS->Server_SelectRole(NewRole);
				LocalPS->Server_SetReady(NewRole != EThreadTalesCharacterRole::None);
			}
		}
	}

	ForceRefreshLobbyState();
}

FText UThreadTalesLobbyWidget::GetRoleOwnerName(EThreadTalesCharacterRole Role) const
{
	if (Role == EThreadTalesCharacterRole::None)
	{
		return FText::GetEmpty();
	}

	UWorld* World = GetWorld();
	AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		return FText::GetEmpty();
	}

	const bool bIsLocal = IsLocalCoopMatch();

	for (int32 Index = 0; Index < GS->PlayerArray.Num(); ++Index)
	{
		APlayerState* PS = GS->PlayerArray[Index];
		AThreadTalesPlayerState* TTPS = Cast<AThreadTalesPlayerState>(PS);
		if (TTPS && TTPS->GetSelectedRole() == Role)
		{
			if (bIsLocal)
			{
				if (Index == 0)
				{
					return FText::FromString(TEXT("P1 (Клава / Геймпад 2)"));
				}
				else
				{
					return FText::FromString(TEXT("P2 (Геймпад 1)"));
				}
			}

			FString Name = TTPS->GetPlayerName();
			if (Name.IsEmpty())
			{
				Name = FString::Printf(TEXT("Player %d"), Index + 1);
			}
			return FText::FromString(Name);
		}
	}

	return FText::GetEmpty();
}

bool UThreadTalesLobbyWidget::IsRoleTaken(EThreadTalesCharacterRole Role) const
{
	return !GetRoleOwnerName(Role).IsEmpty();
}

EThreadTalesCharacterRole UThreadTalesLobbyWidget::GetPlayerRole(int32 PlayerIndex) const
{
	UWorld* World = GetWorld();
	AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (GS && GS->PlayerArray.IsValidIndex(PlayerIndex))
	{
		AThreadTalesPlayerState* TTPS = Cast<AThreadTalesPlayerState>(GS->PlayerArray[PlayerIndex]);
		if (TTPS)
		{
			return TTPS->GetSelectedRole();
		}
	}
	return EThreadTalesCharacterRole::None;
}

void UThreadTalesLobbyWidget::ForceRefreshLobbyState()
{
	UWorld* World = GetWorld();
	AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	const int32 CurrentPlayers = GS ? GS->PlayerArray.Num() : 0;
	const bool bCanStart = CanStartMatch();

	AThreadTalesPlayerState* P1State = (GS && GS->PlayerArray.IsValidIndex(0)) ? Cast<AThreadTalesPlayerState>(GS->PlayerArray[0]) : nullptr;
	AThreadTalesPlayerState* P2State = (GS && GS->PlayerArray.IsValidIndex(1)) ? Cast<AThreadTalesPlayerState>(GS->PlayerArray[1]) : nullptr;

	LastRole1 = P1State ? P1State->GetSelectedRole() : EThreadTalesCharacterRole::None;
	LastRole2 = P2State ? P2State->GetSelectedRole() : EThreadTalesCharacterRole::None;
	bLastReady1 = P1State ? P1State->IsReady() : false;
	bLastReady2 = P2State ? P2State->IsReady() : false;
	bLastCanStart = bCanStart;
	LastPlayerCount = CurrentPlayers;

	if (StartMatchButton)
	{
		StartMatchButton->SetIsEnabled(bCanStart && IsLobbyHost());
	}

	BP_OnLobbyStateUpdated(CurrentPlayers, bCanStart);
}

void UThreadTalesLobbyWidget::RequestStartMatch()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AThreadTalesLobbyGameMode* GM = Cast<AThreadTalesLobbyGameMode>(UGameplayStatics::GetGameMode(World));
	if (GM && GM->CanStartGame())
	{
		// Restore Game Input Mode and hide cursor for all local players before seamless travel
		for (int32 Index = 0; Index < 2; ++Index)
		{
			APlayerController* PC = UGameplayStatics::GetPlayerController(World, Index);
			if (PC && PC->IsLocalPlayerController())
			{
				FInputModeGameOnly InputMode;
				PC->SetInputMode(InputMode);
				PC->SetShowMouseCursor(false);
			}
		}

		BP_OnMatchStarting();
		GM->StartGame();
	}
}

void UThreadTalesLobbyWidget::LeaveLobby()
{
	UGameInstance* GI = GetGameInstance();
	if (GI)
	{
		UThreadTalesSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UThreadTalesSessionSubsystem>();
		if (SessionSubsystem)
		{
			if (SessionSubsystem->IsLocalCoopActive())
			{
				SessionSubsystem->StopLocalCoop();
			}
			else
			{
				SessionSubsystem->DestroySession();
			}
		}
	}

	BP_OnReturnToMainMenu();
	OnReturnToMainMenu.Broadcast();

	if (!MainMenuWidgetClass)
	{
		MainMenuWidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UI/WBP_MainMenu.WBP_MainMenu_C"));
	}

	if (MainMenuWidgetClass)
	{
		APlayerController* PC = GetOwningPlayer();
		if (!PC && GetWorld())
		{
			PC = GetWorld()->GetFirstPlayerController();
		}
		UUserWidget* MenuWidget = nullptr;
		if (PC)
		{
			MenuWidget = CreateWidget<UUserWidget>(PC, MainMenuWidgetClass);
		}
		else if (GI)
		{
			MenuWidget = CreateWidget<UUserWidget>(GI, MainMenuWidgetClass);
		}
		if (MenuWidget)
		{
			MenuWidget->AddToViewport(0);
		}
		RemoveFromParent();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		TArray<UUserWidget*> FoundWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, FoundWidgets, UThreadTalesMenuWidget::StaticClass(), false);
		for (UUserWidget* W : FoundWidgets)
		{
			if (UThreadTalesMenuWidget* MenuWidget = Cast<UThreadTalesMenuWidget>(W))
			{
				MenuWidget->RestoreMenuFocus();
			}
		}
	}
}

bool UThreadTalesLobbyWidget::CanStartMatch() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	AGameStateBase* GS = World->GetGameState();
	if (!GS || GS->PlayerArray.Num() != 2)
	{
		return false;
	}

	AThreadTalesPlayerState* P1State = Cast<AThreadTalesPlayerState>(GS->PlayerArray[0]);
	AThreadTalesPlayerState* P2State = Cast<AThreadTalesPlayerState>(GS->PlayerArray[1]);

	if (!P1State || !P2State)
	{
		return false;
	}

	if (!P1State->IsReady() || !P2State->IsReady())
	{
		return false;
	}

	const EThreadTalesCharacterRole Role1 = P1State->GetSelectedRole();
	const EThreadTalesCharacterRole Role2 = P2State->GetSelectedRole();

	if (Role1 == EThreadTalesCharacterRole::None || Role2 == EThreadTalesCharacterRole::None)
	{
		return false;
	}

	return (Role1 != Role2);
}

bool UThreadTalesLobbyWidget::IsLocalCoopMatch() const
{
	UGameInstance* GI = GetGameInstance();
	UThreadTalesSessionSubsystem* SessionSubsystem = GI ? GI->GetSubsystem<UThreadTalesSessionSubsystem>() : nullptr;
	return SessionSubsystem ? SessionSubsystem->IsLocalCoopActive() : false;
}

bool UThreadTalesLobbyWidget::IsOnlineMatch() const
{
	UGameInstance* GI = GetGameInstance();
	UThreadTalesSessionSubsystem* SessionSubsystem = GI ? GI->GetSubsystem<UThreadTalesSessionSubsystem>() : nullptr;
	return SessionSubsystem ? SessionSubsystem->IsOnlineSessionActive() : false;
}

bool UThreadTalesLobbyWidget::IsLobbyHost() const
{
	if (IsLocalCoopMatch()) return true;
	UWorld* World = GetWorld();
	return !World || (World->GetNetMode() != NM_Client);
}

int32 UThreadTalesLobbyWidget::GetLocalPlayerIndex() const
{
	if (IsLocalCoopMatch())
	{
		return 0;
	}

	UWorld* World = GetWorld();
	APlayerController* LocalPC = GetOwningPlayer();
	if (!LocalPC && World)
	{
		LocalPC = UGameplayStatics::GetPlayerController(World, 0);
	}

	AThreadTalesPlayerState* LocalPS = LocalPC ? LocalPC->GetPlayerState<AThreadTalesPlayerState>() : nullptr;
	AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (GS && LocalPS)
	{
		const int32 FoundIndex = GS->PlayerArray.IndexOfByKey(LocalPS);
		if (FoundIndex != INDEX_NONE)
		{
			return FoundIndex;
		}
	}

	return (World && World->GetNetMode() == NM_Client) ? 1 : 0;
}

bool UThreadTalesLobbyWidget::IsPlayerReady(int32 PlayerIndex) const
{
	UWorld* World = GetWorld();
	AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (GS && GS->PlayerArray.IsValidIndex(PlayerIndex))
	{
		AThreadTalesPlayerState* TTPS = Cast<AThreadTalesPlayerState>(GS->PlayerArray[PlayerIndex]);
		if (TTPS)
		{
			return TTPS->IsReady();
		}
	}
	return false;
}

void UThreadTalesLobbyWidget::HandleStartMatchClicked()
{
	RequestStartMatch();
}

void UThreadTalesLobbyWidget::HandleBackButtonClicked()
{
	LeaveLobby();
}

void UThreadTalesLobbyWidget::HandleGameModeLobbyUpdated(int32 CurrentPlayers, bool bCanStart)
{
	ForceRefreshLobbyState();
}

bool UThreadTalesLobbyWidget::HandleUINavigation(int32 PlayerSlotIndex, EThreadTalesNavDirection Direction)
{
	// Discrete carousel/focus navigation in lobby is fully replaced by the free virtual cursor.
	return false;
}

bool UThreadTalesLobbyWidget::HandleUIAction(int32 PlayerSlotIndex, EThreadTalesUIAction Action)
{
	// 1. If virtual cursor has an interactive widget hovered, clicking A executes directly on that widget!
	if (Action == EThreadTalesUIAction::Accept && VirtualCursor.HoveredWidget.IsValid() && VirtualCursor.HoveredWidget != BackButton)
	{
		return FThreadTalesVirtualCursor::ExecuteP2Action(PlayerSlot_2.Get(), BackButton.Get(), Action, VirtualCursor.HoveredWidget.Get());
	}

	// 2. Direct Player 2 action routing
	if (PlayerSlotIndex == 1)
	{
		return FThreadTalesVirtualCursor::ExecuteP2Action(PlayerSlot_2.Get(), BackButton.Get(), Action, VirtualCursor.HoveredWidget.Get());
	}

	// 3. Player 1 action routing (Host / Mouse & Keyboard / Gamepad 0)
	if (PlayerSlotIndex == 0)
	{
		if (Action == EThreadTalesUIAction::StartMatch || (Action == EThreadTalesUIAction::Accept && CanStartMatch()))
		{
			RequestStartMatch();
			return true;
		}
		if (Action == EThreadTalesUIAction::Cancel)
		{
			LeaveLobby();
			return true;
		}
		if (Action == EThreadTalesUIAction::ToggleReady && PlayerSlot_1)
		{
			PlayerSlot_1->ToggleReady();
			return true;
		}
		if (Action == EThreadTalesUIAction::Accept && PlayerSlot_2 && !CanStartMatch())
		{
			return FThreadTalesVirtualCursor::ExecuteP2Action(PlayerSlot_2.Get(), BackButton.Get(), Action, VirtualCursor.HoveredWidget.Get());
		}
		return false;
	}

	return false;
}

bool UThreadTalesLobbyWidget::HandleAnalogNavigation(int32 PlayerSlotIndex, const FVector2D& AxisValue)
{
	// Virtual cursor handles analog stick directly in NativeTick.
	return true;
}

void UThreadTalesLobbyWidget::SetVirtualCursorPosition(const FVector2D& NewPosition)
{
	VirtualCursor.Position = VirtualCursorPosition = NewPosition;
	FThreadTalesVirtualCursor::ApplyPositionToWidget(VirtualCursor_P2, NewPosition);
}

static void DispatchVirtualCursorHover(UWidget* Widget, bool bHovered)
{
	if (!Widget) return;

	if (UThreadTalesButton* TTBtn = Cast<UThreadTalesButton>(Widget))
	{
		TTBtn->SetButtonVisualState(bHovered ? EThreadTalesButtonVisualState::Hovered : EThreadTalesButtonVisualState::Normal);
		if (bHovered) TTBtn->OnButtonHovered.Broadcast(TTBtn);
	}

	if (UButton* RawBtn = Cast<UButton>(Widget))
	{
		if (bHovered) RawBtn->OnHovered.Broadcast();
		else RawBtn->OnUnhovered.Broadcast();
	}
	else if (UUserWidget* UserW = Cast<UUserWidget>(Widget))
	{
		if (UserW->WidgetTree)
		{
			UserW->WidgetTree->ForEachWidget([bHovered](UWidget* Child)
			{
				if (UButton* InnerBtn = Cast<UButton>(Child))
				{
					if (bHovered) InnerBtn->OnHovered.Broadcast();
					else InnerBtn->OnUnhovered.Broadcast();
				}
			});
		}
	}
}

void UThreadTalesLobbyWidget::UpdateVirtualCursor(const FGeometry& MyGeometry, float DeltaTime)
{
	if (!VirtualCursor.bIsActive) return;
	UGameInstance* GI = GetGameInstance();
	UThreadTalesUIInputSubsystem* Sub = GI ? GI->GetSubsystem<UThreadTalesUIInputSubsystem>() : nullptr;
	if (!Sub) return;

	VirtualCursor.Speed = VirtualCursorSpeed; VirtualCursor.DeadZone = VirtualCursorDeadZone; VirtualCursor.FrictionMultiplier = HoverFrictionMultiplier;

	FVector2D StickInput = Sub->GetPlayerLeftStickAxis(1);
	if (StickInput.IsNearlyZero())
	{
		StickInput = Sub->GetPlayerLeftStickAxis(0);
	}

	FVector2D Velocity = FVector2D::ZeroVector;
	if (VirtualCursor.UpdatePosition(StickInput, MyGeometry, DeltaTime, Velocity))
	{
		VirtualCursorPosition = VirtualCursor.Position;
		BP_OnVirtualCursorMoved(VirtualCursorPosition, Velocity);
	}
	FThreadTalesVirtualCursor::ApplyPositionToWidget(VirtualCursor_P2, VirtualCursor.Position);

	UWidget* HitWidget = HitTestVirtualCursor(MyGeometry, VirtualCursor.Position);
	if (HitWidget != VirtualCursor.HoveredWidget.Get())
	{
		if (VirtualCursor.HoveredWidget.IsValid())
		{
			UWidget* OldWidget = VirtualCursor.HoveredWidget.Get();
			DispatchVirtualCursorHover(OldWidget, false);
			BP_OnVirtualCursorHoverChanged(OldWidget, false);
		}
		if ((VirtualCursor.HoveredWidget = HitWidget).IsValid())
		{
			UWidget* NewWidget = VirtualCursor.HoveredWidget.Get();
			DispatchVirtualCursorHover(NewWidget, true);
			BP_OnVirtualCursorHoverChanged(NewWidget, true);
		}
	}
}

UWidget* UThreadTalesLobbyWidget::HitTestVirtualCursor(const FGeometry& MyGeometry, const FVector2D& ScreenPos)
{
	return FThreadTalesVirtualCursor::HitTest(PlayerSlot_2.Get(), BackButton.Get(), MyGeometry, ScreenPos);
}

void UThreadTalesLobbyWidget::OnUIContextActivated()
{
	FocusDefaultButton();
}

void UThreadTalesLobbyWidget::OnUIContextDeactivated()
{
}

bool UThreadTalesLobbyWidget::IsUIContextActive() const
{
	return IsVisible() && GetIsEnabled();
}
