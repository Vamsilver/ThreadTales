// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ThreadTalesEnemyStateTreeTasks.h"
#include "Components/CapsuleComponent.h"
#include "AI/ThreadTalesAIController.h"
#include "AI/ThreadTalesEnemyConfig.h"
#include "Characters/ThreadTalesBaseCharacter.h"
#include "Characters/ThreadTalesCombatReactionHandler.h"
#include "Characters/ThreadTalesEnemyCharacter.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
#include "Combat/ThreadTalesWeapon.h"
#include "StateTreeExecutionContext.h"
#include "Components/ActorComponent.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "ThreadTales.h"

static AThreadTalesBaseCharacter* GetEnemyFromContext(FStateTreeExecutionContext& Context)
{
	UObject* OwnerObj = &(*Context.GetOwner());
	if (AThreadTalesBaseCharacter* Enemy = Cast<AThreadTalesBaseCharacter>(OwnerObj))
	{
		return Enemy;
	}
	if (AActor* ActorOwner = Cast<AActor>(OwnerObj))
	{
		if (AThreadTalesBaseCharacter* Enemy = Cast<AThreadTalesBaseCharacter>(ActorOwner))
		{
			return Enemy;
		}
	}
	else if (UActorComponent* CompOwner = Cast<UActorComponent>(OwnerObj))
	{
		return Cast<AThreadTalesBaseCharacter>(CompOwner->GetOwner());
	}
	return nullptr;
}

static AThreadTalesAIController* GetAIController(AThreadTalesBaseCharacter* Enemy)
{
	return Enemy ? Cast<AThreadTalesAIController>(Enemy->GetController()) : nullptr;
}

static void ApplyConfigSpeed(AThreadTalesBaseCharacter* Enemy, float UThreadTalesEnemyConfig::* SpeedMember)
{
	if (const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(Enemy))
	{
		if (const UThreadTalesEnemyConfig* Config = EnemyChar->GetEnemyConfig())
		{
			if (Config->bOverrideWalkSpeed)
			{
				if (UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement())
				{
					MoveComp->MaxWalkSpeed = Config->*SpeedMember;
				}
			}
		}
	}
}

