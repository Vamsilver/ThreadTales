#include "Abilities/ThreadTalesGA_Evade.h"
#include "ThreadTales.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Abilities/ThreadTalesGameplayEffects.h"
#include "Abilities/ThreadTalesAbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesStaminaComponent.h"
#include "Characters/ThreadTalesTraversalInputHelper.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetMathLibrary.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
#include "Combat/ThreadTalesTargetLockComponent.h"
#include "Abilities/ThreadTalesAttributeSet.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
UThreadTalesGA_Evade::UThreadTalesGA_Evade()
{
	AbilityName = TEXT("Evade / Dodge");
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	SetAssetTags(FGameplayTagContainer(ThreadTalesGameplayTags::TAG_Ability_Evade));
	ActivationOwnedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Evading);
	// NOTE: TAG_State_Combat_Invulnerable is dynamically applied during the active I-Frame window (IFrameDuration),
	// allowing damage during the recovery frames while maintaining TAG_State_Combat_Evading throughout the full roll.

	// Block evade while Staggered or Posture Broken
	ActivationBlockedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered);
	ActivationBlockedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_PostureBroken);
	ActivationBlockedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Movement_Airborne);

	// Set default Stamina cost and post-evade regen delay
	CostGameplayEffectClass = UThreadTalesGE_CostEvade::StaticClass();
	StaminaDelayEffectClass = UThreadTalesGE_StaminaDelay::StaticClass();

	// Evade cancels melee attacks and aiming
	CancelAbilitiesWithTag.AddTag(ThreadTalesGameplayTags::TAG_Ability_Melee);
	CancelAbilitiesWithTag.AddTag(ThreadTalesGameplayTags::TAG_Ability_Ranged);

	// Evade blocks attack actions while rolling/evading
	BlockAbilitiesWithTag = CancelAbilitiesWithTag;

	// Populate default 8-directional Samurai evade montages
	static ConstructorHelpers::FObjectFinder<UAnimMontage> ForwardMontageObj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/Evade/AM_Evade_Forward.AM_Evade_Forward"));
	if (ForwardMontageObj.Succeeded())
	{
		EvadeForwardMontage = ForwardMontageObj.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ForwardRightMontageObj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/Evade/AM_Evade_Forward_Right.AM_Evade_Forward_Right"));
	if (ForwardRightMontageObj.Succeeded())
	{
		EvadeForwardRightMontage = ForwardRightMontageObj.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> RightMontageObj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/Evade/AM_Evade_Right.AM_Evade_Right"));
	if (RightMontageObj.Succeeded())
	{
		EvadeRightMontage = RightMontageObj.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> BackwardRightMontageObj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/Evade/AM_Evade_Backward_Right.AM_Evade_Backward_Right"));
	if (BackwardRightMontageObj.Succeeded())
	{
		EvadeBackwardRightMontage = BackwardRightMontageObj.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> BackwardMontageObj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/Evade/AM_Evade_Backward.AM_Evade_Backward"));
	if (BackwardMontageObj.Succeeded())
	{
		EvadeBackwardMontage = BackwardMontageObj.Object;
		DefaultDodgeMontage = BackwardMontageObj.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> BackwardLeftMontageObj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/Evade/AM_Evade_Backward_Left.AM_Evade_Backward_Left"));
	if (BackwardLeftMontageObj.Succeeded())
	{
		EvadeBackwardLeftMontage = BackwardLeftMontageObj.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> LeftMontageObj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/Evade/AM_Evade_Left.AM_Evade_Left"));
	if (LeftMontageObj.Succeeded())
	{
		EvadeLeftMontage = LeftMontageObj.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ForwardLeftMontageObj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/Evade/AM_Evade_Forward_Left.AM_Evade_Forward_Left"));
	if (ForwardLeftMontageObj.Succeeded())
	{
		EvadeForwardLeftMontage = ForwardLeftMontageObj.Object;
	}
}

float UThreadTalesGA_Evade::GetCancelTimeForMontage(const UAnimMontage* Montage, EEvadeDirection Direction) const
{
	if (!Montage) return EarlyCancelWindowStartTime;
	if (const float* FoundPct = MontageCancelPercentages.Find(Montage)) return Montage->GetPlayLength() * FMath::Clamp(*FoundPct, 0.0f, 1.0f);
	float Pct = BackwardCancelPercentage;
	switch (Direction)
	{
	case EEvadeDirection::Forward: Pct = ForwardCancelPercentage; break;
	case EEvadeDirection::ForwardRight:
	case EEvadeDirection::ForwardLeft: Pct = DiagonalForwardCancelPercentage; break;
	case EEvadeDirection::Right:
	case EEvadeDirection::Left: Pct = LateralCancelPercentage; break;
	case EEvadeDirection::BackwardRight:
	case EEvadeDirection::BackwardLeft: Pct = (LateralCancelPercentage + BackwardCancelPercentage) * 0.5f; break;
	case EEvadeDirection::Backward:
	default: Pct = BackwardCancelPercentage; break;
	}
	return Montage->GetPlayLength() * Pct;
}

FVector UThreadTalesGA_Evade::ComputeMovementIntent(const ACharacter* Character) const
{
	if (!Character) return FVector::ZeroVector;

	FVector MoveDir = FVector::ZeroVector;
	if (const APlayerController* PC = Cast<APlayerController>(Character->GetController()))
	{
		const FVector2D Input2D = FThreadTalesTraversalInputHelper::GetMovementInputVector(PC, Character);
		if (Input2D.SizeSquared() > 0.04f)
		{
			const FRotator CamRot = PC->GetControlRotation();
			const FRotator YawRot(0.0f, CamRot.Yaw, 0.0f);
			const FVector CamFwd = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
			const FVector CamRight = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
			MoveDir = (CamFwd * Input2D.Y + CamRight * Input2D.X).GetSafeNormal();
		}
	}

	if (MoveDir.IsNearlyZero())
	{
		if (!Character->GetPendingMovementInputVector().IsNearlyZero())
		{
			MoveDir = Character->GetPendingMovementInputVector().GetSafeNormal();
		}
		else if (!Character->GetLastMovementInputVector().IsNearlyZero())
		{
			MoveDir = Character->GetLastMovementInputVector().GetSafeNormal();
		}
		else if (Character->GetCharacterMovement() && !Character->GetCharacterMovement()->GetCurrentAcceleration().IsNearlyZero())
		{
			MoveDir = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal();
		}
	}
	return MoveDir;
}

EEvadeDirection UThreadTalesGA_Evade::CalculateEvadeDirection(float& OutAngle) const
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		OutAngle = 180.0f;
		return EEvadeDirection::Backward;
	}

	const FVector InputVector = ComputeMovementIntent(Character);
	if (InputVector.IsNearlyZero())
	{
		OutAngle = 180.0f;
		return EEvadeDirection::Backward;
	}

	AController* Controller = Character->GetController();
	const FRotator CameraRotation = Controller ? Controller->GetControlRotation() : Character->GetActorRotation();
	const FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(InputVector.Rotation(), CameraRotation);
	const float Angle = DeltaRot.Yaw;
	OutAngle = Angle;

	if (Angle >= -22.5f && Angle < 22.5f) return EEvadeDirection::Forward;
	if (Angle >= 22.5f && Angle < 67.5f) return EEvadeDirection::ForwardRight;
	if (Angle >= 67.5f && Angle < 112.5f) return EEvadeDirection::Right;
	if (Angle >= 112.5f && Angle < 157.5f) return EEvadeDirection::BackwardRight;
	if (Angle >= -67.5f && Angle < -22.5f) return EEvadeDirection::ForwardLeft;
	if (Angle >= -112.5f && Angle < -67.5f) return EEvadeDirection::Left;
	if (Angle >= -157.5f && Angle < -112.5f) return EEvadeDirection::BackwardLeft;
	return EEvadeDirection::Backward;
}

UAnimMontage* UThreadTalesGA_Evade::GetMontageForDirection(EEvadeDirection Direction) const
{
	switch (Direction)
	{
	case EEvadeDirection::Forward:
		return EvadeForwardMontage ? EvadeForwardMontage : DefaultDodgeMontage;
	case EEvadeDirection::ForwardRight:
		return EvadeForwardRightMontage ? EvadeForwardRightMontage : (EvadeRightMontage ? EvadeRightMontage : DefaultDodgeMontage);
	case EEvadeDirection::Right:
		return EvadeRightMontage ? EvadeRightMontage : DefaultDodgeMontage;
	case EEvadeDirection::BackwardRight:
		return EvadeBackwardRightMontage ? EvadeBackwardRightMontage : (EvadeBackwardMontage ? EvadeBackwardMontage : DefaultDodgeMontage);
	case EEvadeDirection::Backward:
		return EvadeBackwardMontage ? EvadeBackwardMontage : DefaultDodgeMontage;
	case EEvadeDirection::BackwardLeft:
		return EvadeBackwardLeftMontage ? EvadeBackwardLeftMontage : (EvadeBackwardMontage ? EvadeBackwardMontage : DefaultDodgeMontage);
	case EEvadeDirection::Left:
		return EvadeLeftMontage ? EvadeLeftMontage : DefaultDodgeMontage;
	case EEvadeDirection::ForwardLeft:
		return EvadeForwardLeftMontage ? EvadeForwardLeftMontage : (EvadeLeftMontage ? EvadeLeftMontage : DefaultDodgeMontage);
	default:
		return DefaultDodgeMontage;
	}
}

bool UThreadTalesGA_Evade::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		if (UThreadTalesStaminaComponent* StamComp = TTChar->GetStaminaComponent())
		{
			if (!StamComp->CanSpendStamina())
			{
				if (TTChar->GetCharacterMovement() && !TTChar->GetCharacterMovement()->IsFalling())
				{
					StamComp->NotifyInsufficientStamina();
				}
				return false;
			}
			return true;
		}
		if (!TTChar->CanSpendStamina())
		{
			return false;
		}
		return true;
	}
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}

void UThreadTalesGA_Evade::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		if (UThreadTalesStaminaComponent* StamComp = TTChar->GetStaminaComponent())
		{
			StamComp->TryConsumeStamina(StamComp->GetGroundEvadeStaminaCost(), true);
		}
	}
}

