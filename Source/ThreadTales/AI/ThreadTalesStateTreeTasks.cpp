// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ThreadTalesStateTreeTasks.h"
#include "ThreadTales.h"
#include "Characters/ThreadTalesDummy.h"
#include "Characters/ThreadTalesBaseCharacter.h"
#include "Characters/ThreadTalesEnemyCharacter.h"
#include "Characters/ThreadTalesCharacter.h"
#include "AI/ThreadTalesEnemyStateTreeTasks.h"
#include "AI/ThreadTalesEnemyWindupTask.h"
#include "AI/ThreadTalesEnemyFaceTargetTask.h"
#include "AI/ThreadTalesEnemyAirTasks.h"
#include "StateTreeExecutionContext.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Core/ThreadTalesGameplayTags.h"

#if WITH_EDITOR
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "Components/StateTreeComponentSchema.h"
#include "StateTreeCompilerManager.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimCompositeBase.h"
#endif

static AThreadTalesDummy* GetDummyFromContext(FStateTreeExecutionContext& Context)
{
	UObject* OwnerObj = &(*Context.GetOwner());
	if (AThreadTalesDummy* Dummy = Cast<AThreadTalesDummy>(OwnerObj))
	{
		return Dummy;
	}
	if (AActor* ActorOwner = Cast<AActor>(OwnerObj))
	{
		if (AThreadTalesDummy* Dummy = Cast<AThreadTalesDummy>(ActorOwner))
		{
			return Dummy;
		}
	}
	else if (UActorComponent* CompOwner = Cast<UActorComponent>(OwnerObj))
	{
		return Cast<AThreadTalesDummy>(CompOwner->GetOwner());
	}
	return nullptr;
}

FStateTreeTask_DummyCombatAction::FStateTreeTask_DummyCombatAction()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

static float ExecutePhaseActionAndGetDuration(AThreadTalesDummy* Dummy, ECombatDummyPhaseAction PhaseAction, bool bUseDummyDuration, float DefaultDuration)
{
	if (!Dummy)
	{
		return DefaultDuration;
	}

	switch (PhaseAction)
	{
	case ECombatDummyPhaseAction::IdleWait:
		Dummy->SetAttackState(ECombatDummyAttackState::Idle);
		return Dummy->GetAttackInterval();
	case ECombatDummyPhaseAction::Windup:
		Dummy->StartWindup();
		return bUseDummyDuration ? Dummy->GetParryWindowOpenDelay() : DefaultDuration;
	case ECombatDummyPhaseAction::OpenParryWindow:
		Dummy->OpenParryWindow();
		return bUseDummyDuration ? Dummy->GetParryWindowRemainingDuration() : DefaultDuration;
	case ECombatDummyPhaseAction::ExecuteStrike:
		Dummy->ExecuteStrike();
		return bUseDummyDuration ? Dummy->GetStrikeActiveDuration() : DefaultDuration;
	case ECombatDummyPhaseAction::EndStrike:
		Dummy->EndStrike();
		return 0.0f;
	case ECombatDummyPhaseAction::FinishRecovery:
		Dummy->FinishRecovery();
		return bUseDummyDuration ? Dummy->GetRecoveryDuration() : DefaultDuration;
	case ECombatDummyPhaseAction::StartParryStagger:
		Dummy->StartParryStagger();
		return bUseDummyDuration ? Dummy->GetStaggerDuration() : DefaultDuration;
	case ECombatDummyPhaseAction::FinishParryRecovery:
		Dummy->FinishParryRecovery();
		return bUseDummyDuration ? Dummy->GetPostParryAttackDelay() : DefaultDuration;
	case ECombatDummyPhaseAction::StartPostureBreakStun:
		Dummy->StartPostureBreakStun();
		return bUseDummyDuration ? Dummy->GetPostureBreakStunDuration() : DefaultDuration;
	default:
		return DefaultDuration;
	}
}