static void EnsureGroundWalking(AThreadTalesBaseCharacter* Enemy)
{
	if (Enemy && Enemy->GetCharacterMovement())
	{
		if (Enemy->GetCharacterMovement()->MovementMode != MOVE_Walking)
		{
			Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
		Enemy->GetCharacterMovement()->bOrientRotationToMovement = true;
	}
}

// -----------------------------------------------------------------------------
// FStateTreeTask_EnemyChaseTarget
// -----------------------------------------------------------------------------

FStateTreeTask_EnemyChaseTarget::FStateTreeTask_EnemyChaseTarget()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreeTask_EnemyChaseTarget::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	AThreadTalesAIController* AICon = GetAIController(Enemy);
	if (!Enemy || !AICon)
	{
		return EStateTreeRunStatus::Failed;
	}

	ApplyConfigSpeed(Enemy, &UThreadTalesEnemyConfig::ChaseWalkSpeed);
	EnsureGroundWalking(Enemy);

	if (UThreadTalesEquipmentComponent* EquipComp = Enemy->GetEquipmentComponent())
	{
		if (AThreadTalesWeapon* Weapon = EquipComp->GetCurrentWeapon())
		{
			if (!Weapon->IsDrawn())
			{
				Weapon->AttachToHand();
				Weapon->SetWeaponState(EWeaponCombatState::Drawn);
				Enemy->SetOverlayPoseByName(Weapon->GetOverlayPoseName());
			}
		}
	}

	AThreadTalesCharacter* Target = AICon->GetCombatTarget();
	if (!Target)
	{
		Target = AICon->EvaluateBestTarget();
	}

	if (Target && Target->IsAlive())
	{
		AICon->SetFocus(Target, EAIFocusPriority::Gameplay);

		const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(Enemy);
		const UThreadTalesEnemyConfig* EnemyConfig = EnemyChar ? EnemyChar->GetEnemyConfig() : nullptr;

		const float AttackRange = EnemyConfig ? EnemyConfig->Ranges.MeleeAttackRange : (AcceptanceRadius > 0.0f ? AcceptanceRadius : Context.GetInstanceData(*this).AcceptanceRadius);
		const float MoveAcceptance = FMath::Max(40.0f, AttackRange - 30.0f);

		const float DistSq = FVector::DistSquared2D(Enemy->GetActorLocation(), Target->GetActorLocation());
		if (DistSq <= FMath::Square(AttackRange))
		{
			AICon->StopMovement();
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Enemy StateTree] '%s' already in strike range (Dist: %.1f cm, Range: %.1f) -> ATTACKING!"),
				*Enemy->GetName(), FMath::Sqrt(DistSq), AttackRange);
			return EStateTreeRunStatus::Succeeded;
		}

		const EPathFollowingRequestResult::Type MoveResult = AICon->MoveToActor(Target, MoveAcceptance);
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Enemy StateTree] '%s' CHASING '%s' (Radius: %.1f, MoveResult: %d)"),
			*Enemy->GetName(), *Target->GetName(), MoveAcceptance, (int32)MoveResult);

		if (MoveResult == EPathFollowingRequestResult::Failed)
		{
			UE_LOG(LogThreadTalesCombat, Warning, TEXT("[Enemy StateTree] '%s' MoveToActor failed! Check if NavMesh / RecastNavMesh covers the level!"),
				*Enemy->GetName());
		}
	}
	else
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Enemy StateTree] '%s' entered Chase with no target, falling back to Patrol..."),
			*Enemy->GetName());
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EnemyChaseTarget::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	AThreadTalesAIController* AICon = GetAIController(Enemy);
	if (!Enemy || !AICon)
	{
		return EStateTreeRunStatus::Failed;
	}

	AThreadTalesCharacter* Target = AICon->GetCombatTarget();
	if (!Target)
	{
		Target = AICon->EvaluateBestTarget();
	}

	if (!Target || !Target->IsAlive())
	{
		AICon->ClearFocus(EAIFocusPriority::Gameplay);
		return EStateTreeRunStatus::Failed;
	}

	AICon->SetFocus(Target, EAIFocusPriority::Gameplay);

	const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(Enemy);
	const UThreadTalesEnemyConfig* EnemyConfig = EnemyChar ? EnemyChar->GetEnemyConfig() : nullptr;

	const float AttackRange = EnemyConfig ? EnemyConfig->Ranges.MeleeAttackRange : (AcceptanceRadius > 0.0f ? AcceptanceRadius : Context.GetInstanceData(*this).AcceptanceRadius);
	const float MoveAcceptance = FMath::Max(40.0f, AttackRange - 30.0f);
	const float LoseSightRange = EnemyConfig ? EnemyConfig->Ranges.LoseSightRadius : 2200.0f;
	const float DistSq = FVector::DistSquared2D(Enemy->GetActorLocation(), Target->GetActorLocation());

	if (DistSq > FMath::Square(LoseSightRange))
	{
		AICon->ClearFocus(EAIFocusPriority::Gameplay);
		AICon->StopMovement();
		return EStateTreeRunStatus::Failed;
	}

	// Dynamic strike trigger: within MeleeAttackRange OR PathFollowing arrived near target
	const bool bWithinAttackRange = (DistSq <= FMath::Square(AttackRange));
	const bool bMovementArrived = (AICon->GetMoveStatus() == EPathFollowingStatus::Idle && DistSq <= FMath::Square(AttackRange + 40.0f));

	if (bWithinAttackRange || bMovementArrived)
	{
		AICon->StopMovement();
		return EStateTreeRunStatus::Succeeded;
	}

	if (AICon->GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		AICon->MoveToActor(Target, MoveAcceptance);
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EnemyChaseTarget::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	if (AThreadTalesAIController* AICon = GetAIController(Enemy))
	{
		AICon->StopMovement();
	}
}

// -----------------------------------------------------------------------------
// FStateTreeTask_EnemyStrafeAroundTarget
// -----------------------------------------------------------------------------

