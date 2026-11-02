// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesLobbySlotWidget.h"
#include "ThreadTalesButton.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Core/ThreadTalesPlayerState.h"
#include "ThreadTalesUIFocusUtils.h"
#include "Framework/Application/SlateApplication.h"

UThreadTalesLobbySlotWidget::UThreadTalesLobbySlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UThreadTalesLobbySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!GetPrevRoleButton() && WidgetTree)
	{
		for (const TCHAR* Candidate : { TEXT("PrevRole_MenuButton"), TEXT("Btn_PrevRole"), TEXT("PrevRoleMenuButton"), TEXT("Prev Role Menu Button"), TEXT("Btn_Prev"), TEXT("Button_Prev") })
		{
			if ((PrevRole_MenuButton = WidgetTree->FindWidget(FName(Candidate)))) break;
		}
	}
	if (!GetNextRoleButton() && WidgetTree)
	{
		for (const TCHAR* Candidate : { TEXT("NextRole_MenuButton"), TEXT("Btn_NextRole"), TEXT("NextRoleMenuButton"), TEXT("Next Role Menu Button"), TEXT("Btn_Next"), TEXT("Button_Next") })
		{
			if ((NextRole_MenuButton = WidgetTree->FindWidget(FName(Candidate)))) break;
		}
	}
	if (!GetReadyButtonWidget() && WidgetTree)
	{
		for (const TCHAR* Candidate : { TEXT("Ready_MenuButton"), TEXT("Btn_Ready"), TEXT("ReadyMenuButton"), TEXT("ReadyButton"), TEXT("Button_Ready") })
		{
			if ((Ready_MenuButton = WidgetTree->FindWidget(FName(Candidate)))) break;
		}
	}

	if (!Txt_RoleName && WidgetTree)
	{
		Txt_RoleName = Cast<UTextBlock>(WidgetTree->FindWidget(FName("Txt_RoleName")));
		if (!Txt_RoleName) Txt_RoleName = Cast<UTextBlock>(WidgetTree->FindWidget(FName("Role Name Text Block")));
		if (!Txt_RoleName) Txt_RoleName = Cast<UTextBlock>(WidgetTree->FindWidget(FName("RoleNameTextBlock")));
	}
	if (!Txt_PlayerTitle && WidgetTree)
	{
		Txt_PlayerTitle = Cast<UTextBlock>(WidgetTree->FindWidget(FName("Txt_PlayerTitle")));
		if (!Txt_PlayerTitle) Txt_PlayerTitle = Cast<UTextBlock>(WidgetTree->FindWidget(FName("Player Title Text Block")));
		if (!Txt_PlayerTitle) Txt_PlayerTitle = Cast<UTextBlock>(WidgetTree->FindWidget(FName("PlayerTitleTextBlock")));
	}
	if (!Txt_Status && WidgetTree)
	{
		Txt_Status = Cast<UTextBlock>(WidgetTree->FindWidget(FName("Txt_Status")));
		if (!Txt_Status) Txt_Status = Cast<UTextBlock>(WidgetTree->FindWidget(FName("Status Text Block")));
		if (!Txt_Status) Txt_Status = Cast<UTextBlock>(WidgetTree->FindWidget(FName("StatusTextBlock")));
	}

	if (UWidget* PrevW = GetPrevRoleButton())
	{
		if (UButton* B = Cast<UButton>(PrevW))
		{
			B->OnClicked.RemoveAll(this);
			B->OnClicked.AddDynamic(this, &UThreadTalesLobbySlotWidget::HandlePrevRoleClicked);
		}
		else if (UThreadTalesButton* TTB = Cast<UThreadTalesButton>(PrevW))
		{
			TTB->OnActionTriggered.RemoveAll(this);
			TTB->OnActionTriggered.AddDynamic(this, &UThreadTalesLobbySlotWidget::HandlePrevRoleActionTriggered);
		}
	}
	if (UWidget* NextW = GetNextRoleButton())
	{
		if (UButton* B = Cast<UButton>(NextW))
		{
			B->OnClicked.RemoveAll(this);
			B->OnClicked.AddDynamic(this, &UThreadTalesLobbySlotWidget::HandleNextRoleClicked);
		}
		else if (UThreadTalesButton* TTB = Cast<UThreadTalesButton>(NextW))
		{
			TTB->OnActionTriggered.RemoveAll(this);
			TTB->OnActionTriggered.AddDynamic(this, &UThreadTalesLobbySlotWidget::HandleNextRoleActionTriggered);
		}
	}
	if (UWidget* ReadyW = GetReadyButtonWidget())
	{
		if (UButton* B = Cast<UButton>(ReadyW))
		{
			B->OnClicked.RemoveAll(this);
			B->OnClicked.AddDynamic(this, &UThreadTalesLobbySlotWidget::HandleReadyClicked);
		}
		else if (UThreadTalesButton* TTB = Cast<UThreadTalesButton>(ReadyW))
		{
			TTB->OnActionTriggered.RemoveAll(this);
			TTB->OnActionTriggered.AddDynamic(this, &UThreadTalesLobbySlotWidget::HandleReadyActionTriggered);
		}
	}

	RefreshSlotState();
}

void UThreadTalesLobbySlotWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void UThreadTalesLobbySlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Poll player state updates to keep visuals reactive without tight coupling
	AThreadTalesPlayerState* PS = GetAssignedPlayerState();
	const bool bConnected = (PS != nullptr);
	const EThreadTalesCharacterRole CurrentRole = PS ? PS->GetSelectedRole() : EThreadTalesCharacterRole::None;
	const bool bCurrentReady = PS ? PS->IsReady() : false;

	if (bConnected != bLastCachedConnected || CurrentRole != LastCachedRole || bCurrentReady != bLastCachedReady)
	{
		bLastCachedConnected = bConnected;
		LastCachedRole = CurrentRole;
		bLastCachedReady = bCurrentReady;
		RefreshSlotState();
	}
}

FReply UThreadTalesLobbySlotWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const bool bIsGamepadKey = InKeyEvent.GetKey().IsGamepadKey();
	if (bIsGamepadActive != bIsGamepadKey)
	{
		bIsGamepadActive = bIsGamepadKey;
		BP_OnSlotInputMethodChanged(bIsGamepadActive);
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UThreadTalesLobbySlotWidget::InitializeSlot(int32 InSlotIndex)
{
	PlayerSlotIndex = InSlotIndex;
	if (PlayerSlotIndex == 1)
	{
		bIsGamepadActive = true;
	}

	if (GetAssignedRole() == EThreadTalesCharacterRole::None)
	{
		const EThreadTalesCharacterRole DefaultRole = (PlayerSlotIndex == 0)
			? EThreadTalesCharacterRole::Swordsman_Male
			: EThreadTalesCharacterRole::Archer_Female;
		SelectRole(DefaultRole);
	}

	RefreshSlotState();
}

void UThreadTalesLobbySlotWidget::SelectRole(EThreadTalesCharacterRole NewRole)
{
	AThreadTalesPlayerState* PS = GetAssignedPlayerState();
	if (!PS || PS->IsReady())
	{
		return;
	}

	PS->Server_SelectRole(NewRole);
	RefreshSlotState();
}

void UThreadTalesLobbySlotWidget::CycleRole()
{
	const EThreadTalesCharacterRole Current = GetAssignedRole();
	const EThreadTalesCharacterRole Next = (Current == EThreadTalesCharacterRole::Swordsman_Male)
		? EThreadTalesCharacterRole::Archer_Female
		: EThreadTalesCharacterRole::Swordsman_Male;
	SelectRole(Next);
}

void UThreadTalesLobbySlotWidget::ToggleReady()
{
	AThreadTalesPlayerState* PS = GetAssignedPlayerState();
	if (!PS)
	{
		return;
	}

	if (PS->GetSelectedRole() == EThreadTalesCharacterRole::None)
	{
		return;
	}

	PS->Server_SetReady(!PS->IsReady());
	RefreshSlotState();
}

EThreadTalesCharacterRole UThreadTalesLobbySlotWidget::GetAssignedRole() const
{
	AThreadTalesPlayerState* PS = GetAssignedPlayerState();
	return PS ? PS->GetSelectedRole() : EThreadTalesCharacterRole::None;
}

bool UThreadTalesLobbySlotWidget::IsReady() const
{
	AThreadTalesPlayerState* PS = GetAssignedPlayerState();
	return PS ? PS->IsReady() : false;
}

bool UThreadTalesLobbySlotWidget::IsPlayerConnected() const
{
	return (GetAssignedPlayerState() != nullptr);
}

bool UThreadTalesLobbySlotWidget::IsRoleTakenByOther(EThreadTalesCharacterRole Role) const
{
	AThreadTalesPlayerState* PS = GetAssignedPlayerState();
	return PS ? PS->IsRoleTakenByOther(Role) : false;
}

FReply UThreadTalesLobbySlotWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	UWidget* Desired = GetDesiredFocusWidget();
	if (Desired)
	{
		TSharedPtr<SWidget> SlateWidget = Desired->GetCachedWidget();
		if (!SlateWidget.IsValid())
		{
			SlateWidget = Desired->TakeWidget();
		}

		if (SlateWidget.IsValid())
		{
			return FReply::Handled().SetUserFocus(SlateWidget.ToSharedRef(), InFocusEvent.GetCause());
		}
	}

	return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

UWidget* UThreadTalesLobbySlotWidget::GetDesiredFocusWidget() const
{
	if (ActiveElement == EThreadTalesSlotActiveElement::ReadyButton || IsReady())
	{
		if (Btn_Ready && ThreadTalesUIFocusUtils::IsWidgetFocusable(Btn_Ready))
		{
			return Btn_Ready;
		}
		if (WidgetTree)
		{
			if (UWidget* W = WidgetTree->FindWidget(FName("Btn_Ready")))
			{
				if (ThreadTalesUIFocusUtils::IsWidgetFocusable(W)) return W;
			}
		}
		if (ReadyButton && ThreadTalesUIFocusUtils::IsWidgetFocusable(ReadyButton))
		{
			return ReadyButton;
		}
	}

	if (!IsReady())
	{
		if (Btn_PrevRole && ThreadTalesUIFocusUtils::IsWidgetFocusable(Btn_PrevRole))
		{
			return Btn_PrevRole;
		}
		if (WidgetTree)
		{
			if (UWidget* W = WidgetTree->FindWidget(FName("Btn_PrevRole")))
			{
				if (ThreadTalesUIFocusUtils::IsWidgetFocusable(W)) return W;
			}
			if (UWidget* W = WidgetTree->FindWidget(FName("Prev Role Menu Button")))
			{
				if (ThreadTalesUIFocusUtils::IsWidgetFocusable(W)) return W;
			}
			if (UWidget* W = WidgetTree->FindWidget(FName("PrevRoleMenuButton")))
			{
				if (ThreadTalesUIFocusUtils::IsWidgetFocusable(W)) return W;
			}
		}

		if (Btn_NextRole && ThreadTalesUIFocusUtils::IsWidgetFocusable(Btn_NextRole))
		{
			return Btn_NextRole;
		}
		if (WidgetTree)
		{
			if (UWidget* W = WidgetTree->FindWidget(FName("Btn_NextRole")))
			{
				if (ThreadTalesUIFocusUtils::IsWidgetFocusable(W)) return W;
			}
		}
	}

	if (Btn_Ready && ThreadTalesUIFocusUtils::IsWidgetFocusable(Btn_Ready))
	{
		return Btn_Ready;
	}
	if (WidgetTree)
	{
		if (UWidget* W = WidgetTree->FindWidget(FName("Btn_Ready")))
		{
			if (ThreadTalesUIFocusUtils::IsWidgetFocusable(W)) return W;
		}
	}

	return nullptr;
}

void UThreadTalesLobbySlotWidget::SetActiveElement(EThreadTalesSlotActiveElement NewElement)
{
	if (ActiveElement != NewElement)
	{
		ActiveElement = NewElement;
		BP_OnSlotActiveElementChanged(ActiveElement);
	}

	APlayerController* PC = GetAssignedPlayerController();
	if (!PC)
	{
		PC = GetOwningPlayer();
	}

	if (PC && PC->IsLocalPlayerController())
	{
		UWidget* Target = GetDesiredFocusWidget();
		if (Target && ThreadTalesUIFocusUtils::IsWidgetFocusable(Target))
		{
			Target->SetUserFocus(PC);
			Target->SetFocus();
			ThreadTalesUIFocusUtils::SetUserFocusExplicit(Target, PC);
		}
	}
}

void UThreadTalesLobbySlotWidget::NavigateActiveElement(bool bUp)
{
	if (bUp)
	{
		if (!IsReady())
		{
			SetActiveElement(EThreadTalesSlotActiveElement::RoleCarousel);
		}
	}
	else
	{
		SetActiveElement(EThreadTalesSlotActiveElement::ReadyButton);
	}
}

void UThreadTalesLobbySlotWidget::FocusSlotDefaultButton()
{
	APlayerController* PC = GetAssignedPlayerController();
	if (!PC)
	{
		PC = GetOwningPlayer();
	}

	if (!PC || !PC->IsLocalPlayerController())
	{
		return;
	}

	UWidget* Desired = GetDesiredFocusWidget();
	if (Desired)
	{
		Desired->SetUserFocus(PC);
		Desired->SetFocus();
		ThreadTalesUIFocusUtils::SetUserFocusExplicit(Desired, PC);
	}
	else if (ReadyButton && ReadyButton->GetIsEnabled())
	{
		ReadyButton->SetUserFocus(PC);
		ThreadTalesUIFocusUtils::SetUserFocusExplicit(ReadyButton, PC);
	}
	else if (SelectSwordsmanButton)
	{
		SelectSwordsmanButton->SetUserFocus(PC);
		ThreadTalesUIFocusUtils::SetUserFocusExplicit(SelectSwordsmanButton, PC);
	}
	else if (WidgetTree)
	{
		if (UWidget* ReadyBtn = WidgetTree->FindWidget(FName("Btn_Ready")))
		{
			ReadyBtn->SetUserFocus(PC);
			ReadyBtn->SetFocus();
		}
		else if (UWidget* PrevBtn = WidgetTree->FindWidget(FName("Btn_PrevRole")))
		{
			PrevBtn->SetUserFocus(PC);
			PrevBtn->SetFocus();
		}
	}
}

void UThreadTalesLobbySlotWidget::RefreshSlotState()
{
	AThreadTalesPlayerState* PS = GetAssignedPlayerState();
	const bool bConnected = (PS != nullptr);
	const FString PlayerName = PS ? PS->GetPlayerName() : FString::Printf(TEXT("Player %d"), PlayerSlotIndex + 1);
	const EThreadTalesCharacterRole CurrentRole = PS ? PS->GetSelectedRole() : EThreadTalesCharacterRole::None;
	const bool bPlayerReady = PS ? PS->IsReady() : false;

	const bool bSwordsmanTaken = IsRoleTakenByOther(EThreadTalesCharacterRole::Swordsman_Male);
	const bool bArcherTaken = IsRoleTakenByOther(EThreadTalesCharacterRole::Archer_Female);

	if (SelectSwordsmanButton)
	{
		SelectSwordsmanButton->SetIsEnabled(!bPlayerReady && !bSwordsmanTaken);
	}

	if (SelectArcherButton)
	{
		SelectArcherButton->SetIsEnabled(!bPlayerReady && !bArcherTaken);
	}

	if (Btn_PrevRole)
	{
		Btn_PrevRole->SetIsEnabled(bConnected && !bPlayerReady);
	}

	if (Btn_NextRole)
	{
		Btn_NextRole->SetIsEnabled(bConnected && !bPlayerReady);
	}

	if (Btn_Ready)
	{
		Btn_Ready->SetIsEnabled(bConnected);
	}

	if (Txt_RoleName)
	{
		FText RoleText = NSLOCTEXT("ThreadTalesLobby", "Role_None", "NONE");
		if (CurrentRole == EThreadTalesCharacterRole::Swordsman_Male)
		{
			RoleText = NSLOCTEXT("ThreadTalesLobby", "Role_Swordsman", "SWORDSMAN");
		}
		else if (CurrentRole == EThreadTalesCharacterRole::Archer_Female)
		{
			RoleText = NSLOCTEXT("ThreadTalesLobby", "Role_Archer", "ARCHER");
		}
		Txt_RoleName->SetText(RoleText);
	}

	if (Txt_PlayerTitle)
	{
		Txt_PlayerTitle->SetText(FText::FromString(PlayerName));
	}

	if (Txt_Status)
	{
		const FText StatusText = bPlayerReady 
			? NSLOCTEXT("ThreadTalesLobby", "Status_Ready", "READY") 
			: NSLOCTEXT("ThreadTalesLobby", "Status_NotReady", "NOT READY");
		Txt_Status->SetText(StatusText);
	}

	if (bPlayerReady && ActiveElement == EThreadTalesSlotActiveElement::RoleCarousel)
	{
		SetActiveElement(EThreadTalesSlotActiveElement::ReadyButton);
	}

	BP_OnSlotStateUpdated(bConnected, PlayerName, CurrentRole, bPlayerReady, bSwordsmanTaken, bArcherTaken);
}

void UThreadTalesLobbySlotWidget::HandlePrevRoleClicked()
{
	CycleRole();
}

void UThreadTalesLobbySlotWidget::HandleNextRoleClicked()
{
	CycleRole();
}

void UThreadTalesLobbySlotWidget::HandleSelectSwordsmanClicked()
{
	SelectRole(EThreadTalesCharacterRole::Swordsman_Male);
}

void UThreadTalesLobbySlotWidget::HandleSelectArcherClicked()
{
	SelectRole(EThreadTalesCharacterRole::Archer_Female);
}

void UThreadTalesLobbySlotWidget::HandleReadyClicked()
{
	ToggleReady();
}

APlayerController* UThreadTalesLobbySlotWidget::GetAssignedPlayerController() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	return UGameplayStatics::GetPlayerController(World, PlayerSlotIndex);
}

