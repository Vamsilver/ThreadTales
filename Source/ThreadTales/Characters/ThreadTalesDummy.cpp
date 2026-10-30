// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesDummy.h"
#include "Characters/ThreadTalesCombatReactionHandler.h"
#include "ThreadTales.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Abilities/ThreadTalesGameplayEffects.h"
#include "Abilities/ThreadTalesAttributeSet.h"
#include "Abilities/ThreadTalesAbilitySystemComponent.h"
#include "Abilities/ThreadTalesPostureConfig.h"
#include "AbilitySystemComponent.h"
#include "Engine/DamageEvents.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/StateTreeComponent.h"
#include "StateTree.h"
#include "AI/ThreadTalesStateTreeTasks.h"
#if WITH_EDITOR
#include "StateTreeCompilerManager.h"
#endif
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/AnimSequence.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
#include "Combat/ThreadTalesWeapon.h"

static TAutoConsoleVariable<int32> CVarDummyDebug(
	TEXT("tt.Combat.DummyDebug"),
	0,
	TEXT("Visualize dummy attack windup, parry window, and hitbox in real-time. 0 = off, 1 = on."),
	ECVF_Cheat);

void AThreadTalesDummy::InitializeCombatDefaults()
{
	// Health & Respawn defaults
	bIsImmortal = true;
	bAutoRespawn = true;
	RespawnDelay = 3.0f;

	// Combat & Attack defaults (enabled by default for combat training)
	bEnableAttacks = true;
	AttackInterval = 0.0f;
	AttackIntervalVariance = 0.0f;
	PostParryAttackDelay = 0.0f;
	AttackRange = 280.0f;
	bRequireTargetInRange = true;
	bFaceTargetDuringWindup = true;
	FaceTargetRotationSpeed = 8.0f;
	bRandomizeAttacks = false;
	CurrentAttackMontageIndex = 0;

	// Default weapon is assigned via BP_Dummy Details panel (Data-Driven Architecture)
	DefaultWeaponClass = nullptr;

	// Populate default attack and hit react montages
	static ConstructorHelpers::FObjectFinder<UAnimMontage> Attack1Obj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/Attack/AM_Attack_Combo1.AM_Attack_Combo1"));
	if (Attack1Obj.Succeeded())
	{
		AttackMontages.Add(Attack1Obj.Object);
	}
	static ConstructorHelpers::FObjectFinder<UAnimMontage> Attack2Obj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/Attack/AM_Attack_Combo2.AM_Attack_Combo2"));
	if (Attack2Obj.Succeeded())
	{
		AttackMontages.Add(Attack2Obj.Object);
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> HitReactObj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/AM_HitReact.AM_HitReact"));
	if (HitReactObj.Succeeded())
	{
		if (CombatReactionHandler)
		{
			CombatReactionHandler->HitReactMontage = HitReactObj.Object;
		}
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> StaggerObj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/AM_Stagger.AM_Stagger"));
	if (StaggerObj.Succeeded())
	{
		if (CombatReactionHandler)
		{
			CombatReactionHandler->StaggerMontage = StaggerObj.Object;
		}
	}

	// Default to null montage so WindupSequence (Heavy1_Start stance) plays as the pure telegraph stance
	WindupMontage = nullptr;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> WindupSeqObj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Anim_ARPGSamurai_Attack_Heavy1_Start_Anim.Anim_ARPGSamurai_Attack_Heavy1_Start_Anim"));
	if (WindupSeqObj.Succeeded())
	{
		WindupSequence = WindupSeqObj.Object;
	}

	// Timing defaults: calibrated readable windup (1.10s) and full strike swing duration (0.60s)
	WindupDuration = 1.10f;
	ParryWindowStartTime = 0.80f;
	ParryWindowDuration = 0.35f;
	StrikeActiveDuration = 0.60f;
	StrikeHitboxDelay = 0.18f;
	RecoveryDuration = 0.70f;
	StaggerDuration = 1.80f;

	// Damage defaults
	AttackDamage = 25.0f;
	AttackPoiseDamage = 30.0f;
	bInterruptOnHit = true;
	bShowDebugVisualization = true;

	CurrentAttackState = ECombatDummyAttackState::Idle;
	bParryWindowOpen = false;
}

void AThreadTalesDummy::InitializeHitboxAndVisualIndicators()
{
	// Configure Capsule
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		GetCapsuleComponent()->CanCharacterStepUpOn = ECB_No;
	}

	// Create Attack Hitbox matching Katana blade dimensions
	AttackHitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("AttackHitbox"));
	if (GetMesh())
	{
		AttackHitbox->SetupAttachment(GetMesh(), FName("hand_r"));
	}
	else
	{
		AttackHitbox->SetupAttachment(RootComponent);
	}
	AttackHitbox->InitBoxExtent(FVector(28.0f, 28.0f, 80.0f));
	AttackHitbox->SetRelativeLocation(FVector(0.0f, 15.0f, 45.0f));
	AttackHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AttackHitbox->SetCollisionObjectType(ECC_WorldDynamic);
	AttackHitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
	AttackHitbox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	AttackHitbox->OnComponentBeginOverlap.AddDynamic(this, &AThreadTalesDummy::OnAttackHitboxOverlap);

	// Create Parry Indicator Mesh (Visual overhead telegraph)
	ParryIndicatorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ParryIndicatorMesh"));
	ParryIndicatorMesh->SetupAttachment(RootComponent);
	ParryIndicatorMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 115.0f));
	ParryIndicatorMesh->SetRelativeScale3D(FVector(0.25f, 0.25f, 0.25f));
	ParryIndicatorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ParryIndicatorMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshObj(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshObj.Succeeded())
	{
		ParryIndicatorMesh->SetStaticMesh(SphereMeshObj.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMatObj(TEXT("/Game/LevelPrototyping/Interactable/JumpPad/Assets/Materials/M_SimpleGlow.M_SimpleGlow"));
	if (GlowMatObj.Succeeded())
	{
		ParryIndicatorMesh->SetMaterial(0, GlowMatObj.Object);
	}

	WindupIndicatorColor = FLinearColor(1.0f, 0.45f, 0.0f, 1.0f) * 6.0f;
	ParryWindowIndicatorColor = FLinearColor(0.0f, 0.9f, 1.0f, 1.0f) * 18.0f;
	StrikeIndicatorColor = FLinearColor(1.0f, 0.05f, 0.02f, 1.0f) * 10.0f;

	// StateTree AI Component initialization
	bUseStateTree = true;
	StateTreeComponent = CreateDefaultSubobject<UStateTreeComponent>(TEXT("StateTreeComponent"));
	StateTreeComponent->PrimaryComponentTick.bCanEverTick = true; // [TICK-ALWAYS-OK]
	StateTreeComponent->PrimaryComponentTick.bStartWithTickEnabled = true;
	StateTreeComponent->bAutoActivate = true;
	StateTreeComponent->SetStartLogicAutomatically(false);

	static ConstructorHelpers::FObjectFinder<UStateTree> StateTreeObj(TEXT("/Game/AI/StateTree/ST_Dummy_Combat.ST_Dummy_Combat"));
	if (StateTreeObj.Succeeded())
	{
		StateTreeComponent->SetStateTree(StateTreeObj.Object);
	}
}

AThreadTalesDummy::AThreadTalesDummy()
{
	PrimaryActorTick.bCanEverTick = true; // [TICK-ALWAYS-OK]
	PrimaryActorTick.bStartWithTickEnabled = true;

	InitializeCombatDefaults();
	InitializeHitboxAndVisualIndicators();
}

void AThreadTalesDummy::BeginPlay()
{
	Super::BeginPlay();

	// Ensure Dummy spawns with Katana drawn in right hand and Katana combat stance active
	if (EquipmentComponent)
	{
		if (!EquipmentComponent->GetCurrentWeapon() && DefaultWeaponClass && HasAuthority())
		{
			EquipmentComponent->EquipWeapon(DefaultWeaponClass);
		}
		if (AThreadTalesWeapon* EquippedWeapon = EquipmentComponent->GetCurrentWeapon())
		{
			EquippedWeapon->AttachToHand();
			EquippedWeapon->SetWeaponState(EWeaponCombatState::Drawn);
			SetOverlayPoseByName(EquippedWeapon->GetOverlayPoseName());
		}
	}

	InitialSpawnTransform = GetActorTransform();
	if (GetMesh())
	{
		InitialMeshRelativeTransform = GetMesh()->GetRelativeTransform();
	}

	if (ParryIndicatorMesh)
	{
		IndicatorMID = ParryIndicatorMesh->CreateDynamicMaterialInstance(0);
	}

	SetAttackState(ECombatDummyAttackState::Idle);

	if (bEnableAttacks)
	{
		bUseStateTree = true;
		if (StateTreeComponent)
		{
			bStateTreeEnteredFirstState = false;
#if WITH_EDITOR
			if (UStateTree* TreeAsset = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, TEXT("/Game/AI/StateTree/ST_Dummy_Combat.ST_Dummy_Combat"))))
			{
				UThreadTalesStateTreeHelper::BuildAndCompileDummyStateTree(TreeAsset);
				StateTreeComponent->SetStateTree(TreeAsset);
			}
#endif
			StateTreeComponent->SetComponentTickEnabled(true);
			StateTreeComponent->Activate(true);
			StateTreeComponent->StartLogic();
			EStateTreeRunStatus RunStatus = StateTreeComponent->GetStateTreeRunStatus();

#if WITH_EDITOR
			// If BP_Dummy serialized component held a stale StateTree reference across HotReload, build a fresh compiled StateTree instance on this component and restart logic
			if (RunStatus != EStateTreeRunStatus::Running || !bStateTreeEnteredFirstState)
			{
				StateTreeComponent->StopLogic(TEXT("HotReload Schema Rebind"));
				UStateTree* FreshTree = NewObject<UStateTree>(this, TEXT("ST_Dummy_Combat_Runtime"));
				if (UThreadTalesStateTreeHelper::BuildAndCompileDummyStateTree(FreshTree))
				{
					StateTreeComponent->SetStateTree(FreshTree);
					StateTreeComponent->StartLogic();
					RunStatus = StateTreeComponent->GetStateTreeRunStatus();
				}
			}
#endif
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Dummy '%s'] StateTree StartLogic called! Status: %d, EnteredFirstState: %s"),
				*GetName(), static_cast<int32>(RunStatus), bStateTreeEnteredFirstState ? TEXT("TRUE") : TEXT("FALSE"));

			if (RunStatus != EStateTreeRunStatus::Running || !bStateTreeEnteredFirstState)
			{
				UE_LOG(LogThreadTalesCombat, Error, TEXT("[Dummy '%s'] CRITICAL: StateTree failed to enter first state (Status=%d)! C++ fallback is disabled by design."),
					*GetName(), static_cast<int32>(RunStatus));
			}
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Cyan,
			FString::Printf(TEXT("[Dummy '%s'] ACTIVE (Attacks=%s, Range=%.0f, Interv=%.1fs)"),
				*GetName(), bEnableAttacks ? TEXT("ON") : TEXT("OFF"), AttackRange, AttackInterval));
	}
}