FStateTreeTask_EnemyStrafeAroundTarget::FStateTreeTask_EnemyStrafeAroundTarget()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreeTask_EnemyStrafeAroundTarget::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	AThreadTalesAIController* AICon = GetAIController(Enemy);
	if (!Enemy || !AICon)
	{
		return EStateTreeRunStatus::Failed;
	}

	ApplyConfigSpeed(Enemy, &UThreadTalesEnemyConfig::StrafeWalkSpeed);

	AThreadTalesCharacter* Target = AICon->GetCombatTarget();
	if (!Target)
	{
		Target = AICon->EvaluateBestTarget();
	}

	if (!Target || !Target->IsAlive())
	{
		return EStateTreeRunStatus::Running;
	}

	const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(Enemy);
	const UThreadTalesEnemyConfig* EnemyConfig = EnemyChar ? EnemyChar->GetEnemyConfig() : nullptr;

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime = EnemyConfig ? EnemyConfig->Timing.StrafeDuration : InstanceData.StrafeDuration;
	InstanceData.StrafeDirection = FMath::RandBool() ? 1 : -1;

	AICon->SetFocus(Target, EAIFocusPriority::Gameplay);

	const float StrafeDist = EnemyConfig ? ((EnemyConfig->Ranges.StrafeMinDistance + EnemyConfig->Ranges.StrafeMaxDistance) * 0.5f) : 250.0f;
	const FVector EnemyLoc = Enemy->GetActorLocation();
	const FVector TargetLoc = Target->GetActorLocation();
	const FVector ToTarget = (TargetLoc - EnemyLoc).GetSafeNormal2D();
	const FVector Tangent = FVector(-ToTarget.Y, ToTarget.X, 0.0f) * static_cast<float>(InstanceData.StrafeDirection);
	const FVector MoveGoal = EnemyLoc + Tangent * StrafeDist;
	AICon->MoveToLocation(MoveGoal, 40.0f, false, true, true);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EnemyStrafeAroundTarget::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	AThreadTalesAIController* AICon = GetAIController(Enemy);
	if (!Enemy || !AICon)
	{
		return EStateTreeRunStatus::Failed;
	}

	AThreadTalesCharacter* Target = AICon->GetCombatTarget();
	if (!Target)
	{
		Target = AICon->EvaluateBestTarget();
	}

	if (!Target || !Target->IsAlive())
	{
		return EStateTreeRunStatus::Running;
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime -= DeltaTime;
	if (InstanceData.RemainingTime <= 0.0f)
	{
		AICon->StopMovement();
		return EStateTreeRunStatus::Succeeded;
	}

	const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(Enemy);
	const UThreadTalesEnemyConfig* EnemyConfig = EnemyChar ? EnemyChar->GetEnemyConfig() : nullptr;

	const float StrafeMinDist = EnemyConfig ? EnemyConfig->Ranges.StrafeMinDistance : 240.0f;
	const float StrafeMaxDist = EnemyConfig ? EnemyConfig->Ranges.StrafeMaxDistance : 400.0f;
	const float DistSq = FVector::DistSquared2D(Enemy->GetActorLocation(), Target->GetActorLocation());

	// If player closes in during strafe, immediately exit strafe to strike back!
	if (DistSq <= FMath::Square(StrafeMinDist))
	{
		AICon->StopMovement();
		return EStateTreeRunStatus::Succeeded;
	}

	if (DistSq > FMath::Square(StrafeMaxDist))
	{
		AICon->StopMovement();
		return EStateTreeRunStatus::Succeeded;
	}

	if (AICon->GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		const float StrafeDist = EnemyConfig ? ((StrafeMinDist + StrafeMaxDist) * 0.5f) : 250.0f;
		const FVector EnemyLoc = Enemy->GetActorLocation();
		const FVector TargetLoc = Target->GetActorLocation();
		const FVector ToTarget = (TargetLoc - EnemyLoc).GetSafeNormal2D();
		const FVector Tangent = FVector(-ToTarget.Y, ToTarget.X, 0.0f) * static_cast<float>(InstanceData.StrafeDirection);
		const FVector MoveGoal = EnemyLoc + Tangent * StrafeDist;

		AICon->MoveToLocation(MoveGoal, 40.0f, false, true, true);
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EnemyStrafeAroundTarget::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	if (AThreadTalesAIController* AICon = GetAIController(Enemy))
	{
		AICon->ClearFocus(EAIFocusPriority::Gameplay);
		AICon->StopMovement();
	}

	ApplyConfigSpeed(Enemy, &UThreadTalesEnemyConfig::ChaseWalkSpeed);
	EnsureGroundWalking(Enemy);
}

static void ForceGroundFlyingEnemy(AThreadTalesBaseCharacter* Enemy, AThreadTalesAIController* AICon, float Knockback)
{
	if (!Enemy || !Enemy->GetCharacterMovement()) return;
	if (Enemy->GetCharacterMovement()->MovementMode != MOVE_Flying) return;

	FCollisionQueryParams TraceParams(TEXT("ParryGroundCheck"), false, Enemy);
	TraceParams.AddIgnoredActor(Enemy);

	// Graceful physical transition to natural gravity falling
	Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Falling);

	if (AThreadTalesCharacter* T = AICon ? AICon->GetCombatTarget() : nullptr)
	{
		const FVector Loc = Enemy->GetActorLocation();
		const FVector KnockbackDir = (Loc - T->GetActorLocation()).GetSafeNormal2D();
		Enemy->LaunchCharacter(KnockbackDir * Knockback, true, false);
	}
}

