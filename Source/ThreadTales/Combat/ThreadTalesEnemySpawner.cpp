// Copyright ThreadTales. All Rights Reserved.

#include "Combat/ThreadTalesEnemySpawner.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BillboardComponent.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesEnemyCharacter.h"
#include "NavigationSystem.h"
#include "Engine/World.h"
#include "TimerManager.h"

AThreadTalesEnemySpawner::AThreadTalesEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	TriggerComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerComponent"));
	TriggerComponent->SetupAttachment(SceneRoot);
	TriggerComponent->SetBoxExtent(FVector(500.0f, 500.0f, 200.0f));
	TriggerComponent->SetCollisionProfileName(TEXT("Trigger"));
	TriggerComponent->SetGenerateOverlapEvents(true);

#if WITH_EDITORONLY_DATA
	EditorSprite = CreateDefaultSubobject<UBillboardComponent>(TEXT("EditorSprite"));
	EditorSprite->SetupAttachment(SceneRoot);
	EditorSprite->bIsScreenSizeScaled = true;

	AreaSphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("AreaSphereComponent"));
	AreaSphereComponent->SetupAttachment(SceneRoot);
	AreaSphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AreaSphereComponent->SetHiddenInGame(true);
	AreaSphereComponent->SetSphereRadius(SpawnRadius);

	AreaBoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("AreaBoxComponent"));
	AreaBoxComponent->SetupAttachment(SceneRoot);
	AreaBoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AreaBoxComponent->SetHiddenInGame(true);
	AreaBoxComponent->SetBoxExtent(SpawnBoxExtent);
#endif
}

void AThreadTalesEnemySpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITORONLY_DATA
	if (AreaSphereComponent && AreaBoxComponent)
	{
		const bool bIsRadius = (AreaShape == ESpawnAreaShape::Radius);
		const bool bIsBox = (AreaShape == ESpawnAreaShape::Box);

		AreaSphereComponent->SetVisibility(bIsRadius);
		AreaSphereComponent->SetSphereRadius(SpawnRadius);

		AreaBoxComponent->SetVisibility(bIsBox);
		AreaBoxComponent->SetBoxExtent(SpawnBoxExtent);
	}
#endif
}

void AThreadTalesEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerComponent)
	{
		TriggerComponent->OnComponentBeginOverlap.AddDynamic(this, &AThreadTalesEnemySpawner::OnTriggerBeginOverlap);
		TriggerComponent->OnComponentEndOverlap.AddDynamic(this, &AThreadTalesEnemySpawner::OnTriggerEndOverlap);
	}

	for (TObjectPtr<AActor>& ExtActor : ExternalTriggerActors)
	{
		if (ExtActor)
		{
			ExtActor->OnActorBeginOverlap.AddDynamic(this, &AThreadTalesEnemySpawner::OnExternalActorBeginOverlap);
		}
	}

	if (HasAuthority() && bAutoActivateOnBeginPlay)
	{
		StartSpawning();
	}
}

void AThreadTalesEnemySpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (TObjectPtr<AActor>& ExtActor : ExternalTriggerActors)
	{
		if (ExtActor)
		{
			ExtActor->OnActorBeginOverlap.RemoveDynamic(this, &AThreadTalesEnemySpawner::OnExternalActorBeginOverlap);
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		World->GetTimerManager().ClearTimer(TriggerDelayTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AThreadTalesEnemySpawner::ExecuteTrigger_Implementation(AActor* InstigatorActor)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bTriggerOnce && bHasBeenTriggered)
	{
		return;
	}

	if (TriggerDelay > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				TriggerDelayTimerHandle,
				this,
				&AThreadTalesEnemySpawner::StartSpawning,
				TriggerDelay,
				false
			);
		}
	}
	else
	{
		StartSpawning();
	}
}

void AThreadTalesEnemySpawner::ExecuteResetTrigger_Implementation(AActor* InstigatorActor)
{
	ResetSpawner();
}

void AThreadTalesEnemySpawner::ExecuteSetTriggerState_Implementation(bool bActive, AActor* InstigatorActor)
{
	if (bActive)
	{
		ExecuteTrigger_Implementation(InstigatorActor);
	}
	else
	{
		StopSpawning(false);
	}
}

void AThreadTalesEnemySpawner::StartSpawning()
{
	if (!HasAuthority())
	{
		return;
	}

	if (bTriggerOnce && bHasBeenTriggered)
	{
		return;
	}

	bHasBeenTriggered = true;
	bIsSpawningActive = true;

	InitializeWaveQueue();
	OnWaveSpawnStarted.Broadcast(this);

	ProcessNextSpawn();
}