void AThreadTalesDummy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 1. Face target player during windup / parry window
	if (bEnableAttacks && bFaceTargetDuringWindup && (CurrentAttackState == ECombatDummyAttackState::Windup || CurrentAttackState == ECombatDummyAttackState::ParryWindow))
	{
		if (AActor* Target = CurrentTargetActor.Get())
		{
			FVector Direction = Target->GetActorLocation() - GetActorLocation();
			Direction.Z = 0.0f;
			if (!Direction.IsNearlyZero())
			{
				FRotator TargetRot = Direction.Rotation();
				FRotator NewRot = FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaSeconds, FaceTargetRotationSpeed);
				SetActorRotation(NewRot);
			}
		}
	}

	// 2. Real-time Debug Visualization
	const bool bDebugEnabled = bShowDebugVisualization || (CVarDummyDebug.GetValueOnGameThread() > 0);
	if (bDebugEnabled && GetWorld())
	{
		const FVector OverheadLoc = GetActorLocation() + FVector(0.0f, 0.0f, 110.0f);
		FString StateText;
		FColor StateColor = FColor::White;

		switch (CurrentAttackState)
		{
		case ECombatDummyAttackState::Idle:
			StateText = bEnableAttacks ? TEXT("[DUMMY IDLE (ATTACKS ON)]") : TEXT("[DUMMY (PASSIVE)]");
			StateColor = FColor::White;
			break;
		case ECombatDummyAttackState::Windup:
			StateText = FString::Printf(TEXT("[WINDUP TELEGRAPH (%.2fs)]"), WindupDuration);
			StateColor = FColor::Orange;
			DrawDebugSphere(GetWorld(), OverheadLoc, 24.0f, 12, FColor::Orange, false, -1.0f, 0, 2.0f);
			break;
		case ECombatDummyAttackState::ParryWindow:
			StateText = TEXT(">>> PARRY WINDOW ACTIVE! <<<");
			StateColor = FColor::Cyan;
			DrawDebugSphere(GetWorld(), OverheadLoc, 36.0f, 16, FColor::Cyan, false, -1.0f, 0, 3.5f);
			if (AttackHitbox)
			{
				DrawDebugBox(GetWorld(), AttackHitbox->GetComponentLocation(), AttackHitbox->GetScaledBoxExtent(), AttackHitbox->GetComponentQuat(), FColor::Cyan, false, -1.0f, 0, 1.5f);
			}
			break;
		case ECombatDummyAttackState::Strike:
			StateText = TEXT("[STRIKE ACTIVE HITBOX]");
			StateColor = FColor::Red;
			if (AttackHitbox)
			{
				DrawDebugBox(GetWorld(), AttackHitbox->GetComponentLocation(), AttackHitbox->GetScaledBoxExtent(), AttackHitbox->GetComponentQuat(), FColor::Red, false, -1.0f, 0, 2.0f);
			}
			break;
		case ECombatDummyAttackState::Recovery:
			StateText = TEXT("[RECOVERY]");
			StateColor = FColor::Silver;
			break;
		case ECombatDummyAttackState::Staggered:
			StateText = TEXT("[STAGGERED / DEFLECTED]");
			StateColor = FColor::Magenta;
			DrawDebugSphere(GetWorld(), OverheadLoc, 28.0f, 12, FColor::Magenta, false, -1.0f, 0, 2.5f);
			break;
		}

		DrawDebugString(GetWorld(), OverheadLoc + FVector(0.0f, 0.0f, 25.0f), StateText, nullptr, StateColor, -1.0f, true, 1.3f);
	}
}