// -----------------------------------------------------------------------------
// FStateTreeTask_EnemyStagger
// -----------------------------------------------------------------------------

FStateTreeTask_EnemyStagger::FStateTreeTask_EnemyStagger()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreeTask_EnemyStagger::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	AThreadTalesAIController* AICon = GetAIController(Enemy);
	if (!Enemy) return EStateTreeRunStatus::Failed;
	if (AICon) AICon->StopMovement();

	ForceGroundFlyingEnemy(Enemy, AICon, 350.0f);

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const float Duration = Enemy->GetParryRecoilDuration();
	InstanceData.RemainingTime = FMath::Max(0.5f, Duration);

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Enemy StateTree] '%s' ENTERED PARRY STAGGER (Duration: %.2fs)"),
		*Enemy->GetName(), InstanceData.RemainingTime);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EnemyStagger::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	if (!Enemy)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Do not tick down remaining stagger time while enemy is falling from mid-air interrupt
	if (Enemy->GetCharacterMovement() && Enemy->GetCharacterMovement()->IsFalling())
	{
		return EStateTreeRunStatus::Running;
	}

	InstanceData.RemainingTime -= DeltaTime;

	// Exit stagger when timer expired or character is no longer staggered
	if (!Enemy->IsStaggered() || InstanceData.RemainingTime <= 0.0f)
	{
		if (InstanceData.RemainingTime <= 0.0f && Enemy->IsStaggered())
		{
			// Safe fallback cleanup: ensure tag and timers are cleared if animation or delegate was delayed
			if (UThreadTalesCombatReactionHandler* ReactionHandler = Enemy->FindComponentByClass<UThreadTalesCombatReactionHandler>())
			{
				ReactionHandler->FinishParriedStagger();
			}
		}
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EnemyStagger::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	if (!Enemy)
	{
		return;
	}

	if (AThreadTalesAIController* AICon = GetAIController(Enemy))
	{
		AICon->StopMovement();
	}

	// Invariant: guarantee stagger tag is completely removed when leaving StaggerState
	if (UThreadTalesCombatReactionHandler* ReactionHandler = Enemy->FindComponentByClass<UThreadTalesCombatReactionHandler>())
	{
		ReactionHandler->FinishParriedStagger();
	}
}

// -----------------------------------------------------------------------------
// FStateTreeTask_EnemyPostureBreak
// -----------------------------------------------------------------------------

FStateTreeTask_EnemyPostureBreak::FStateTreeTask_EnemyPostureBreak()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreeTask_EnemyPostureBreak::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	AThreadTalesAIController* AICon = GetAIController(Enemy);
	if (!Enemy)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (AICon) AICon->StopMovement();
	ForceGroundFlyingEnemy(Enemy, AICon, 400.0f);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const float Duration = Enemy->GetPostureBreakStunDuration();
	InstanceData.RemainingTime = FMath::Max(1.0f, Duration);

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Enemy StateTree] '%s' ENTERED POSTURE BREAK STUN (Duration: %.2fs)"),
		*Enemy->GetName(), InstanceData.RemainingTime);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EnemyPostureBreak::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Do not tick down remaining posture break duration while enemy is falling from mid-air knockdown
	if (Enemy && Enemy->GetCharacterMovement() && Enemy->GetCharacterMovement()->IsFalling())
	{
		return EStateTreeRunStatus::Running;
	}

	InstanceData.RemainingTime -= DeltaTime;

	// Exit posture break when character is no longer staggered (both PostureBreak and Recover have completed)
	if (!Enemy || !Enemy->IsStaggered())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EnemyPostureBreak::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	if (AThreadTalesAIController* AICon = GetAIController(Enemy))
	{
		AICon->StopMovement();
	}
}

