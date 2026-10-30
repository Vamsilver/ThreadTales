// Copyright Epic Games, Inc. All Rights Reserved.

#include "Abilities/ThreadTalesGA_BowDraw.h"
#include "ThreadTales.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Combat/ThreadTalesWeapon_Bow.h"
#include "Data/ThreadTalesBowConfig.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
#include "Characters/ThreadTalesStaminaComponent.h"
#include "Characters/ThreadTalesDummy.h"
#include "Abilities/ThreadTalesPostureConfig.h"
#include "Combat/ThreadTalesTargetLockComponent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

UThreadTalesGA_BowDraw::UThreadTalesGA_BowDraw()
{
	AbilityName = TEXT("Bow Draw & Aim");
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Tag configuration: Bow aim ability activates on Ranged or Block input
	FGameplayTagContainer AssetTagsContainer;
	AssetTagsContainer.AddTag(ThreadTalesGameplayTags::TAG_Ability_Ranged);
	AssetTagsContainer.AddTag(ThreadTalesGameplayTags::TAG_Ability_Block);
	SetAssetTags(AssetTagsContainer);

	// Grants aiming state and blocks passive stamina regeneration while held
	ActivationOwnedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Aiming);
	ActivationOwnedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_StaminaRegenBlocked);

	// Blocked during stuns, posture breaks, evades, or hit reacts
	ActivationBlockedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered);
	ActivationBlockedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_PostureBroken);
	ActivationBlockedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Evading);
	ActivationBlockedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_HitReact);
}

bool UThreadTalesGA_BowDraw::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) { return false; }

	const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!TTChar || !TTChar->CanAimOrBlock()) { return false; }

	const AThreadTalesWeapon_Bow* Bow = GetEquippedBow();
	if (!Bow || !Bow->IsDrawn()) { return false; }

	const UThreadTalesStaminaComponent* StamComp = TTChar->GetStaminaComponent();
	if (StamComp && !StamComp->CanSpendStamina()) { return false; }

	return true;
}