void AThreadTalesDummy::SetAttackState(ECombatDummyAttackState NewState)
{
	CurrentAttackState = NewState;
	UpdateIndicatorVisuals();
	OnAttackStateChanged.Broadcast(NewState);

	if (AbilitySystemComponent)
	{
		if (NewState == ECombatDummyAttackState::Windup)
		{
			AbilitySystemComponent->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Dummy_Windup);
		}
		else if (AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Dummy_Windup))
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Dummy_Windup);
		}

		if (NewState == ECombatDummyAttackState::ParryWindow)
		{
			AbilitySystemComponent->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Dummy_ParryWindow);
		}
		else if (AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Dummy_ParryWindow))
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Dummy_ParryWindow);
		}
	}
}

AActor* AThreadTalesDummy::FindTargetPlayer() const
{
	if (!GetWorld()) return nullptr;

	TArray<AActor*> Players;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AThreadTalesCharacter::StaticClass(), Players);

	AActor* Closest = nullptr;
	float ClosestDistSq = MAX_FLT;
	const FVector MyLoc = GetActorLocation();

	for (AActor* Actor : Players)
	{
		if (Actor && Actor != this && !Actor->IsA<AThreadTalesDummy>())
		{
			if (AThreadTalesCharacter* Char = Cast<AThreadTalesCharacter>(Actor))
			{
				if (!Char->IsAlive() || !Char->IsPlayerControlled()) continue;

				const float DistSq = FVector::DistSquared(MyLoc, Char->GetActorLocation());
				if (DistSq < ClosestDistSq)
				{
					ClosestDistSq = DistSq;
					Closest = Actor;
				}
			}
		}
	}

	return Closest;
}

