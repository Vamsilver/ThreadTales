#include "Abilities/ThreadTalesGA_Melee.h"
#include "ThreadTales.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Abilities/ThreadTalesGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
#include "Characters/ThreadTalesBaseCharacter.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesStaminaComponent.h"
#include "Combat/ThreadTalesWeapon.h"
#include "Combat/ThreadTalesTargetLockComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Abilities/ThreadTalesAttributeSet.h"
#include "UObject/ConstructorHelpers.h"

UThreadTalesGA_Melee::UThreadTalesGA_Melee()
{
	AbilityName = TEXT("Melee Attack Combo");
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	SetAssetTags(FGameplayTagContainer(ThreadTalesGameplayTags::TAG_Ability_Melee));
	ActivationOwnedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Attacking);

	// Block melee activation while already attacking, Staggered, or Posture Broken
	ActivationBlockedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Attacking);
	ActivationBlockedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered);
	ActivationBlockedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_PostureBroken);

	// Trigger Melee Ability on Dodge Attack gameplay event
	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = ThreadTalesGameplayTags::TAG_Event_Combat_Attack_Dodge;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);

	// Set default Stamina cost and post-attack regen delay
	CostGameplayEffectClass = UThreadTalesGE_CostMelee::StaticClass();
	StaminaDelayEffectClass = UThreadTalesGE_StaminaDelay::StaticClass();

	ComboSectionNames.Add(FName("Combo1"));
	ComboSectionNames.Add(FName("Combo2"));
	ComboSectionNames.Add(FName("Combo3"));
	ComboSectionNames.Add(FName("Combo4"));

	CurrentComboIndex = 0;
	bComboWindowOpen = false;
	bQueueNextCombo = false;
	bIsDodgeAttack = false;
	bIsSprintAttack = false;
	LastAttackInputTime = -100.0f;
	CurrentPlayingMontage = nullptr;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> DodgeMontageObj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/Attack/AM_Attack_Dodge.AM_Attack_Dodge"));
	if (DodgeMontageObj.Succeeded())
	{
		DodgeAttackMontage = DodgeMontageObj.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> SprintMontageObj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/Attack/AM_Attack_Sprint.AM_Attack_Sprint"));
	if (SprintMontageObj.Succeeded())
	{
		SprintAttackMontage = SprintMontageObj.Object;
	}
}

bool UThreadTalesGA_Melee::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (Avatar)
	{
		if (const UThreadTalesEquipmentComponent* EquipComp = Avatar->FindComponentByClass<UThreadTalesEquipmentComponent>())
		{
			if (const AThreadTalesWeapon* CurWeapon = EquipComp->GetCurrentWeapon())
			{
				if (CurWeapon->GetPrimaryAttackTag().MatchesTagExact(ThreadTalesGameplayTags::TAG_Ability_Ranged_Fire) ||
				    CurWeapon->GetOverlayPoseName() == FName("Bow"))
				{
					return false;
				}
			}
		}
	}

	return true;
}

bool UThreadTalesGA_Melee::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (AThreadTalesBaseCharacter* TTChar = Cast<AThreadTalesBaseCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		if (!TTChar->CanSpendStamina())
		{
			TTChar->NotifyInsufficientStamina();
			return false;
		}
		return true;
	}
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}

void UThreadTalesGA_Melee::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// Handled explicitly in ActivateAbility and JumpToNextComboSection using DA_StaminaConfig per-attack costs.
}

bool UThreadTalesGA_Melee::HasValidComboStrikeAtIndex(int32 Index) const
{
	if (Index < 0)
	{
		return false;
	}

	TArray<UAnimMontage*> ActiveStrikeMontages = ComboStrikeMontages;
	if (const AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		if (const UThreadTalesEquipmentComponent* EquipComp = Avatar->FindComponentByClass<UThreadTalesEquipmentComponent>())
		{
			if (EquipComp->GetCurrentWeapon() && EquipComp->GetCurrentWeapon()->GetComboStrikeMontages().Num() > 0)
			{
				ActiveStrikeMontages = EquipComp->GetCurrentWeapon()->GetComboStrikeMontages();
			}
		}
	}

	if (ActiveStrikeMontages.Num() > 0)
	{
		return ActiveStrikeMontages.IsValidIndex(Index) && (ActiveStrikeMontages[Index] != nullptr);
	}

	return ComboSectionNames.IsValidIndex(Index);
}