// -----------------------------------------------------------------------------
// FStateTreeTask_EnemyBackstep
// -----------------------------------------------------------------------------

FStateTreeTask_EnemyBackstep::FStateTreeTask_EnemyBackstep()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreeTask_EnemyBackstep::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	AThreadTalesAIController* AICon = GetAIController(Enemy);
	if (!Enemy) return EStateTreeRunStatus::Failed;

	const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(Enemy);
	const FThreadTalesEnemyDifficultySettings& Difficulty = EnemyChar ? EnemyChar->GetDifficultySettings() : FThreadTalesEnemyDifficultySettings();
	AThreadTalesCharacter* Target = AICon ? AICon->GetCombatTarget() : nullptr;

	const float TargetDist = Target ? FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation()) : 9999.0f;
	const bool bCloseThreat = TargetDist <= Difficulty.CloseThreatDistance;
	const float EvadeChance = bCloseThreat ? Difficulty.ReactiveEvadeChance : Difficulty.PostAttackEvadeChance;
	const bool bLadybug = (EnemyChar && EnemyChar->GetEnemyConfig() && !EnemyChar->GetEnemyConfig()->AttackComboMontages.IsEmpty() &&
		EnemyChar->GetEnemyConfig()->AttackComboMontages[0].GetAssetName().Contains(TEXT("Ladybug")));
	if (!bLadybug && FMath::FRand() > EvadeChance) return EStateTreeRunStatus::Succeeded;

	if (AICon) AICon->StopMovement();

	const FVector AwayFromTarget = (Target && Target->IsAlive())
		? (Enemy->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D()
		: -Enemy->GetActorForwardVector();

	if (!AwayFromTarget.IsNearlyZero())
	{
		Enemy->SetActorRotation(FRotator(0.0f, AwayFromTarget.Rotation().Yaw, 0.0f));
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.DisengageDirection = AwayFromTarget;
	InstanceData.bIsLadybug = bLadybug;

	EnsureGroundWalking(Enemy);
	if (Enemy->GetCharacterMovement()) Enemy->GetCharacterMovement()->Velocity = FVector::ZeroVector;

	float MontageDuration = InstanceData.RecoveryDuration;
	UAnimMontage* EvadeMontage = (EnemyChar && EnemyChar->GetEnemyConfig() && !EnemyChar->GetEnemyConfig()->EvadeMontage.IsNull())
		? EnemyChar->GetEnemyConfig()->EvadeMontage.LoadSynchronous() : nullptr;
	if (!EvadeMontage)
	{
		static const FString FallbackEvadePath = TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/Evade/AM_Evade_Backward.AM_Evade_Backward");
		EvadeMontage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *FallbackEvadePath));
	}
	if (EvadeMontage)
	{
		const float PlayedLength = Enemy->PlayAnimMontage(EvadeMontage, 1.0f);
		if (PlayedLength > 0.0f) MontageDuration = PlayedLength;
	}

	InstanceData.RemainingTime = FMath::Max(MontageDuration, InstanceData.RecoveryDuration);
	InstanceData.TotalDuration = InstanceData.RemainingTime;

	const bool bHasRootMotion = EvadeMontage && EvadeMontage->HasRootMotion();
	if (AICon && !bHasRootMotion)
	{
		if (!bLadybug)
		{
			AICon->MoveToLocation(Enemy->GetActorLocation() + AwayFromTarget * 180.0f, 30.0f, false, true, true);
		}
	}

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Enemy StateTree] '%s' EXECUTING TACTICAL BACKSTEP (Dist: %.1f cm, Duration: %.2fs)"),
		*Enemy->GetName(), TargetDist, InstanceData.RemainingTime);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EnemyBackstep::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime -= DeltaTime;

	// Ground turn before takeoff: rapidly reorient toward combat target in post-roll recovery
	if (InstanceData.RemainingTime <= 0.30f)
	{
		AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
		AThreadTalesAIController* AICon = GetAIController(Enemy);
		AThreadTalesCharacter* Target = AICon ? AICon->GetCombatTarget() : nullptr;
		if (Enemy && Target && Target->IsAlive())
		{
			const FVector ToTarget = (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
			if (!ToTarget.IsNearlyZero())
			{
				const FRotator CurrentRot = Enemy->GetActorRotation();
				const FRotator TargetRot = FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f);
				const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(Enemy);
				const float CfgTurnRate = (EnemyChar && EnemyChar->GetEnemyConfig() && EnemyChar->GetEnemyConfig()->FlightSettings.GroundTurnRate > 0.0f)
					? EnemyChar->GetEnemyConfig()->FlightSettings.GroundTurnRate : 720.0f;
				Enemy->SetActorRotation(FMath::RInterpConstantTo(CurrentRot, TargetRot, DeltaTime, CfgTurnRate));
			}
		}
	}

	if (InstanceData.RemainingTime <= 0.0f)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EnemyBackstep::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	if (AThreadTalesAIController* AICon = GetAIController(Enemy))
	{
		AICon->StopMovement();
	}
	if (Enemy && Enemy->GetCharacterMovement())
	{
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Enemy->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	}
}