AThreadTalesPlayerState* UThreadTalesLobbySlotWidget::GetAssignedPlayerState() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	APlayerController* PC = GetAssignedPlayerController();
	if (PC)
	{
		if (AThreadTalesPlayerState* PS = PC->GetPlayerState<AThreadTalesPlayerState>())
		{
			return PS;
		}
	}

	AGameStateBase* GS = World->GetGameState();
	if (GS && GS->PlayerArray.IsValidIndex(PlayerSlotIndex))
	{
		return Cast<AThreadTalesPlayerState>(GS->PlayerArray[PlayerSlotIndex]);
	}

	return nullptr;
}

UWidget* UThreadTalesLobbySlotWidget::GetReadyButtonWidget() const
{
	if (Ready_MenuButton) return Ready_MenuButton.Get();
	if (Btn_Ready) return Btn_Ready.Get();
	return Cast<UWidget>(ReadyButton.Get());
}

UWidget* UThreadTalesLobbySlotWidget::GetPrevRoleButton() const
{
	if (PrevRole_MenuButton) return PrevRole_MenuButton.Get();
	return Btn_PrevRole.Get();
}

UWidget* UThreadTalesLobbySlotWidget::GetNextRoleButton() const
{
	if (NextRole_MenuButton) return NextRole_MenuButton.Get();
	return Btn_NextRole.Get();
}

void UThreadTalesLobbySlotWidget::HandlePrevRoleActionTriggered(EThreadTalesButtonAction Action)
{
	HandlePrevRoleClicked();
}

void UThreadTalesLobbySlotWidget::HandleNextRoleActionTriggered(EThreadTalesButtonAction Action)
{
	HandleNextRoleClicked();
}

void UThreadTalesLobbySlotWidget::HandleReadyActionTriggered(EThreadTalesButtonAction Action)
{
	HandleReadyClicked();
}