bool UThreadTalesGA_Evade::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// Double-layer check: avatar character must be in combat stance (weapon drawn) or have InCombat tag
	const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	const bool bInCombatStance = TTChar && TTChar->IsInCombatStance();
	const bool bInCombatTag = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid() &&
	                          ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_InCombat);
	if (!bInCombatStance && !bInCombatTag)
	{
		UE_LOG(LogThreadTalesCombat, Verbose, TEXT("[Evade GA] CanActivateAbility rejected: %s is not in combat stance"), *GetNameSafe(TTChar));
		return false;
	}

	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (Character && Character->GetCharacterMovement())
	{
		// Ground evade should NOT activate if falling
		if (!Character->GetCharacterMovement()->IsMovingOnGround() || Character->GetCharacterMovement()->IsFalling())
		{
			return false;
		}
	}
	return true;
}

void UThreadTalesGA_Evade::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bQueueDodgeAttack = false;
	LastAttackInputTime = -100.0f;

	// Register Melee and Ranged_Fire input tags dynamically while evading so attack clicks are dispatched to InputPressed()
	if (FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec())
	{
		Spec->GetDynamicSpecSourceTags().AddTag(ThreadTalesGameplayTags::TAG_Input_Ability_Melee);
		Spec->GetDynamicSpecSourceTags().AddTag(ThreadTalesGameplayTags::TAG_Input_Ability_Ranged_Fire);
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(Character);
	const bool bTargetLocked = TTChar && TTChar->GetTargetLockComponent() && TTChar->GetTargetLockComponent()->IsTargetLocked();

	const FVector MoveDir = ComputeMovementIntent(Character);
	const bool bHasMoveInput = !MoveDir.IsNearlyZero();

	float Angle = 0.0f;
	EEvadeDirection Direction = EEvadeDirection::Forward;
	UAnimMontage* MontageToPlay = nullptr;

	AController* Controller = Character->GetController();

	if (bTargetLocked)
	{
		// With Target Lock: 8-way strafe dodge facing locked target / camera forward
		Direction = CalculateEvadeDirection(Angle);
		MontageToPlay = GetMontageForDirection(Direction);
		if (Controller)
		{
			// Orient character to face camera forward so 8-way strafe root motion moves in the exact relative camera direction
			Character->SetActorRotation(FRotator(0.0f, Controller->GetControlRotation().Yaw, 0.0f));
		}
	}
	else
	{
		// Without Target Lock (Free Locomotion):
		// Do NOT snap to camera! Align by character movement direction.
		// For the character, rolling in the direction of movement is a forward roll in that direction.
		if (bHasMoveInput)
		{
			Direction = EEvadeDirection::Forward;
			MontageToPlay = EvadeForwardMontage ? EvadeForwardMontage : DefaultDodgeMontage;
			Character->SetActorRotation(FRotator(0.0f, MoveDir.Rotation().Yaw, 0.0f));
		}
		else
		{
			// Neutral dodge (no input): Backstep in place without changing rotation
			Direction = EEvadeDirection::Backward;
			MontageToPlay = EvadeBackwardMontage ? EvadeBackwardMontage : DefaultDodgeMontage;
		}
	}

	// Clear leftover walking/running velocity so Root Motion takes 100% clean authority
	if (UCharacterMovementComponent* CMC = Character->GetCharacterMovement())
	{
		CMC->StopMovementImmediately();
	}

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Evade GA] Evading in direction %d (Angle: %.1f deg) with Montage: %s"),
		static_cast<int32>(Direction), Angle, *GetNameSafe(MontageToPlay));

	// Activate dynamic I-Frames for the configured IFrameDuration window
	bInvulnerableActive = true;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Invulnerable);
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Evade GA] Invulnerability (I-Frames) active for %.2fs."), IFrameDuration);
	}

	if (IFrameDuration > 0.0f)
	{
		class UAbilityTask_WaitDelay* IFrameTask = UAbilityTask_WaitDelay::WaitDelay(this, IFrameDuration);
		if (IFrameTask)
		{
			IFrameTask->OnFinish.AddDynamic(this, &UThreadTalesGA_Evade::OnIFrameTimerFinished);
			IFrameTask->ReadyForActivation();
		}
	}

	if (MontageToPlay)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay, 1.0f, NAME_None, true, 1.0f);
		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UThreadTalesGA_Evade::OnMontageCompleted);
			MontageTask->OnInterrupted.AddDynamic(this, &UThreadTalesGA_Evade::OnMontageInterrupted);
			MontageTask->OnCancelled.AddDynamic(this, &UThreadTalesGA_Evade::OnMontageCancelled);
			MontageTask->ReadyForActivation();
		}
	}
	else
	{
		UE_LOG(LogThreadTalesCombat, Warning, TEXT("[Evade GA] No montage assigned for Evade direction %d!"), static_cast<int32>(Direction));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// Schedule the start of the direction-calibrated early cancel recovery window
	const float CancelTime = GetCancelTimeForMontage(MontageToPlay, Direction);
	ActiveCancelTime = CancelTime;
	const float SafeCancelTime = FMath::Max(0.01f, CancelTime);
	class UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, SafeCancelTime);
	if (DelayTask)
	{
		DelayTask->OnFinish.AddDynamic(this, &UThreadTalesGA_Evade::OnCheckInputTimerFinished);
		DelayTask->ReadyForActivation();
	}
}

