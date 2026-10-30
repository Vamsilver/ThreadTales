// Copyright Epic Games, Inc. All Rights Reserved.
#include "Abilities/ThreadTalesGA_BowRelease.h"
#include "Abilities/ThreadTalesGA_BowDraw.h"
#include "Data/ThreadTalesBowConfig.h"
#include "ThreadTales.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Combat/ThreadTalesWeapon_Bow.h"
#include "Combat/ThreadTalesProjectile_Arrow.h"
#include "Combat/ThreadTalesTargetLockComponent.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
#include "Characters/ThreadTalesStaminaComponent.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraShakeBase.h"
#include "Animation/AnimMontage.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "TimerManager.h"
UThreadTalesGA_BowRelease::UThreadTalesGA_BowRelease()
{
	AbilityName = TEXT("Bow Release & Fire");
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	FGameplayTagContainer AssetTagsContainer;
	AssetTagsContainer.AddTag(ThreadTalesGameplayTags::TAG_Ability_Ranged_Fire);
	SetAssetTags(AssetTagsContainer);
	ActivationRequiredTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Aiming);
	ActivationBlockedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered);
	ActivationBlockedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_PostureBroken);
	ActivationBlockedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Evading);
	ActivationBlockedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_HitReact);
	ActivationOwnedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_StaminaRegenBlocked);
	ArrowClass = AThreadTalesProjectile_Arrow::StaticClass();
}
bool UThreadTalesGA_BowRelease::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) { return false; }
	if (const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		if (ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_ParryCounterWindow)) { return false; }
	}
	if (const UWorld* World = GetWorld())
	{
		if (World->GetTimeSeconds() - LastFireTimestamp < RefireCooldown) { return false; }
	}
	const UThreadTalesGA_BowDraw* ActiveDraw = GetActiveBowDrawAbility();
	if (!ActiveDraw || !ActiveDraw->IsActive()) { return false; }
	const AThreadTalesWeapon_Bow* Bow = GetEquippedBow();
	return (Bow && Bow->IsDrawn());
}
void UThreadTalesGA_BowRelease::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	AThreadTalesWeapon_Bow* Bow = GetEquippedBow();
	UThreadTalesGA_BowDraw* ActiveDraw = GetActiveBowDrawAbility();
	if (!AvatarActor || !Bow || !ActiveDraw || !ActiveDraw->IsActive() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	CachedBowWeapon = Bow;
	ChargeStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	CurrentChargeRatio = 0.0f;
	CurrentChargeTier = 1;
	const UThreadTalesBowConfig* BowConfig = Bow->GetBowConfig();
	const float EffectiveMaxChargeDuration = BowConfig ? BowConfig->LV3ChargeTimeSeconds : MaxChargeDuration;
	if (ActiveDraw)
	{
		ActiveDraw->StopAimReadyMontage();
	}
	Bow->AttachNockedArrowToString();
	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(AvatarActor))
	{
		TTChar->SetAimInputDown(true);
	}
	constexpr float HandDrawTensionDuration = 0.42f;
	Bow->StartDrawingTension(HandDrawTensionDuration);
	UAnimMontage* DrawHoldMontageToPlay = BowConfig ? BowConfig->GetDrawHoldMontage() : nullptr;
	if (!DrawHoldMontageToPlay)
	{
		DrawHoldMontageToPlay = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Draw_Hold.AM_Bow_Draw_Hold"));
	}
	if (DrawHoldMontageToPlay)
	{
		ActiveDrawHoldMontage = DrawHoldMontageToPlay;
		if (ACharacter* Character = Cast<ACharacter>(AvatarActor))
		{
			Character->PlayAnimMontage(DrawHoldMontageToPlay, 1.0f);
		}
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ChargeTimerHandle, this, &UThreadTalesGA_BowRelease::OnChargeTick, 0.02f, true);
	}
	OnChargeTick();
	UE_LOG(LogThreadTalesCombat, Display, TEXT("[BowRelease] Began 3-phase draw on '%s' with bow '%s' (MaxDuration=%.2fs)"),
		*AvatarActor->GetName(), *Bow->GetName(), EffectiveMaxChargeDuration);
}
void UThreadTalesGA_BowRelease::OnChargeTick()
{
	AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo());
	AThreadTalesWeapon_Bow* Bow = CachedBowWeapon.Get();
	if (!TTChar || !Bow)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	CurrentChargeRatio = Bow->GetDrawRatio();
	const float HoldElapsed = GetWorld() ? (GetWorld()->GetTimeSeconds() - ChargeStartTime) : Bow->GetDrawElapsed();
	if (const UThreadTalesBowConfig* BowConfig = Bow->GetBowConfig())
	{
		const int32 NewTier = BowConfig->EvaluateChargeTier(HoldElapsed);
		if (NewTier != CurrentChargeTier)
		{
			CurrentChargeTier = NewTier;
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[BowRelease] Charge tier transitioned to LV%d on '%s' (HoldElapsed=%.2fs, DrawRatio=%.2f)"),
				CurrentChargeTier, *TTChar->GetName(), HoldElapsed, CurrentChargeRatio);

			if (UAnimMontage* TierDrawMontage = BowConfig->GetDrawHoldMontageForTier(CurrentChargeTier))
			{
				if (TierDrawMontage != ActiveDrawHoldMontage.Get())
				{
					ActiveDrawHoldMontage = TierDrawMontage;
					TTChar->PlayAnimMontage(TierDrawMontage, 1.0f);
				}
			}
		}
	}
	if (HoldStaminaDrainRate > 0.0f)
	{
		if (UThreadTalesStaminaComponent* StamComp = TTChar->GetStaminaComponent())
		{
			const float ScaledDrainRate = HoldStaminaDrainRate * FMath::Lerp(0.5f, 1.0f, CurrentChargeRatio);
			if (!StamComp->TryConsumeStamina(ScaledDrainRate * 0.02f, false))
			{
				StopDrawHoldMontage(TTChar);
				Bow->SnapDrawTensionToRest();
				Bow->SetNockedArrowVisible(false);
				Bow->ClearTrajectoryArc();
				LastFireTimestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
			}
		}
	}
	Bow->UpdateTrajectoryArc(TTChar, CurrentChargeRatio, CurrentChargeTier);
}
void UThreadTalesGA_BowRelease::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(ChargeTimerHandle); }
	AThreadTalesWeapon_Bow* Bow = CachedBowWeapon.Get();
	if (Bow) { Bow->ClearTrajectoryArc(); }
	const UThreadTalesBowConfig* BowConfig = Bow ? Bow->GetBowConfig() : nullptr;
	const float EffectiveMaxDuration = BowConfig ? BowConfig->LV3ChargeTimeSeconds : MaxChargeDuration;
	const float EffectiveMinThreshold = BowConfig ? BowConfig->MinReleaseThreshold : MinReleaseThreshold;
	const float HoldDuration = GetWorld()
		? FMath::Max(0.0f, GetWorld()->GetTimeSeconds() - ChargeStartTime)
		: (Bow ? Bow->GetDrawElapsed() : 0.0f);
	const float CurrentRatio = Bow
		? Bow->GetDrawRatio()
		: FMath::Clamp(HoldDuration / FMath::Max(EffectiveMaxDuration, 0.05f), 0.0f, 1.0f);
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (CurrentRatio < EffectiveMinThreshold)
	{
		if (DryReleaseSound && AvatarActor)
		{
			UGameplayStatics::PlaySoundAtLocation(this, DryReleaseSound, AvatarActor->GetActorLocation());
		}
		StopDrawHoldMontage(AvatarActor);
		if (Bow)
		{
			Bow->SnapDrawTensionToRest();
			Bow->AttachNockedArrowToString();
		}
		LastFireTimestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		if (bEndAimOnRelease)
		{
			if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(AvatarActor)) { TTChar->SetAimInputDown(false); }
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	if (bEndAimOnRelease)
	{
		if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(AvatarActor)) { TTChar->SetAimInputDown(false); }
	}
	LastFireTimestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	ExecuteFire(CurrentRatio, HoldDuration);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
