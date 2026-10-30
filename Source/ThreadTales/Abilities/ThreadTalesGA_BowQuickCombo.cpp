// Copyright Epic Games, Inc. All Rights Reserved.

#include "Abilities/ThreadTalesGA_BowQuickCombo.h"
#include "ThreadTales.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Combat/ThreadTalesWeapon_Bow.h"
#include "Combat/ThreadTalesProjectile_Arrow.h"
#include "Combat/ThreadTalesTargetLockComponent.h"
#include "Data/ThreadTalesBowConfig.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
#include "Characters/ThreadTalesStaminaComponent.h"
#include "Abilities/ThreadTalesAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Animation/AnimMontage.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

UThreadTalesGA_BowQuickCombo::UThreadTalesGA_BowQuickCombo()
{
	AbilityName = TEXT("Bow Quick Combo & Dodge Shot");
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer AssetTagsContainer;
	AssetTagsContainer.AddTag(ThreadTalesGameplayTags::TAG_Ability_Ranged_Fire);
	SetAssetTags(AssetTagsContainer);

	ActivationOwnedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Attacking);
	ActivationBlockedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered);
	ActivationBlockedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_PostureBroken);
	ActivationBlockedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_HitReact);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = ThreadTalesGameplayTags::TAG_Event_Combat_Attack_Dodge;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);

	FAbilityTriggerData ParryCounterTrigger;
	ParryCounterTrigger.TriggerTag = ThreadTalesGameplayTags::TAG_Event_Combat_Attack_ParryCounter;
	ParryCounterTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(ParryCounterTrigger);

	ArrowClass = AThreadTalesProjectile_Arrow::StaticClass();
}

bool UThreadTalesGA_BowQuickCombo::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) { return false; }

	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!Avatar) { return false; }

	const UThreadTalesEquipmentComponent* EquipComp = Avatar->FindComponentByClass<UThreadTalesEquipmentComponent>();
	if (!EquipComp || !EquipComp->GetCurrentWeapon() || !EquipComp->GetCurrentWeapon()->IsA<AThreadTalesWeapon_Bow>()) { return false; }
	if (!EquipComp->GetCurrentWeapon()->IsDrawn()) { return false; }

	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

	const bool bHasParryCounterWindow = ASC && ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_ParryCounterWindow);
	if (!bHasParryCounterWindow && ASC && ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Aiming))
	{
		return false;
	}

	return true;
}

bool UThreadTalesGA_BowQuickCombo::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		if (!TTChar->CanSpendStamina())
		{
			const_cast<AThreadTalesCharacter*>(TTChar)->NotifyInsufficientStamina();
			return false;
		}
		return true;
	}
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}

void UThreadTalesGA_BowQuickCombo::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// Handled granularly in PlayComboStep and PlayDodgeShot
}

void UThreadTalesGA_BowQuickCombo::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo());
	if (!TTChar)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CachedBowWeapon = GetEquippedBow();
	bQueueNextCombo = false;

	const bool bDodgeTriggered = TriggerEventData && TriggerEventData->EventTag.MatchesTag(ThreadTalesGameplayTags::TAG_Event_Combat_Attack_Dodge);
	const bool bParryCounterTriggered = TriggerEventData && TriggerEventData->EventTag.MatchesTag(ThreadTalesGameplayTags::TAG_Event_Combat_Attack_ParryCounter);
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const bool bHasParryCounterTag = ASC && ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_ParryCounterWindow);

	if (bParryCounterTriggered || bHasParryCounterTag)
	{
		PlayParryCounterShot(DetermineParryCounterDirectionIndex(TTChar));
	}
	else if (bDodgeTriggered)
	{
		PlayDodgeShot(DetermineDodgeDirectionIndex(TTChar));
	}
	else
	{
		CurrentComboIndex = 0;
		bIsDodgeShot = false;
		bIsParryCounterShot = false;
		PlayComboStep(0);
	}
}

void UThreadTalesGA_BowQuickCombo::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);
	NotifyComboInput();
}

