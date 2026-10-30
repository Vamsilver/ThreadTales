// Copyright Epic Games, Inc. All Rights Reserved.


#include "ThreadTalesPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "UI/ThreadTalesHUDWidget.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesCharacterDebug.h"
#include "Core/ThreadTalesSpawnPoint.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "ThreadTales.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "UObject/ConstructorHelpers.h"

AThreadTalesPlayerController::AThreadTalesPlayerController()
{
	static ConstructorHelpers::FClassFinder<UThreadTalesHUDWidget> HUDWidgetBPClass(TEXT("/Game/UI/WBP_PlayerHUD"));
	if (HUDWidgetBPClass.Class)
	{
		PlayerHUDWidgetClass = HUDWidgetBPClass.Class;
	}
	else
	{
		PlayerHUDWidgetClass = UThreadTalesHUDWidget::StaticClass();
	}
}

void AThreadTalesPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only spawn touch controls on local player controllers
	if (ShouldUseTouchControls() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogThreadTales, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}

	if (IsLocalPlayerController())
	{
		EnsurePlayerHUDCreated(Cast<AThreadTalesCharacter>(GetPawn()));
	}
}

void AThreadTalesPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Reset camera and control rotation to spawn point orientation so character is never tilted or backwards
	if (CurrentSpawnPoint)
	{
		SetControlRotation(FRotator(0.0f, CurrentSpawnPoint->GetSpawnTransform().Rotator().Yaw, 0.0f));
	}

	if (IsLocalPlayerController())
	{
		EnsurePlayerHUDCreated(Cast<AThreadTalesCharacter>(InPawn));
	}
}

void AThreadTalesPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);

	if (IsLocalPlayerController())
	{
		EnsurePlayerHUDCreated(Cast<AThreadTalesCharacter>(P));
	}
}

UThreadTalesHUDWidget* AThreadTalesPlayerController::EnsurePlayerHUDCreated(AThreadTalesCharacter* InCharacter)
{
	if (!IsLocalPlayerController())
	{
		return nullptr;
	}

	if (!PlayerHUDWidget)
	{
		TSubclassOf<UThreadTalesHUDWidget> ClassToSpawn = PlayerHUDWidgetClass ? PlayerHUDWidgetClass : TSubclassOf<UThreadTalesHUDWidget>(UThreadTalesHUDWidget::StaticClass());
		PlayerHUDWidget = CreateWidget<UThreadTalesHUDWidget>(this, ClassToSpawn);
		if (PlayerHUDWidget)
		{
			PlayerHUDWidget->AddToPlayerScreen(10);
		}
	}

	if (PlayerHUDWidget)
	{
		AThreadTalesCharacter* TargetChar = InCharacter ? InCharacter : Cast<AThreadTalesCharacter>(GetPawn());
		if (TargetChar)
		{
			PlayerHUDWidget->InitializeForCharacter(TargetChar);
		}
	}

	return PlayerHUDWidget;
}

void AThreadTalesPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool AThreadTalesPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void AThreadTalesPlayerController::TogglePlayableCharacter()
{
#if !UE_BUILD_SHIPPING
	if (PlayableCharacterClasses.Num() == 0)
	{
		UE_LOG(LogThreadTales, Warning, TEXT("[PlayerController] TogglePlayableCharacter: PlayableCharacterClasses list is empty! Configure it in BP_ThreadTalesPlayerController Details."));
		return;
	}

	AThreadTalesCharacter* SourceChar = Cast<AThreadTalesCharacter>(GetPawn());
	if (!SourceChar)
	{
		return;
	}

	int32 CurrentIndex = -1;
	for (int32 i = 0; i < PlayableCharacterClasses.Num(); ++i)
	{
		if (PlayableCharacterClasses[i] && SourceChar->IsA(PlayableCharacterClasses[i]))
		{
			CurrentIndex = i;
			break;
		}
	}

	const int32 NextIndex = (CurrentIndex + 1) % PlayableCharacterClasses.Num();
	TSubclassOf<AThreadTalesCharacter> TargetClass = PlayableCharacterClasses[NextIndex];
	if (TargetClass)
	{
		UThreadTalesCharacterDebug::SwitchCharacter(SourceChar, TargetClass);
	}
#endif
}

void AThreadTalesPlayerController::SetCurrentSpawnPoint(AThreadTalesSpawnPoint* NewSpawnPoint)
{
	CurrentSpawnPoint = NewSpawnPoint;
}

FTransform AThreadTalesPlayerController::GetActiveSpawnTransform() const
{
	if (CurrentSpawnPoint)
	{
		return CurrentSpawnPoint->GetSpawnTransform();
	}

	// Canonical fallback: Look for a PlayerStart in the world
	if (UWorld* World = GetWorld())
	{
		TArray<AActor*> PlayerStarts;
		UGameplayStatics::GetAllActorsOfClass(World, APlayerStart::StaticClass(), PlayerStarts);
		if (PlayerStarts.Num() > 0 && PlayerStarts[0])
		{
			FTransform StartTransform = PlayerStarts[0]->GetActorTransform();
			FRotator CleanRotator(0.0f, StartTransform.Rotator().Yaw, 0.0f);
			StartTransform.SetRotation(FQuat(CleanRotator));
			return StartTransform;
		}
	}

	return FTransform::Identity;
}