void AThreadTalesDummy::ScheduleNextAttack()
{
	// StateTree exclusively drives attack scheduling via IdleWait state
}

void AThreadTalesDummy::TriggerAttackManual()
{
	if (CurrentAttackState == ECombatDummyAttackState::Idle || CurrentAttackState == ECombatDummyAttackState::Recovery)
	{
		StartAttackSequence();
	}
}

void AThreadTalesDummy::StartAttackSequence()
{
	if (!bEnableAttacks || !IsAlive() || IsStaggered())
	{
		return;
	}

	// Target Proximity Check
	CurrentTargetActor = FindTargetPlayer();
	if (bRequireTargetInRange)
	{
		if (!CurrentTargetActor.IsValid())
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this) + 1, 1.5f, FColor::Yellow,
					FString::Printf(TEXT("[Dummy '%s'] Idle: No player character found nearby"), *GetName()));
			}
			ScheduleNextAttack();
			return;
		}

		const float Dist = FVector::Dist(GetActorLocation(), CurrentTargetActor->GetActorLocation());
		if (Dist > AttackRange)
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this) + 1, 1.5f, FColor::Yellow,
					FString::Printf(TEXT("[Dummy '%s'] Player '%s' at %.0fcm (Needs <= %.0fcm)"),
						*GetName(), *CurrentTargetActor->GetName(), Dist, AttackRange));
			}
			ScheduleNextAttack();
			return;
		}
	}

	StartWindup();
}

void AThreadTalesDummy::StartWindup()
{
	SetAttackState(ECombatDummyAttackState::Windup);
	bParryWindowOpen = false;

	if (!CurrentTargetActor.IsValid())
	{
		CurrentTargetActor = FindTargetPlayer();
	}

	// 1. Play dedicated telegraph windup animation so character visibly raises blade and prepares strike
	const bool bHasTelegraphWindup = (WindupMontage != nullptr || WindupSequence != nullptr);
	if (WindupMontage)
	{
		PlayAnimMontage(WindupMontage, 1.0f);
	}
	else if (WindupSequence && GetMesh() && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->PlaySlotAnimationAsDynamicMontage(WindupSequence, FName("DefaultSlot"), 0.15f, 0.15f, 1.0f);
	}
	else
	{
		// Fallback: early attack montage playback if no dedicated telegraph animation is configured
		UAnimMontage* MontageToPlay = nullptr;
		if (AttackMontages.Num() > 0)
		{
			if (bRandomizeAttacks)
			{
				CurrentAttackMontageIndex = FMath::RandRange(0, AttackMontages.Num() - 1);
			}
			else
			{
				CurrentAttackMontageIndex = (CurrentAttackMontageIndex + 1) % AttackMontages.Num();
			}
			MontageToPlay = AttackMontages[CurrentAttackMontageIndex];
		}

		if (MontageToPlay)
		{
			PlayAnimMontage(MontageToPlay, 1.0f);
		}
	}

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Dummy] '%s' started attack WINDUP (Duration: %.2fs). Target: %s"),
		*GetName(), WindupDuration, CurrentTargetActor.IsValid() ? *CurrentTargetActor->GetName() : TEXT("None"));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this) + 2, 2.0f, FColor::Orange,
			FString::Printf(TEXT("[Dummy '%s'] ATTACK WINDUP! (Duration: %.2fs)"),
				*GetName(), WindupDuration));
	}

	OnWindupStarted.Broadcast(WindupDuration);
}