// -----------------------------------------------------------------------------
// FStateTreeTask_EnemyPatrol
// -----------------------------------------------------------------------------

FStateTreeTask_EnemyPatrol::FStateTreeTask_EnemyPatrol()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreeTask_EnemyPatrol::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	AThreadTalesAIController* AICon = GetAIController(Enemy);
	if (!Enemy || !AICon)
	{
		return EStateTreeRunStatus::Failed;
	}

	ApplyConfigSpeed(Enemy, &UThreadTalesEnemyConfig::PatrolWalkSpeed);

	if (UThreadTalesEquipmentComponent* EquipComp = Enemy->GetEquipmentComponent())
	{
		if (AThreadTalesWeapon* Weapon = EquipComp->GetCurrentWeapon())
		{
			if (!Weapon->IsSheathed())
			{
				Weapon->AttachToSheath();
				Weapon->SetWeaponState(EWeaponCombatState::Sheathed);
				Enemy->SetOverlayPoseByName(Weapon->GetOverlayPoseName());
			}
		}
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bIsWaitingAtPoint = false;
	InstanceData.RemainingWaitTime = 0.0f;

	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Enemy->GetWorld()))
	{
		FNavLocation RandomNavLoc;
		if (NavSys->GetRandomReachablePointInRadius(Enemy->GetActorLocation(), InstanceData.PatrolRadius, RandomNavLoc))
		{
			InstanceData.CurrentGoalLocation = RandomNavLoc.Location;
			AICon->MoveToLocation(RandomNavLoc.Location, 60.0f);
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Enemy StateTree] '%s' PATROL -> walking to random point (Dist: %.1f cm)"),
				*Enemy->GetName(), FVector::Dist(Enemy->GetActorLocation(), RandomNavLoc.Location));
		}
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EnemyPatrol::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	AThreadTalesAIController* AICon = GetAIController(Enemy);
	if (!Enemy || !AICon)
	{
		return EStateTreeRunStatus::Failed;
	}

	AThreadTalesCharacter* Target = AICon->GetCombatTarget();
	if (!Target)
	{
		Target = AICon->EvaluateBestTarget();
	}
	if (Target && Target->IsAlive())
	{
		AICon->StopMovement();
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Enemy StateTree] '%s' PATROL INTERRUPTED -> target '%s' spotted!"),
			*Enemy->GetName(), *Target->GetName());
		return EStateTreeRunStatus::Succeeded;
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const float DistSq = FVector::DistSquared2D(Enemy->GetActorLocation(), InstanceData.CurrentGoalLocation);
	const bool bArrived = (DistSq <= FMath::Square(80.0f)) || (AICon->GetMoveStatus() == EPathFollowingStatus::Idle);

	if (bArrived)
	{
		if (!InstanceData.bIsWaitingAtPoint)
		{
			InstanceData.bIsWaitingAtPoint = true;
			InstanceData.RemainingWaitTime = InstanceData.WaitDuration;
			AICon->StopMovement();
		}
		else
		{
			InstanceData.RemainingWaitTime -= DeltaTime;
			if (InstanceData.RemainingWaitTime <= 0.0f)
			{
				InstanceData.bIsWaitingAtPoint = false;
				if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Enemy->GetWorld()))
				{
					FNavLocation RandomNavLoc;
					if (NavSys->GetRandomReachablePointInRadius(Enemy->GetActorLocation(), InstanceData.PatrolRadius, RandomNavLoc))
					{
						InstanceData.CurrentGoalLocation = RandomNavLoc.Location;
						AICon->MoveToLocation(RandomNavLoc.Location, 60.0f);
					}
				}
			}
		}
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EnemyPatrol::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	if (AThreadTalesAIController* AICon = GetAIController(Enemy))
	{
		AICon->StopMovement();
	}

	ApplyConfigSpeed(Enemy, &UThreadTalesEnemyConfig::ChaseWalkSpeed);
	EnsureGroundWalking(Enemy);
}

