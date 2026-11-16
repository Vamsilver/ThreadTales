// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "ThreadTalesPlayerState.h"
#include "ThreadTalesSessionSubsystem.generated.h"

/**
 * Lightweight, designer-friendly session representation for UMG.
 */
USTRUCT(BlueprintType)
struct FThreadTalesSessionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ThreadTales|Session")
	FString ServerName;

	UPROPERTY(BlueprintReadOnly, Category = "ThreadTales|Session")
	FString HostName;

	UPROPERTY(BlueprintReadOnly, Category = "ThreadTales|Session")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ThreadTales|Session")
	int32 MaxPlayers = 2;

	UPROPERTY(BlueprintReadOnly, Category = "ThreadTales|Session")
	int32 PingInMs = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ThreadTales|Session")
	int32 SessionIndex = 0;
};

// Dynamic Multicast Delegates for UMG Event Dispatchers
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThreadTalesCreateSessionComplete, bool, bSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnThreadTalesFindSessionsComplete, const TArray<FThreadTalesSessionInfo>&, SessionResults, bool, bSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThreadTalesJoinSessionComplete, bool, bSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThreadTalesDestroySessionComplete, bool, bSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThreadTalesLocalCoopStateChanged, bool, bIsActive);

/**
 * Core Network & Session Management Subsystem for ThreadTales.
 * Handles Steam Online 2P Co-op (with AppID 480 filtering & invites) and Local Split-Screen Co-op.
 */
UCLASS()
class THREADTALES_API UThreadTalesSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UThreadTalesSessionSubsystem();

	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	// ==========================================
	// Online Steam Session API
	// ==========================================

	/** Hosts a new 2P Steam Session and travels to the Lobby map */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Network")
	void HostOnlineSession(int32 MaxPlayers = 2, bool bIsLAN = false, const FString& ServerName = TEXT("ThreadTales Room"));

	/** Searches for ThreadTales sessions, strictly filtering by MatchType to ignore Spacewar noise */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Network")
	void FindOnlineSessions(int32 MaxSearchResults = 20, bool bIsLAN = false);

	/** Joins a discovered session by index in the results list */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Network")
	void JoinOnlineSession(int32 SessionIndex);

	/** Destroys current active session */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Network")
	void DestroySession();

	/** Returns true if an online session currently exists */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Network")
	bool IsOnlineSessionActive() const;

	// ==========================================
	// Local Split-Screen Co-op API
	// ==========================================

	/** Spawns a second local player for split-screen co-op on the same device */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|LocalCoop")
	bool StartLocalCoop();

	/** Removes the second local player and restores single-player viewport */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|LocalCoop")
	bool StopLocalCoop();

	/** Returns true if a second local player is active */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|LocalCoop")
	bool IsLocalCoopActive() const;

	/** Releases SteamInput hooks so native Enhanced Input has direct gamepad control */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Input")
	void ReleaseSteamInputHooks();

	/** Configures gamepad offset routing strictly for local couch co-op vs solo/menu */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Input")
	void SetLocalCoopDeviceRouting(bool bEnableOffset);

	// ==========================================
	// Role Persistence API (Across Travel & PIE)
	// ==========================================

	/** Sets the selected role for a given local player slot (0 = P1, 1 = P2) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Session")
	void SetLocalPlayerRole(int32 PlayerIndex, EThreadTalesCharacterRole InRole);

	/** Gets the selected role for a given local player slot (returns None if unassigned) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Session")
	EThreadTalesCharacterRole GetLocalPlayerRole(int32 PlayerIndex) const;

	/** Clears all stored local player roles */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Session")
	void ClearLocalPlayerRoles();

	/** Ensures second local player is active for couch co-op if entering lobby alone */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Session")
	void EnsureLocalCoopLobby(UWorld* World);

	/** Synchronizes role selection during lobby configuration with mutual exclusion */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Session")
	void SyncLobbyRoleSelection(int32 PlayerIndex, EThreadTalesCharacterRole InRole, bool bIsLocalCoop);

	/** Commits final chosen roles for both slots before travel */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Session")
	void CommitLobbyRoles(EThreadTalesCharacterRole Role1, EThreadTalesCharacterRole Role2, bool bIsLocalCoop);

	// ==========================================
	// Blueprint Event Dispatchers
	// ==========================================

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Network|Events")
	FOnThreadTalesCreateSessionComplete OnCreateSessionComplete;

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Network|Events")
	FOnThreadTalesFindSessionsComplete OnFindSessionsComplete;

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Network|Events")
	FOnThreadTalesJoinSessionComplete OnJoinSessionComplete;

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Network|Events")
	FOnThreadTalesDestroySessionComplete OnDestroySessionComplete;

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|LocalCoop|Events")
	FOnThreadTalesLocalCoopStateChanged OnLocalCoopStateChanged;

protected:
	/** Stored roles for local players surviving across level transitions and PIE hard travel */
	UPROPERTY()
	TMap<int32, EThreadTalesCharacterRole> SavedLocalPlayerRoles;

private:
	// Internal Delegate Callbacks
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleSessionUserInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);

	// OnlineSubsystem Delegate Handles
	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
	FDelegateHandle CreateSessionCompleteDelegateHandle;

	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	FDelegateHandle FindSessionsCompleteDelegateHandle;

	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
	FDelegateHandle JoinSessionCompleteDelegateHandle;

	FOnDestroySessionCompleteDelegate DestroySessionCompleteDelegate;
	FDelegateHandle DestroySessionCompleteDelegateHandle;

	FOnSessionUserInviteAcceptedDelegate SessionUserInviteAcceptedDelegate;
	FDelegateHandle SessionUserInviteAcceptedDelegateHandle;

	// Active search handle
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch;
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings;

	// Pending travel map on host
	FString PendingLobbyMapPath;

	// Safe sequential session recreation
	void InternalCreateSession(int32 MaxPlayers, bool bIsLAN, const FString& ServerName);
	bool bPendingHostAfterDestroy = false;
	int32 PendingHostMaxPlayers = 2;
	bool bPendingHostLAN = false;
	FString PendingHostServerName;
};
