// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ThreadTalesAIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesEnemyCharacter.h"
#include "AI/ThreadTalesEnemyConfig.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "ThreadTales.h"

AThreadTalesAIController::AThreadTalesAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	bWantsPlayerState = false;

	EnemyPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("EnemyPerceptionComponent"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 1600.0f;
	SightConfig->LoseSightRadius = 2200.0f;
	SightConfig->PeripheralVisionAngleDegrees = 65.0f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->SetMaxAge(5.0f);
	EnemyPerceptionComponent->ConfigureSense(*SightConfig);
	EnemyPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());

	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = 1500.0f;
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	HearingConfig->SetMaxAge(3.0f);
	EnemyPerceptionComponent->ConfigureSense(*HearingConfig);

	DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageConfig"));
	DamageConfig->SetMaxAge(5.0f);
	EnemyPerceptionComponent->ConfigureSense(*DamageConfig);

	EnemyPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AThreadTalesAIController::HandleTargetPerceptionUpdated);
}

void AThreadTalesAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ClearCombatTarget();
	EvaluateBestTarget();
}

void AThreadTalesAIController::OnUnPossess()
{
	ClearCombatTarget();
	Super::OnUnPossess();
}

void AThreadTalesAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	DecayThreat(DeltaTime);
	EvaluateBestTarget();

	if (AThreadTalesCharacter* Target = GetCombatTarget())
	{
		SetFocus(Target);
	}
	else
	{
		ClearFocus(EAIFocusPriority::Gameplay);
	}
}

void AThreadTalesAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	AThreadTalesCharacter* SensedChar = Cast<AThreadTalesCharacter>(Actor);
	if (!SensedChar || !SensedChar->IsAlive() || SensedChar == GetPawn() || !SensedChar->IsPlayerControlled())
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		VisiblePlayers.Add(SensedChar);

		const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(GetPawn());
		const float CfgDamageThreat = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->ThreatSettings.DamageSenseThreat : 50.0f;
		const float CfgSightThreat = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->ThreatSettings.SightSenseThreat : 15.0f;
		const float ThreatAdd = (Stimulus.Type == UAISense::GetSenseID<UAISense_Damage>()) ? CfgDamageThreat : CfgSightThreat;
		RegisterThreat(SensedChar, ThreatAdd);

		UE_LOG(LogThreadTalesCombat, Display, TEXT("[AIController] '%s' SENSED '%s' via Sense ID %d (Threat +%.1f)"),
			*GetName(), *SensedChar->GetName(), (int32)Stimulus.Type.Index, ThreatAdd);
	}
	else
	{
		VisiblePlayers.Remove(SensedChar);
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[AIController] '%s' LOST sight of '%s'"),
			*GetName(), *SensedChar->GetName());
	}
}

void AThreadTalesAIController::RegisterThreat(AThreadTalesCharacter* InstigatorChar, float ThreatAmount)
{
	if (!InstigatorChar || !InstigatorChar->IsAlive() || ThreatAmount <= 0.0f)
	{
		return;
	}

	float& ThreatVal = ThreatTable.FindOrAdd(InstigatorChar);
	ThreatVal += ThreatAmount;
}

AThreadTalesCharacter* AThreadTalesAIController::GetCombatTarget() const
{
	return CurrentCombatTarget.IsValid() ? CurrentCombatTarget.Get() : nullptr;
}

void AThreadTalesAIController::SetCombatTarget(AThreadTalesCharacter* NewTarget)
{
	AThreadTalesCharacter* OldTarget = CurrentCombatTarget.IsValid() ? CurrentCombatTarget.Get() : nullptr;
	if (NewTarget && NewTarget->IsAlive())
	{
		CurrentCombatTarget = NewTarget;
		if (OldTarget != NewTarget)
		{
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[AIController] '%s' ACQUIRED combat target '%s'"),
				*GetName(), *NewTarget->GetName());
		}
	}
	else
	{
		ClearCombatTarget();
	}
}

void AThreadTalesAIController::ClearCombatTarget()
{
	CurrentCombatTarget = nullptr;
	ClearFocus(EAIFocusPriority::Gameplay);
}

bool AThreadTalesAIController::HasValidTarget() const
{
	return CurrentCombatTarget.IsValid() && CurrentCombatTarget->IsAlive();
}