void AThreadTalesDummy::OpenParryWindow()
{
	if (CurrentAttackState != ECombatDummyAttackState::Windup) return;

	bParryWindowOpen = true;
	SetAttackState(ECombatDummyAttackState::ParryWindow);

	const float EffectiveParryDuration = (WindupDuration - ParryWindowStartTime) + StrikeActiveDuration;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this) + 3, EffectiveParryDuration, FColor::Cyan,
			FString::Printf(TEXT(">>> [Dummy '%s'] PARRY WINDOW OPEN! (%.2fs) <<<"), *GetName(), EffectiveParryDuration));
	}

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Dummy] '%s' PARRY WINDOW OPENED (Effective Duration: %.2fs)!"),
		*GetName(), EffectiveParryDuration);

	OnParryWindowOpened.Broadcast(EffectiveParryDuration);
	// Note: Parry window remains open through the strike until EndStrike() or HandleParried()
}

void AThreadTalesDummy::CloseParryWindow()
{
	if (bParryWindowOpen)
	{
		bParryWindowOpen = false;
		GetWorldTimerManager().ClearTimer(ParryWindowCloseTimerHandle);
		OnParryWindowClosed.Broadcast();

		if (CurrentAttackState == ECombatDummyAttackState::ParryWindow)
		{
			SetAttackState(ECombatDummyAttackState::Windup);
		}

		UE_LOG(LogThreadTalesCombat, Log, TEXT("[Dummy] '%s' Parry window closed."), *GetName());
	}
}

void AThreadTalesDummy::ExecuteStrike()
{
	// Keep parry window active across the strike impact so blocking on impact triggers a Perfect Parry!
	bParryWindowOpen = true;
	SetAttackState(ECombatDummyAttackState::ParryWindow);

	HitActorsThisStrike.Empty();

	const bool bHasTelegraphWindup = (WindupMontage != nullptr || WindupSequence != nullptr);
	UAnimMontage* CurrentMontage = nullptr;
	if (AttackMontages.Num() > 0)
	{
		if (bHasTelegraphWindup)
		{
			if (bRandomizeAttacks)
			{
				CurrentAttackMontageIndex = FMath::RandRange(0, AttackMontages.Num() - 1);
			}
			else
			{
				CurrentAttackMontageIndex = (CurrentAttackMontageIndex + 1) % AttackMontages.Num();
			}
		}
		CurrentMontage = AttackMontages.IsValidIndex(CurrentAttackMontageIndex) ? AttackMontages[CurrentAttackMontageIndex] : nullptr;
	}

	if (bHasTelegraphWindup && CurrentMontage)
	{
		PlayAnimMontage(CurrentMontage, 1.15f);
	}

	OnAttackStrike.Broadcast(CurrentMontage);

	// Start with hitbox disabled! Activated by AnimNotifyState_WeaponHitbox on forward swing or fallback timer
	if (AttackHitbox)
	{
		AttackHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Dummy] '%s' executed STRIKE with montage '%s' (Forward swing delay: %.2fs, Duration: %.2fs)."),
		*GetName(), CurrentMontage ? *CurrentMontage->GetName() : TEXT("None"), StrikeHitboxDelay, StrikeActiveDuration);

	// Always arm fallback hitbox activation timer in case montage lacks or delays AnimNotifyState_WeaponHitbox
	const float Delay = FMath::Clamp(StrikeHitboxDelay, 0.05f, StrikeActiveDuration * 0.8f);
	GetWorldTimerManager().SetTimer(HitboxActivationTimerHandle, this, &AThreadTalesDummy::EnableHitboxFallback, Delay, false);
}

void AThreadTalesDummy::SetHitboxActive(bool bActive)
{
	GetWorldTimerManager().ClearTimer(HitboxActivationTimerHandle);

	if (EquipmentComponent)
	{
		if (AThreadTalesWeapon* EquippedWeapon = EquipmentComponent->GetCurrentWeapon())
		{
			EquippedWeapon->SetHitboxEnabled(bActive);
		}
	}

	if (AttackHitbox)
	{
		AttackHitbox->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		if (bActive)
		{
			AttackHitbox->UpdateOverlaps();
			TArray<AActor*> OverlappingActors;
			AttackHitbox->GetOverlappingActors(OverlappingActors, AThreadTalesCharacter::StaticClass());
			for (AActor* OverlappedActor : OverlappingActors)
			{
				if (OverlappedActor && OverlappedActor != this)
				{
					FHitResult DummyHit;
					OnAttackHitboxOverlap(AttackHitbox, OverlappedActor, OverlappedActor->FindComponentByClass<UPrimitiveComponent>(), INDEX_NONE, false, DummyHit);
				}
			}
		}
	}
	if (!bActive)
	{
		HitActorsThisStrike.Empty();
	}

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Dummy] '%s' Hitbox %s!"),
		*GetName(), bActive ? TEXT("ACTIVATED (Forward Swing)") : TEXT("DEACTIVATED"));
}

void AThreadTalesDummy::EnableHitboxFallback()
{
	if (CurrentAttackState == ECombatDummyAttackState::Strike || CurrentAttackState == ECombatDummyAttackState::ParryWindow)
	{
		SetHitboxActive(true);
	}
}