void UThreadTalesGA_BowQuickCombo::NotifyComboInput()
{
	if (bIsDodgeShot || bIsParryCounterShot || CurrentComboIndex >= 3) { return; }
	if (bComboWindowOpen && bHasFiredCurrentShot)
	{
		bComboWindowOpen = false;
		bQueueNextCombo = false;
		PlayComboStep(CurrentComboIndex + 1);
	}
	else
	{
		bQueueNextCombo = true;
	}
}

void UThreadTalesGA_BowQuickCombo::NotifyComboWindowBegin()
{
	if (bIsDodgeShot || bIsParryCounterShot || CurrentComboIndex >= 3)
	{
		bComboWindowOpen = false;
		bQueueNextCombo = false;
		return;
	}

	bComboWindowOpen = true;
	if (bQueueNextCombo)
	{
		if (!bHasFiredCurrentShot) { NotifyQuickShotFire(); }
		bComboWindowOpen = false;
		bQueueNextCombo = false;
		PlayComboStep(CurrentComboIndex + 1);
	}
}

void UThreadTalesGA_BowQuickCombo::NotifyComboWindowEnd()
{
	bComboWindowOpen = false;
	if (bQueueNextCombo && !bIsDodgeShot && !bIsParryCounterShot && CurrentComboIndex >= 0 && CurrentComboIndex < 3)
	{
		if (!bHasFiredCurrentShot) { NotifyQuickShotFire(); }
		bQueueNextCombo = false;
		PlayComboStep(CurrentComboIndex + 1);
	}
}

bool UThreadTalesGA_BowQuickCombo::TryCancelToLocomotion()
{
	if (!IsActive() || bQueueNextCombo || !bHasFiredCurrentShot) { return false; }
	UE_LOG(LogThreadTalesCombat, Display, TEXT("[BowQuickCombo] Early Locomotion Cancel triggered! Ending ability cleanly into locomotion."));
	if (ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UAnimInstance* AnimInst = Char->GetMesh() ? Char->GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInst->Montage_Stop(0.35f, CurrentPlayingMontage);
		}
	}
	if (MontageTask) { MontageTask->EndTask(); MontageTask = nullptr; }
	bIsDodgeShot = false;
	bIsParryCounterShot = false;
	bQueueNextCombo = false;
	bComboWindowOpen = false;
	bCanEarlyLocomotionCancel = false;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	return true;
}

void UThreadTalesGA_BowQuickCombo::HandleEarlyRecoveryTimer()
{
	bCanEarlyLocomotionCancel = true;
	if (const ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		const FVector InputVec = Char->GetLastMovementInputVector();
		if (!InputVec.IsNearlyZero(0.01f))
		{
			TryCancelToLocomotion();
			return;
		}
	}

	// Schedule lightweight 20 Hz poll to catch WASD input during remaining recovery
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EarlyRecoveryTimerHandle);
		World->GetTimerManager().SetTimer(EarlyRecoveryTimerHandle, this, &UThreadTalesGA_BowQuickCombo::CheckEarlyLocomotionInput, 0.05f, true);
	}
}

void UThreadTalesGA_BowQuickCombo::CheckEarlyLocomotionInput()
{
	if (!bCanEarlyLocomotionCancel || !IsActive())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(EarlyRecoveryTimerHandle);
		}
		return;
	}

	if (const ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		const FVector InputVec = Char->GetLastMovementInputVector();
		if (!InputVec.IsNearlyZero(0.01f))
		{
			TryCancelToLocomotion();
		}
	}
}