void UThreadTalesGA_Melee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	CurrentComboIndex = 0;
	bComboWindowOpen = false;
	bQueueNextCombo = false;
	bIsDodgeAttack = false;
	bIsSprintAttack = false;
	CurrentPlayingMontage = nullptr;

	// Capture sprint state BEFORE consuming stamina (since consuming remaining stamina to 0.0f disables IsSprinting())
	AThreadTalesBaseCharacter* TTChar = Cast<AThreadTalesBaseCharacter>(GetAvatarActorFromActorInfo());
	const bool bWasSprintingOnTrigger = TTChar && TTChar->IsSprinting();

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogThreadTalesCombat, Warning, TEXT("[Melee GA] Failed to Commit Ability!"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UThreadTalesStaminaComponent* StamComp = TTChar ? TTChar->GetStaminaComponent() : nullptr;

	// 1. Check if triggered specifically as a Dodge Attack event
	const bool bDodgeTriggered = TriggerEventData && TriggerEventData->EventTag.MatchesTag(ThreadTalesGameplayTags::TAG_Event_Combat_Attack_Dodge);
	if (bDodgeTriggered)
	{
		if (StamComp && !StamComp->TryConsumeStamina(StamComp->GetDodgeAttackStaminaCost(), true))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		PlayDodgeAttack();
		return;
	}

	// 2. Check if triggered while sprinting (Sprint Attack)
	if (TTChar && bWasSprintingOnTrigger)
	{
		UThreadTalesEquipmentComponent* EquipComp = TTChar->GetEquipmentComponent();
		UAnimMontage* ActiveSprintMontage = SprintAttackMontage;
		if (EquipComp && EquipComp->GetCurrentWeapon() && EquipComp->GetCurrentWeapon()->GetSprintAttackMontage())
		{
			ActiveSprintMontage = EquipComp->GetCurrentWeapon()->GetSprintAttackMontage();
		}

		if (ActiveSprintMontage)
		{
			if (StamComp && !StamComp->TryConsumeStamina(StamComp->GetSprintAttackStaminaCost(), true))
			{
				EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
				return;
			}
			PlaySprintAttack();
			return;
		}
	}

	// 3. Standard combo strike 0 (Strike 1)
	if (StamComp && !StamComp->TryConsumeStamina(StamComp->GetLightAttackStaminaCost(), true))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	PlayComboStrike(0);
}

void UThreadTalesGA_Melee::PlayDodgeAttack()
{
	bIsDodgeAttack = true;
	bIsSprintAttack = false;
	CurrentComboIndex = -1; // -1 so the next chained strike in JumpToNextComboSection will be index 0!

	// Orient character towards camera or movement direction
	AlignCharacterToCameraDirection();

	UAnimMontage* MontageToPlay = DodgeAttackMontage;
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (Avatar)
	{
		UThreadTalesEquipmentComponent* EquipComp = Avatar->FindComponentByClass<UThreadTalesEquipmentComponent>();
		if (EquipComp && EquipComp->GetCurrentWeapon() && EquipComp->GetCurrentWeapon()->GetDodgeAttackMontage())
		{
			MontageToPlay = EquipComp->GetCurrentWeapon()->GetDodgeAttackMontage();
		}
	}

	if (!MontageToPlay)
	{
		UE_LOG(LogThreadTalesCombat, Warning, TEXT("[Melee GA] PlayDodgeAttack called, but no DodgeAttackMontage assigned! Falling back to strike 0."));
		PlayComboStrike(0);
		return;
	}

	CurrentPlayingMontage = MontageToPlay;
	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Melee GA] >>> Playing DODGE ATTACK Montage: %s <<<"), *GetNameSafe(MontageToPlay));

	if (MontageTask)
	{
		MontageTask->OnCompleted.Clear();
		MontageTask->OnInterrupted.Clear();
		MontageTask->OnCancelled.Clear();
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	float PlayRate = DodgeAttackPlayRate;
	if (Avatar)
	{
		UThreadTalesEquipmentComponent* EquipComp = Avatar->FindComponentByClass<UThreadTalesEquipmentComponent>();
		if (EquipComp && EquipComp->GetCurrentWeapon())
		{
			PlayRate = EquipComp->GetCurrentWeapon()->GetDodgeAttackPlayRate();
		}
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay, PlayRate, NAME_None, true, 1.f);
	if (MontageTask)
	{
		MontageTask->OnCompleted.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageCompleted);
		MontageTask->OnInterrupted.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageInterrupted);
		MontageTask->OnCancelled.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageCancelled);
		MontageTask->ReadyForActivation();
	}
}

