// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesGameMode.h"
#include "ThreadTalesPlayerState.h"
#include "ThreadTalesSessionSubsystem.h"
#include "Core/ThreadTalesSpawnPoint.h"
#include "Core/ThreadTalesPlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "GameMapsSettings.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerController.h"
#include "ThreadTales.h"

AThreadTalesGameMode::AThreadTalesGameMode()
{
	bUseSeamlessTravel = true;
	PlayerStateClass = AThreadTalesPlayerState::StaticClass();

	static ConstructorHelpers::FClassFinder<APawn> MalePawnFinder(TEXT("/Game/Blueprints/Characters/BP_HeroMale.BP_HeroMale_C"));
	if (MalePawnFinder.Succeeded())
	{
		HeroMaleClass = MalePawnFinder.Class;
		DefaultPawnClass = HeroMaleClass;
	}

	static ConstructorHelpers::FClassFinder<APawn> FemalePawnFinder(TEXT("/Game/Blueprints/Characters/BP_HeroFemale.BP_HeroFemale_C"));
	if (FemalePawnFinder.Succeeded())
	{
		HeroFemaleClass = FemalePawnFinder.Class;
	}
}

void AThreadTalesGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && bAutoActivateInitialSpawnPoint)
	{
		if (UWorld* World = GetWorld())
		{
			AThreadTalesSpawnPoint* BestInitialPoint = nullptr;
			float ClosestDistSq = TNumericLimits<float>::Max();

			// 1. Look for explicitly marked initial spawn point
			for (TActorIterator<AThreadTalesSpawnPoint> It(World); It; ++It)
			{
				AThreadTalesSpawnPoint* Point = *It;
				if (Point && Point->IsInitialSpawnPoint())
				{
					BestInitialPoint = Point;
					break;
				}
			}

			// 2. Fallback: find spawn point closest to any PlayerStart
			if (!BestInitialPoint)
			{
				TArray<APlayerStart*> Starts;
				for (TActorIterator<APlayerStart> StartIt(World); StartIt; ++StartIt)
				{
					if (*StartIt)
					{
						Starts.Add(*StartIt);
					}
				}

				if (Starts.Num() > 0)
				{
					for (TActorIterator<AThreadTalesSpawnPoint> It(World); It; ++It)
					{
						AThreadTalesSpawnPoint* Point = *It;
						if (!Point)
						{
							continue;
						}

						for (APlayerStart* Start : Starts)
						{
							const float DistSq = FVector::DistSquared(Point->GetActorLocation(), Start->GetActorLocation());
							if (DistSq < ClosestDistSq)
							{
								ClosestDistSq = DistSq;
								BestInitialPoint = Point;
							}
						}
					}
				}
			}

			if (BestInitialPoint)
			{
				SetActiveSpawnPoint(BestInitialPoint);
			}
		}
	}
}

AThreadTalesSpawnPoint* AThreadTalesGameMode::GetActiveSpawnPoint() const
{
	return ActiveSpawnPoint.Get();
}

void AThreadTalesGameMode::SetActiveSpawnPoint(AThreadTalesSpawnPoint* NewSpawnPoint)
{
	if (!HasAuthority() || ActiveSpawnPoint.Get() == NewSpawnPoint)
	{
		return;
	}

	// 1. Deactivate old spawn point
	if (AThreadTalesSpawnPoint* OldSpawnPoint = ActiveSpawnPoint.Get())
	{
		OldSpawnPoint->SetSpawnPointActive(false);
	}

	// 2. Assign new active spawn point
	ActiveSpawnPoint = NewSpawnPoint;

	if (NewSpawnPoint)
	{
		NewSpawnPoint->SetSpawnPointActive(true);
	}

	// 3. Update all connected player controllers for 2P co-op
	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (AThreadTalesPlayerController* PC = Cast<AThreadTalesPlayerController>(It->Get()))
			{
				PC->SetCurrentSpawnPoint(NewSpawnPoint);
			}
		}
	}

	UE_LOG(LogThreadTales, Display, TEXT("[GameMode] Team active spawn point changed to '%s'"),
		NewSpawnPoint ? *NewSpawnPoint->GetName() : TEXT("None"));
}