const UThreadTalesEnemyConfig* AThreadTalesAIController::GetEnemyConfig() const
{
	if (const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(GetPawn()))
	{
		return EnemyChar->GetEnemyConfig();
	}
	return nullptr;
}

AThreadTalesCharacter* AThreadTalesAIController::EvaluateBestTarget()
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		ClearCombatTarget();
		return nullptr;
	}

	const FVector PawnLocation = ControlledPawn->GetActorLocation();
	AThreadTalesCharacter* BestCandidate = nullptr;
	float BestScore = -1.0f;

	const float DistanceWeight = 2000.0f;
	const float SwitchHysteresis = 1.25f;

	// Fallback discovery: If ThreatTable has no entries, search for nearest alive player within SightRadius
	if (ThreatTable.IsEmpty())
	{
		const float SearchRadius = SightConfig ? SightConfig->SightRadius : 1600.0f;
		AThreadTalesCharacter* ClosestPlayer = nullptr;
		float ClosestDistSq = FMath::Square(SearchRadius);

		if (UWorld* World = GetWorld())
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (APlayerController* PC = It->Get())
				{
					if (AThreadTalesCharacter* PlayerChar = Cast<AThreadTalesCharacter>(PC->GetPawn()))
					{
						if (PlayerChar->IsAlive())
						{
							const float DistSq = FVector::DistSquared(PawnLocation, PlayerChar->GetActorLocation());
							if (DistSq < ClosestDistSq)
							{
								ClosestDistSq = DistSq;
								ClosestPlayer = PlayerChar;
							}
						}
					}
				}
			}
		}

		if (ClosestPlayer)
		{
			RegisterThreat(ClosestPlayer, 10.0f);
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[AIController] Auto-acquired closest player '%s' (Dist: %.1f cm)"), 
				*ClosestPlayer->GetName(), FMath::Sqrt(ClosestDistSq));
		}
	}

	for (auto It = ThreatTable.CreateIterator(); It; ++It)
	{
		AThreadTalesCharacter* PlayerChar = It.Key().Get();
		if (!PlayerChar || !PlayerChar->IsAlive() || !PlayerChar->IsPlayerControlled())
		{
			It.RemoveCurrent();
			continue;
		}

		const float Dist = FVector::Dist(PawnLocation, PlayerChar->GetActorLocation());
		const float ProximityScore = (Dist > 10.0f) ? (DistanceWeight / Dist) : 100.0f;
		const float ThreatScore = It.Value();
		const float VisibilityBonus = VisiblePlayers.Contains(PlayerChar) ? 25.0f : 0.0f;

		const float TotalScore = ProximityScore + ThreatScore + VisibilityBonus;
		if (TotalScore > BestScore)
		{
			BestScore = TotalScore;
			BestCandidate = PlayerChar;
		}
	}

	// Apply target hysteresis to avoid frequent target thrashing
	if (CurrentCombatTarget.IsValid() && CurrentCombatTarget->IsAlive())
	{
		AThreadTalesCharacter* Cur = CurrentCombatTarget.Get();
		float* CurThreat = ThreatTable.Find(Cur);
		const float CurDist = FVector::Dist(PawnLocation, Cur->GetActorLocation());
		const float CurProximity = (CurDist > 10.0f) ? (DistanceWeight / CurDist) : 100.0f;
		const float CurVisibility = VisiblePlayers.Contains(Cur) ? 25.0f : 0.0f;
		const float CurScore = CurProximity + (CurThreat ? *CurThreat : 0.0f) + CurVisibility;

		if (BestCandidate && BestCandidate != Cur)
		{
			if (BestScore < CurScore * SwitchHysteresis)
			{
				BestCandidate = Cur;
			}
		}
	}

	SetCombatTarget(BestCandidate);
	return BestCandidate;
}

void AThreadTalesAIController::DecayThreat(float DeltaTime)
{
	const float DecayRate = 5.0f;
	const float DecayAmount = DecayRate * DeltaTime;

	for (auto It = ThreatTable.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !It.Key()->IsAlive())
		{
			It.RemoveCurrent();
			continue;
		}

		It.Value() = FMath::Max(0.0f, It.Value() - DecayAmount);
	}
}