void UThreadTalesGA_Evade::OnIFrameTimerFinished()
{
	if (bInvulnerableActive)
	{
		bInvulnerableActive = false;
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Invulnerable);
		}
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Evade GA] I-Frames ended at %.2fs. Character is now in recovery window."), IFrameDuration);
	}
}

void UThreadTalesGA_Evade::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);

	// ONLY process as attack input if the input that was pressed is Melee attack or Ranged Fire (LMB)
	// and NOT if the character is currently blocking or holding block!
	const UThreadTalesAbilitySystemComponent* TTASC = Cast<UThreadTalesAbilitySystemComponent>(ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr);
	if (!TTASC || (!TTASC->GetLastPressedInputTag().MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Ability_Melee) &&
	               !TTASC->GetLastPressedInputTag().MatchesTagExact(ThreadTalesGameplayTags::TAG_Input_Ability_Ranged_Fire)))
	{
		return;
	}

	const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (TTChar && TTChar->IsBlocking())
	{
		return;
	}

	NotifyAttackInput();
}

void UThreadTalesGA_Evade::NotifyAttackInput()
{
	if (!bEnableDodgeAttack)
	{
		return;
	}

	// Double-check blocking state: rolling out of block must NEVER trigger a melee attack
	const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo());
	if (TTChar && TTChar->IsBlocking())
	{
		return;
	}
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Blocking))
		{
			return;
		}
	}

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	LastAttackInputTime = CurrentTime;

	// Check if we are past the active I-Frame duration (i.e. in the recovery cancel window)
	// If so, cancel recovery immediately and launch the dodge attack!
	if (!bInvulnerableActive)
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Evade GA] Attack Input received during recovery window -> Executing Dodge Attack immediately!"));
		TryExecuteDodgeAttack();
	}
	else
	{
		bQueueDodgeAttack = true;
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Evade GA] Attack Input received during roll (I-Frames active) -> Buffered for Dodge Attack!"));
	}
}