// ================================================================================
// LADYBUG-SPECIFIC TASKS IMPLEMENTATION
// ================================================================================

// -----------------------------------------------------------------------------
// FStateTreeTask_LadybugCircleStrafe
// -----------------------------------------------------------------------------

FStateTreeTask_LadybugCircleStrafe::FStateTreeTask_LadybugCircleStrafe()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreeTask_LadybugCircleStrafe::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	AThreadTalesAIController* AICon = GetAIController(Enemy);
	if (!Enemy || !AICon)
	{
		return EStateTreeRunStatus::Failed;
	}

	// Set strafe walk speed
	if (UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = StrafeWalkSpeed;
	}

	AThreadTalesCharacter* Target = AICon->GetCombatTarget();
	if (!Target)
	{
		Target = AICon->EvaluateBestTarget();
	}

	if (!Target || !Target->IsAlive())
	{
		return EStateTreeRunStatus::Failed;
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime = InstanceData.StrafeDuration;
	InstanceData.TimeSinceLastFlip = 0.0f;
	InstanceData.StrafeDirection = FMath::RandBool() ? 1 : -1;

	AICon->SetFocus(Target, EAIFocusPriority::Gameplay);

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Ladybug StateTree] '%s' CIRCLE STRAFE started (Dir: %d)"),
		*Enemy->GetName(), InstanceData.StrafeDirection);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_LadybugCircleStrafe::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	AThreadTalesAIController* AICon = GetAIController(Enemy);
	if (!Enemy || !AICon)
	{
		return EStateTreeRunStatus::Failed;
	}

	AThreadTalesCharacter* Target = AICon->GetCombatTarget();
	if (!Target)
	{
		Target = AICon->EvaluateBestTarget();
	}

	if (!Target || !Target->IsAlive())
	{
		return EStateTreeRunStatus::Failed;
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime -= DeltaTime;
	InstanceData.TimeSinceLastFlip += DeltaTime;

	// Random direction flip for unpredictability
	if (InstanceData.TimeSinceLastFlip >= InstanceData.DirectionFlipInterval)
	{
		if (FMath::FRand() < 0.5f)
		{
			InstanceData.StrafeDirection *= -1;
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Ladybug StateTree] '%s' FLIPPED STRAFE DIRECTION"), *Enemy->GetName());
		}
		InstanceData.TimeSinceLastFlip = 0.0f;
	}

	// Check for timeout
	if (InstanceData.RemainingTime <= 0.0f)
	{
		AICon->StopMovement();
		return EStateTreeRunStatus::Succeeded;
	}

	const FVector EnemyLoc = Enemy->GetActorLocation();
	const FVector TargetLoc = Target->GetActorLocation();
	const float DistSq = FVector::DistSquared2D(EnemyLoc, TargetLoc);

	// If player invaded close space, exit to disengage
	if (DistSq <= FMath::Square(180.0f))
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Ladybug StateTree] '%s' STRAFE INTERRUPTED - player too close!"), *Enemy->GetName());
		AICon->StopMovement();
		return EStateTreeRunStatus::Succeeded;
	}

	// If player ran far away, exit to chase
	if (DistSq > FMath::Square(500.0f))
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Ladybug StateTree] '%s' STRAFE INTERRUPTED - player too far!"), *Enemy->GetName());
		AICon->StopMovement();
		return EStateTreeRunStatus::Succeeded;
	}

	// Continuous orbit calculation (THIS IS THE KEY FIX)
	const FVector ToTarget = (TargetLoc - EnemyLoc).GetSafeNormal2D();
	const FVector Tangent = FVector(-ToTarget.Y, ToTarget.X, 0.0f) * static_cast<float>(InstanceData.StrafeDirection);

	// Orbit point = maintain distance + lateral offset
	const FVector OrbitGoal = TargetLoc + ToTarget * InstanceData.OrbitRadius + Tangent * InstanceData.TangentOffset;

	// Restart movement if idle (continuous recalculation)
	if (AICon->GetMoveStatus() == EPathFollowingStatus::Idle || FVector::DistSquared2D(EnemyLoc, OrbitGoal) > FMath::Square(100.0f))
	{
		AICon->MoveToLocation(OrbitGoal, 60.0f, false, true, true);
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_LadybugCircleStrafe::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	if (AThreadTalesAIController* AICon = GetAIController(Enemy))
	{
		AICon->ClearFocus(EAIFocusPriority::Gameplay);
		AICon->StopMovement();
	}

	// Restore chase speed
	if (UCharacterMovementComponent* MoveComp = Enemy ? Enemy->GetCharacterMovement() : nullptr)
	{
		MoveComp->MaxWalkSpeed = 600.0f;  // Chase speed
	}
}