void UThreadTalesGA_BowDraw::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	AThreadTalesWeapon_Bow* Bow = GetEquippedBow();
	if (!TTChar || !Bow || !TTChar->CanAimOrBlock())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (UThreadTalesStaminaComponent* StamComp = TTChar->GetStaminaComponent())
	{
		if (!StamComp->CanSpendStamina())
		{
			StamComp->NotifyInsufficientStamina();
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		StamComp->TryConsumeStamina(InitialStaminaCost, true);
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CachedBowWeapon = Bow;
	SetAvatarAimInput(true);
	Bow->SnapDrawTensionToRest();
	Bow->ClearTrajectoryArc();

	if (bAutoDrawTension)
	{
		ResetDrawTension();
		Bow->StartDrawingTension(DrawDuration);
		if (UWorld* World = GetWorld()) { World->GetTimerManager().SetTimer(DrawTimerHandle, this, &UThreadTalesGA_BowDraw::OnDrawTick, 0.02f, true); }
	}

	if (DrawStartSound && Bow) { UGameplayStatics::PlaySoundAtLocation(this, DrawStartSound, Bow->GetActorLocation()); }

	InitiateParryWindowAndMontage(TTChar, Bow);
}

void UThreadTalesGA_BowDraw::InitiateParryWindowAndMontage(AThreadTalesCharacter* TTChar, AThreadTalesWeapon_Bow* Bow)
{
	if (!TTChar || !Bow) { return; }

	AActor* ThreatActor = nullptr;
	const bool bIncomingMeleeThreat = DetectIncomingMeleeThreat(ThreatActor);
	const UThreadTalesPostureConfig* PostureCfg = TTChar->GetStaminaComponent() ? TTChar->GetStaminaComponent()->GetPostureConfig() : nullptr;
	const float EffectiveParryWindow = PostureCfg ? PostureCfg->ParryWindowDuration : ParryWindowDuration;

	if (bIncomingMeleeThreat)
	{
		bParryActive = true;
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo()) { ASC->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Parrying); }
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ParryWindowTimerHandle);
			World->GetTimerManager().SetTimer(ParryWindowTimerHandle, this, &UThreadTalesGA_BowDraw::OnParryWindowExpired, EffectiveParryWindow, false);
		}
		TTChar->OnSuccessfulParry.RemoveDynamic(this, &UThreadTalesGA_BowDraw::HandleSuccessfulParry);
		TTChar->OnSuccessfulParry.AddDynamic(this, &UThreadTalesGA_BowDraw::HandleSuccessfulParry);
	}
	else { bParryActive = false; }

	const UThreadTalesBowConfig* BowConfig = Bow->GetBowConfig();
	UAnimMontage* MontageToPlay = nullptr;
	if (bIncomingMeleeThreat)
	{
		LastParryDirectionIndex = DetermineParryDirectionIndex(ThreatActor);
		MontageToPlay = BowConfig ? BowConfig->GetParryMontage(LastParryDirectionIndex) : nullptr;
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[BowDraw] Close-quarters melee threat '%s' detected! Executing Bow Parry deflect '%s' (Dir: %d, Window: %.2fs)"),
			ThreatActor ? *ThreatActor->GetName() : TEXT("Nearby"), *GetNameSafe(MontageToPlay), LastParryDirectionIndex, EffectiveParryWindow);
	}
	else if (!Bow->IsNockedArrowVisible())
	{
		MontageToPlay = BowConfig ? BowConfig->GetAimReadyMontage() : nullptr;
		if (!MontageToPlay) { MontageToPlay = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Aim_Ready.AM_Bow_Aim_Ready")); }
	}

	if (MontageToPlay)
	{
		ActiveAimMontage = MontageToPlay;
		const float ReadyPlayRate = BowConfig ? BowConfig->GetAimReadyPlayRate() : 0.42f;
		TTChar->PlayAnimMontage(MontageToPlay, ReadyPlayRate);

		if (!bIncomingMeleeThreat)
		{
			const float NockDelay = BowConfig ? BowConfig->GetNockArrowDelaySeconds() : 0.28f;
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(NockArrowTimerHandle);
				World->GetTimerManager().SetTimer(NockArrowTimerHandle, [WeakBow = MakeWeakObjectPtr(Bow)]()
				{
					if (WeakBow.IsValid()) { WeakBow->AttachNockedArrowToHand(); }
				}, NockDelay, false);
			}
			else { Bow->AttachNockedArrowToHand(); }
		}
	}

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[BowDraw] Aim stance active on '%s' with bow '%s' (Native GASPALS OverlayPose=%d, Montage: %s, Threat=%s)"),
		*TTChar->GetName(), *Bow->GetName(), TTChar->GetOverlayPose(), *GetNameSafe(MontageToPlay), bIncomingMeleeThreat ? TEXT("True") : TEXT("False"));
}

void UThreadTalesGA_BowDraw::OnDrawTick()
{
	// validate state if not active
	AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo());
	AThreadTalesWeapon_Bow* Bow = CachedBowWeapon.Get();
	if (!TTChar || !Bow || !TTChar->CanAimOrBlock())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const float DeltaTime = 0.02f;
	DrawElapsed += DeltaTime;
	CurrentDrawRatio = Bow->GetDrawRatio();

	if (HoldStaminaDrainRate > 0.0f)
	{
		if (UThreadTalesStaminaComponent* StamComp = TTChar->GetStaminaComponent())
		{
			const float ScaledDrainRate = HoldStaminaDrainRate * FMath::Lerp(0.5f, 1.0f, CurrentDrawRatio);
			if (!StamComp->TryConsumeStamina(ScaledDrainRate * DeltaTime, false))
			{
				UE_LOG(LogThreadTalesCombat, Display, TEXT("[BowDraw] Stamina exhausted during draw hold on '%s' — releasing aim"), *TTChar->GetName());
				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
			}
		}
	}
}