void AThreadTalesEnemySpawner::StopSpawning(bool bKillExistingEnemies)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		World->GetTimerManager().ClearTimer(TriggerDelayTimerHandle);
	}

	bIsSpawningActive = false;
	PendingSpawnQueue.Empty();

	if (bKillExistingEnemies)
	{
		for (TWeakObjectPtr<AThreadTalesEnemyCharacter>& EnemyPtr : ActiveEnemies)
		{
			if (AThreadTalesEnemyCharacter* Enemy = EnemyPtr.Get())
			{
				Enemy->Destroy();
			}
		}
		ActiveEnemies.Empty();
	}
}

void AThreadTalesEnemySpawner::ResetSpawner()
{
	StopSpawning(true);
	bHasBeenTriggered = false;
	OverlappingPlayers.Empty();
}

int32 AThreadTalesEnemySpawner::GetAliveEnemyCount() const
{
	int32 AliveCount = 0;
	for (const TWeakObjectPtr<AThreadTalesEnemyCharacter>& EnemyPtr : ActiveEnemies)
	{
		if (const AThreadTalesEnemyCharacter* Enemy = EnemyPtr.Get())
		{
			if (Enemy->IsAlive())
			{
				AliveCount++;
			}
		}
	}
	return AliveCount;
}

int32 AThreadTalesEnemySpawner::GetPendingSpawnCount() const
{
	int32 TotalPending = 0;
	for (const FThreadTalesSpawnEntry& Entry : PendingSpawnQueue)
	{
		TotalPending += Entry.Count;
	}
	return TotalPending;
}

void AThreadTalesEnemySpawner::InitializeWaveQueue()
{
	PendingSpawnQueue.Empty();
	for (const FThreadTalesSpawnEntry& Entry : EnemyEntries)
	{
		if (Entry.EnemyClass && Entry.Count > 0)
		{
			PendingSpawnQueue.Add(Entry);
		}
	}
}

void AThreadTalesEnemySpawner::ProcessNextSpawn()
{
	if (!HasAuthority() || !bIsSpawningActive)
	{
		return;
	}

	if (WaveMode == ESpawnWaveMode::Reinforcement)
	{
		if (GetAliveEnemyCount() >= MaxConcurrentAlive)
		{
			return;
		}
	}

	if (PendingSpawnQueue.Num() == 0)
	{
		CheckWaveCleared();
		return;
	}

	FThreadTalesSpawnEntry& CurrentEntry = PendingSpawnQueue[0];
	if (CurrentEntry.Count <= 0)
	{
		PendingSpawnQueue.RemoveAt(0);
		ProcessNextSpawn();
		return;
	}

	FTransform SpawnTransform;
	if (GenerateSpawnTransform(SpawnTransform))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = GetInstigator();
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		if (UWorld* World = GetWorld())
		{
			AThreadTalesEnemyCharacter* SpawnedEnemy = World->SpawnActor<AThreadTalesEnemyCharacter>(
				CurrentEntry.EnemyClass,
				SpawnTransform,
				SpawnParams
			);

			if (SpawnedEnemy)
			{
				ActiveEnemies.Add(SpawnedEnemy);
				SpawnedEnemy->OnCharacterDeath.AddDynamic(this, &AThreadTalesEnemySpawner::HandleEnemyDeath);
				SpawnedEnemy->OnDestroyed.AddDynamic(this, &AThreadTalesEnemySpawner::HandleEnemyDestroyed);

				OnEnemySpawned.Broadcast(this, SpawnedEnemy);
				CurrentEntry.Count--;
			}
		}
	}

	if (CurrentEntry.Count <= 0)
	{
		PendingSpawnQueue.RemoveAt(0);
	}

	if (WaveMode == ESpawnWaveMode::Burst)
	{
		if (PendingSpawnQueue.Num() > 0)
		{
			ProcessNextSpawn();
		}
	}
	else if (WaveMode == ESpawnWaveMode::Staggered)
	{
		if (PendingSpawnQueue.Num() > 0 && GetWorld())
		{
			GetWorld()->GetTimerManager().SetTimer(
				SpawnTimerHandle,
				this,
				&AThreadTalesEnemySpawner::ProcessNextSpawn,
				FMath::Max(0.1f, SpawnInterval),
				false
			);
		}
	}
	else if (WaveMode == ESpawnWaveMode::Reinforcement)
	{
		if (PendingSpawnQueue.Num() > 0 && GetAliveEnemyCount() < MaxConcurrentAlive)
		{
			ProcessNextSpawn();
		}
	}

	CheckWaveCleared();
}