static void LogPhaseDebugInfo(const AThreadTalesDummy* Dummy, ECombatDummyPhaseAction PhaseAction, float CalculatedDuration)
{
	if (!Dummy)
	{
		return;
	}

	const TCHAR* PhaseStr = TEXT("Unknown");
	switch (PhaseAction)
	{
	case ECombatDummyPhaseAction::IdleWait: PhaseStr = TEXT("IdleWait"); break;
	case ECombatDummyPhaseAction::Windup: PhaseStr = TEXT("Windup (Telegraph)"); break;
	case ECombatDummyPhaseAction::OpenParryWindow: PhaseStr = TEXT("OpenParryWindow (FLASH)"); break;
	case ECombatDummyPhaseAction::ExecuteStrike: PhaseStr = TEXT("ExecuteStrike (Hitbox)"); break;
	case ECombatDummyPhaseAction::EndStrike: PhaseStr = TEXT("EndStrike"); break;
	case ECombatDummyPhaseAction::FinishRecovery: PhaseStr = TEXT("FinishRecovery"); break;
	case ECombatDummyPhaseAction::StartParryStagger: PhaseStr = TEXT("PARRY STAGGER (AM_Stagger)!"); break;
	case ECombatDummyPhaseAction::FinishParryRecovery: PhaseStr = TEXT("PostParryRecovery"); break;
	case ECombatDummyPhaseAction::StartPostureBreakStun: PhaseStr = TEXT("POSTURE BREAK (Long Stun)!"); break;
	default: break;
	}

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[StateTree AI] '%s' -> Phase: %s (Duration: %.2fs)"),
		*Dummy->GetName(), PhaseStr, CalculatedDuration);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(998877, 2.5f, FColor::Emerald,
			FString::Printf(TEXT("[STATETREE AI] '%s' -> Phase: %s (Time: %.2fs)"),
				*Dummy->GetName(), PhaseStr, CalculatedDuration));
	}
}