void UThreadTalesGA_BowDraw::ResetDrawTension()
{
	DrawElapsed = 0.0f;
	CurrentDrawRatio = 0.0f;

	if (AThreadTalesWeapon_Bow* Bow = CachedBowWeapon.Get())
	{
		Bow->SnapDrawTensionToRest();
		Bow->ClearTrajectoryArc();

		if (bAutoDrawTension && IsActive())
		{
			Bow->StartDrawingTension(DrawDuration);
		}
		else if (IsActive())
		{
			Bow->SetNockedArrowVisible(true);
		}
		else
		{
			Bow->SetNockedArrowVisible(false);
		}
	}
}

void UThreadTalesGA_BowDraw::CancelAimAndEnd()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UThreadTalesGA_BowDraw::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	UE_LOG(LogThreadTalesCombat, Display, TEXT("[BowDraw] Input released on '%s' — ending aim stance"), *GetNameSafe(GetAvatarActorFromActorInfo()));
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UThreadTalesGA_BowDraw::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DrawTimerHandle);
		World->GetTimerManager().ClearTimer(ResumeAimTimerHandle);
		World->GetTimerManager().ClearTimer(NockArrowTimerHandle);
	}

	SetAvatarAimInput(false);

	if (CachedBowWeapon.IsValid())
	{
		CachedBowWeapon->SetNockedArrowVisible(false);
		CachedBowWeapon->SnapDrawTensionToRest();
		CachedBowWeapon->ClearTrajectoryArc();
		CachedBowWeapon.Reset();
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayTagContainer ReleaseTags;
		ReleaseTags.AddTag(ThreadTalesGameplayTags::TAG_Ability_Ranged_Fire);
		ASC->CancelAbilities(&ReleaseTags);
	}

	if (ActiveAimMontage.IsValid())
	{
		if (ACharacter* Char = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr)) { Char->StopAnimMontage(ActiveAimMontage.Get()); }
		ActiveAimMontage.Reset();
	}

	if (bParryActive)
	{
		bParryActive = false;
		if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid() &&
			ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Parrying)) { ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Parrying); }
	}

	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr)) { TTChar->OnSuccessfulParry.RemoveDynamic(this, &UThreadTalesGA_BowDraw::HandleSuccessfulParry); }

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UThreadTalesGA_BowDraw::ResumeAimReadyMontage()
{
	if (!IsActive()) { return; }

	AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo());
	AThreadTalesWeapon_Bow* Bow = CachedBowWeapon.Get();
	if (!TTChar || !Bow) { return; }

	Bow->SnapDrawTensionToRest();

	if (Bow->IsNockedArrowVisible())
	{
		return;
	}

	const UThreadTalesBowConfig* BowCfg = Bow->GetBowConfig();
	UAnimMontage* AimReady = BowCfg ? BowCfg->GetAimReadyMontage() : nullptr;
	if (!AimReady) { AimReady = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Aim_Ready.AM_Bow_Aim_Ready")); }

	if (AimReady)
	{
		ActiveAimMontage = AimReady;
		const float ReadyPlayRate = BowCfg ? BowCfg->GetAimReadyPlayRate() : 0.42f;
		TTChar->PlayAnimMontage(AimReady, ReadyPlayRate);

		const float NockDelay = BowCfg ? BowCfg->GetNockArrowDelaySeconds() : 0.28f;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(NockArrowTimerHandle);
			World->GetTimerManager().SetTimer(NockArrowTimerHandle, [WeakBow = MakeWeakObjectPtr(Bow)]()
			{
				if (WeakBow.IsValid()) { WeakBow->AttachNockedArrowToHand(); }
			}, NockDelay, false);
		}
		else { Bow->AttachNockedArrowToHand(); }
	}
}

void UThreadTalesGA_BowDraw::StopAimReadyMontage()
{
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(NockArrowTimerHandle); }

	if (ActiveAimMontage.IsValid())
	{
		if (ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo())) { Char->StopAnimMontage(ActiveAimMontage.Get()); }
		ActiveAimMontage.Reset();
	}
}

void UThreadTalesGA_BowDraw::SetAvatarAimInput(bool bAiming) const
{
	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo())) { TTChar->SetAimInputDown(bAiming); }
}

