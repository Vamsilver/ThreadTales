// Copyright Epic Games, Inc. All Rights Reserved.
#include "AI/ThreadTalesEnemyAirTasks.h"
#include "AI/ThreadTalesAIController.h"
#include "AI/ThreadTalesEnemyConfig.h"
#include "Characters/ThreadTalesBaseCharacter.h"
#include "Characters/ThreadTalesEnemyCharacter.h"
#include "Characters/ThreadTalesCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "StateTreeExecutionContext.h"
#include "Animation/AnimMontage.h"
#include "ThreadTales.h"
// -----------------------------------------------------------------------------
// FStateTreeTask_LadybugTakeoff
// -----------------------------------------------------------------------------
FStateTreeTask_LadybugTakeoff::FStateTreeTask_LadybugTakeoff()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}
EStateTreeRunStatus FStateTreeTask_LadybugTakeoff::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = Cast<AThreadTalesBaseCharacter>(Context.GetOwner());
	if (!Enemy || !Enemy->GetCharacterMovement())
	{
		return EStateTreeRunStatus::Failed;
	}
	AThreadTalesAIController* AICon = Cast<AThreadTalesAIController>(Enemy->GetController());
	AThreadTalesCharacter* Target = AICon ? AICon->GetCombatTarget() : nullptr;
	if (AICon)
	{
		AICon->StopMovement();
		if (Target)
		{
			AICon->SetFocus(Target, EAIFocusPriority::Gameplay);
		}
	}
	Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Enemy->GetCharacterMovement()->BrakingDecelerationFlying = 2000.0f;
	Enemy->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(Enemy);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const float ConfigAltitude = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->FlightSettings.HoverAltitude : 0.0f;
	const float EffectiveAltitude = ConfigAltitude > 0.0f ? ConfigAltitude : (HoverAltitude > 0.0f ? HoverAltitude : InstanceData.HoverAltitude);
	FHitResult GroundHit;
	FCollisionQueryParams QueryParams(TEXT("LadybugTakeoffGroundTrace"), false, Enemy);
	QueryParams.AddIgnoredActor(Enemy);
	const FVector StartLoc = Enemy->GetActorLocation();
	const FVector EndLoc = StartLoc - FVector(0.0f, 0.0f, 800.0f);
	float GroundZ = StartLoc.Z;
	if (Enemy->GetWorld()->LineTraceSingleByChannel(GroundHit, StartLoc, EndLoc, ECC_Visibility, QueryParams))
	{
		GroundZ = GroundHit.ImpactPoint.Z;
	}
	InstanceData.GroundZ = GroundZ;

	// Target altitude relative to combat target (player) while guaranteeing safe ground clearance
	const float TargetRefZ = Target ? Target->GetActorLocation().Z : GroundZ;
	const float BaseRefZ = FMath::Max(GroundZ, TargetRefZ);
	InstanceData.TargetAltitudeZ = BaseRefZ + EffectiveAltitude;
	UAnimMontage* TakeoffMontage = nullptr;
	if (EnemyChar && EnemyChar->GetEnemyConfig() && !EnemyChar->GetEnemyConfig()->TakeoffMontage.IsNull())
	{
		TakeoffMontage = EnemyChar->GetEnemyConfig()->TakeoffMontage.LoadSynchronous();
	}
	if (!TakeoffMontage)
	{
		static const FString FallbackTakeoff = TEXT("/Game/Animations/Enemies/Ladybug/JumpToFly/AM_Ladybug_Takeoff.AM_Ladybug_Takeoff");
		TakeoffMontage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *FallbackTakeoff));
	}
	const float ConfigTakeoffDur = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->FlightSettings.TakeoffDuration : 0.0f;
	float Duration = ConfigTakeoffDur > 0.0f ? ConfigTakeoffDur : MaxTakeoffDuration;
	if (TakeoffMontage)
	{
		const float PlayedLen = Enemy->PlayAnimMontage(TakeoffMontage, 1.2f);
		if (PlayedLen > 0.0f)
		{
			Duration = PlayedLen;
		}
	}
	InstanceData.RemainingTime = Duration;
	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Ladybug Air] '%s' TAKEOFF -> target Z: %.1f (Duration: %.2fs)"),
		*Enemy->GetName(), InstanceData.TargetAltitudeZ, Duration);
	return EStateTreeRunStatus::Running;
}
EStateTreeRunStatus FStateTreeTask_LadybugTakeoff::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime -= DeltaTime;
	AThreadTalesBaseCharacter* Enemy = Cast<AThreadTalesBaseCharacter>(Context.GetOwner());
	if (!Enemy)
	{
		return EStateTreeRunStatus::Failed;
	}
	const FVector CurrentLoc = Enemy->GetActorLocation();

	// Only lift off once AnimNotifyState_RootMotionFlight switches movement mode to MOVE_Flying
	if (Enemy->GetCharacterMovement() && Enemy->GetCharacterMovement()->MovementMode == MOVE_Flying)
	{
		const float NewZ = FMath::FInterpTo(CurrentLoc.Z, InstanceData.TargetAltitudeZ, DeltaTime, 4.5f);
		Enemy->SetActorLocation(FVector(CurrentLoc.X, CurrentLoc.Y, NewZ), false);
	}
	else if (Enemy->GetCharacterMovement() && InstanceData.RemainingTime <= 0.8f)
	{
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	}
	AThreadTalesAIController* AICon = Cast<AThreadTalesAIController>(Enemy->GetController());
	AThreadTalesCharacter* Target = AICon ? AICon->GetCombatTarget() : nullptr;
	if (Target)
	{
		const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(Enemy);
		const float ConfigAltitude = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->FlightSettings.HoverAltitude : 0.0f;
		const float EffectiveAltitude = ConfigAltitude > 0.0f ? ConfigAltitude : (HoverAltitude > 0.0f ? HoverAltitude : InstanceData.HoverAltitude);
		const float DesiredTargetZ = FMath::Max(InstanceData.GroundZ, Target->GetActorLocation().Z) + EffectiveAltitude;
		InstanceData.TargetAltitudeZ = FMath::FInterpTo(InstanceData.TargetAltitudeZ, DesiredTargetZ, DeltaTime, 5.0f);

		const FVector ToTarget = (Target->GetActorLocation() - CurrentLoc).GetSafeNormal2D();
		if (!ToTarget.IsNearlyZero())
		{
			const FRotator TargetRot = FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f);
			const float CfgGroundTurnRate = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->FlightSettings.GroundTurnRate : 0.0f;
			const float CfgAirTurnRate = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->FlightSettings.AirTurnRate : 0.0f;
			const float EffectiveTurnRate = CfgGroundTurnRate > 0.0f ? CfgGroundTurnRate : (GroundTurnRate > 0.0f ? GroundTurnRate : InstanceData.GroundTurnRate);
			const float TurnRate = (Enemy->GetCharacterMovement() && Enemy->GetCharacterMovement()->MovementMode == MOVE_Flying)
				? (CfgAirTurnRate > 0.0f ? CfgAirTurnRate : 540.0f) : (EffectiveTurnRate > 0.0f ? EffectiveTurnRate : 720.0f);
			Enemy->SetActorRotation(FMath::RInterpConstantTo(Enemy->GetActorRotation(), TargetRot, DeltaTime, TurnRate));
		}
	}
	// Direct Cross-fade into next montage: transition slightly before Takeoff montage ends
	// so AirHover montage directly preempts it in DefaultSlot without exposing base AnimGraph Idle
	constexpr float CrossfadeLeadTime = 0.20f;
	const bool bAltitudeReached = (FMath::Abs(CurrentLoc.Z - InstanceData.TargetAltitudeZ) <= 15.0f && InstanceData.RemainingTime <= 0.45f);
	if (InstanceData.RemainingTime <= CrossfadeLeadTime || bAltitudeReached)
	{
		return EStateTreeRunStatus::Succeeded;
	}
	return EStateTreeRunStatus::Running;
}
void FStateTreeTask_LadybugTakeoff::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// Sustain movement mode and momentum for seamless cross-fade into AirHover
}
// -----------------------------------------------------------------------------
// FStateTreeTask_LadybugAirHover
// -----------------------------------------------------------------------------
FStateTreeTask_LadybugAirHover::FStateTreeTask_LadybugAirHover()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}
EStateTreeRunStatus FStateTreeTask_LadybugAirHover::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = Cast<AThreadTalesBaseCharacter>(Context.GetOwner());
	if (!Enemy || !Enemy->GetCharacterMovement())
	{
		return EStateTreeRunStatus::Failed;
	}
	Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	AThreadTalesAIController* AICon = Cast<AThreadTalesAIController>(Enemy->GetController());
	AThreadTalesCharacter* Target = AICon ? AICon->GetCombatTarget() : nullptr;
	if (AICon)
	{
		AICon->StopMovement();
		if (Target)
		{
			AICon->SetFocus(Target, EAIFocusPriority::Gameplay);
		}
	}
	const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(Enemy);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const float CfgMaxFlightDur = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->FlightSettings.AirFlightMaxDuration : 4.0f;
	InstanceData.RemainingTime = CfgMaxFlightDur;
	InstanceData.BaseAltitudeZ = Enemy->GetActorLocation().Z;
	InstanceData.HoverElapsedTime = 0.0f;
	UAnimMontage* HoverMontage = nullptr;
	if (EnemyChar && EnemyChar->GetEnemyConfig() && !EnemyChar->GetEnemyConfig()->AirHoverMontage.IsNull())
	{
		HoverMontage = EnemyChar->GetEnemyConfig()->AirHoverMontage.LoadSynchronous();
	}
	if (!HoverMontage)
	{
		static const FString FallbackHover = TEXT("/Game/Animations/Enemies/Ladybug/Air/AM_Ladybug_Air_Hover.AM_Ladybug_Air_Hover");
		HoverMontage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *FallbackHover));
	}
	if (HoverMontage)
	{
		Enemy->PlayAnimMontage(HoverMontage, 1.2f);
	}
	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Ladybug Air] '%s' AIR FLIGHT TOWARDS TARGET -> (MaxDuration: %.2fs, BaseZ: %.1f)"),
		*Enemy->GetName(), InstanceData.RemainingTime, InstanceData.BaseAltitudeZ);
	return EStateTreeRunStatus::Running;
}
EStateTreeRunStatus FStateTreeTask_LadybugAirHover::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime -= DeltaTime;
	InstanceData.HoverElapsedTime += DeltaTime;
	AThreadTalesBaseCharacter* Enemy = Cast<AThreadTalesBaseCharacter>(Context.GetOwner());
	AThreadTalesAIController* AICon = Enemy ? Cast<AThreadTalesAIController>(Enemy->GetController()) : nullptr;
	AThreadTalesCharacter* Target = AICon ? AICon->GetCombatTarget() : nullptr;
	if (Enemy && Target)
	{
		const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(Enemy);

		// 1. Maintain looping flight montage
		if (UAnimInstance* AnimInst = Enemy->GetMesh() ? Enemy->GetMesh()->GetAnimInstance() : nullptr)
		{
			if (!AnimInst->Montage_IsPlaying(nullptr))
			{
				UAnimMontage* HoverMontage = (EnemyChar && EnemyChar->GetEnemyConfig() && !EnemyChar->GetEnemyConfig()->AirHoverMontage.IsNull())
					? EnemyChar->GetEnemyConfig()->AirHoverMontage.LoadSynchronous() : nullptr;
				if (HoverMontage)
				{
					Enemy->PlayAnimMontage(HoverMontage, 1.2f);
				}
			}
		}
		// 2. Track target orientation
		const FVector CurrentLoc = Enemy->GetActorLocation();
		const FVector TargetLoc = Target->GetActorLocation();
		const FVector ToTarget = (TargetLoc - CurrentLoc).GetSafeNormal2D();
		const float HorizDist = FVector::Dist2D(CurrentLoc, TargetLoc);
		if (!ToTarget.IsNearlyZero())
		{
			const FRotator TargetRot = FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f);
			const float CfgAirTurn = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->FlightSettings.AirTurnRate : 540.0f;
			const float TurnRate = CfgAirTurn > 0.0f ? CfgAirTurn : 540.0f;
			Enemy->SetActorRotation(FMath::RInterpConstantTo(Enemy->GetActorRotation(), TargetRot, DeltaTime, TurnRate));
		}
		// 3. Keep altitude above ground with gentle wing-flapping oscillation
		FHitResult GroundHit;
		FCollisionQueryParams QueryParams(TEXT("LadybugAirGroundTrace"), false, Enemy);
		QueryParams.AddIgnoredActor(Enemy);
		const float CfgHoverAlt = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->FlightSettings.HoverAltitude : 260.0f;
		const float CfgWobbleFreq = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->FlightSettings.HoverWobbleFrequency : 3.0f;
		const float CfgWobbleAmp = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->FlightSettings.HoverWobbleAmplitude : 15.0f;
		const float CfgDiveRange = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->FlightSettings.DiveInitiationRange : 300.0f;

		float GroundZ = CurrentLoc.Z - CfgHoverAlt;
		if (Enemy->GetWorld()->LineTraceSingleByChannel(GroundHit, CurrentLoc, CurrentLoc - FVector(0.0f, 0.0f, 1000.0f), ECC_Visibility, QueryParams))
		{
			GroundZ = GroundHit.ImpactPoint.Z;
		}
		const float BobZ = FMath::Sin(InstanceData.HoverElapsedTime * CfgWobbleFreq) * CfgWobbleAmp;
		const float DesiredBaseZ = FMath::Max(GroundZ, TargetLoc.Z);
		const float DesiredZ = DesiredBaseZ + CfgHoverAlt + BobZ;
		const float VertVelocity = (DesiredZ - CurrentLoc.Z) * 4.0f;

		// 4. Move towards target in air
		const float FlightSpeed = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->ChaseWalkSpeed : 550.0f;
		if (Enemy->GetCharacterMovement())
		{
			Enemy->GetCharacterMovement()->Velocity = ToTarget * FlightSpeed + FVector(0.0f, 0.0f, VertVelocity);
		}
		// 5. Trigger dive attack when within range or timeout
		const float DiveTriggerRange = CfgDiveRange;
		if (HorizDist <= DiveTriggerRange || InstanceData.RemainingTime <= 0.0f)
		{
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Ladybug Air] '%s' REACHED DIVE RANGE (Dist: %.1f cm) -> triggering DiveAttack"),
				*Enemy->GetName(), HorizDist);
			return EStateTreeRunStatus::Succeeded;
		}
	}
	return EStateTreeRunStatus::Running;
}
void FStateTreeTask_LadybugAirHover::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (AThreadTalesBaseCharacter* Enemy = Cast<AThreadTalesBaseCharacter>(Context.GetOwner()))
	{
		const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(Enemy);
		UAnimMontage* HoverMontage = (EnemyChar && EnemyChar->GetEnemyConfig() && !EnemyChar->GetEnemyConfig()->AirHoverMontage.IsNull())
			? EnemyChar->GetEnemyConfig()->AirHoverMontage.Get() : nullptr;

		// Only stop hover montage if it is currently playing - do NOT clobber incoming combat reactions (posture break / stagger)
		const bool bPendingInterrupt = EnemyChar && (EnemyChar->IsPendingAirKnockdown() || EnemyChar->IsPendingAirParryStagger());
		if (HoverMontage && Enemy->GetCurrentMontage() == HoverMontage)
		{
			Enemy->StopAnimMontage(HoverMontage);
		}
		else if (!Enemy->IsStaggered() && !bPendingInterrupt)
		{
			Enemy->StopAnimMontage();
		}

		if (Enemy->GetCharacterMovement())
		{
			Enemy->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		}
	}
}
// -----------------------------------------------------------------------------
// FStateTreeTask_LadybugDiveAttack
// -----------------------------------------------------------------------------
FStateTreeTask_LadybugDiveAttack::FStateTreeTask_LadybugDiveAttack()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}
EStateTreeRunStatus FStateTreeTask_LadybugDiveAttack::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = Cast<AThreadTalesBaseCharacter>(Context.GetOwner());
	if (!Enemy || !Enemy->GetCharacterMovement())
	{
		return EStateTreeRunStatus::Failed;
	}
	AThreadTalesAIController* AICon = Cast<AThreadTalesAIController>(Enemy->GetController());
	AThreadTalesCharacter* Target = AICon ? AICon->GetCombatTarget() : nullptr;
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bHasLanded = false;
	const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(Enemy);
	const float CfgDiveSpeed = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->FlightSettings.DiveSpeed : 0.0f;
	const float EffectiveSpeed = CfgDiveSpeed > 0.0f ? CfgDiveSpeed : (DiveSpeed > 0.0f ? DiveSpeed : InstanceData.DiveSpeed);
	FVector TargetPos = Target ? Target->GetActorLocation() : (Enemy->GetActorLocation() + Enemy->GetActorForwardVector() * 500.0f);
	FVector DiveDir = (TargetPos - Enemy->GetActorLocation()).GetSafeNormal();
	if (DiveDir.IsNearlyZero())
	{
		DiveDir = (Enemy->GetActorForwardVector() - FVector(0.0f, 0.0f, 0.8f)).GetSafeNormal();
	}
	InstanceData.DiveVelocity = DiveDir * EffectiveSpeed;
	Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	Enemy->GetCharacterMovement()->Velocity = InstanceData.DiveVelocity;
	UAnimMontage* DiveMontage = nullptr;
	if (EnemyChar && EnemyChar->GetEnemyConfig() && !EnemyChar->GetEnemyConfig()->AirDiveAttackMontage.IsNull())
	{
		DiveMontage = EnemyChar->GetEnemyConfig()->AirDiveAttackMontage.LoadSynchronous();
	}
	if (!DiveMontage)
	{
		static const FString FallbackDive = TEXT("/Game/Animations/Enemies/Ladybug/Air/AM_Ladybug_Air_ToGroundAttack.AM_Ladybug_Air_ToGroundAttack");
		DiveMontage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *FallbackDive));
	}
	float Duration = 1.1f;
	if (DiveMontage)
	{
		const float PlayedLen = Enemy->PlayAnimMontage(DiveMontage, 1.25f);
		if (PlayedLen > 0.0f)
		{
			Duration = PlayedLen;
		}
	}
	InstanceData.RemainingTime = Duration;
	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Ladybug Air] '%s' DIVE ATTACK -> speed %.1f cm/s, Duration: %.2fs"),
		*Enemy->GetName(), EffectiveSpeed, Duration);
	return EStateTreeRunStatus::Running;
}
EStateTreeRunStatus FStateTreeTask_LadybugDiveAttack::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime -= DeltaTime;
	AThreadTalesBaseCharacter* Enemy = Cast<AThreadTalesBaseCharacter>(Context.GetOwner());
	if (!Enemy || !Enemy->GetCharacterMovement())
	{
		return EStateTreeRunStatus::Failed;
	}
	if (!InstanceData.bHasLanded)
	{
		Enemy->GetCharacterMovement()->Velocity = InstanceData.DiveVelocity;

		// Check ground contact
		FHitResult GroundHit;
		FCollisionQueryParams QueryParams(TEXT("LadybugDiveGroundCheck"), false, Enemy);
		QueryParams.AddIgnoredActor(Enemy);
		const FVector Loc = Enemy->GetActorLocation();
		const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(Enemy);
		const float TraceGroundDist = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->FlightSettings.DiveGroundCheckDistance : 75.0f;
		if (Enemy->GetWorld()->LineTraceSingleByChannel(GroundHit, Loc, Loc - FVector(0.0f, 0.0f, TraceGroundDist), ECC_Visibility, QueryParams))
		{
			InstanceData.bHasLanded = true;
			Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			Enemy->GetCharacterMovement()->Velocity = FVector::ZeroVector;
			InstanceData.RemainingTime = FMath::Min(InstanceData.RemainingTime, 0.20f);
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Ladybug Air] '%s' DIVE IMPACT -> Ground contacted, switched to MOVE_Walking"),
				*Enemy->GetName());
		}
	}
	if (InstanceData.RemainingTime <= 0.0f)
	{
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Enemy->GetCharacterMovement()->Velocity = FVector::ZeroVector;

		AThreadTalesAIController* AICon = Cast<AThreadTalesAIController>(Enemy->GetController());
		AThreadTalesCharacter* Target = AICon ? AICon->GetCombatTarget() : nullptr;
		if (!Target || !Target->IsAlive())
		{
			if (AICon)
			{
				Target = AICon->EvaluateBestTarget();
			}
		}

		const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(Enemy);
		const float MeleeAttackRange = (EnemyChar && EnemyChar->GetEnemyConfig())
			? EnemyChar->GetEnemyConfig()->Ranges.MeleeAttackRange
			: 160.0f;

		const float DistToTarget = (Target && Target->IsAlive())
			? FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation())
			: 9999.0f;

		if (Target && DistToTarget <= MeleeAttackRange)
		{
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Ladybug Air] '%s' Target in melee range (%.1f <= %.1f) -> follow up with MeleeCombo"),
				*Enemy->GetName(), DistToTarget, MeleeAttackRange);
			return EStateTreeRunStatus::Succeeded;
		}
		else
		{
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Ladybug Air] '%s' Target outside melee range (%.1f > %.1f) -> abort combo, return to Takeoff"),
				*Enemy->GetName(), DistToTarget, MeleeAttackRange);
			return EStateTreeRunStatus::Failed;
		}
	}
	return EStateTreeRunStatus::Running;
}
void FStateTreeTask_LadybugDiveAttack::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (AThreadTalesBaseCharacter* Enemy = Cast<AThreadTalesBaseCharacter>(Context.GetOwner()))
	{
		if (Enemy->GetCharacterMovement())
		{
			Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			Enemy->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		}
	}
}