void AThreadTalesDummy::EndStrike()
{
	CloseParryWindow();
	GetWorldTimerManager().ClearTimer(HitboxActivationTimerHandle);
	SetHitboxActive(false);

	SetAttackState(ECombatDummyAttackState::Recovery);

	UE_LOG(LogThreadTalesCombat, Log, TEXT("[Dummy] '%s' strike ended. Entering recovery (%.2fs)."), *GetName(), RecoveryDuration);
}

void AThreadTalesDummy::FinishRecovery()
{
	bParryWindowOpen = false;
	SetAttackState(ECombatDummyAttackState::Idle);
}

void AThreadTalesDummy::FinishParryRecovery()
{
	bParryWindowOpen = false;
	SetAttackState(ECombatDummyAttackState::Idle);

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Dummy] '%s' recovered from parry stagger. Preparing next attack in %.2fs."),
		*GetName(), PostParryAttackDelay);
}

void AThreadTalesDummy::StartParryStagger()
{
	bParryWindowOpen = false;
	SetHitboxActive(false);
	SetAttackState(ECombatDummyAttackState::Staggered);

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Dummy] '%s' StartParryStagger (Montage: %s, Length: %.2fs)"),
		*GetName(), GetStaggerMontage() ? *GetStaggerMontage()->GetName() : TEXT("None"), GetStaggerDuration());
}

void AThreadTalesDummy::StartPostureBreakStun()
{
	bParryWindowOpen = false;
	SetHitboxActive(false);
	SetAttackState(ECombatDummyAttackState::Staggered);

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Dummy] '%s' StartPostureBreakStun (Duration: %.2fs)"),
		*GetName(), GetPostureBreakStunDuration());
}

void AThreadTalesDummy::FinishPostureBreakStun()
{
	FinishPostureBreak();
}

void AThreadTalesDummy::OnAttackHitboxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;
	if (!OtherActor || OtherActor == this || OtherActor->IsA<AThreadTalesDummy>() || HitActorsThisStrike.Contains(OtherActor)) return;

	AThreadTalesCharacter* TargetChar = Cast<AThreadTalesCharacter>(OtherActor);
	if (!TargetChar || !TargetChar->IsAlive()) return;

	HitActorsThisStrike.Add(OtherActor);

	// Apply GAS Damage & Poise Damage directly to Target.
	// Target's UThreadTalesAttributeSet::PostGameplayEffectExecute centralizes all defensive layers:
	// 1. Evade / I-Frames
	// 2. Perfect Parry (deflects this dummy via HandleParried)
	// 3. Block / Guard (mitigates damage, deducts stamina, calls HandleGuardHit)
	// 4. Guard Break (stamina depleted -> stagger)
	// 5. Unblocked direct hit
	UAbilitySystemComponent* TargetASC = TargetChar->GetAbilitySystemComponent();
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponent();

	if (TargetASC && SourceASC)
	{
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddInstigator(this, this);
		Context.AddHitResult(SweepResult);

		FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(UThreadTalesGE_Damage::StaticClass(), 1.0f, Context);
		if (SpecHandle.IsValid())
		{
			SpecHandle.Data->SetSetByCallerMagnitude(ThreadTalesGameplayTags::TAG_SetByCaller_Damage, AttackDamage);
			SpecHandle.Data->SetSetByCallerMagnitude(ThreadTalesGameplayTags::TAG_SetByCaller_PoiseDamage, AttackPoiseDamage);
			TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Dummy] Hit %s with AttackDamage=%.1f, AttackPoiseDamage=%.1f via GAS!"),
				*TargetChar->GetName(), AttackDamage, AttackPoiseDamage);
		}
	}
	else
	{
		TargetChar->TakeDamage(AttackDamage, FDamageEvent(), GetController(), this);
	}
}

void AThreadTalesDummy::HandleParried(AActor* ParryingActor)
{
	GetWorldTimerManager().ClearTimer(WindupTimerHandle);
	GetWorldTimerManager().ClearTimer(ParryWindowOpenTimerHandle);
	GetWorldTimerManager().ClearTimer(ParryWindowCloseTimerHandle);
	GetWorldTimerManager().ClearTimer(HitboxActivationTimerHandle);
	GetWorldTimerManager().ClearTimer(StrikeDurationTimerHandle);

	bParryWindowOpen = false;
	SetHitboxActive(false);

	SetAttackState(ECombatDummyAttackState::Staggered);

	Super::HandleParried(ParryingActor);

	if (StateTreeComponent)
	{
		StateTreeComponent->SendStateTreeEvent(ThreadTalesGameplayTags::TAG_Event_Combat_Parry);
	}

	OnDummyParried.Broadcast(ParryingActor);

	const float DummyPoise = AttributeSet ? AttributeSet->GetPoise() : 0.0f;
	const float DummyMaxPoise = AttributeSet ? AttributeSet->GetMaxPoise() : 100.0f;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Cyan,
			FString::Printf(TEXT(">>> PARRY DEFLECT! %s Posture: %.0f/%.0f (-60) <<<"),
				*GetName(), DummyPoise, DummyMaxPoise));
	}
}

