// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesSessionSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameMapsSettings.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
#include "steam/isteaminput.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace
{
	const FName SETTING_MATCH_TYPE(TEXT("MATCH_TYPE"));
	const FString MATCH_TYPE_COOP_2P(TEXT("ThreadTales_2P_Coop"));
	const FName SETTING_SERVER_NAME(TEXT("SERVER_NAME"));
}

UThreadTalesSessionSubsystem::UThreadTalesSessionSubsystem()
	: PendingLobbyMapPath(TEXT("/Game/Maps/Lvl_Lobby?listen"))
{
}

void UThreadTalesSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Ensure default solo input routing: Gamepad 0 assigned to Player 0
	SetLocalCoopDeviceRouting(false);
	ReleaseSteamInputHooks();

	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (!OnlineSubsystem)
	{
		return;
	}

	IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		return;
	}

	CreateSessionCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(this, &UThreadTalesSessionSubsystem::HandleCreateSessionComplete);
	FindSessionsCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &UThreadTalesSessionSubsystem::HandleFindSessionsComplete);
	JoinSessionCompleteDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(this, &UThreadTalesSessionSubsystem::HandleJoinSessionComplete);
	DestroySessionCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &UThreadTalesSessionSubsystem::HandleDestroySessionComplete);

	SessionUserInviteAcceptedDelegate = FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UThreadTalesSessionSubsystem::HandleSessionUserInviteAccepted);
	SessionUserInviteAcceptedDelegateHandle = SessionInterface->AddOnSessionUserInviteAcceptedDelegate_Handle(SessionUserInviteAcceptedDelegate);
}

void UThreadTalesSessionSubsystem::Deinitialize()
{
	SetLocalCoopDeviceRouting(false);
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem)
	{
		IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnSessionUserInviteAcceptedDelegate_Handle(SessionUserInviteAcceptedDelegateHandle);
			SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
			SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
			SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		}
	}

	Super::Deinitialize();
}

bool UThreadTalesSessionSubsystem::IsOnlineSessionActive() const
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (!OnlineSubsystem)
	{
		return false;
	}

	IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		return false;
	}

	return (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr);
}

void UThreadTalesSessionSubsystem::HostOnlineSession(int32 MaxPlayers, bool bIsLAN, const FString& ServerName)
{
	SetLocalCoopDeviceRouting(false);
	ReleaseSteamInputHooks();

	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (!OnlineSubsystem)
	{
		OnCreateSessionComplete.Broadcast(false);
		return;
	}

	IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnCreateSessionComplete.Broadcast(false);
		return;
	}

	FNamedOnlineSession* ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession != nullptr)
	{
		bPendingHostAfterDestroy = true;
		PendingHostMaxPlayers = MaxPlayers;
		bPendingHostLAN = bIsLAN;
		PendingHostServerName = ServerName;

		DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);
		SessionInterface->DestroySession(NAME_GameSession);
		return;
	}

	InternalCreateSession(MaxPlayers, bIsLAN, ServerName);
}

void UThreadTalesSessionSubsystem::InternalCreateSession(int32 MaxPlayers, bool bIsLAN, const FString& ServerName)
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (!OnlineSubsystem)
	{
		OnCreateSessionComplete.Broadcast(false);
		return;
	}

	IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnCreateSessionComplete.Broadcast(false);
		return;
	}

	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
	LastSessionSettings->bIsLANMatch = bIsLAN;
	LastSessionSettings->NumPublicConnections = FMath::Clamp(MaxPlayers, 2, 2);
	LastSessionSettings->bAllowJoinInProgress = true;
	LastSessionSettings->bAllowInvites = true;
	LastSessionSettings->bUsesPresence = true;
	LastSessionSettings->bShouldAdvertise = true;
	LastSessionSettings->bUseLobbiesIfAvailable = true;

	// Critical Invariant: Hard filter to eliminate Spacewar noise on shared AppID 480
	LastSessionSettings->Set(SETTING_MATCH_TYPE, MATCH_TYPE_COOP_2P, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->Set(SETTING_SERVER_NAME, ServerName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	const FUniqueNetIdRepl NetId = LocalPlayer ? LocalPlayer->GetPreferredUniqueNetId() : FUniqueNetIdRepl();

	if (!SessionInterface->CreateSession(*NetId.GetUniqueNetId(), NAME_GameSession, *LastSessionSettings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		OnCreateSessionComplete.Broadcast(false);
	}
}

void UThreadTalesSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem)
	{
		IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		}
	}

	if (bWasSuccessful)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			World->ServerTravel(PendingLobbyMapPath, true);
		}
	}

	OnCreateSessionComplete.Broadcast(bWasSuccessful);
}

