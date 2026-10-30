// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ThreadTalesEnemyAttackTask.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AI/ThreadTalesAIController.h"
#include "AI/ThreadTalesEnemyConfig.h"
#include "Characters/ThreadTalesBaseCharacter.h"
#include "Characters/ThreadTalesEnemyCharacter.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
#include "Combat/ThreadTalesWeapon.h"
#include "StateTreeExecutionContext.h"
#include "Animation/AnimMontage.h"
#include "ThreadTales.h"

FStateTreeTask_EnemyMeleeCombo::FStateTreeTask_EnemyMeleeCombo()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreeTask_EnemyMeleeCombo::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = Cast<AThreadTalesBaseCharacter>(Context.GetOwner());
	if (!Enemy)
	{
		return EStateTreeRunStatus::Failed;
	}

	AThreadTalesAIController* AICon = Cast<AThreadTalesAIController>(Enemy->GetController());
	AThreadTalesCharacter* Target = nullptr;
	if (AICon)
	{
		AICon->StopMovement();
		Target = AICon->GetCombatTarget();
		if (!Target || !Target->IsAlive())
		{
			Target = AICon->EvaluateBestTarget();
		}
		if (Target)
		{
			AICon->SetFocus(Target, EAIFocusPriority::Gameplay);
			const FVector ToTarget = (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
			if (!ToTarget.IsNearlyZero())
			{
				Enemy->SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
			}
		}
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.CurrentStrikeIndex = 0;
	float MontageDuration = 1.2f;

	// Ground combat invariant: strictly enforce MOVE_Walking and kill residual vertical velocity
	if (UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement())
	{
		if (MoveComp->MovementMode != MOVE_Walking)
		{
			MoveComp->SetMovementMode(MOVE_Walking);
		}
		MoveComp->Velocity = FVector(MoveComp->Velocity.X, MoveComp->Velocity.Y, 0.0f);
	}

	const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(Enemy);
	UAnimMontage* AttackMontage = nullptr;

	if (EnemyChar && EnemyChar->GetEnemyConfig() && EnemyChar->GetEnemyConfig()->AttackComboMontages.Num() > 0)
	{
		AttackMontage = EnemyChar->GetEnemyConfig()->AttackComboMontages[0].LoadSynchronous();
	}
	if (!AttackMontage)
	{
		if (const UThreadTalesEquipmentComponent* EquipComp = Enemy->GetEquipmentComponent())
		{
			if (AThreadTalesWeapon* Weapon = EquipComp->GetCurrentWeapon())
			{
				AttackMontage = Weapon->GetComboStrikeMontages().Num() > 0 ? Weapon->GetComboStrikeMontages()[0] : Weapon->GetComboAttackMontage();
			}
		}
	}
	if (!AttackMontage)
	{
		static const FString Combo1Path = TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/Attack/AM_Attack_Combo1.AM_Attack_Combo1");
		AttackMontage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *Combo1Path));
	}

	if (AttackMontage)
	{
		// Dynamic leap distance scaling: scale root motion translation so strike lands in sweet spot without overshooting
		if (Target && Target->IsAlive())
		{
			const float DistToTarget = FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation());
			const float DesiredMeleeDist = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->LeapSettings.DesiredMeleeDistance : 65.0f;
			const float ExpectedLeapDist = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->LeapSettings.NominalLeapDistance : 145.0f;
			const float MinScale = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->LeapSettings.MinLeapTranslationScale : 0.85f;
			const float MaxScale = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->LeapSettings.MaxLeapTranslationScale : 1.0f;
			const float ForwardNeeded = FMath::Max(0.0f, DistToTarget - DesiredMeleeDist);
			const float LeapScale = FMath::Clamp(ForwardNeeded / FMath::Max(1.0f, ExpectedLeapDist), MinScale, MaxScale);
			Enemy->SetAnimRootMotionTranslationScale(LeapScale);
		}
		else
		{
			Enemy->SetAnimRootMotionTranslationScale(1.0f);
		}

		MontageDuration = Enemy->PlayAnimMontage(AttackMontage, 1.15f);
		if (MontageDuration <= 0.0f) MontageDuration = 1.2f;
	}

	InstanceData.CurrentMontageDuration = MontageDuration;
	InstanceData.RemainingTime = FMath::Min(MontageDuration, InstanceData.MaxComboTimeout);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EnemyMeleeCombo::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime -= DeltaTime;

	AThreadTalesBaseCharacter* Enemy = Cast<AThreadTalesBaseCharacter>(Context.GetOwner());
	AThreadTalesAIController* AICon = Enemy ? Cast<AThreadTalesAIController>(Enemy->GetController()) : nullptr;
	AThreadTalesCharacter* Target = AICon ? AICon->GetCombatTarget() : nullptr;
	if ((!Target || !Target->IsAlive()) && AICon)
	{
		Target = AICon->EvaluateBestTarget();
	}

	const AThreadTalesEnemyCharacter* EnemyChar = Cast<AThreadTalesEnemyCharacter>(Enemy);
	const FThreadTalesEnemyDifficultySettings& Difficulty = EnemyChar ? EnemyChar->GetDifficultySettings() : FThreadTalesEnemyDifficultySettings();

	// Active Attack Steering during the initial leap windup phase of each strike
	const float ElapsedInStrike = InstanceData.CurrentMontageDuration - InstanceData.RemainingTime;
	const float ConfigSteeringWindow = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->LeapSettings.SteeringWindow : 0.40f;
	const float SteeringWindow = (InstanceData.CurrentStrikeIndex == 0) ? ConfigSteeringWindow : 0.0f;
	if (ElapsedInStrike >= 0.0f && ElapsedInStrike <= SteeringWindow && Target && Target->IsAlive() && Enemy)
	{
		const FVector ToTarget = (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
		if (!ToTarget.IsNearlyZero())
		{
			const FRotator CurrentRot = Enemy->GetActorRotation();
			const FRotator TargetRot = FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f);
			const float YawDelta = FMath::FindDeltaAngleDegrees(CurrentRot.Yaw, TargetRot.Yaw);
			const float MaxYawAngle = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->LeapSettings.SteeringMaxYawAngle : 120.0f;
			if (FMath::Abs(YawDelta) <= MaxYawAngle)
			{
				const float ConfigTurnRate = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->LeapSettings.SteeringTurnRate : 540.0f;
				const float BaseTurnRate = (Difficulty.TrackingTurnRate > 0.0f ? Difficulty.TrackingTurnRate : 360.0f);
				const float TurnRate = FMath::Max(BaseTurnRate, ConfigTurnRate);
				Enemy->SetActorRotation(FMath::RInterpConstantTo(CurrentRot, TargetRot, DeltaTime, TurnRate));
			}
		}
	}

	// Seamless crossfade transition early lead-time from LeapSettings
	const float ConfigLeadTime = (EnemyChar && EnemyChar->GetEnemyConfig()) ? EnemyChar->GetEnemyConfig()->LeapSettings.CrossfadeLeadTime : 0.20f;
	const float CrossfadeLeadTime = FMath::Min(ConfigLeadTime, InstanceData.CurrentMontageDuration * 0.25f);
	if (InstanceData.RemainingTime <= CrossfadeLeadTime)
	{
		const float Dist = (Target && Enemy) ? FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation()) : 9999.0f;
		const bool bIsHeavy = (EnemyChar && EnemyChar->GetEnemyConfig() && !EnemyChar->GetEnemyConfig()->HeavyAttackMontage.IsNull()
			&& Enemy && Enemy->GetCurrentMontage() == EnemyChar->GetEnemyConfig()->HeavyAttackMontage.Get());

		int32 MaxComboCount = 3;
		if (EnemyChar && EnemyChar->GetEnemyConfig() && EnemyChar->GetEnemyConfig()->AttackComboMontages.Num() > 0)
		{
			MaxComboCount = EnemyChar->GetEnemyConfig()->AttackComboMontages.Num();
		}
		else if (Enemy)
		{
			if (const UThreadTalesEquipmentComponent* EquipComp = Enemy->GetEquipmentComponent())
			{
				if (AThreadTalesWeapon* Weapon = EquipComp->GetCurrentWeapon())
				{
					if (Weapon->GetComboStrikeMontages().Num() > 0)
					{
						MaxComboCount = Weapon->GetComboStrikeMontages().Num();
					}
				}
			}
		}

		const float MeleeAttackRange = (EnemyChar && EnemyChar->GetEnemyConfig())
			? EnemyChar->GetEnemyConfig()->Ranges.MeleeAttackRange
			: 220.0f;
		const float MaxChainRange = MeleeAttackRange * 1.5f;

		const bool bCanChain = !bIsHeavy && (InstanceData.CurrentStrikeIndex < (MaxComboCount - 1)) && Target && Target->IsAlive() && (Dist <= MaxChainRange);
		const float ChainChance = (EnemyChar && EnemyChar->GetEnemyConfig() && EnemyChar->GetEnemyConfig()->AttackComboMontages.Num() <= 2) ? 1.0f : Difficulty.ComboChainChance;

		if (bCanChain && (FMath::FRand() <= ChainChance))
		{
			InstanceData.CurrentStrikeIndex++;
			UAnimMontage* NextMontage = nullptr;
			if (EnemyChar && EnemyChar->GetEnemyConfig() && EnemyChar->GetEnemyConfig()->AttackComboMontages.Num() > InstanceData.CurrentStrikeIndex)
			{
				NextMontage = EnemyChar->GetEnemyConfig()->AttackComboMontages[InstanceData.CurrentStrikeIndex].LoadSynchronous();
			}
			if (!NextMontage && (!EnemyChar || !EnemyChar->GetEnemyConfig()))
			{
				const FString FallbackPath = (InstanceData.CurrentStrikeIndex == 1)
					? TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/Attack/AM_Attack_Combo2.AM_Attack_Combo2")
					: TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/Attack/AM_Attack_Combo3.AM_Attack_Combo3");
				NextMontage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *FallbackPath));
			}

			if (NextMontage && Enemy)
			{
				// Orient directly towards combat target before launching chained strike
				if (Target && Target->IsAlive())
				{
					const FVector ToTarget = (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
					if (!ToTarget.IsNearlyZero())
					{
						Enemy->SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
						if (AICon)
						{
							AICon->SetFocus(Target, EAIFocusPriority::Gameplay);
						}
					}
				}

				// Dynamic leap distance scaling for chained strikes:
				// If already in melee contact after Strike 1, scale root motion translation
				// so Strike 2 lands cleanly on the standing target instead of leaping 100cm past it!
				if (Target && Target->IsAlive())
				{
					const float DistToTarget = FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation());
					const float DesiredMeleeDist = 65.0f;
					const float ForwardNeeded = FMath::Max(0.0f, DistToTarget - DesiredMeleeDist);
					const float ExpectedLeapDist = 115.0f; // Strike 2 nominal leap
					const float LeapScale = FMath::Clamp(ForwardNeeded / ExpectedLeapDist, 0.85f, 1.0f);
					Enemy->SetAnimRootMotionTranslationScale(LeapScale);
				}
				else
				{
					Enemy->SetAnimRootMotionTranslationScale(1.0f);
				}

				const float Duration = Enemy->PlayAnimMontage(NextMontage, 1.0f);
				InstanceData.CurrentMontageDuration = Duration > 0.0f ? Duration : 1.1f;
				InstanceData.RemainingTime = InstanceData.CurrentMontageDuration;
				UE_LOG(LogThreadTalesCombat, Display, TEXT("[Enemy StateTree] '%s' CHAINING COMBO STRIKE %d / %d with seamless crossfade!"),
					*Enemy->GetName(), InstanceData.CurrentStrikeIndex + 1, MaxComboCount);
				return EStateTreeRunStatus::Running;
			}
		}

		// On the final strike of the combo, do not exit early with CrossfadeLeadTime.
		// Wait for the full strike animation and recovery to finish so the enemy stays facing the target!
		if (InstanceData.RemainingTime <= 0.0f)
		{
			return EStateTreeRunStatus::Succeeded;
		}

		return EStateTreeRunStatus::Running;
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EnemyMeleeCombo::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AThreadTalesBaseCharacter* Enemy = Cast<AThreadTalesBaseCharacter>(Context.GetOwner());
	if (Enemy)
	{
		Enemy->SetAnimRootMotionTranslationScale(1.0f);

		// Ensure character remains firmly in MOVE_Walking upon exiting melee combo
		if (UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement())
		{
			if (MoveComp->MovementMode != MOVE_Walking)
			{
				MoveComp->SetMovementMode(MOVE_Walking);
			}
		}

		if (AThreadTalesAIController* AICon = Cast<AThreadTalesAIController>(Enemy->GetController()))
		{
			AICon->ClearFocus(EAIFocusPriority::Gameplay);
		}
	}
}