// -----------------------------------------------------------------------------
// FStateTreeTask_LadybugRollDisengage
// -----------------------------------------------------------------------------

FStateTreeTask_LadybugRollDisengage::FStateTreeTask_LadybugRollDisengage()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreeTask_LadybugRollDisengage::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = GetEnemyFromContext(Context);
	AThreadTalesAIController* AICon = GetAIController(Enemy);
	if (!Enemy || !AICon)
	{
		return EStateTreeRunStatus::Failed;
	}

	AICon->StopMovement();

	AThreadTalesCharacter* Target = AICon->GetCombatTarget();
	if (!Target)
	{
		Target = AICon->EvaluateBestTarget();
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bHasRotated = false;
	InstanceData.RemainingTime = 0.0f;

	// CRITICAL: Turn 180° AWAY from target before rolling
	if (Target && Target->IsAlive())
	{
		const FVector DirAwayFromTarget = (Enemy->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
		const FRotator OppositeRot = FRotator(0.0f, DirAwayFromTarget.Rotation().Yaw, 0.0f);
		Enemy->SetActorRotation(OppositeRot);
		InstanceData.bHasRotated = true;
	}

	const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(Enemy);
	UAnimMontage* EvadeMontage = nullptr;
	if (EnemyChar && EnemyChar->GetEnemyConfig() && !EnemyChar->GetEnemyConfig()->EvadeMontage.IsNull())
	{
		EvadeMontage = EnemyChar->GetEnemyConfig()->EvadeMontage.LoadSynchronous();
	}

	if (!EvadeMontage)
	{
		// Fallback: hardcoded path to Ladybug roll
		static const FString RollPath = TEXT("/Game/Animations/Enemies/Ladybug/AM_Ladybug_Roll_Evade.AM_Ladybug_Roll_Evade");
		EvadeMontage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *RollPath));
	}

	if (EvadeMontage)
	{
		const float Duration = Enemy->PlayAnimMontage(EvadeMontage, 1.0f);
		InstanceData.RemainingTime = Duration > 0.0f ? Duration : 1.0f;
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Ladybug StateTree] '%s' ROLL DISENGAGE (Duration: %.2fs)"), *Enemy->GetName(), InstanceData.RemainingTime);
	}
	else
	{
		UE_LOG(LogThreadTalesCombat, Warning, TEXT("[Ladybug StateTree] '%s' ROLL DISENGAGE FAILED - No montage!"), *Enemy->GetName());
		InstanceData.RemainingTime = 0.5f;
	}

	// DO NOT call MoveToLocation — Root Motion handles everything
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_LadybugRollDisengage::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime -= DeltaTime;

	if (InstanceData.RemainingTime <= 0.0f)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_LadybugRollDisengage::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// Nothing to clean up — Root Motion handled movement
}