EStateTreeRunStatus FStateTreeTask_DummyCombatAction::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesDummy* Dummy = GetDummyFromContext(Context);
	if (!Dummy)
	{
		return EStateTreeRunStatus::Failed;
	}

	Dummy->NotifyStateTreeActive();

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const float CalculatedDuration = ExecutePhaseActionAndGetDuration(Dummy, PhaseAction, bUseDummyConfiguredDuration, InstanceData.Duration);

	if (CalculatedDuration <= 0.0f)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	LogPhaseDebugInfo(Dummy, PhaseAction, CalculatedDuration);

	InstanceData.RemainingTime = CalculatedDuration;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_DummyCombatAction::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	AThreadTalesDummy* Dummy = GetDummyFromContext(Context);
	if (!Dummy)
	{
		return EStateTreeRunStatus::Failed;
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (PhaseAction == ECombatDummyPhaseAction::IdleWait)
	{
		if (!Dummy->bEnableAttacks)
		{
			return EStateTreeRunStatus::Running;
		}

		if (Dummy->IsTargetInAttackRange())
		{
			InstanceData.RemainingTime -= DeltaTime;
			if (InstanceData.RemainingTime <= 0.0f)
			{
				return EStateTreeRunStatus::Succeeded;
			}
		}
		else
		{
			// Reset countdown while out of range so approaching player gets a clean 2.0-2.5s window
			InstanceData.RemainingTime = Dummy->GetAttackInterval();
		}
		return EStateTreeRunStatus::Running;
	}

	InstanceData.RemainingTime -= DeltaTime;
	if (InstanceData.RemainingTime <= 0.0f)
	{
		if (PhaseAction == ECombatDummyPhaseAction::ExecuteStrike)
		{
			Dummy->EndStrike();
		}
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_DummyCombatAction::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesDummy* Dummy = GetDummyFromContext(Context);
	if (!Dummy)
	{
		return;
	}

	if (PhaseAction == ECombatDummyPhaseAction::ExecuteStrike)
	{
		Dummy->CloseParryWindow();
		Dummy->SetHitboxActive(false);
	}
	else if (PhaseAction == ECombatDummyPhaseAction::OpenParryWindow && Transition.CurrentRunStatus != EStateTreeRunStatus::Succeeded)
	{
		Dummy->CloseParryWindow();
	}
}

#if WITH_EDITOR
bool UThreadTalesStateTreeHelper::BuildAndCompileDummyStateTree(UStateTree* StateTreeAsset)
{
	if (!StateTreeAsset)
	{
		return false;
	}

	UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTreeAsset->EditorData);
	if (!EditorData)
	{
		EditorData = NewObject<UStateTreeEditorData>(StateTreeAsset, TEXT("EditorData"), RF_Transactional);
		StateTreeAsset->EditorData = EditorData;
	}

	// 1. Configure Schema
	UStateTreeComponentSchema* Schema = Cast<UStateTreeComponentSchema>(EditorData->Schema);
	if (!Schema)
	{
		Schema = NewObject<UStateTreeComponentSchema>(EditorData);
		EditorData->Schema = Schema;
	}
	if (FProperty* Prop = UStateTreeComponentSchema::StaticClass()->FindPropertyByName(FName(TEXT("ContextActorClass"))))
	{
		TSubclassOf<AActor> ActorClass = AThreadTalesDummy::StaticClass();
		Prop->SetValue_InContainer(Schema, &ActorClass);
	}

	// 2. Clear old states
	EditorData->SubTrees.Empty();

	// 3. Create Root State
	UStateTreeState& RootState = EditorData->AddRootState();
	RootState.Name = FName(TEXT("Root"));
	RootState.SelectionBehavior = EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder;

	// 4. Create States
	UStateTreeState& IdleState = RootState.AddChildState(FName(TEXT("Idle")));
	UStateTreeState& WindupState = RootState.AddChildState(FName(TEXT("Windup")));
	UStateTreeState& ParryWindowState = RootState.AddChildState(FName(TEXT("ParryWindow")));
	UStateTreeState& StrikeState = RootState.AddChildState(FName(TEXT("Strike")));
	UStateTreeState& RecoveryState = RootState.AddChildState(FName(TEXT("Recovery")));
	UStateTreeState& StaggerState = RootState.AddChildState(FName(TEXT("ParryStagger")));
	UStateTreeState& PostParryState = RootState.AddChildState(FName(TEXT("PostParryRecovery")));
	UStateTreeState& PostureBreakState = RootState.AddChildState(FName(TEXT("PostureBreakStun")));

	// 5. Add Tasks
	{
		auto& TaskNode = IdleState.AddTask<FStateTreeTask_DummyCombatAction>();
		TaskNode.Node.GetMutable<FStateTreeTask_DummyCombatAction>().PhaseAction = ECombatDummyPhaseAction::IdleWait;
	}
	{
		auto& TaskNode = WindupState.AddTask<FStateTreeTask_DummyCombatAction>();
		TaskNode.Node.GetMutable<FStateTreeTask_DummyCombatAction>().PhaseAction = ECombatDummyPhaseAction::Windup;
	}
	{
		auto& TaskNode = ParryWindowState.AddTask<FStateTreeTask_DummyCombatAction>();
		TaskNode.Node.GetMutable<FStateTreeTask_DummyCombatAction>().PhaseAction = ECombatDummyPhaseAction::OpenParryWindow;
	}
	{
		auto& TaskNode = StrikeState.AddTask<FStateTreeTask_DummyCombatAction>();
		TaskNode.Node.GetMutable<FStateTreeTask_DummyCombatAction>().PhaseAction = ECombatDummyPhaseAction::ExecuteStrike;
	}
	{
		auto& TaskNode = RecoveryState.AddTask<FStateTreeTask_DummyCombatAction>();
		TaskNode.Node.GetMutable<FStateTreeTask_DummyCombatAction>().PhaseAction = ECombatDummyPhaseAction::FinishRecovery;
	}
	{
		auto& TaskNode = StaggerState.AddTask<FStateTreeTask_DummyCombatAction>();
		TaskNode.Node.GetMutable<FStateTreeTask_DummyCombatAction>().PhaseAction = ECombatDummyPhaseAction::StartParryStagger;
	}
	{
		auto& TaskNode = PostParryState.AddTask<FStateTreeTask_DummyCombatAction>();
		TaskNode.Node.GetMutable<FStateTreeTask_DummyCombatAction>().PhaseAction = ECombatDummyPhaseAction::FinishParryRecovery;
	}
	{
		auto& TaskNode = PostureBreakState.AddTask<FStateTreeTask_DummyCombatAction>();
		TaskNode.Node.GetMutable<FStateTreeTask_DummyCombatAction>().PhaseAction = ECombatDummyPhaseAction::StartPostureBreakStun;
	}

	// 6. Add Normal Sequence Transitions (OnStateCompleted)
	IdleState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &WindupState);
	WindupState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &ParryWindowState);
	ParryWindowState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &StrikeState);
	StrikeState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &RecoveryState);
	RecoveryState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &IdleState);
	StaggerState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &PostParryState);
	PostParryState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &WindupState);
	PostureBreakState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &IdleState);

	// 7. Add Global Parry & Posture Break Interrupt Transitions to RootState
	const FGameplayTag& ParryTag = ThreadTalesGameplayTags::TAG_Event_Combat_Parry;
	RootState.AddTransition(EStateTreeTransitionTrigger::OnEvent, ParryTag, EStateTreeTransitionType::GotoState, &StaggerState);

	const FGameplayTag& PostureBreakTag = ThreadTalesGameplayTags::TAG_Event_Combat_PostureBreak;
	RootState.AddTransition(EStateTreeTransitionTrigger::OnEvent, PostureBreakTag, EStateTreeTransitionType::GotoState, &PostureBreakState);

	// 8. Compile synchronously
	bool bCompileSuccess = UE::StateTree::Compiler::FCompilerManager::CompileSynchronously(StateTreeAsset);
	StateTreeAsset->MarkPackageDirty();

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[StateTreeHelper] Successfully built and compiled ST_Dummy_Combat! ReadyToRun: %s (CompileSuccess: %s)"),
		StateTreeAsset->IsReadyToRun() ? TEXT("TRUE") : TEXT("FALSE"),
		bCompileSuccess ? TEXT("TRUE") : TEXT("FALSE"));

	return StateTreeAsset->IsReadyToRun();
}