void UThreadTalesGA_Evade::OnCheckInputTimerFinished()
{
	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Evade GA] Recovery early cancel window opened at %.2fs. Checking for buffered attack or movement input."), ActiveCancelTime);

	// 1. Check if attack was buffered while rolling
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (bEnableDodgeAttack && (bQueueDodgeAttack || (CurrentTime - LastAttackInputTime <= AttackBufferTolerance)))
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Evade GA] Buffered Attack detected at recovery window -> Executing Dodge Attack!"));
		if (TryExecuteDodgeAttack())
		{
			return;
		}
	}

	// 2. If player is already holding movement keys, exit immediately into locomotion
	if (TryEarlyCancelToLocomotion())
	{
		return;
	}

	// 3. Otherwise, start continuous polling until montage completes or player presses input
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RecoveryTimerHandle, this, &UThreadTalesGA_Evade::PollRecoveryInput, RecoveryCheckInterval, true);
	}
}

void UThreadTalesGA_Evade::PollRecoveryInput()
{
	if (!IsActive())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
		}
		return;
	}

	// 1. Check if attack was queued
	if (bEnableDodgeAttack && bQueueDodgeAttack)
	{
		if (TryExecuteDodgeAttack())
		{
			return;
		}
	}

	// 2. Check movement input
	TryEarlyCancelToLocomotion();
}