void UThreadTalesGA_Melee::PlaySprintAttack()
{
	bIsSprintAttack = true;
	bIsDodgeAttack = false;
	CurrentComboIndex = -1; // -1 so next chained strike will be index 0!

	AlignCharacterToCameraDirection();

	UAnimMontage* MontageToPlay = SprintAttackMontage;
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (Avatar)
	{
		UThreadTalesEquipmentComponent* EquipComp = Avatar->FindComponentByClass<UThreadTalesEquipmentComponent>();
		if (EquipComp && EquipComp->GetCurrentWeapon() && EquipComp->GetCurrentWeapon()->GetSprintAttackMontage())
		{
			MontageToPlay = EquipComp->GetCurrentWeapon()->GetSprintAttackMontage();
		}
	}

	if (!MontageToPlay)
	{
		UE_LOG(LogThreadTalesCombat, Warning, TEXT("[Melee GA] PlaySprintAttack called, but no SprintAttackMontage assigned! Falling back to strike 0."));
		PlayComboStrike(0);
		return;
	}

	CurrentPlayingMontage = MontageToPlay;
	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Melee GA] >>> Playing SPRINT ATTACK Montage: %s <<<"), *GetNameSafe(MontageToPlay));

	if (MontageTask)
	{
		MontageTask->OnCompleted.Clear();
		MontageTask->OnInterrupted.Clear();
		MontageTask->OnCancelled.Clear();
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay, 1.f, NAME_None, true, 1.f);
	if (MontageTask)
	{
		MontageTask->OnCompleted.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageCompleted);
		MontageTask->OnInterrupted.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageInterrupted);
		MontageTask->OnCancelled.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageCancelled);
		MontageTask->ReadyForActivation();
	}
}

void UThreadTalesGA_Melee::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);

	NotifyComboInput();
}