void AThreadTalesDummy::HandlePostureBreak(AActor* ParryingActor, float StunDuration)
{
	GetWorldTimerManager().ClearTimer(WindupTimerHandle);
	GetWorldTimerManager().ClearTimer(ParryWindowOpenTimerHandle);
	GetWorldTimerManager().ClearTimer(ParryWindowCloseTimerHandle);
	GetWorldTimerManager().ClearTimer(HitboxActivationTimerHandle);
	GetWorldTimerManager().ClearTimer(StrikeDurationTimerHandle);

	bParryWindowOpen = false;
	SetHitboxActive(false);

	SetAttackState(ECombatDummyAttackState::Staggered);

	Super::HandlePostureBreak(ParryingActor, StunDuration);

	if (StateTreeComponent)
	{
		StateTreeComponent->SendStateTreeEvent(ThreadTalesGameplayTags::TAG_Event_Combat_PostureBreak);
	}

	OnDummyParried.Broadcast(ParryingActor);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red,
			FString::Printf(TEXT(">>> POSTURE BROKEN! %s LONG STUN (%.1fs)! <<<"),
				*GetName(), StunDuration));
	}
}

void AThreadTalesDummy::FinishPostureBreak()
{
	Super::FinishPostureBreak();
	SetAttackState(ECombatDummyAttackState::Idle);
}

void AThreadTalesDummy::HandleHealthChanged(float NewHealth, float MaxHealthVal)
{
	Super::HandleHealthChanged(NewHealth, MaxHealthVal);

	if (bIsImmortal && NewHealth < MaxHealthVal)
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Dummy] '%s' took damage (Health: %.1f/%.1f). Immortal -> Resetting Health."),
			*GetName(), NewHealth, MaxHealthVal);

		if (AttributeSet)
		{
			AttributeSet->ResetHealth();
		}
	}
}

float AThreadTalesDummy::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	if (!IsAlive() || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	// Interruption during windup
	if (bInterruptOnHit && bEnableAttacks && (CurrentAttackState == ECombatDummyAttackState::Windup || CurrentAttackState == ECombatDummyAttackState::ParryWindow))
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Dummy] Attack INTERRUPTED by player hit during windup!"));
		CloseParryWindow();
		SetAttackState(ECombatDummyAttackState::Recovery);
	}

	if (bIsImmortal)
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Dummy] Immortal Dummy '%s' took %.1f damage! (Immortal - Resetting Health)"),
			*GetName(), DamageAmount);

		if (AttributeSet)
		{
			const float CurrentPoiseVal = AttributeSet->GetPoise();
			const float NewPoise = FMath::Max(0.0f, CurrentPoiseVal - DamageAmount);
			AttributeSet->SetPoise(NewPoise);

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
					FString::Printf(TEXT("[Dummy '%s'] HIT! Dmg: %.1f | Poise: %.0f/%.0f"),
						*GetName(), DamageAmount, NewPoise, AttributeSet->GetMaxPoise()));
			}

			if (NewPoise <= 0.0f)
			{
				AttributeSet->ResetPoise();
				HandleStaggered(DamageCauser);
			}
			else
			{
				HandlePoiseChanged(NewPoise, AttributeSet->GetMaxPoise());
				HandleHitReact(DamageCauser);
			}

			AttributeSet->ResetHealth();
		}

		return DamageAmount;
	}

	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

bool AThreadTalesDummy::IsStaggered() const
{
	return CurrentAttackState == ECombatDummyAttackState::Staggered || Super::IsStaggered();
}

void AThreadTalesDummy::HandleStaggered(AActor* AttackerActor)
{
	CloseParryWindow();
	SetHitboxActive(false);

	SetAttackState(ECombatDummyAttackState::Staggered);

	Super::HandleStaggered(AttackerActor);

	if (StateTreeComponent)
	{
		StateTreeComponent->SendStateTreeEvent(ThreadTalesGameplayTags::TAG_Event_Combat_Parry);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Purple,
			FString::Printf(TEXT(">>> [Dummy '%s'] STAGGERED (POISE BROKEN)! <<<"), *GetName()));
	}
}

void AThreadTalesDummy::HandleHitReact(AActor* AttackerActor)
{
	// Protect Stagger state and StaggerMontage from being cancelled or overwritten by light HitReact
	if (IsStaggered())
	{
		UE_LOG(LogThreadTalesCombat, Log, TEXT("[Dummy] '%s' is Staggered - preserving AM_Stagger and ignoring light HitReact overwrite."), *GetName());
		return;
	}

	// Interrupt active attack if hit
	if (bInterruptOnHit && bEnableAttacks && (CurrentAttackState == ECombatDummyAttackState::Windup || CurrentAttackState == ECombatDummyAttackState::ParryWindow))
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Dummy] Attack INTERRUPTED by player hit during windup!"));
		CloseParryWindow();
		SetAttackState(ECombatDummyAttackState::Recovery);
	}

	Super::HandleHitReact(AttackerActor);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Magenta,
			FString::Printf(TEXT("[Dummy '%s'] Hit React Played (Montage: %s)"),
				*GetName(), GetHitReactMontage() ? *GetHitReactMontage()->GetName() : TEXT("None")));
	}
}

