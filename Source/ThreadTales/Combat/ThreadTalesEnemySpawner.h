// Copyright ThreadTales. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/ThreadTalesTriggerableInterface.h"
#include "Combat/ThreadTalesSpawnerTypes.h"
#include "ThreadTalesEnemySpawner.generated.h"

class UBoxComponent;
class USphereComponent;
class UBillboardComponent;
class AThreadTalesCharacter;
class AThreadTalesEnemyCharacter;

/**
 * Universal Enemy Spawner Actor supporting Radius, Box, and Point areas,
 * multiple wave release modes (Burst, Staggered, Reinforcement),
 * and 2P Co-op trigger activation via collision, levers, or scripts.
 */
UCLASS(Blueprintable, ClassGroup = (ThreadTales), meta = (BlueprintSpawnableComponent))
class THREADTALES_API AThreadTalesEnemySpawner : public AActor, public IThreadTalesTriggerableInterface
{
	GENERATED_BODY()

public:
	AThreadTalesEnemySpawner();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	// ── IThreadTalesTriggerableInterface ─────────────────────────────────────
	virtual void ExecuteTrigger_Implementation(AActor* InstigatorActor) override;
	virtual void ExecuteResetTrigger_Implementation(AActor* InstigatorActor) override;
	virtual void ExecuteSetTriggerState_Implementation(bool bActive, AActor* InstigatorActor) override;

	// ── Control API ─────────────────────────────────────────────────────────
	/** Begins spawning the configured enemy wave */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Spawner")
	void StartSpawning();

	/** Stops spawning immediately and optionally destroys active spawned enemies */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Spawner")
	void StopSpawning(bool bKillExistingEnemies = false);

	/** Resets the spawner to its initial pre-triggered state */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Spawner")
	void ResetSpawner();

	/** Returns true if a wave is currently spawning or active */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Spawner")
	bool IsWaveActive() const { return bIsSpawningActive; }

	/** Returns the count of enemies currently alive in the world */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Spawner")
	int32 GetAliveEnemyCount() const;

	/** Returns the count of enemies pending to be spawned */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Spawner")
	int32 GetPendingSpawnCount() const;

	// ── Components ──────────────────────────────────────────────────────────
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Components")
	TObjectPtr<UBoxComponent> TriggerComponent;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Components")
	TObjectPtr<UBillboardComponent> EditorSprite;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Components")
	TObjectPtr<USphereComponent> AreaSphereComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Components")
	TObjectPtr<UBoxComponent> AreaBoxComponent;
#endif

	// ── Area Settings ───────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Area")
	ESpawnAreaShape AreaShape = ESpawnAreaShape::Radius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Area", meta = (ClampMin = "100.0", ClampMax = "10000.0", ToolTip = "Spawn radius in cm when AreaShape is Radius."))
	float SpawnRadius = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Area", meta = (ToolTip = "Spawn box extents in cm when AreaShape is Box."))
	FVector SpawnBoxExtent = FVector(800.0f, 800.0f, 150.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Area", meta = (ToolTip = "Explicit spawn point components or child transforms."))
	TArray<TObjectPtr<USceneComponent>> CustomSpawnPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Area", meta = (ClampMin = "50.0", ClampMax = "1000.0", ToolTip = "Minimum distance between spawned enemy origins."))
	float MinSeparationDistance = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Area", meta = (ToolTip = "Whether to project random spawn points onto NavMesh."))
	bool bProjectToNavMesh = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Area", meta = (ToolTip = "Vertical line trace downwards to snap enemy capsule base to floor."))
	bool bSnapToGround = true;

	// ── Wave Settings ───────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Wave", meta = (ToolTip = "List of enemy classes and counts composing the wave."))
	TArray<FThreadTalesSpawnEntry> EnemyEntries;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Wave")
	ESpawnWaveMode WaveMode = ESpawnWaveMode::Burst;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Wave", meta = (ClampMin = "0.1", ClampMax = "60.0", ToolTip = "Interval in seconds between individual spawns in Staggered mode."))
	float SpawnInterval = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Wave", meta = (ClampMin = "1", ClampMax = "50", ToolTip = "Max concurrent alive enemies permitted in Reinforcement mode."))
	int32 MaxConcurrentAlive = 3;

	// ── Trigger Settings ────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Trigger")
	ETriggerActivationMode ActivationMode = ETriggerActivationMode::OverlapAnyPlayer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Trigger", meta = (ToolTip = "If true, the spawner can only be triggered once."))
	bool bTriggerOnce = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Trigger", meta = (ClampMin = "0.0", ClampMax = "60.0", ToolTip = "Delay in seconds between trigger event and first enemy spawn."))
	float TriggerDelay = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Trigger", meta = (ToolTip = "Automatically trigger wave on BeginPlay."))
	bool bAutoActivateOnBeginPlay = false;

	/** External trigger actors (e.g. TriggerBox, TriggerVolume) that will activate this spawner when touched by a player */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Trigger", meta = (ToolTip = "External trigger actors on the level (e.g. TriggerBox) that will also activate this spawner when overlapped by a player."))
	TArray<TObjectPtr<AActor>> ExternalTriggerActors;

	// ── Clear Action Settings ───────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Events", meta = (ToolTip = "Actors to trigger via IThreadTalesTriggerableInterface when the entire wave is defeated."))
	TArray<TObjectPtr<AActor>> TargetActorsToTriggerOnClear;

	// ── Delegates ───────────────────────────────────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Spawner|Events")
	FOnEnemySpawnerWaveStartedDelegate OnWaveSpawnStarted;

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Spawner|Events")
	FOnEnemySpawnedDelegate OnEnemySpawned;

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Spawner|Events")
	FOnEnemyDefeatedDelegate OnEnemyDefeated;

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Spawner|Events")
	FOnEnemySpawnerWaveClearedDelegate OnWaveCleared;

protected:
	// ── Internal Helpers ────────────────────────────────────────────────────
	void InitializeWaveQueue();
	void ProcessNextSpawn();
	bool GenerateSpawnTransform(FTransform& OutTransform);
	void CheckWaveCleared();

	UFUNCTION()
	void HandleEnemyDeath(AActor* DeadActor, AActor* KillerActor);

	UFUNCTION()
	void HandleEnemyDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void OnExternalActorBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);

private:
	UPROPERTY()
	bool bIsSpawningActive = false;

	UPROPERTY()
	bool bHasBeenTriggered = false;

	UPROPERTY()
	TArray<FThreadTalesSpawnEntry> PendingSpawnQueue;

	UPROPERTY()
	TArray<TWeakObjectPtr<AThreadTalesEnemyCharacter>> ActiveEnemies;

	UPROPERTY()
	TSet<TWeakObjectPtr<AThreadTalesCharacter>> OverlappingPlayers;

	FTimerHandle SpawnTimerHandle;
	FTimerHandle TriggerDelayTimerHandle;
};