void UThreadTalesSessionSubsystem::FindOnlineSessions(int32 MaxSearchResults, bool bIsLAN)
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (!OnlineSubsystem)
	{
		OnFindSessionsComplete.Broadcast(TArray<FThreadTalesSessionInfo>(), false);
		return;
	}

	IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnFindSessionsComplete.Broadcast(TArray<FThreadTalesSessionInfo>(), false);
		return;
	}

	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastSessionSearch->MaxSearchResults = MaxSearchResults;
	LastSessionSearch->bIsLanQuery = bIsLAN;
	LastSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

	// Critical Invariant: Strict matchmaking query filter on AppID 480
	LastSessionSearch->QuerySettings.Set(SETTING_MATCH_TYPE, MATCH_TYPE_COOP_2P, EOnlineComparisonOp::Equals);

	ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	const FUniqueNetIdRepl NetId = LocalPlayer ? LocalPlayer->GetPreferredUniqueNetId() : FUniqueNetIdRepl();

	if (!SessionInterface->FindSessions(*NetId.GetUniqueNetId(), LastSessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		OnFindSessionsComplete.Broadcast(TArray<FThreadTalesSessionInfo>(), false);
	}
}

void UThreadTalesSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem)
	{
		IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		}
	}

	TArray<FThreadTalesSessionInfo> DiscoveredSessions;

	if (bWasSuccessful && LastSessionSearch.IsValid())
	{
		for (int32 Index = 0; Index < LastSessionSearch->SearchResults.Num(); ++Index)
		{
			const FOnlineSessionSearchResult& Result = LastSessionSearch->SearchResults[Index];

			FThreadTalesSessionInfo Info;
			Info.SessionIndex = Index;
			Info.HostName = Result.Session.OwningUserName;
			Info.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
			Info.CurrentPlayers = Info.MaxPlayers - Result.Session.NumOpenPublicConnections;
			Info.PingInMs = Result.PingInMs;

			FString CustomServerName;
			if (Result.Session.SessionSettings.Get(SETTING_SERVER_NAME, CustomServerName))
			{
				Info.ServerName = CustomServerName;
			}
			else
			{
				Info.ServerName = FString::Printf(TEXT("%s's Game"), *Info.HostName);
			}

			DiscoveredSessions.Add(Info);
		}
	}

	OnFindSessionsComplete.Broadcast(DiscoveredSessions, bWasSuccessful);
}

void UThreadTalesSessionSubsystem::JoinOnlineSession(int32 SessionIndex)
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (!OnlineSubsystem || !LastSessionSearch.IsValid() || !LastSessionSearch->SearchResults.IsValidIndex(SessionIndex))
	{
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	const FUniqueNetIdRepl NetId = LocalPlayer ? LocalPlayer->GetPreferredUniqueNetId() : FUniqueNetIdRepl();

	if (!SessionInterface->JoinSession(*NetId.GetUniqueNetId(), NAME_GameSession, LastSessionSearch->SearchResults[SessionIndex]))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		OnJoinSessionComplete.Broadcast(false);
	}
}

void UThreadTalesSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem)
	{
		IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		}
	}

	const bool bSuccess = (Result == EOnJoinSessionCompleteResult::Success);

	if (bSuccess && OnlineSubsystem)
	{
		IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			FString ConnectString;
			if (SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectString))
			{
				APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
				if (PC)
				{
					PC->ClientTravel(ConnectString, TRAVEL_Absolute);
				}
			}
		}
	}

	OnJoinSessionComplete.Broadcast(bSuccess);
}

void UThreadTalesSessionSubsystem::DestroySession()
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (!OnlineSubsystem)
	{
		OnDestroySessionComplete.Broadcast(false);
		return;
	}

	IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnDestroySessionComplete.Broadcast(false);
		return;
	}

	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);

	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		OnDestroySessionComplete.Broadcast(false);
	}
}

void UThreadTalesSessionSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem)
	{
		IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		}
	}

	if (bPendingHostAfterDestroy)
	{
		bPendingHostAfterDestroy = false;
		InternalCreateSession(PendingHostMaxPlayers, bPendingHostLAN, PendingHostServerName);
		return;
	}

	SetLocalCoopDeviceRouting(false);
	OnDestroySessionComplete.Broadcast(bWasSuccessful);
}

void UThreadTalesSessionSubsystem::HandleSessionUserInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	if (!bWasSuccessful)
	{
		return;
	}

	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (!OnlineSubsystem)
	{
		return;
	}

	IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		return;
	}

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	if (!SessionInterface->JoinSession(ControllerId, NAME_GameSession, InviteResult))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		OnJoinSessionComplete.Broadcast(false);
	}
}

bool UThreadTalesSessionSubsystem::StartLocalCoop()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	if (IsLocalCoopActive())
	{
		return true;
	}

	// Dynamically delegate first gamepad to Player 1 for Couch Co-op
	SetLocalCoopDeviceRouting(true);

	FString Error;
	ULocalPlayer* NewPlayer = GetGameInstance()->CreateLocalPlayer(1, Error, true);
	const bool bCreated = (NewPlayer != nullptr);

	if (bCreated)
	{
		OnLocalCoopStateChanged.Broadcast(true);
	}
	else
	{
		// Roll back on failure
		SetLocalCoopDeviceRouting(false);
	}

	return bCreated;
}

bool UThreadTalesSessionSubsystem::StopLocalCoop()
{
	// Restore default solo gamepad assignment
	SetLocalCoopDeviceRouting(false);

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	APlayerController* SecondPC = UGameplayStatics::GetPlayerController(World, 1);
	if (!SecondPC)
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = SecondPC->GetLocalPlayer();
	if (!LocalPlayer)
	{
		return false;
	}

	GetGameInstance()->RemoveLocalPlayer(LocalPlayer);
	OnLocalCoopStateChanged.Broadcast(false);
	return true;
}

bool UThreadTalesSessionSubsystem::IsLocalCoopActive() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return (UGameplayStatics::GetPlayerController(World, 1) != nullptr);
}

void UThreadTalesSessionSubsystem::ReleaseSteamInputHooks()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	if (SteamAPI_IsSteamRunning())
	{
		if (ISteamInput* SteamInputPtr = SteamInput())
		{
			SteamInputPtr->Shutdown();
			UE_LOG(LogTemp, Log, TEXT("[ThreadTalesSessionSubsystem] SteamInput successfully shutdown to release controller hooks for native Enhanced Input."));
		}
	}
#endif
}

void UThreadTalesSessionSubsystem::SetLocalCoopDeviceRouting(bool bEnableOffset)
{
	UGameMapsSettings* MapsSettings = GetMutableDefault<UGameMapsSettings>();
	if (MapsSettings)
	{
		if (bEnableOffset)
		{
			MapsSettings->SetSkipAssigningGamepadToPlayer1(true);
		}
		else
		{
			MapsSettings->SetSkipAssigningGamepadToPlayer1(false);
		}
		MapsSettings->bOffsetPlayerGamepadIds = bEnableOffset;
	}
}