void UThreadTalesGA_BowQuickCombo::ArmAttackTimers(float FireDelay, float EarlyRecDelay, float EffectivePlayRate, int32 StepIndex)
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	World->GetTimerManager().ClearTimer(FireTimerHandle);
	World->GetTimerManager().SetTimer(FireTimerHandle, this, &UThreadTalesGA_BowQuickCombo::NotifyQuickShotFire, FireDelay, false);

	if (StepIndex >= 0 && StepIndex < 3)
	{
		const float OpenDelay = FireDelay + (0.06f / EffectivePlayRate);
		const float CloseDelay = FireDelay + (0.42f / EffectivePlayRate);
		World->GetTimerManager().ClearTimer(ComboWindowOpenTimerHandle);
		World->GetTimerManager().SetTimer(ComboWindowOpenTimerHandle, this, &UThreadTalesGA_BowQuickCombo::NotifyComboWindowBegin, OpenDelay, false);
		World->GetTimerManager().ClearTimer(ComboWindowCloseTimerHandle);
		World->GetTimerManager().SetTimer(ComboWindowCloseTimerHandle, this, &UThreadTalesGA_BowQuickCombo::NotifyComboWindowEnd, CloseDelay, false);
	}

	World->GetTimerManager().ClearTimer(EarlyRecoveryTimerHandle);
	World->GetTimerManager().SetTimer(EarlyRecoveryTimerHandle, this, &UThreadTalesGA_BowQuickCombo::HandleEarlyRecoveryTimer, EarlyRecDelay, false);
}

void UThreadTalesGA_BowQuickCombo::PlayComboStep(int32 StepIndex)
{
	CurrentComboIndex = StepIndex;
	bIsDodgeShot = false;
	bQueueNextCombo = false;
	bHasFiredCurrentShot = false;
	bComboWindowOpen = false;

	AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo());
	if (!TTChar)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (UCharacterMovementComponent* MoveComp = TTChar->GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
	}

	UThreadTalesStaminaComponent* StamComp = TTChar->GetStaminaComponent();
	if (StamComp && !StamComp->TryConsumeStamina(StamComp->GetLightAttackStaminaCost(), true))
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[BowQuickCombo] Insufficient stamina for step %d!"), StepIndex + 1);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	AlignFacingToTargetOrCamera();

	const UThreadTalesBowConfig* Config = GetBowConfig();
	UAnimMontage* MontageToPlay = (Config && Config->QuickShotComboMontages.IsValidIndex(StepIndex))
		? Config->QuickShotComboMontages[StepIndex].LoadSynchronous() : nullptr;

	if (!MontageToPlay)
	{
		UE_LOG(LogThreadTalesCombat, Warning, TEXT("[BowQuickCombo] No montage found for step %d! Aborting."), StepIndex);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	const float EffectivePlayRate = FMath::Max(ComboAttackPlayRate, 0.1f);
	StartMontageTask(MontageToPlay, EffectivePlayRate);

	const float StepDelays[] = { 0.30f, 0.32f, 0.38f, 0.52f };
	const float RecoveryDelays[] = { 0.75f, 0.80f, 0.90f, 0.95f };
	const int32 ClampedStep = FMath::Clamp(StepIndex, 0, 3);
	ArmAttackTimers(StepDelays[ClampedStep] / EffectivePlayRate, RecoveryDelays[ClampedStep] / EffectivePlayRate, EffectivePlayRate, StepIndex);

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[BowQuickCombo] >>> Playing COMBO Step %d: '%s' <<<"), StepIndex + 1, *GetNameSafe(MontageToPlay));
}

void UThreadTalesGA_BowQuickCombo::PlayDodgeShot(int32 DirectionIndex)
{
	bIsDodgeShot = true;
	CurrentComboIndex = -1;
	bQueueNextCombo = false;
	bHasFiredCurrentShot = false;

	AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo());
	if (!TTChar)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	UThreadTalesStaminaComponent* StamComp = TTChar->GetStaminaComponent();
	if (StamComp && !StamComp->TryConsumeStamina(StamComp->GetDodgeAttackStaminaCost(), true))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	AlignFacingToTargetOrCamera();

	const UThreadTalesBowConfig* Config = GetBowConfig();
	UAnimMontage* MontageToPlay = (Config && Config->DodgeShotMontages.IsValidIndex(DirectionIndex))
		? Config->DodgeShotMontages[DirectionIndex].LoadSynchronous()
		: (Config && Config->DodgeShotMontages.Num() > 0 ? Config->DodgeShotMontages[0].LoadSynchronous() : nullptr);

	if (!MontageToPlay)
	{
		UE_LOG(LogThreadTalesCombat, Warning, TEXT("[BowQuickCombo] No DodgeShot montage available! Falling back to Combo step 0."));
		PlayComboStep(0);
		return;
	}

	const float EffectivePlayRate = FMath::Max(DodgeAttackPlayRate, 0.1f);
	StartMontageTask(MontageToPlay, EffectivePlayRate);
	ArmAttackTimers(0.45f / EffectivePlayRate, 0.75f / EffectivePlayRate, EffectivePlayRate, -1);

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[BowQuickCombo] >>> Playing DODGE-TO-SHOT [Dir %d]: '%s' <<<"), DirectionIndex, *GetNameSafe(MontageToPlay));
}