bool UThreadTalesGA_Evade::CanExecuteDodgeAttack(const ACharacter* Character, const UAbilitySystemComponent* ASC) const
{
	if (!bEnableDodgeAttack || !Character || !ASC)
	{
		return false;
	}

	const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(Character);
	if (TTChar)
	{
		if (!TTChar->IsInCombatStance())
		{
			UE_LOG(LogThreadTalesCombat, Warning, TEXT("[Evade GA] TryExecuteDodgeAttack rejected: Character not in combat stance!"));
			return false;
		}

		if (TTChar->IsBlocking() || ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Blocking))
		{
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Evade GA] TryExecuteDodgeAttack rejected: Character is blocking!"));
			return false;
		}

		if (UThreadTalesEquipmentComponent* EquipComp = TTChar->GetEquipmentComponent())
		{
			if (AThreadTalesWeapon* Weapon = EquipComp->GetCurrentWeapon())
			{
				if (!Weapon->IsDodgeAttackEnabled())
				{
					UE_LOG(LogThreadTalesCombat, Display, TEXT("[Evade GA] TryExecuteDodgeAttack skipped: Disabled on weapon '%s'."), *GetNameSafe(Weapon));
					return false;
				}
			}
		}
	}

	const float CurrentStamina = ASC->GetNumericAttribute(UThreadTalesAttributeSet::GetStaminaAttribute());
	if (CurrentStamina <= 0.0f)
	{
		if (AThreadTalesCharacter* MutableTTChar = Cast<AThreadTalesCharacter>(const_cast<ACharacter*>(Character)))
		{
			MutableTTChar->NotifyInsufficientStamina();
		}
		UE_LOG(LogThreadTalesCombat, Warning, TEXT("[Evade GA] TryExecuteDodgeAttack rejected: Stamina is 0.0!"));
		return false;
	}

	return true;
}

FVector UThreadTalesGA_Evade::ComputeDodgeAttackDirection(const ACharacter* Character) const
{
	return ComputeMovementIntent(Character);
}

bool UThreadTalesGA_Evade::TryExecuteDodgeAttack()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

	if (!CanExecuteDodgeAttack(Character, ASC))
	{
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
	}
	bQueueDodgeAttack = false;
	const FVector MoveDir = ComputeDodgeAttackDirection(Character);
	AController* Controller = Character->GetController();
	FRotator TargetRot = Character->GetActorRotation();
	if (!MoveDir.IsNearlyZero())
	{
		TargetRot = FRotator(0.0f, MoveDir.Rotation().Yaw, 0.0f);
	}
	else if (Controller)
	{
		TargetRot = FRotator(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
	}

	Character->SetActorRotation(TargetRot);

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Evade GA] >>> Seamlessly Canceling Evade into DODGE ATTACK! (Yaw: %.1f) <<<"), TargetRot.Yaw);

	MontageStop(0.10f);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);

	FGameplayEventData EventData;
	EventData.EventTag = ThreadTalesGameplayTags::TAG_Event_Combat_Attack_Dodge;
	EventData.Instigator = Character;
	EventData.Target = Character;
	ASC->HandleGameplayEvent(ThreadTalesGameplayTags::TAG_Event_Combat_Attack_Dodge, &EventData);

	return true;
}