void UThreadTalesGA_BowRelease::ResolveCameraAndFacing(AActor* AvatarActor, APlayerController* PC, FVector& OutCameraLoc, FRotator& OutCameraRot) const
{
	OutCameraLoc = AvatarActor->GetActorLocation();
	OutCameraRot = PC ? PC->GetControlRotation() : AvatarActor->GetActorRotation();
	if (PC && PC->PlayerCameraManager)
	{
		OutCameraLoc = PC->PlayerCameraManager->GetCameraLocation();
		OutCameraRot = PC->PlayerCameraManager->GetCameraRotation();
	}
	else if (!PC)
	{
		AvatarActor->GetActorEyesViewPoint(OutCameraLoc, OutCameraRot);
	}
}
FVector UThreadTalesGA_BowRelease::ResolveMuzzleOrigin(const AThreadTalesWeapon_Bow* Bow, const AActor* AvatarActor) const
{
	FVector MuzzleOrigin = Bow ? Bow->GetTrajectoryStartLocation() : FVector::ZeroVector;
	if (const ACharacter* Char = Cast<ACharacter>(AvatarActor))
	{
		if (MuzzleOrigin.IsZero() || FVector::DistSquared(MuzzleOrigin, Char->GetActorLocation()) > 200.0f * 200.0f)
		{
			if (const USkeletalMeshComponent* Mesh = Char->GetMesh())
			{
				if (Mesh->DoesSocketExist(FName("overlay_bow")))
				{
					MuzzleOrigin = Mesh->GetBoneLocation(FName("hand_l"));
				}
				else if (Mesh->DoesSocketExist(FName("hand_rSocket")))
				{
					MuzzleOrigin = Mesh->GetBoneLocation(FName("hand_r"));
				}
			}
		}
	}
	return MuzzleOrigin;
}