void UThreadTalesGA_BowQuickCombo::PlayParryCounterShot(int32 DirectionIndex)
{
	bIsParryCounterShot = true;
	bIsDodgeShot = false;
	CurrentComboIndex = -1;
	bQueueNextCombo = false;
	bHasFiredCurrentShot = false;
	bComboWindowOpen = false;
	bCanEarlyLocomotionCancel = false;

	AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo());
	if (!TTChar)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	TTChar->CloseParryCounterWindow();
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		const FGameplayTagContainer AimTags(ThreadTalesGameplayTags::TAG_Ability_Ranged);
		ASC->CancelAbilities(&AimTags);
	}

	if (UCharacterMovementComponent* MoveComp = TTChar->GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
	}

	UThreadTalesStaminaComponent* StamComp = TTChar->GetStaminaComponent();
	if (StamComp && !StamComp->TryConsumeStamina(StamComp->GetLightAttackStaminaCost(), true))
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[BowQuickCombo] Insufficient stamina for Parry Counter Shot!"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	AlignFacingToTargetOrCamera();

	const UThreadTalesBowConfig* Config = GetBowConfig();
	UAnimMontage* MontageToPlay = Config ? Config->GetParryCounterMontage(DirectionIndex) : nullptr;
	if (!MontageToPlay)
	{
		UE_LOG(LogThreadTalesCombat, Warning, TEXT("[BowQuickCombo] No ParryCounter montage found (Dir: %d)! Falling back to Combo 0."), DirectionIndex);
		PlayComboStep(0);
		return;
	}

	const float EffectivePlayRate = FMath::Max(ComboAttackPlayRate * 1.15f, 0.1f);
	StartMontageTask(MontageToPlay, EffectivePlayRate);

	const float BaseFireDelay = (DirectionIndex == 1) ? 0.72f : 0.50f;
	const float BaseRecDelay = (DirectionIndex == 1) ? 1.05f : 0.85f;
	ArmAttackTimers(BaseFireDelay / EffectivePlayRate, BaseRecDelay / EffectivePlayRate, EffectivePlayRate, -1);

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[BowQuickCombo] >>> Playing POINT-BLANK PARRY COUNTER-SHOT [Dir %d]: '%s' <<<"),
		DirectionIndex, *GetNameSafe(MontageToPlay));
}

void UThreadTalesGA_BowQuickCombo::StartMontageTask(UAnimMontage* MontageToPlay, float PlayRate)
{
	CurrentPlayingMontage = MontageToPlay;

	if (MontageTask)
	{
		MontageTask->OnCompleted.Clear();
		MontageTask->OnBlendOut.Clear();
		MontageTask->OnInterrupted.Clear();
		MontageTask->OnCancelled.Clear();
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, MontageToPlay, PlayRate, NAME_None, false, 1.0f);

	if (MontageTask)
	{
		MontageTask->OnCompleted.AddDynamic(this, &UThreadTalesGA_BowQuickCombo::OnMontageCompleted);
		MontageTask->OnBlendOut.AddDynamic(this, &UThreadTalesGA_BowQuickCombo::OnMontageBlendOut);
		MontageTask->OnInterrupted.AddDynamic(this, &UThreadTalesGA_BowQuickCombo::OnMontageInterrupted);
		MontageTask->OnCancelled.AddDynamic(this, &UThreadTalesGA_BowQuickCombo::OnMontageCancelled);
		MontageTask->ReadyForActivation();
	}

	if (UWorld* World = GetWorld())
	{
		const float EffectivePlayRate = FMath::Max(PlayRate, 0.1f);
		const float SafetyDuration = (MontageToPlay->GetPlayLength() / EffectivePlayRate) + 0.08f;
		World->GetTimerManager().ClearTimer(SafetyTimerHandle);
		World->GetTimerManager().SetTimer(SafetyTimerHandle, this, &UThreadTalesGA_BowQuickCombo::OnMontageSafetyTimeout, SafetyDuration, false);
	}
}