UClass* AThreadTalesGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (InController)
	{
		AThreadTalesPlayerState* TTPS = InController->GetPlayerState<AThreadTalesPlayerState>();
		APlayerController* PC = Cast<APlayerController>(InController);
		int32 PlayerIndex = INDEX_NONE;
		if (PC)
		{
			ULocalPlayer* LP = PC->GetLocalPlayer();
			PlayerIndex = LP ? LP->GetControllerId() : (GetWorld() && InController == GetWorld()->GetFirstPlayerController() ? 0 : 1);
		}

		EThreadTalesCharacterRole SelectedCharRole = TTPS ? TTPS->GetSelectedRole() : EThreadTalesCharacterRole::None;

		// Recover role from SessionSubsystem if reset to None (PIE Hard Travel support)
		if (SelectedCharRole == EThreadTalesCharacterRole::None && PlayerIndex != INDEX_NONE)
		{
			if (UGameInstance* GI = GetGameInstance())
			{
				if (UThreadTalesSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UThreadTalesSessionSubsystem>())
				{
					SelectedCharRole = SessionSubsystem->GetLocalPlayerRole(PlayerIndex);
					if (TTPS && SelectedCharRole != EThreadTalesCharacterRole::None)
					{
						TTPS->Server_SelectRole(SelectedCharRole);
						TTPS->Server_SetReady(true);
					}
				}
			}
		}

		if (SelectedCharRole == EThreadTalesCharacterRole::Archer_Female && HeroFemaleClass)
		{
			return HeroFemaleClass.Get();
		}
		if (SelectedCharRole == EThreadTalesCharacterRole::Swordsman_Male && HeroMaleClass)
		{
			return HeroMaleClass.Get();
		}

		// Fallback for standalone/direct PIE testing without lobby selection
		if (PC)
		{
			const bool bIsFirstPlayer = (PlayerIndex == 0);
			if (bIsFirstPlayer)
			{
				if (HeroMaleClass)
				{
					return HeroMaleClass.Get();
				}
			}
			else
			{
				if (HeroFemaleClass)
				{
					return HeroFemaleClass.Get();
				}
			}
		}
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void AThreadTalesGameMode::GenericPlayerInitialization(AController* C)
{
	Super::GenericPlayerInitialization(C);

	APlayerController* PC = Cast<APlayerController>(C);
	if (PC && PC->IsLocalPlayerController())
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(false);
	}

	// Force vertical two-player split-screen layout (P1 Left, P2 Right)
	if (UGameMapsSettings* Settings = GetMutableDefault<UGameMapsSettings>())
	{
		Settings->bUseSplitscreen = true;
		Settings->TwoPlayerSplitscreenLayout = ETwoPlayerSplitScreenType::Vertical;
	}

	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			ViewportClient->SetForceDisableSplitscreen(false);
			ViewportClient->UpdateActiveSplitscreenType();
		}
	}

	// In local co-op, ensure Player 2 is created if Player 1 had a role assigned before travel
	if (C && C == GetWorld()->GetFirstPlayerController())
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UThreadTalesSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UThreadTalesSessionSubsystem>())
			{
				if (SessionSubsystem->GetLocalPlayerRole(1) != EThreadTalesCharacterRole::None && !SessionSubsystem->IsOnlineSessionActive())
				{
					if (GetWorld()->GetNumPlayerControllers() < 2 && !SessionSubsystem->IsLocalCoopActive())
					{
						SessionSubsystem->StartLocalCoop();
					}
				}
			}
		}
	}

	// Sync active team spawn point to player controller
	if (AThreadTalesPlayerController* TTPC = Cast<AThreadTalesPlayerController>(C))
	{
		if (ActiveSpawnPoint.IsValid())
		{
			TTPC->SetCurrentSpawnPoint(ActiveSpawnPoint.Get());
		}
	}
}

void AThreadTalesGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	APlayerController* PC = Cast<APlayerController>(NewPlayer);
	if (PC && PC->IsLocalPlayerController())
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(false);
	}
}

AActor* AThreadTalesGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	int32 PlayerIndex = 0;
	if (APlayerController* PC = Cast<APlayerController>(Player))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			PlayerIndex = LP->GetControllerId();
		}
		else if (GetWorld() && Player != GetWorld()->GetFirstPlayerController())
		{
			PlayerIndex = 1;
		}
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// 1. Check for specific PlayerStart tagged or named P1 / P2
	const FName TargetTag = (PlayerIndex == 0) ? FName(TEXT("P1")) : FName(TEXT("P2"));
	const FString TargetName = (PlayerIndex == 0) ? TEXT("P1") : TEXT("P2");

	APlayerStart* TaggedStart = nullptr;
	TArray<APlayerStart*> AllStarts;

	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		APlayerStart* Start = *It;
		if (!Start)
		{
			continue;
		}

		if (Start->PlayerStartTag == TargetTag || Start->GetName().Contains(TargetName))
		{
			TaggedStart = Start;
			break;
		}

		AllStarts.Add(Start);
	}

	if (TaggedStart)
	{
		return TaggedStart;
	}

	// 2. Deterministic spatial sort by X ascending: smaller X = Left = P1, larger X = Right = P2
	if (AllStarts.Num() >= 2)
	{
		AllStarts.Sort([](const APlayerStart& A, const APlayerStart& B)
		{
			return A.GetActorLocation().X < B.GetActorLocation().X;
		});

		if (AllStarts.IsValidIndex(PlayerIndex))
		{
			return AllStarts[PlayerIndex];
		}
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}