FVector UThreadTalesGA_BowRelease::CalculateLaunchDirection(const AActor* AvatarActor, const AThreadTalesWeapon_Bow* Bow, const FVector& MuzzleOrigin, const FVector& CameraLoc, const FRotator& CameraRot) const
{
	if (Bow)
	{
		FVector TargetLocation;
		FVector LaunchDir;
		if (Bow->GetAimConvergenceTargetAndDirection(AvatarActor, MuzzleOrigin, TargetLocation, LaunchDir))
		{
			return LaunchDir;
		}
	}
	return CameraRot.Vector();
}
void UThreadTalesGA_BowRelease::ExecuteFire(float InDrawRatio, float InHoldDurationSeconds)
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	AThreadTalesWeapon_Bow* Bow = CachedBowWeapon.Get();
	if (!AvatarActor || !Bow) { return; }
	Bow->ClearTrajectoryArc();
	const UThreadTalesBowConfig* BowConfig = Bow->GetBowConfig();
	const int32 ChargeTier = BowConfig ? BowConfig->EvaluateChargeTier(InHoldDurationSeconds) : 1;
	CurrentChargeTier = ChargeTier;
	APlayerController* PC = Cast<APlayerController>(CurrentActorInfo ? CurrentActorInfo->PlayerController.Get() : nullptr);
	FVector CameraLoc;
	FRotator CameraRot;
	ResolveCameraAndFacing(AvatarActor, PC, CameraLoc, CameraRot);
	FVector MuzzleOrigin = ResolveMuzzleOrigin(Bow, AvatarActor);
	Bow->SetNockedArrowVisible(false);
	const FVector LaunchDirection = CalculateLaunchDirection(AvatarActor, Bow, MuzzleOrigin, CameraLoc, CameraRot);
	MuzzleOrigin += LaunchDirection * ArrowSpawnForwardOffset;
	const FRotator LaunchRotation = LaunchDirection.Rotation();
	SpawnArrowProjectile(AvatarActor, LaunchRotation, MuzzleOrigin, InDrawRatio, ChargeTier, BowConfig);
	PlayFireCosmetics(AvatarActor, PC, MuzzleOrigin, ChargeTier, BowConfig);
	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(AvatarActor))
	{
		if (UThreadTalesStaminaComponent* StamComp = TTChar->GetStaminaComponent())
		{
			StamComp->TryConsumeStamina(ReleaseStaminaCost * FMath::Lerp(0.5f, 1.0f, InDrawRatio), true);
		}
	}
	const float TierSpeed = BowConfig ? BowConfig->GetSpeedForTier(ChargeTier) : 3500.0f;
	const float TierDmg = BowConfig ? BowConfig->GetHealthDamageForTier(ChargeTier) : BaseDamage;
	const float TierPoise = BowConfig ? BowConfig->GetPoiseDamageForTier(ChargeTier) : PoiseDamage;
	UE_LOG(LogThreadTalesCombat, Display, TEXT("[BowRelease] Fired LV%d arrow from '%s' | Hold: %.2fs | Ratio: %.2f | Speed: %.0f cm/s | Dmg: %.1f | Poise: %.1f | Muzzle: %s"),
		ChargeTier, *AvatarActor->GetName(), InHoldDurationSeconds, InDrawRatio, TierSpeed, TierDmg, TierPoise, *MuzzleOrigin.ToString());
}
void UThreadTalesGA_BowRelease::StopDrawHoldMontage(AActor* AvatarActor)
{
	if (ActiveDrawHoldMontage.IsValid())
	{
		if (ACharacter* Character = Cast<ACharacter>(AvatarActor))
		{
			Character->StopAnimMontage(ActiveDrawHoldMontage.Get());
		}
		ActiveDrawHoldMontage.Reset();
	}
}
void UThreadTalesGA_BowRelease::PlayFireCosmetics(AActor* AvatarActor, APlayerController* PC, const FVector& MuzzleOrigin, int32 ChargeTier, const UThreadTalesBowConfig* BowConfig)
{
	AThreadTalesWeapon_Bow* Bow = CachedBowWeapon.Get();
	UWorld* World = GetWorld();
	if (ReleaseSound && World)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ReleaseSound, MuzzleOrigin);
	}
	ActiveDrawHoldMontage.Reset();
	UAnimMontage* ReleaseMontageToPlay = BowConfig ? BowConfig->GetReleaseMontageForTier(ChargeTier) : FireMontage.Get();
	if (!ReleaseMontageToPlay)
	{
		ReleaseMontageToPlay = FireMontage.Get();
	}
	float RecoilDuration = 0.28f;
	if (ReleaseMontageToPlay)
	{
		if (ACharacter* Character = Cast<ACharacter>(AvatarActor))
		{
			const float PlayedLen = Character->PlayAnimMontage(ReleaseMontageToPlay);
			if (PlayedLen > 0.05f)
			{
				RecoilDuration = PlayedLen;
			}
		}
	}
	if (PC && FireCameraShake)
	{
		PC->ClientStartCameraShake(FireCameraShake);
	}
	if (Bow)
	{
		Bow->SnapDrawTensionToRest();
	}
	if (UThreadTalesGA_BowDraw* ActiveDraw = GetActiveBowDrawAbility())
	{
		if (bEndAimOnRelease)
		{
			ActiveDraw->CancelAimAndEnd();
		}
		else if (bResetDrawTensionOnFire && World)
		{
			ActiveDraw->ResetDrawTension();
			const float ReNockDelay = FMath::Max(RecoilDuration + 0.05f, 0.33f);
			FTimerHandle RenockTimer;
			World->GetTimerManager().SetTimer(RenockTimer, [WeakDraw = MakeWeakObjectPtr(ActiveDraw)]()
			{
				if (WeakDraw.IsValid() && WeakDraw->IsActive())
				{
					WeakDraw->ResumeAimReadyMontage();
				}
			}, ReNockDelay, false);
		}
	}
}
void UThreadTalesGA_BowRelease::SpawnArrowProjectile(AActor* AvatarActor, const FRotator& LaunchRotation, const FVector& MuzzleOrigin, float InDrawRatio, int32 ChargeTier, const UThreadTalesBowConfig* BowConfig)
{
	UWorld* World = GetWorld();
	if (!World || !ArrowClass) { return; }
	const FTransform ProjectileTransform(LaunchRotation, MuzzleOrigin);
	AThreadTalesProjectile_Arrow* Arrow = World->SpawnActorDeferred<AThreadTalesProjectile_Arrow>(
		ArrowClass, ProjectileTransform, AvatarActor, Cast<APawn>(AvatarActor),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Arrow)
	{
		const FVector NormalizedDir = LaunchRotation.Vector().GetSafeNormal();
		if (BowConfig)
		{
			const float TierSpeed = BowConfig->GetSpeedForTier(ChargeTier);
			const float TierGravity = BowConfig->GetGravityForTier(ChargeTier);
			const float TierDmg = BowConfig->GetHealthDamageForTier(ChargeTier);
			const float TierPoise = BowConfig->GetPoiseDamageForTier(ChargeTier);
			Arrow->InitializeArrow(1.0f, TierDmg, TierPoise, AvatarActor, NormalizedDir);
			Arrow->BaseDamage = TierDmg;
			Arrow->PoiseDamage = TierPoise;
			if (UProjectileMovementComponent* ProjMove = Arrow->ProjectileMovement)
			{
				ProjMove->InitialSpeed = TierSpeed;
				ProjMove->MaxSpeed = TierSpeed;
				ProjMove->bInitialVelocityInLocalSpace = false;
				ProjMove->Velocity = NormalizedDir * TierSpeed;
				ProjMove->ProjectileGravityScale = TierGravity;
			}
		}
		else
		{
			const float NormalizedRatio = FMath::Clamp(InDrawRatio, 0.0f, 1.0f);
			const float FullDrawMultiplier = (NormalizedRatio >= 0.98f) ? 1.15f : 1.0f;
			Arrow->InitializeArrow(InDrawRatio, BaseDamage * FullDrawMultiplier, PoiseDamage * FullDrawMultiplier, AvatarActor, NormalizedDir);
		}
		if (ACharacter* ShooterChar = Cast<ACharacter>(AvatarActor))
		{
			ShooterChar->MoveIgnoreActorAdd(Arrow);
		}
		UGameplayStatics::FinishSpawningActor(Arrow, ProjectileTransform);
	}
}
void UThreadTalesGA_BowRelease::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeTimerHandle);
	}
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (bWasCancelled)
	{
		StopDrawHoldMontage(AvatarActor);
		if (CachedBowWeapon.IsValid())
		{
			CachedBowWeapon->SnapDrawTensionToRest();
			CachedBowWeapon->SetNockedArrowVisible(false);
			CachedBowWeapon->ClearTrajectoryArc();
		}
		if (!bEndAimOnRelease)
		{
			if (UThreadTalesGA_BowDraw* ActiveDraw = GetActiveBowDrawAbility())
			{
				ActiveDraw->ResumeAimReadyMontage();
			}
		}
	}
	CurrentChargeRatio = 0.0f;
	ChargeStartTime = 0.0f;
	CurrentChargeTier = 1;
	if (bEndAimOnRelease)
	{
		if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(AvatarActor))
		{
			TTChar->SetAimInputDown(false);
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
AThreadTalesWeapon_Bow* UThreadTalesGA_BowRelease::GetEquippedBow() const
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
UThreadTalesGA_BowDraw* UThreadTalesGA_BowRelease::GetActiveBowDrawAbility() const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.IsActive())
			{
				if (UThreadTalesGA_BowDraw* DrawAbility = Cast<UThreadTalesGA_BowDraw>(Spec.GetPrimaryInstance()))
				{
					return DrawAbility;
				}
			}
		}
	}
	return nullptr;
}