int32 UThreadTalesGA_BowQuickCombo::DetermineParryCounterDirectionIndex(const ACharacter* Character) const
{
	if (const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(Character))
	{
		if (const UThreadTalesTargetLockComponent* TargetLock = TTChar->GetTargetLockComponent())
		{
			if (TargetLock->IsTargetLocked())
			{
				const FVector ToTarget = TargetLock->GetTargetLocation() - Character->GetActorLocation();
				return (FVector::DotProduct(ToTarget.GetSafeNormal2D(), Character->GetActorRightVector()) < 0.0f) ? 0 : 1;
			}
		}
	}
	return 0;
}

void UThreadTalesGA_BowQuickCombo::NotifyQuickShotFire()
{
	if (bHasFiredCurrentShot) { return; }
	bHasFiredCurrentShot = true;

	AActor* Avatar = GetAvatarActorFromActorInfo();
	AThreadTalesWeapon_Bow* Bow = CachedBowWeapon.IsValid() ? CachedBowWeapon.Get() : GetEquippedBow();
	if (!Avatar || !Bow) { return; }

	FVector MuzzleOrigin = Bow->GetArrowSpawnTransform().GetLocation();
	if (MuzzleOrigin.IsZero() || FVector::DistSquared(MuzzleOrigin, Avatar->GetActorLocation()) > 62500.0f)
	{
		const ACharacter* CharAvatar = Cast<ACharacter>(Avatar);
		const USkeletalMeshComponent* Mesh = CharAvatar ? CharAvatar->GetMesh() : nullptr;
		if (Mesh && Mesh->DoesSocketExist(FName("overlay_bow"))) { MuzzleOrigin = Mesh->GetSocketLocation(FName("overlay_bow")); }
		else if (Mesh && Mesh->DoesSocketExist(FName("hand_lSocket"))) { MuzzleOrigin = Mesh->GetSocketLocation(FName("hand_lSocket")); }
		else { MuzzleOrigin = Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * 50.0f + FVector(0.0f, 0.0f, 60.0f); }
	}

	FVector LaunchDirection = Avatar->GetActorForwardVector();
	const AThreadTalesCharacter* TTCharAvatar = Cast<AThreadTalesCharacter>(Avatar);
	const UThreadTalesTargetLockComponent* TargetLock = TTCharAvatar ? TTCharAvatar->GetTargetLockComponent() : nullptr;
	if (TargetLock && TargetLock->IsTargetLocked())
	{
		LaunchDirection = (TargetLock->GetTargetLocation() - MuzzleOrigin).GetSafeNormal();
	}
	else if (const AController* Controller = TTCharAvatar ? TTCharAvatar->GetController() : nullptr)
	{
		LaunchDirection = Controller->GetControlRotation().Vector();
	}

	MuzzleOrigin += LaunchDirection * ArrowSpawnForwardOffset;

	const UThreadTalesBowConfig* Config = GetBowConfig();
	float Speed = Config ? Config->GetSpeedForTier(1) : 1800.0f;
	float GravityScale = Config ? Config->GetGravityForTier(1) : 0.35f;
	float Dmg = BaseComboDamage;
	float PoiseDmg = BaseComboPoiseDamage;

	if (bIsParryCounterShot)
	{
		Dmg *= ParryCounterDamageMultiplier;
		PoiseDmg *= ParryCounterPoiseMultiplier;
		Speed = Config ? Config->GetSpeedForTier(2) : 2800.0f;
		GravityScale = 0.15f;
	}
	else if (bIsDodgeShot)
	{
		Dmg *= 1.25f;
		PoiseDmg *= DodgeShotPoiseMultiplier;
	}
	else
	{
		Dmg *= (1.0f + CurrentComboIndex * 0.15f);
		PoiseDmg *= (1.0f + CurrentComboIndex * 0.25f);
	}

	SpawnQuickArrowProjectile(LaunchDirection, MuzzleOrigin, Dmg, PoiseDmg, Speed, GravityScale);

	if (QuickShotSound && GetWorld())
	{
		UGameplayStatics::PlaySoundAtLocation(this, QuickShotSound, MuzzleOrigin);
	}
}

