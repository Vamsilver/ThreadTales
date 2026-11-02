// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ThreadTalesPlayerState.generated.h"

/**
 * Co-op Character Roles in ThreadTales.
 */
UENUM(BlueprintType)
enum class EThreadTalesCharacterRole : uint8
{
	None = 0 UMETA(DisplayName = "None"),
	Swordsman_Male = 1 UMETA(DisplayName = "Swordsman (Male)"),
	Archer_Female = 2 UMETA(DisplayName = "Archer (Female)")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThreadTalesRoleChanged, EThreadTalesCharacterRole, NewRole);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThreadTalesReadyChanged, bool, bIsReady);

/**
 * Replicated Player State storing character selection and ready status.
 * Persists seamlessly across level travel (Lobby -> Gameplay Level).
 */
UCLASS()
class THREADTALES_API AThreadTalesPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AThreadTalesPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void OverrideWith(APlayerState* PlayerState) override;

	// ==========================================
	// Role & Ready API
	// ==========================================

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Lobby")
	EThreadTalesCharacterRole GetSelectedRole() const { return SelectedRole; }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Lobby")
	bool IsReady() const { return bIsReady; }

	/** Requests role selection on the server with mutual exclusivity check */
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ThreadTales|Lobby")
	void Server_SelectRole(EThreadTalesCharacterRole InRole);

	/** Sets player ready state on the server */
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ThreadTales|Lobby")
	void Server_SetReady(bool bInReady);

	/** Checks whether a role is already taken by another player in the session */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Lobby")
	bool IsRoleTakenByOther(EThreadTalesCharacterRole InRole) const;

	// ==========================================
	// Blueprint Event Dispatchers
	// ==========================================

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Lobby|Events")
	FOnThreadTalesRoleChanged OnRoleChanged;

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Lobby|Events")
	FOnThreadTalesReadyChanged OnReadyChanged;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_SelectedRole, BlueprintReadOnly, Category = "ThreadTales|Lobby", meta = (AllowPrivateAccess = "true"))
	EThreadTalesCharacterRole SelectedRole;

	UPROPERTY(ReplicatedUsing = OnRep_IsReady, BlueprintReadOnly, Category = "ThreadTales|Lobby", meta = (AllowPrivateAccess = "true"))
	bool bIsReady;

	UFUNCTION()
	void OnRep_SelectedRole(EThreadTalesCharacterRole OldRole);

	UFUNCTION()
	void OnRep_IsReady(bool bOldReady);
};