void UThreadTalesGA_Melee::PlayComboStrike(int32 Index)
{
	// Orient character towards camera or movement direction
	AlignCharacterToCameraDirection();

	// 1. Check if weapon or ability provides an array of separate combo strike montages
	TArray<UAnimMontage*> ActiveStrikeMontages = ComboStrikeMontages;
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (Avatar)
	{
		UThreadTalesEquipmentComponent* EquipComp = Avatar->FindComponentByClass<UThreadTalesEquipmentComponent>();
		if (EquipComp && EquipComp->GetCurrentWeapon() && EquipComp->GetCurrentWeapon()->GetComboStrikeMontages().Num() > 0)
		{
			ActiveStrikeMontages = EquipComp->GetCurrentWeapon()->GetComboStrikeMontages();
		}
	}

	if (ActiveStrikeMontages.Num() > 0)
	{
		if (ActiveStrikeMontages.IsValidIndex(Index) && ActiveStrikeMontages[Index])
		{
			UAnimMontage* MontageToPlay = ActiveStrikeMontages[Index];
			CurrentPlayingMontage = MontageToPlay;

			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Melee GA] Playing Separate Strike %d Montage: %s"), Index + 1, *GetNameSafe(MontageToPlay));

			if (MontageTask)
			{
				MontageTask->OnCompleted.Clear();
				MontageTask->OnInterrupted.Clear();
				MontageTask->OnCancelled.Clear();
				MontageTask->EndTask();
				MontageTask = nullptr;
			}

			MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay, 1.f, NAME_None, true, 1.f);
			if (MontageTask)
			{
				MontageTask->OnCompleted.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageCompleted);
				MontageTask->OnInterrupted.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageInterrupted);
				MontageTask->OnCancelled.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageCancelled);
				MontageTask->ReadyForActivation();
			}
			return;
		}
		else
		{
			UE_LOG(LogThreadTalesCombat, Log, TEXT("[Melee GA] Reached end of separate combo strike chain (%d strikes)."), ActiveStrikeMontages.Num());
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			return;
		}
	}

	// 2. Fallback to Section-based single Montage
	UAnimMontage* SingleMontage = FallbackAttackMontage;
	if (Avatar)
	{
		UThreadTalesEquipmentComponent* EquipComp = Avatar->FindComponentByClass<UThreadTalesEquipmentComponent>();
		if (EquipComp && EquipComp->GetCurrentWeapon() && EquipComp->GetCurrentWeapon()->GetComboAttackMontage())
		{
			SingleMontage = EquipComp->GetCurrentWeapon()->GetComboAttackMontage();
		}
	}

	if (SingleMontage)
	{
		CurrentPlayingMontage = SingleMontage;
		if (Index == 0)
		{
			FName StartSection = ComboSectionNames.Num() > 0 ? ComboSectionNames[0] : NAME_None;
			UE_LOG(LogThreadTalesCombat, Log, TEXT("[Melee GA] Started Section Combo Strike 1 on Montage: %s (Section: %s)"), *GetNameSafe(SingleMontage), *StartSection.ToString());

			MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, SingleMontage, 1.f, StartSection, true, 1.f);
			if (MontageTask)
			{
				MontageTask->OnCompleted.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageCompleted);
				MontageTask->OnInterrupted.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageInterrupted);
				MontageTask->OnCancelled.AddDynamic(this, &UThreadTalesGA_Melee::OnMontageCancelled);
				MontageTask->ReadyForActivation();
			}
		}
		else if (ComboSectionNames.IsValidIndex(Index))
		{
			FName NextSection = ComboSectionNames[Index];
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Melee GA] === Transitioning to Section: %s (Strike %d) ==="), *NextSection.ToString(), Index + 1);

			ACharacter* Character = Cast<ACharacter>(Avatar);
			if (Character && Character->GetMesh() && Character->GetMesh()->GetAnimInstance())
			{
				Character->GetMesh()->GetAnimInstance()->Montage_JumpToSection(NextSection, SingleMontage);
			}
		}
		else
		{
			UE_LOG(LogThreadTalesCombat, Log, TEXT("[Melee GA] All combo sections in chain completed."));
		}
	}
	else
	{
		UE_LOG(LogThreadTalesCombat, Error, TEXT("[Melee GA] ERROR: No Combo Attack Montage assigned in Weapon or Ability!"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UThreadTalesGA_Melee::NotifyComboInput()
{
	LastAttackInputTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	if (bComboWindowOpen)
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Melee GA] Attack Input Received while window open -> Jumping to strike %d!"), CurrentComboIndex + 2);
		JumpToNextComboSection();
	}
	else
	{
		bQueueNextCombo = true;
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Melee GA] Attack Input Received before window -> Buffered for strike %d."), CurrentComboIndex + 2);
	}
}

void UThreadTalesGA_Melee::NotifyComboWindowBegin()
{
	bComboWindowOpen = true;

	// Check if player clicked shortly before the window opened (Input Buffering)
	if (bQueueNextCombo || (GetWorld() && (GetWorld()->GetTimeSeconds() - LastAttackInputTime <= InputBufferTolerance)))
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Melee GA] Combo Window OPEN with Buffered Input -> Jumping to strike %d!"), CurrentComboIndex + 2);
		bQueueNextCombo = false;
		JumpToNextComboSection();
		return;
	}

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Melee GA] >>> Combo Window OPEN for Strike %d <<<"), CurrentComboIndex + 1);
}

void UThreadTalesGA_Melee::NotifyComboWindowEnd()
{
	bComboWindowOpen = false;

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Melee GA] <<< Combo Window CLOSED for Strike %d (HasQueued: %s) >>>"),
		CurrentComboIndex + 1, bQueueNextCombo ? TEXT("YES") : TEXT("NO"));

	if (bQueueNextCombo)
	{
		JumpToNextComboSection();
	}
}