void UThreadTalesGA_BowDraw::SetAvatarStrafeInput(bool bStrafing) const
{
	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo())) { TTChar->SetStrafeInputDown(bStrafing); }
}

AThreadTalesWeapon_Bow* UThreadTalesGA_BowDraw::GetEquippedBow() const
{
	if (const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (const UThreadTalesEquipmentComponent* EquipComp = TTChar->GetEquipmentComponent()) { return Cast<AThreadTalesWeapon_Bow>(EquipComp->GetCurrentWeapon()); }
	}
	return nullptr;
}

bool UThreadTalesGA_BowDraw::DetectIncomingMeleeThreat(AActor*& OutThreatActor) const
{
	const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo());
	if (!TTChar) { return false; }

	const UThreadTalesBowConfig* BowConfig = CachedBowWeapon.IsValid() ? CachedBowWeapon->GetBowConfig() : nullptr;
	const float BaseRadius = BowConfig ? BowConfig->BowParryThreatRadiusCm : BowParryThreatRadiusCm;
	const float ThreatRadius = FMath::Max(BaseRadius, 380.0f);

	if (const UThreadTalesTargetLockComponent* TargetLock = TTChar->GetTargetLockComponent())
	{
		if (TargetLock->IsTargetLocked())
		{
			if (AActor* LockedActor = TargetLock->GetCurrentTarget())
			{
				if (FVector::DistSquared(LockedActor->GetActorLocation(), TTChar->GetActorLocation()) <= FMath::Square(ThreatRadius) &&
					IsActorMeleeThreat(LockedActor, TTChar))
				{
					OutThreatActor = LockedActor;
					return true;
				}
			}
		}
	}

	UWorld* World = GetWorld();
	if (!World) { return false; }

	TArray<FOverlapResult> Overlaps;
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(ThreatRadius);
	FCollisionQueryParams QueryParams(TEXT("BowParryThreatDetection"), false, TTChar);
	World->OverlapMultiByChannel(Overlaps, TTChar->GetActorLocation(), FQuat::Identity, ECC_Pawn, SphereShape, QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (Candidate && Candidate != TTChar && IsActorMeleeThreat(Candidate, TTChar))
		{
			OutThreatActor = Candidate;
			return true;
		}
	}
	return false;
}

bool UThreadTalesGA_BowDraw::IsActorMeleeThreat(AActor* Candidate, const AThreadTalesCharacter* TTChar) const
{
	if (!Candidate || !TTChar || FVector::Dist2D(Candidate->GetActorLocation(), TTChar->GetActorLocation()) > 320.0f) { return false; }

	if (const AThreadTalesDummy* Dummy = Cast<AThreadTalesDummy>(Candidate)) { return (Dummy->IsInWindup() || Dummy->GetAttackState() != ECombatDummyAttackState::Idle); }

	if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Candidate))
	{
		if (const UAbilitySystemComponent* CandidateASC = ASI->GetAbilitySystemComponent())
		{
			return (CandidateASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Attacking) ||
			        CandidateASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Dummy_Windup) ||
			        CandidateASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Dummy_ParryWindow));
		}
	}
	return false;
}

int32 UThreadTalesGA_BowDraw::DetermineParryDirectionIndex(const AActor* ThreatActor) const
{
	if (!ThreatActor) { return 0; }
	if (const AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		const FVector ToThreat = ThreatActor->GetActorLocation() - Avatar->GetActorLocation();
		const float RightDot = FVector::DotProduct(ToThreat.GetSafeNormal2D(), Avatar->GetActorRightVector());
		return (RightDot < 0.0f) ? 0 : 1;
	}
	return 0;
}

void UThreadTalesGA_BowDraw::OnParryWindowExpired()
{
	if (bParryActive)
	{
		bParryActive = false;
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			if (ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Parrying)) { ASC->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Parrying); }
		}
	}
}

void UThreadTalesGA_BowDraw::HandleSuccessfulParry(AActor* AttackingActor)
{
	bParryCounterWindowActive = true;
	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo())) { TTChar->OpenParryCounterWindow(ParryCounterWindowDuration); }
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(ResumeAimTimerHandle); }
}

