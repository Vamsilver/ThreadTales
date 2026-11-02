// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"

AThreadTalesPlayerState::AThreadTalesPlayerState()
	: SelectedRole(EThreadTalesCharacterRole::None)
	, bIsReady(false)
{
	bReplicates = true;
}

void AThreadTalesPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AThreadTalesPlayerState, SelectedRole);
	DOREPLIFETIME(AThreadTalesPlayerState, bIsReady);
}

void AThreadTalesPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	AThreadTalesPlayerState* TTPS = Cast<AThreadTalesPlayerState>(PlayerState);
	if (TTPS)
	{
		TTPS->SelectedRole = SelectedRole;
		TTPS->bIsReady = bIsReady;
	}
}

void AThreadTalesPlayerState::OverrideWith(APlayerState* PlayerState)
{
	Super::OverrideWith(PlayerState);

	AThreadTalesPlayerState* TTPS = Cast<AThreadTalesPlayerState>(PlayerState);
	if (TTPS)
	{
		SelectedRole = TTPS->SelectedRole;
		bIsReady = TTPS->bIsReady;
	}
}

void AThreadTalesPlayerState::Server_SelectRole_Implementation(EThreadTalesCharacterRole InRole)
{
	if (SelectedRole != InRole)
	{
		SelectedRole = InRole;
		bIsReady = false;

		OnRoleChanged.Broadcast(SelectedRole);
		OnReadyChanged.Broadcast(bIsReady);
	}
}

void AThreadTalesPlayerState::Server_SetReady_Implementation(bool bInReady)
{
	if (bInReady && SelectedRole == EThreadTalesCharacterRole::None)
	{
		return;
	}

	if (bIsReady != bInReady)
	{
		bIsReady = bInReady;
		OnReadyChanged.Broadcast(bIsReady);
	}
}

void AThreadTalesPlayerState::OnRep_SelectedRole(EThreadTalesCharacterRole OldRole)
{
	OnRoleChanged.Broadcast(SelectedRole);
}

void AThreadTalesPlayerState::OnRep_IsReady(bool bOldReady)
{
	OnReadyChanged.Broadcast(bIsReady);
}

bool AThreadTalesPlayerState::IsRoleTakenByOther(EThreadTalesCharacterRole InRole) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	AGameStateBase* GameState = World->GetGameState();
	if (!GameState)
	{
		return false;
	}

	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (PS && PS != this)
		{
			AThreadTalesPlayerState* TTPS = Cast<AThreadTalesPlayerState>(PS);
			if (TTPS && TTPS->GetSelectedRole() == InRole)
			{
				return true;
			}
		}
	}

	return false;
}