void UThreadTalesGA_Melee::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Refresh stamina regeneration delay from the end of the attack montage
	if (AThreadTalesBaseCharacter* TTChar = Cast<AThreadTalesBaseCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		TTChar->ApplyStaminaRegenDelay(TTChar->GetStamina() <= 0.0f);

		// Restore Strafe mode if locked onto a target
		if (AThreadTalesCharacter* PlayerChar = Cast<AThreadTalesCharacter>(TTChar))
		{
			if (UThreadTalesTargetLockComponent* TargetLock = PlayerChar->GetTargetLockComponent())
			{
				if (TargetLock->IsTargetLocked())
				{
					TargetLock->SetCharacterStrafeMode(true);
				}
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UThreadTalesGA_Melee::JumpToNextComboSection()
{
	const int32 NextComboIndex = CurrentComboIndex + 1;

	// Guard: verify next strike actually exists BEFORE deducting stamina
	if (!HasValidComboStrikeAtIndex(NextComboIndex))
	{
		UE_LOG(LogThreadTalesCombat, Log, TEXT("[Melee GA] No further combo strikes after Strike %d -> ignoring extra click without spending stamina."), CurrentComboIndex + 1);
		bComboWindowOpen = false;
		bQueueNextCombo = false;
		return;
	}

	AThreadTalesBaseCharacter* TTChar = Cast<AThreadTalesBaseCharacter>(GetAvatarActorFromActorInfo());
	if (TTChar)
	{
		UThreadTalesStaminaComponent* StamComp = TTChar->GetStaminaComponent();
		if (StamComp && !StamComp->TryConsumeStamina(StamComp->GetLightAttackStaminaCost(), true))
		{
			UE_LOG(LogThreadTalesCombat, Warning, TEXT("[Melee GA] Insufficient stamina (0.0) to chain combo strike %d!"), NextComboIndex + 1);
			bComboWindowOpen = false;
			bQueueNextCombo = false;
			return;
		}
	}

	CurrentComboIndex = NextComboIndex;
	bComboWindowOpen = false;
	bQueueNextCombo = false;
	bIsDodgeAttack = false;
	bIsSprintAttack = false;

	PlayComboStrike(CurrentComboIndex);
}

void UThreadTalesGA_Melee::AlignCharacterToCameraDirection()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character) return;

	// If locked onto a target, always orient directly towards the target (ignoring WASD backward/side vector)
	if (const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(Character))
	{
		if (const UThreadTalesTargetLockComponent* TargetLock = TTChar->GetTargetLockComponent())
		{
			if (TargetLock->IsTargetLocked())
			{
				const FVector TargetLoc = TargetLock->GetTargetLocation();
				const FVector ToTarget = (TargetLoc - Character->GetActorLocation()).GetSafeNormal2D();
				if (!ToTarget.IsNearlyZero())
				{
					Character->SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
					return;
				}
			}
		}
	}

	AController* Controller = Character->GetController();
	if (!Controller) return;

	// Calculate target Yaw from Camera/Controller
	FRotator ControlRot = Controller->GetControlRotation();
	FRotator TargetRotation(0.0f, ControlRot.Yaw, 0.0f);

	// If player is holding a movement input (WASD), attack in that movement direction, otherwise towards camera forward
	FVector LastInput = Character->GetLastMovementInputVector();
	if (!LastInput.IsNearlyZero())
	{
		TargetRotation = FRotator(0.0f, LastInput.Rotation().Yaw, 0.0f);
	}

	Character->SetActorRotation(TargetRotation);
}

void UThreadTalesGA_Melee::OnMontageCompleted()
{
	UE_LOG(LogThreadTalesCombat, Log, TEXT("[Melee GA] Montage Completed normally."));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UThreadTalesGA_Melee::OnMontageInterrupted()
{
	UE_LOG(LogThreadTalesCombat, Log, TEXT("[Melee GA] Montage Interrupted."));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UThreadTalesGA_Melee::OnMontageCancelled()
{
	UE_LOG(LogThreadTalesCombat, Log, TEXT("[Melee GA] Montage Cancelled."));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