bool AThreadTalesEnemySpawner::GenerateSpawnTransform(FTransform& OutTransform)
{
	FVector Origin = GetActorLocation();
	FVector CandidateLoc = Origin;
	FRotator CandidateRot = GetActorRotation();

	if (AreaShape == ESpawnAreaShape::CustomPoints && CustomSpawnPoints.Num() > 0)
	{
		int32 Idx = FMath::RandRange(0, CustomSpawnPoints.Num() - 1);
		if (USceneComponent* Pt = CustomSpawnPoints[Idx])
		{
			CandidateLoc = Pt->GetComponentLocation();
			CandidateRot = Pt->GetComponentRotation();
		}
	}
	else if (AreaShape == ESpawnAreaShape::Box)
	{
		FVector BoxMin = Origin - SpawnBoxExtent;
		FVector BoxMax = Origin + SpawnBoxExtent;
		CandidateLoc = FMath::RandPointInBox(FBox(BoxMin, BoxMax));
	}
	else // Radius
	{
		FVector2D RandCircle = FMath::RandPointInCircle(SpawnRadius);
		CandidateLoc = Origin + FVector(RandCircle.X, RandCircle.Y, 0.0f);
	}

	if (bProjectToNavMesh)
	{
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			FNavLocation ProjectedNavLoc;
			FVector QueryExtent(200.0f, 200.0f, 500.0f);
			if (NavSys->ProjectPointToNavigation(CandidateLoc, ProjectedNavLoc, QueryExtent))
			{
				CandidateLoc = ProjectedNavLoc.Location;
			}
		}
	}

	if (bSnapToGround)
	{
		FHitResult HitResult;
		FVector TraceStart = CandidateLoc + FVector(0.0f, 0.0f, 300.0f);
		FVector TraceEnd = CandidateLoc - FVector(0.0f, 0.0f, 600.0f);
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SpawnerGroundTrace), false, this);
		QueryParams.AddIgnoredActor(this);

		if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
		{
			CandidateLoc.Z = HitResult.ImpactPoint.Z + 10.0f;
		}
	}

	OutTransform = FTransform(CandidateRot, CandidateLoc);
	return true;
}

void AThreadTalesEnemySpawner::HandleEnemyDeath(AActor* DeadActor, AActor* KillerActor)
{
	AThreadTalesEnemyCharacter* DefeatedEnemy = Cast<AThreadTalesEnemyCharacter>(DeadActor);
	int32 RemainingCount = GetAliveEnemyCount() + GetPendingSpawnCount();

	OnEnemyDefeated.Broadcast(this, DefeatedEnemy, RemainingCount);

	if (WaveMode == ESpawnWaveMode::Reinforcement && PendingSpawnQueue.Num() > 0)
	{
		ProcessNextSpawn();
	}

	CheckWaveCleared();
}

void AThreadTalesEnemySpawner::HandleEnemyDestroyed(AActor* DestroyedActor)
{
	ActiveEnemies.RemoveAll([DestroyedActor](const TWeakObjectPtr<AThreadTalesEnemyCharacter>& Ptr)
	{
		return !Ptr.IsValid() || Ptr.Get() == DestroyedActor;
	});

	CheckWaveCleared();
}

void AThreadTalesEnemySpawner::CheckWaveCleared()
{
	if (!bIsSpawningActive)
	{
		return;
	}

	if (GetPendingSpawnCount() == 0 && GetAliveEnemyCount() == 0)
	{
		bIsSpawningActive = false;
		OnWaveCleared.Broadcast(this);

		for (AActor* TargetActor : TargetActorsToTriggerOnClear)
		{
			if (TargetActor && TargetActor->GetClass()->ImplementsInterface(UThreadTalesTriggerableInterface::StaticClass()))
			{
				IThreadTalesTriggerableInterface::Execute_ExecuteTrigger(TargetActor, this);
			}
		}
	}
}

void AThreadTalesEnemySpawner::OnTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	if (ActivationMode == ETriggerActivationMode::ExternalOnly)
	{
		return;
	}

	AThreadTalesCharacter* PlayerChar = Cast<AThreadTalesCharacter>(OtherActor);
	if (!PlayerChar || Cast<AThreadTalesEnemyCharacter>(OtherActor))
	{
		return;
	}

	OverlappingPlayers.Add(PlayerChar);

	if (ActivationMode == ETriggerActivationMode::OverlapAnyPlayer)
	{
		ExecuteTrigger_Implementation(PlayerChar);
	}
	else if (ActivationMode == ETriggerActivationMode::OverlapBothPlayers)
	{
		// Requires 2 distinct player characters in OverlappingPlayers set
		if (OverlappingPlayers.Num() >= 2)
		{
			ExecuteTrigger_Implementation(PlayerChar);
		}
	}
}

void AThreadTalesEnemySpawner::OnTriggerEndOverlap(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (AThreadTalesCharacter* PlayerChar = Cast<AThreadTalesCharacter>(OtherActor))
	{
		OverlappingPlayers.Remove(PlayerChar);
	}
}

void AThreadTalesEnemySpawner::OnExternalActorBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!HasAuthority())
	{
		return;
	}

	AThreadTalesCharacter* PlayerChar = Cast<AThreadTalesCharacter>(OtherActor);
	if (!PlayerChar || Cast<AThreadTalesEnemyCharacter>(OtherActor))
	{
		return;
	}

	ExecuteTrigger_Implementation(PlayerChar);
}