void UThreadTalesGA_BowQuickCombo::SpawnQuickArrowProjectile(
	const FVector& LaunchDirection,
	const FVector& MuzzleOrigin,
	float Damage,
	float PoiseDamage,
	float Speed,
	float GravityScale)
{
	UWorld* World = GetWorld();
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!World || !Avatar || !ArrowClass) { return; }

	const FTransform SpawnTransform(LaunchDirection.Rotation(), MuzzleOrigin);
	AThreadTalesProjectile_Arrow* Arrow = World->SpawnActorDeferred<AThreadTalesProjectile_Arrow>(
		ArrowClass, SpawnTransform, Avatar, Cast<APawn>(Avatar), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (Arrow)
	{
		const FVector NormalizedDir = LaunchDirection.GetSafeNormal();
		Arrow->InitializeArrow(1.0f, Damage, PoiseDamage, Avatar, NormalizedDir);
		Arrow->BaseDamage = Damage;
		Arrow->PoiseDamage = PoiseDamage;

		if (UProjectileMovementComponent* ProjMove = Arrow->ProjectileMovement)
		{
			ProjMove->InitialSpeed = Speed;
			ProjMove->MaxSpeed = Speed;
			ProjMove->bInitialVelocityInLocalSpace = false;
			ProjMove->Velocity = NormalizedDir * Speed;
			ProjMove->ProjectileGravityScale = GravityScale;
		}

		UGameplayStatics::FinishSpawningActor(Arrow, SpawnTransform);
		if (ACharacter* Char = Cast<ACharacter>(Avatar)) { Char->MoveIgnoreActorAdd(Arrow); }

		UE_LOG(LogThreadTalesCombat, Display, TEXT("[BowQuickCombo] Fired arrow from '%s' | Step: %d | Speed: %.0f | Dmg: %.1f"),
			*Avatar->GetName(), CurrentComboIndex, Speed, Damage);
	}
}

void UThreadTalesGA_BowQuickCombo::AlignFacingToTargetOrCamera()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character) { return; }

	if (const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(Character))
	{
		if (const UThreadTalesTargetLockComponent* TargetLock = TTChar->GetTargetLockComponent())
		{
			if (TargetLock->IsTargetLocked())
			{
				FVector ToTarget = (TargetLock->GetTargetLocation() - Character->GetActorLocation()).GetSafeNormal2D();
				if (!ToTarget.IsNearlyZero())
				{
					Character->SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
					return;
				}
			}
		}
	}

	if (const AController* Controller = Character->GetController())
	{
		Character->SetActorRotation(FRotator(0.0f, Controller->GetControlRotation().Yaw, 0.0f));
	}
}

int32 UThreadTalesGA_BowQuickCombo::DetermineDodgeDirectionIndex(const ACharacter* Character) const
{
	if (!Character) { return 1; }
	const AController* Controller = Character->GetController();
	const FRotator CameraRot = Controller ? Controller->GetControlRotation() : Character->GetActorRotation();
	FVector InputVector = Character->GetLastMovementInputVector();
	if (InputVector.IsNearlyZero() && Character->GetCharacterMovement())
	{
		InputVector = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal();
	}
	if (InputVector.IsNearlyZero()) { return 1; }

	const float Angle = UKismetMathLibrary::NormalizedDeltaRotator(InputVector.Rotation(), CameraRot).Yaw;
	if (Angle >= -45.0f && Angle <= 45.0f) { return 0; }
	if (Angle < -45.0f && Angle >= -135.0f) { return 2; }
	if (Angle > 45.0f && Angle <= 135.0f) { return 3; }
	return 1;
}

