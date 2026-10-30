// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "ThreadTalesAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class UAISenseConfig_Damage;
class AThreadTalesCharacter;
class UThreadTalesEnemyConfig;

/**
 * Controller driving Enemy AI perception, 2P Co-op Threat evaluation, and combat navigation.
 */
UCLASS()
class THREADTALES_API AThreadTalesAIController : public AAIController
{
	GENERATED_BODY()

public:
	AThreadTalesAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaTime) override;

	/** Adds threat points to a player (e.g. upon taking damage) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|AI|Threat")
	void RegisterThreat(AThreadTalesCharacter* InstigatorChar, float ThreatAmount);

	/** Returns current high-priority combat target */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|AI|Threat")
	AThreadTalesCharacter* GetCombatTarget() const;

	/** Explicitly assigns combat target */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|AI|Threat")
	void SetCombatTarget(AThreadTalesCharacter* NewTarget);

	/** Clears current target and resets threat */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|AI|Threat")
	void ClearCombatTarget();

	/** Returns true if this AI currently has an active combat target */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|AI|Threat")
	bool HasValidTarget() const;

	/** Evaluates threat across both co-op players and picks the optimal target with hysteresis */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|AI|Threat")
	AThreadTalesCharacter* EvaluateBestTarget();

protected:
	/** Callback when AI Perception receives sensory stimuli */
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/** Decays accumulated threat points over time */
	void DecayThreat(float DeltaTime);

	/** Fetches enemy config from possessed character */
	const UThreadTalesEnemyConfig* GetEnemyConfig() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIPerceptionComponent> EnemyPerceptionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAISenseConfig_Damage> DamageConfig;

private:
	/** Table mapping co-op players to their threat scores */
	TMap<TWeakObjectPtr<AThreadTalesCharacter>, float> ThreatTable;

	/** Currently focused target */
	TWeakObjectPtr<AThreadTalesCharacter> CurrentCombatTarget;

	/** Known actors sensed by perception */
	TSet<TWeakObjectPtr<AThreadTalesCharacter>> VisiblePlayers;
};