bool UThreadTalesGA_Evade::TryEarlyCancelToLocomotion()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return false;
	}

	UCharacterMovementComponent* CMC = Character->GetCharacterMovement();
	if (!CMC)
	{
		return false;
	}

	const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(Character);
	const APlayerController* PC = Cast<APlayerController>(Character->GetController());

	// 1. Gather 2D directional movement intent
	const FVector MoveDir = ComputeMovementIntent(Character);
	if (MoveDir.IsNearlyZero())
	{
		return false;
	}

	// Clear continuous polling timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
	}

	// 2. Determine if sprinting vs running (only sprint if stamina > 0)
	const bool bCanSprint = TTChar && TTChar->CanSpendStamina();
	const bool bIsSprint = bCanSprint && ((TTChar && TTChar->IsSprinting()) || FThreadTalesTraversalInputHelper::IsSprintHeld(PC, Character));
	const float ExitSpeed = bIsSprint ? ExitSprintSpeed : ExitRunSpeed;

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Evade GA] Recovery early cancel triggered: Exiting into %s (Speed: %.1f cm/s, MoveDir: %s)."),
		bIsSprint ? TEXT("SPRINT") : TEXT("RUN"), ExitSpeed, *MoveDir.ToCompactString());

	// 3. Orient character towards movement direction
	FRotator TargetRot = MoveDir.Rotation();
	TargetRot.Pitch = 0.0f;
	TargetRot.Roll = 0.0f;

	const float DeltaYaw = FMath::Abs(FRotator::NormalizeAxis(TargetRot.Yaw - Character->GetActorRotation().Yaw));

	// For forward exit (small angular deviation <= DirectAlignmentYawThreshold), apply exact alignment immediately.
	// For large deviations (backward / lateral exits), DO NOT teleport SetActorRotation:
	// doing so induces violent AnimBP Offset Root Bone compensation and mesh popping.
	// Instead, let CMC smoothly steer rotation via bOrientRotationToMovement at RotationRate.
	const bool bTargetLocked = TTChar && TTChar->GetTargetLockComponent() && TTChar->GetTargetLockComponent()->IsTargetLocked();
	if (DeltaYaw <= DirectAlignmentYawThreshold)
	{
		Character->SetActorRotation(TargetRot);
	}
	if (!bTargetLocked)
	{
		CMC->bOrientRotationToMovement = true;
	}

	// 4. Smooth blend out of evade montage into active locomotion
	MontageStop(EarlyCancelBlendOutTime);

	// 5. Ensure MaxWalkSpeed does not clamp the injected velocity
	CMC->MaxWalkSpeed = FMath::Max(CMC->MaxWalkSpeed, ExitSpeed);

	// 6. Inject momentum and pending input into movement component
	CMC->Velocity = MoveDir * ExitSpeed;
	Character->AddMovementInput(MoveDir, 1.0f);

	// 7. Complete and clean up ability
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);

	return true;
}

void UThreadTalesGA_Evade::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Unregister dynamic attack input tags
	if (FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec())
	{
		Spec->GetDynamicSpecSourceTags().RemoveTag(ThreadTalesGameplayTags::TAG_Input_Ability_Melee);
		Spec->GetDynamicSpecSourceTags().RemoveTag(ThreadTalesGameplayTags::TAG_Input_Ability_Ranged_Fire);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
	}

	// Clear dynamic I-Frames if still active
	if (bInvulnerableActive)
	{
		bInvulnerableActive = false;
		if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
		{
			ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Invulnerable);
		}
	}

	// Refresh stamina regeneration delay from the end of the evade
	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		if (UThreadTalesStaminaComponent* StamComp = TTChar->GetStaminaComponent())
		{
			StamComp->ApplyStaminaRegenDelay(TTChar->GetStamina() <= 0.0f);
		}

		// Ensure smooth free locomotion rotation is active if not target-locked
		const bool bTargetLocked = TTChar->GetTargetLockComponent() && TTChar->GetTargetLockComponent()->IsTargetLocked();
		if (!bTargetLocked)
		{
			if (UCharacterMovementComponent* CMC = TTChar->GetCharacterMovement())
			{
				CMC->bOrientRotationToMovement = true;
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UThreadTalesGA_Evade::OnMontageCompleted() { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false); }
void UThreadTalesGA_Evade::OnMontageInterrupted() { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); }
void UThreadTalesGA_Evade::OnMontageCancelled() { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); }