AThreadTalesWeapon_Bow* UThreadTalesGA_BowQuickCombo::GetEquippedBow() const
{
	if (const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (const UThreadTalesEquipmentComponent* EquipComp = TTChar->GetEquipmentComponent())
		{
			return Cast<AThreadTalesWeapon_Bow>(EquipComp->GetCurrentWeapon());
		}
	}
	return nullptr;
}

const UThreadTalesBowConfig* UThreadTalesGA_BowQuickCombo::GetBowConfig() const
{
	if (const AThreadTalesWeapon_Bow* Bow = CachedBowWeapon.IsValid() ? CachedBowWeapon.Get() : GetEquippedBow())
	{
		return Bow->GetBowConfig();
	}
	return nullptr;
}

void UThreadTalesGA_BowQuickCombo::OnMontageCompleted()
{
	if (!bHasFiredCurrentShot) { NotifyQuickShotFire(); }
	UE_LOG(LogThreadTalesCombat, Display, TEXT("[BowQuickCombo] OnMontageCompleted (Step: %d, Dodge: %d, ParryCounter: %d, Queued: %d)"),
		CurrentComboIndex + 1, bIsDodgeShot ? 1 : 0, bIsParryCounterShot ? 1 : 0, bQueueNextCombo ? 1 : 0);

	if (bQueueNextCombo && !bIsDodgeShot && !bIsParryCounterShot && CurrentComboIndex >= 0 && CurrentComboIndex < 3)
	{
		const int32 NextStep = CurrentComboIndex + 1;
		PlayComboStep(NextStep);
		return;
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UThreadTalesGA_BowQuickCombo::OnMontageBlendOut()
{
	if (!bHasFiredCurrentShot) { NotifyQuickShotFire(); }
	if (bQueueNextCombo && !bIsDodgeShot && !bIsParryCounterShot && CurrentComboIndex >= 0 && CurrentComboIndex < 3)
	{
		const int32 NextStep = CurrentComboIndex + 1;
		bQueueNextCombo = false;
		PlayComboStep(NextStep);
		return;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}


void UThreadTalesGA_BowQuickCombo::OnMontageSafetyTimeout()
{
	if (IsActive()) { OnMontageCompleted(); }
}

void UThreadTalesGA_BowQuickCombo::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UThreadTalesGA_BowQuickCombo::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UThreadTalesGA_BowQuickCombo::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TM = World->GetTimerManager();
		TM.ClearTimer(FireTimerHandle);
		TM.ClearTimer(SafetyTimerHandle);
		TM.ClearTimer(ComboWindowOpenTimerHandle);
		TM.ClearTimer(ComboWindowCloseTimerHandle);
		TM.ClearTimer(EarlyRecoveryTimerHandle);
	}
	bCanEarlyLocomotionCancel = false;

	if (MontageTask)
	{
		MontageTask->OnCompleted.Clear();
		MontageTask->OnBlendOut.Clear();
		MontageTask->OnInterrupted.Clear();
		MontageTask->OnCancelled.Clear();
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		TTChar->ApplyStaminaRegenDelay(TTChar->GetStamina() <= 0.0f);
	}

	CurrentPlayingMontage = nullptr;
	bQueueNextCombo = false;
	bComboWindowOpen = false;
	bIsDodgeShot = false;
	bIsParryCounterShot = false;
	bHasFiredCurrentShot = false;
	CurrentComboIndex = 0;
	const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo());
	const UThreadTalesTargetLockComponent* TargetLock = TTChar ? TTChar->GetTargetLockComponent() : nullptr;
	SetAvatarStrafeInput(TargetLock && TargetLock->IsTargetLocked());
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UThreadTalesGA_BowQuickCombo::SetAvatarStrafeInput(bool bStrafing) const
{
	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo()))
	{
		TTChar->SetStrafeInputDown(bStrafing);
	}
}