bool UThreadTalesStateTreeHelper::BuildAndCompileEnemyStateTree(UStateTree* StateTreeAsset)
{
	if (!StateTreeAsset)
	{
		return false;
	}

	UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTreeAsset->EditorData);
	if (!EditorData)
	{
		EditorData = NewObject<UStateTreeEditorData>(StateTreeAsset, TEXT("EditorData"), RF_Transactional);
		StateTreeAsset->EditorData = EditorData;
	}

	// 1. Configure Schema compatible with UStateTreeComponent on AThreadTalesEnemyCharacter
	UStateTreeComponentSchema* Schema = Cast<UStateTreeComponentSchema>(EditorData->Schema);
	if (!Schema)
	{
		Schema = NewObject<UStateTreeComponentSchema>(EditorData);
		EditorData->Schema = Schema;
	}
	if (FProperty* Prop = UStateTreeComponentSchema::StaticClass()->FindPropertyByName(FName(TEXT("ContextActorClass"))))
	{
		TSubclassOf<AActor> ActorClass = AThreadTalesBaseCharacter::StaticClass();
		Prop->SetValue_InContainer(Schema, &ActorClass);
	}

	// 2. Clear old states
	EditorData->SubTrees.Empty();

	// 3. Create Root State
	UStateTreeState& RootState = EditorData->AddRootState();
	RootState.Name = FName(TEXT("Root"));
	RootState.SelectionBehavior = EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder;

	// 4. Create States: Patrol -> Chase -> FaceTarget -> Windup -> Attack -> Disengage -> Strafe, plus Stagger & PostureBreak
	UStateTreeState& PatrolState = RootState.AddChildState(FName(TEXT("Patrol")));
	UStateTreeState& ChaseState = RootState.AddChildState(FName(TEXT("Chase")));
	UStateTreeState& FaceTargetState = RootState.AddChildState(FName(TEXT("FaceTarget")));
	UStateTreeState& WindupState = RootState.AddChildState(FName(TEXT("Windup")));
	UStateTreeState& AttackState = RootState.AddChildState(FName(TEXT("Attack")));
	UStateTreeState& DisengageState = RootState.AddChildState(FName(TEXT("Disengage")));
	UStateTreeState& StrafeState = RootState.AddChildState(FName(TEXT("Strafe")));
	UStateTreeState& StaggerState = RootState.AddChildState(FName(TEXT("ParryStagger")));
	UStateTreeState& PostureBreakState = RootState.AddChildState(FName(TEXT("PostureBreakStun")));

	// 5. Add Tasks
	// Patrol: Explores surrounding area with random NavMesh points and sheathed katana
	{
		auto& TaskNode = PatrolState.AddTask<FStateTreeTask_EnemyPatrol>();
		TaskNode.Node.GetMutable<FStateTreeTask_EnemyPatrol>().PatrolRadius = 900.0f;
		TaskNode.Node.GetMutable<FStateTreeTask_EnemyPatrol>().PatrolWalkSpeed = 200.0f;
	}
	// Chase: Draws katana and closes in directly to melee attack range
	{
		auto& TaskNode = ChaseState.AddTask<FStateTreeTask_EnemyChaseTarget>();
		TaskNode.Node.GetMutable<FStateTreeTask_EnemyChaseTarget>().AcceptanceRadius = 60.0f;
	}
	// FaceTarget: Smoothly reorients toward player before telegraph/attack strike (~360 deg/s, max 0.45s)
	{
		auto& TaskNode = FaceTargetState.AddTask<FStateTreeTask_EnemyFaceTarget>();
		TaskNode.Node.GetMutable<FStateTreeTask_EnemyFaceTarget>().PrecisionDegrees = 4.0f;
		TaskNode.Node.GetMutable<FStateTreeTask_EnemyFaceTarget>().MaxTurnDuration = 0.45f;
	}
	// Windup: Holds telegraph stance and aims toward player before strike (~0.65s)
	{
		auto& TaskNode = WindupState.AddTask<FStateTreeTask_EnemyWindup>();
		TaskNode.Node.GetMutable<FStateTreeTask_EnemyWindup>().WindupDuration = 0.65f;
	}
	// Attack: Executes Katana Melee Combo directly
	{
		auto& TaskNode = AttackState.AddTask<FStateTreeTask_EnemyMeleeCombo>();
		TaskNode.Node.GetMutable<FStateTreeTask_EnemyMeleeCombo>();
	}
	// Disengage: Executes backward evade + recovery pause (window of vulnerability for player)
	{
		auto& TaskNode = DisengageState.AddTask<FStateTreeTask_EnemyBackstep>();
		TaskNode.Node.GetMutable<FStateTreeTask_EnemyBackstep>();
	}
	// Strafe: Circling/Combat stance after attack
	{
		auto& TaskNode = StrafeState.AddTask<FStateTreeTask_EnemyStrafeAroundTarget>();
		TaskNode.Node.GetMutable<FStateTreeTask_EnemyStrafeAroundTarget>();
	}
	// ParryStagger: Plays AM_Stagger upon being parried
	{
		auto& TaskNode = StaggerState.AddTask<FStateTreeTask_EnemyStagger>();
		TaskNode.Node.GetMutable<FStateTreeTask_EnemyStagger>();
	}
	// PostureBreakStun: Plays AM_PostureBreak knockdown stun
	{
		auto& TaskNode = PostureBreakState.AddTask<FStateTreeTask_EnemyPostureBreak>();
		TaskNode.Node.GetMutable<FStateTreeTask_EnemyPostureBreak>();
	}

	// 6. Transitions
	// Patrol -> Chase (when player is sensed by perception / threat table)
	PatrolState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &ChaseState);
	PatrolState.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &ChaseState);

	// Chase -> FaceTarget (when arrived at melee attack range, smoothly orient first!)
	ChaseState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &FaceTargetState);
	ChaseState.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &PatrolState);

	// FaceTarget -> Windup (aligned to target -> telegraph strike)
	FaceTargetState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &WindupState);
	FaceTargetState.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &ChaseState);

	// Windup -> Attack (telegraph completed -> strike with katana/jaws)
	WindupState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &AttackState);
	WindupState.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &ChaseState);

	// Attack -> Disengage (when combo finishes)
	AttackState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &DisengageState);
	AttackState.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &DisengageState);

	// Disengage -> Strafe (cooldown finished, resume neutral circling)
	DisengageState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &StrafeState);
	DisengageState.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &StrafeState);

	// Strafe -> Chase (after circling, closes in for next strike)
	StrafeState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &ChaseState);
	StrafeState.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &PatrolState);

	// Stagger -> Disengage (after being staggered, creates separation)
	StaggerState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &DisengageState);
	StaggerState.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &DisengageState);

	// PostureBreak -> Strafe (after stun ends, resumes combat)
	PostureBreakState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &StrafeState);
	PostureBreakState.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &StrafeState);

	// 7. Global Interrupt Transitions on RootState
	FGameplayTag ParryTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Combat.Parry")));
	RootState.AddTransition(EStateTreeTransitionTrigger::OnEvent, ParryTag, EStateTreeTransitionType::GotoState, &StaggerState);

	FGameplayTag PostureBreakTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Combat.PostureBreak")));
	RootState.AddTransition(EStateTreeTransitionTrigger::OnEvent, PostureBreakTag, EStateTreeTransitionType::GotoState, &PostureBreakState);

	// 8. Compile synchronously
	bool bCompileSuccess = UE::StateTree::Compiler::FCompilerManager::CompileSynchronously(StateTreeAsset);
	StateTreeAsset->MarkPackageDirty();

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[StateTreeHelper] Successfully built and compiled ST_Enemy_Samurai! ReadyToRun: %s (CompileSuccess: %s)"),
		StateTreeAsset->IsReadyToRun() ? TEXT("TRUE") : TEXT("FALSE"),
		bCompileSuccess ? TEXT("TRUE") : TEXT("FALSE"));

	return StateTreeAsset->IsReadyToRun();
}