void AThreadTalesDummy::Die(AActor* KillerActor)
{
	if (bIsImmortal)
	{
		UE_LOG(LogThreadTalesCombat, Warning, TEXT("[Dummy] '%s' Die() called but Dummy is Immortal! Suppressing death and resetting health."), *GetName());
		if (AttributeSet)
		{
			AttributeSet->ResetHealth();
		}
		return;
	}

	GetWorldTimerManager().ClearTimer(HitboxActivationTimerHandle);

	if (StateTreeComponent)
	{
		StateTreeComponent->StopLogic(TEXT("Dummy Died"));
	}

	CloseParryWindow();
	if (AttackHitbox)
	{
		AttackHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	Super::Die(KillerActor);

	if (bAutoRespawn)
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Dummy] '%s' will respawn in %.1f seconds..."), *GetName(), RespawnDelay);
		GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &AThreadTalesDummy::Respawn, RespawnDelay, false);
	}
}

void AThreadTalesDummy::Respawn()
{
	GetWorldTimerManager().ClearTimer(RespawnTimerHandle);

	bIsAlive = true;
	CurrentAttackState = ECombatDummyAttackState::Idle;
	bParryWindowOpen = false;

	// Reset Transform
	SetActorTransform(InitialSpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);

	// Reset Attributes
	if (AttributeSet)
	{
		AttributeSet->ResetHealth();
		AttributeSet->ResetStamina();
		AttributeSet->ResetPoise();
	}

	// Restore Mesh physics and transform
	if (GetMesh())
	{
		GetMesh()->SetSimulatePhysics(false);
		GetMesh()->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale);
		GetMesh()->SetRelativeTransform(InitialMeshRelativeTransform);
		GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));
	}

	// Restore Capsule and Movement
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
	}

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}

	UpdateIndicatorVisuals();

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Dummy] '%s' RESPAWNED with full health & poise!"), *GetName());

	if (bEnableAttacks && StateTreeComponent)
	{
		StateTreeComponent->RestartLogic();
	}
}

void AThreadTalesDummy::UpdateIndicatorVisuals()
{
	if (!ParryIndicatorMesh) return;

	if (!bEnableAttacks || CurrentAttackState == ECombatDummyAttackState::Idle || CurrentAttackState == ECombatDummyAttackState::Recovery)
	{
		ParryIndicatorMesh->SetVisibility(false);
		return;
	}

	if (!IndicatorMID && ParryIndicatorMesh->GetNumMaterials() > 0)
	{
		IndicatorMID = ParryIndicatorMesh->CreateDynamicMaterialInstance(0);
	}

	ParryIndicatorMesh->SetVisibility(true);

	static const FName ColorParamName(TEXT("Color"));

	switch (CurrentAttackState)
	{
	case ECombatDummyAttackState::Windup:
		ParryIndicatorMesh->SetRelativeScale3D(FVector(0.22f, 0.22f, 0.22f));
		if (IndicatorMID)
		{
			IndicatorMID->SetVectorParameterValue(ColorParamName, WindupIndicatorColor);
		}
		break;
	case ECombatDummyAttackState::ParryWindow:
		ParryIndicatorMesh->SetRelativeScale3D(FVector(0.40f, 0.40f, 0.40f));
		if (IndicatorMID)
		{
			IndicatorMID->SetVectorParameterValue(ColorParamName, ParryWindowIndicatorColor);
		}
		break;
	case ECombatDummyAttackState::Strike:
		ParryIndicatorMesh->SetRelativeScale3D(FVector(0.28f, 0.28f, 0.28f));
		if (IndicatorMID)
		{
			IndicatorMID->SetVectorParameterValue(ColorParamName, StrikeIndicatorColor);
		}
		break;
	case ECombatDummyAttackState::Staggered:
		ParryIndicatorMesh->SetRelativeScale3D(FVector(0.15f, 0.15f, 0.15f));
		if (IndicatorMID)
		{
			IndicatorMID->SetVectorParameterValue(ColorParamName, FLinearColor(0.2f, 0.2f, 0.2f, 1.0f));
		}
		break;
	default:
		break;
	}
}

bool AThreadTalesDummy::IsTargetInAttackRange() const
{
	AActor* Target = FindTargetPlayer();
	if (!Target)
	{
		return false;
	}
	const float Dist = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
	return Dist <= AttackRange;
}

float AThreadTalesDummy::GetParryWindowOpenDelay() const
{
	return FMath::Clamp(ParryWindowStartTime, 0.0f, WindupDuration);
}

float AThreadTalesDummy::GetParryWindowRemainingDuration() const
{
	return FMath::Max(0.05f, WindupDuration - ParryWindowStartTime);
}

bool AThreadTalesDummy::CanBeTargeted_Implementation(const AActor* TargetingActor) const
{
	return (bIsImmortal || GetHealth() > 0.0f) && (TargetingActor != this);
}

FVector AThreadTalesDummy::GetTargetLockLocation_Implementation(const AActor* TargetingActor) const
{
	return Super::GetTargetLockLocation_Implementation(TargetingActor);
}