bool UThreadTalesStateTreeHelper::BuildKnockdownPostureBreakMontage()
{
	const FString StartPath = TEXT("/Game/Animations/KatanaAnimations_Retarget/AS_Knock_Down_Start_Seq.AS_Knock_Down_Start_Seq");
	const FString LoopPath = TEXT("/Game/Animations/KatanaAnimations_Retarget/AS_Knock_Down_Loop_Seq.AS_Knock_Down_Loop_Seq");
	const FString EndPath = TEXT("/Game/Animations/KatanaAnimations_Retarget/AS_Knock_Down_End_Seq.AS_Knock_Down_End_Seq");
	const FString MontagePath = TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/AM_PostureBreak.AM_PostureBreak");

	UAnimSequence* StartSeq = Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, *StartPath));
	UAnimSequence* LoopSeq = Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, *LoopPath));
	UAnimSequence* EndSeq = Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, *EndPath));

	if (!StartSeq || !LoopSeq || !EndSeq)
	{
		UE_LOG(LogThreadTalesCombat, Error, TEXT("[MontageBuilder] Failed to load one or more knock down sequences!"));
		return false;
	}

	UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *MontagePath));
	if (!Montage)
	{
		UE_LOG(LogThreadTalesCombat, Error, TEXT("[MontageBuilder] Failed to load AM_PostureBreak!"));
		return false;
	}

	Montage->Modify();

	// Clear existing tracks & sections
	Montage->SlotAnimTracks.Empty();
	Montage->CompositeSections.Empty();

	// Add DefaultGroup.DefaultSlot
	FSlotAnimationTrack& Track = Montage->AddSlot(FName(TEXT("DefaultSlot")));
	FAnimTrack& AnimTrack = Track.AnimTrack;
	AnimTrack.AnimSegments.Empty();

	float CurrentTime = 0.0f;

	// Segment 0: Start (Knock down to ground)
	{
		FAnimSegment Seg;
		Seg.SetAnimReference(StartSeq);
		Seg.StartPos = CurrentTime;
		Seg.AnimStartTime = 0.0f;
		Seg.AnimEndTime = StartSeq->GetPlayLength();
		Seg.AnimPlayRate = 1.0f;
		Seg.LoopingCount = 1;
		AnimTrack.AnimSegments.Add(Seg);

		Montage->AddAnimCompositeSection(FName(TEXT("Start")), CurrentTime);
		CurrentTime += StartSeq->GetPlayLength();
	}

	// Segment 1: Loop (Stunned on ground)
	{
		FAnimSegment Seg;
		Seg.SetAnimReference(LoopSeq);
		Seg.StartPos = CurrentTime;
		Seg.AnimStartTime = 0.0f;
		Seg.AnimEndTime = LoopSeq->GetPlayLength();
		Seg.AnimPlayRate = 1.0f;
		Seg.LoopingCount = 1;
		AnimTrack.AnimSegments.Add(Seg);

		Montage->AddAnimCompositeSection(FName(TEXT("Loop")), CurrentTime);
		CurrentTime += LoopSeq->GetPlayLength();
	}

	// Segment 2: End (Get up from ground back to combat stance)
	{
		FAnimSegment Seg;
		Seg.SetAnimReference(EndSeq);
		Seg.StartPos = CurrentTime;
		Seg.AnimStartTime = 0.0f;
		Seg.AnimEndTime = EndSeq->GetPlayLength();
		Seg.AnimPlayRate = 1.0f;
		Seg.LoopingCount = 1;
		AnimTrack.AnimSegments.Add(Seg);

		Montage->AddAnimCompositeSection(FName(TEXT("End")), CurrentTime);
		CurrentTime += EndSeq->GetPlayLength();
	}

	if (Montage->CompositeSections.Num() >= 3)
	{
		Montage->CompositeSections[0].NextSectionName = FName(TEXT("Loop"));
		Montage->CompositeSections[1].NextSectionName = FName(TEXT("End"));
		Montage->CompositeSections[2].NextSectionName = NAME_None;
	}

	const float CalcLength = Montage->CalculateSequenceLength();
	Montage->SetCompositeLength(CalcLength);
	Montage->PostEditChange();
	Montage->MarkPackageDirty();

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[MontageBuilder] Successfully built AM_PostureBreak with KnockDown (Start->Loop->End)! Total length: %.2fs"),
		Montage->GetPlayLength());

	return true;
}
bool UThreadTalesStateTreeHelper::BuildAndCompileLadybugStateTree(UStateTree* StateTreeAsset)
{
	if (!StateTreeAsset)
	{
		return false;
	}

	UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTreeAsset->EditorData);
	if (!EditorData)
	{
		EditorData = NewObject<UStateTreeEditorData>(StateTreeAsset, TEXT("EditorData"), RF_Transactional);
		StateTreeAsset->EditorData = EditorData;
	}

	// 1. Configure Schema compatible with UStateTreeComponent on AThreadTalesEnemyCharacter
	UStateTreeComponentSchema* Schema = Cast<UStateTreeComponentSchema>(EditorData->Schema);
	if (!Schema)
	{
		Schema = NewObject<UStateTreeComponentSchema>(EditorData);
		EditorData->Schema = Schema;
	}
	if (FProperty* Prop = UStateTreeComponentSchema::StaticClass()->FindPropertyByName(FName(TEXT("ContextActorClass"))))
	{
		TSubclassOf<AActor> ActorClass = AThreadTalesBaseCharacter::StaticClass();
		Prop->SetValue_InContainer(Schema, &ActorClass);
	}

	// 2. Clear old states
	EditorData->SubTrees.Empty();

	// 3. Create Root State
	UStateTreeState& RootState = EditorData->AddRootState();
	RootState.Name = FName(TEXT("Root"));
	RootState.SelectionBehavior = EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder;

	// 4. Create States: Patrol -> Air Phase -> Melee Phase -> Disengage Roll -> Loop
	UStateTreeState& PatrolState = RootState.AddChildState(FName(TEXT("Patrol")));
	UStateTreeState& TakeoffState = RootState.AddChildState(FName(TEXT("Takeoff")));
	UStateTreeState& AirHoverState = RootState.AddChildState(FName(TEXT("AirHover")));
	UStateTreeState& DiveAttackState = RootState.AddChildState(FName(TEXT("DiveAttack")));
	UStateTreeState& ChaseState = RootState.AddChildState(FName(TEXT("GroundChase")));
	UStateTreeState& FaceTargetState = RootState.AddChildState(FName(TEXT("FaceTarget")));
	UStateTreeState& AttackState = RootState.AddChildState(FName(TEXT("MeleeCombo")));
	UStateTreeState& RollEvadeState = RootState.AddChildState(FName(TEXT("RollEvade")));
	UStateTreeState& StaggerState = RootState.AddChildState(FName(TEXT("ParryStagger")));
	UStateTreeState& PostureBreakState = RootState.AddChildState(FName(TEXT("PostureBreakStun")));

	// 5. Add Tasks
	// Patrol: Explores surrounding area with random NavMesh points
	{
		auto& TaskNode = PatrolState.AddTask<FStateTreeTask_EnemyPatrol>();
		TaskNode.Node.GetMutable<FStateTreeTask_EnemyPatrol>().PatrolRadius = 900.0f;
		TaskNode.Node.GetMutable<FStateTreeTask_EnemyPatrol>().PatrolWalkSpeed = 200.0f;
	}
	// Takeoff: Lifts to ~260cm above ground in MOVE_Flying
	{
		auto& TaskNode = TakeoffState.AddTask<FStateTreeTask_LadybugTakeoff>();
		TaskNode.Node.GetMutable<FStateTreeTask_LadybugTakeoff>().HoverAltitude = 260.0f;
		TaskNode.Node.GetMutable<FStateTreeTask_LadybugTakeoff>().MaxTakeoffDuration = 1.0f;
	}
	// AirHover: Hovers and tracks target player for ~1.5s
	{
		auto& TaskNode = AirHoverState.AddTask<FStateTreeTask_LadybugAirHover>();
		TaskNode.Node.GetMutable<FStateTreeTask_LadybugAirHover>().HoverDuration = 1.5f;
	}
	// DiveAttack: Dives at 1300 cm/s toward player and transitions to MOVE_Walking upon impact
	{
		auto& TaskNode = DiveAttackState.AddTask<FStateTreeTask_LadybugDiveAttack>();
		TaskNode.Node.GetMutable<FStateTreeTask_LadybugDiveAttack>().DiveSpeed = 1300.0f;
	}
	// GroundChase: Aggressive scurrying toward player
	{
		auto& TaskNode = ChaseState.AddTask<FStateTreeTask_EnemyChaseTarget>();
		TaskNode.Node.GetMutable<FStateTreeTask_EnemyChaseTarget>().AcceptanceRadius = 60.0f;
	}
	// FaceTarget: Smoothly reorients toward player before leap combo
	{
		auto& TaskNode = FaceTargetState.AddTask<FStateTreeTask_EnemyFaceTarget>();
		TaskNode.Node.GetMutable<FStateTreeTask_EnemyFaceTarget>().PrecisionDegrees = 4.0f;
		TaskNode.Node.GetMutable<FStateTreeTask_EnemyFaceTarget>().MaxTurnDuration = 0.35f;
	}
	// MeleeCombo: Executes Attack 1 -> Attack 2 leaps with Active Steering
	{
		auto& TaskNode = AttackState.AddTask<FStateTreeTask_EnemyMeleeCombo>();
		TaskNode.Node.GetMutable<FStateTreeTask_EnemyMeleeCombo>();
	}
	// RollEvade: Rolls backward/away curled in a ball (AM_Ladybug_Roll_Evade)
	{
		auto& TaskNode = RollEvadeState.AddTask<FStateTreeTask_EnemyBackstep>();
		TaskNode.Node.GetMutable<FStateTreeTask_EnemyBackstep>();
	}
	// ParryStagger: Stagger upon parry
	{
		auto& TaskNode = StaggerState.AddTask<FStateTreeTask_EnemyStagger>();
		TaskNode.Node.GetMutable<FStateTreeTask_EnemyStagger>();
	}
	// PostureBreakStun: Knockdown on back
	{
		auto& TaskNode = PostureBreakState.AddTask<FStateTreeTask_EnemyPostureBreak>();
		TaskNode.Node.GetMutable<FStateTreeTask_EnemyPostureBreak>();
	}

	// 6. Transitions (Patrol -> Air Phase -> Ground Melee -> Disengage -> Repeat)
	PatrolState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &TakeoffState);
	PatrolState.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &TakeoffState);

	TakeoffState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &AirHoverState);
	TakeoffState.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &ChaseState);

	AirHoverState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &DiveAttackState);
	AirHoverState.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &ChaseState);

	DiveAttackState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &FaceTargetState);
	DiveAttackState.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &TakeoffState);

	ChaseState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &FaceTargetState);
	ChaseState.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &PatrolState);

	FaceTargetState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &AttackState);
	FaceTargetState.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &AttackState);

	AttackState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &RollEvadeState);
	AttackState.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &RollEvadeState);

	RollEvadeState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &TakeoffState);
	RollEvadeState.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &TakeoffState);

	StaggerState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &RollEvadeState);
	StaggerState.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &RollEvadeState);
	PostureBreakState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &TakeoffState);
	PostureBreakState.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &TakeoffState);

	// 7. Global Interrupts on RootState
	const FGameplayTag& ParryTag = ThreadTalesGameplayTags::TAG_Event_Combat_Parry;
	RootState.AddTransition(EStateTreeTransitionTrigger::OnEvent, ParryTag, EStateTreeTransitionType::GotoState, &StaggerState);

	const FGameplayTag& PostureBreakTag = ThreadTalesGameplayTags::TAG_Event_Combat_PostureBreak;
	RootState.AddTransition(EStateTreeTransitionTrigger::OnEvent, PostureBreakTag, EStateTreeTransitionType::GotoState, &PostureBreakState);

	// 8. Compile synchronously
	bool bCompileSuccess = UE::StateTree::Compiler::FCompilerManager::CompileSynchronously(StateTreeAsset);
	StateTreeAsset->MarkPackageDirty();

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[StateTreeHelper] Successfully built and compiled ST_Enemy_Ladybug! ReadyToRun: %s (CompileSuccess: %s)"),
		StateTreeAsset->IsReadyToRun() ? TEXT("TRUE") : TEXT("FALSE"),
		bCompileSuccess ? TEXT("TRUE") : TEXT("FALSE"));

	return StateTreeAsset->IsReadyToRun();
}

#endif
