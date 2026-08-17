// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesGA_RangedAttack.h"
#include "ThreadTales.h"
#include "ThreadTalesGameplayTags.h"
#include "Weapons/ThreadTalesWeapon_Slingshot.h"
#include "Character/ThreadTalesEquipmentComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

UThreadTalesGA_RangedAttack::UThreadTalesGA_RangedAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	AbilityName = TEXT("SlingshotAttack");

	FGameplayTagContainer Tags;
	Tags.AddTag(ThreadTalesGameplayTags::TAG_Ability_Ranged);
	Tags.AddTag(ThreadTalesGameplayTags::TAG_Ability_Ranged_Slingshot);
	SetAssetTags(Tags);

	ActivationOwnedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Aiming);

	bIsCharging = false;
	ChargeStartTime = 0.0f;
	ActiveSlingshot = nullptr;
	WaitInputReleaseTask = nullptr;
}

void UThreadTalesGA_RangedAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UThreadTalesEquipmentComponent* EquipComp = AvatarActor->FindComponentByClass<UThreadTalesEquipmentComponent>();
	if (!EquipComp)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AThreadTalesWeapon* CurrentWeapon = EquipComp->GetCurrentWeapon();
	ActiveSlingshot = Cast<AThreadTalesWeapon_Slingshot>(CurrentWeapon);

	if (!ActiveSlingshot)
	{
		UE_LOG(LogThreadTalesCombat, Warning, TEXT("[GA_RangedAttack] Slingshot not equipped!"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// If weapon is sheathed, draw it first
	if (ActiveSlingshot->IsSheathed())
	{
		EquipComp->DrawWeapon();
	}

	// Orient character towards camera/aim direction
	ACharacter* Character = Cast<ACharacter>(AvatarActor);
	if (Character && Character->GetController())
	{
		Character->SetActorRotation(FRotator(0.0f, Character->GetController()->GetControlRotation().Yaw, 0.0f));
	}

	ChargeStartTime = GetWorld()->GetTimeSeconds();
	bIsCharging = true;

	// Audio cue for pulling string
	if (ActiveSlingshot->PullSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ActiveSlingshot->PullSound, AvatarActor->GetActorLocation());
	}

	// Aim Montage
	if (ActiveSlingshot->AimMontage && Character)
	{
		Character->PlayAnimMontage(ActiveSlingshot->AimMontage);
	}

	// Setup GAS WaitInputRelease task to automatically trigger release when player lets go of input
	WaitInputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	if (WaitInputReleaseTask)
	{
		WaitInputReleaseTask->OnRelease.AddDynamic(this, &UThreadTalesGA_RangedAttack::OnInputReleasedCallback);
		WaitInputReleaseTask->ReadyForActivation();
	}

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[GA_RangedAttack] Slingshot Aim & Charge started!"));
}

void UThreadTalesGA_RangedAttack::OnInputReleasedCallback(float TimeHeld)
{
	ReleaseAndFire();
}

void UThreadTalesGA_RangedAttack::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	ReleaseAndFire();
}

void UThreadTalesGA_RangedAttack::ReleaseAndFire()
{
	if (!bIsCharging || !ActiveSlingshot)
	{
		return;
	}

	bIsCharging = false;

	UWorld* World = GetWorld();
	if (!World)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	float Elapsed = World->GetTimeSeconds() - ChargeStartTime;
	float ChargeAlpha = FMath::Clamp((Elapsed - ActiveSlingshot->MinChargeTime) / (ActiveSlingshot->MaxChargeTime - ActiveSlingshot->MinChargeTime), 0.0f, 1.0f);

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	APlayerController* PC = Cast<APlayerController>(CurrentActorInfo ? CurrentActorInfo->PlayerController.Get() : nullptr);

	FVector AimTargetPoint = FVector::ZeroVector;
	FVector SpawnLoc = ActiveSlingshot->ProjectileSpawnPoint ? ActiveSlingshot->ProjectileSpawnPoint->GetComponentLocation() : ActiveSlingshot->GetActorLocation();

	if (PC && PC->PlayerCameraManager)
	{
		FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
		FVector CamForward = PC->PlayerCameraManager->GetCameraRotation().Vector();
		FVector TraceEnd = CamLoc + (CamForward * 10000.0f);

		FHitResult HitResult;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(AvatarActor);
		QueryParams.AddIgnoredActor(ActiveSlingshot);

		if (World->LineTraceSingleByChannel(HitResult, CamLoc, TraceEnd, ECC_Visibility, QueryParams))
		{
			AimTargetPoint = HitResult.ImpactPoint;
		}
		else
		{
			AimTargetPoint = TraceEnd;
		}
	}
	else if (AvatarActor)
	{
		AimTargetPoint = SpawnLoc + (AvatarActor->GetActorForwardVector() * 5000.0f);
	}

	FRotator AimRotation = (AimTargetPoint - SpawnLoc).Rotation();

	// Spawn and fire the projectile
	ActiveSlingshot->FireProjectile(SpawnLoc, AimRotation, ChargeAlpha);

	// Play release / fire animation
	if (ActiveSlingshot->FireMontage && AvatarActor)
	{
		if (ACharacter* Character = Cast<ACharacter>(AvatarActor))
		{
			Character->PlayAnimMontage(ActiveSlingshot->FireMontage);
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

float UThreadTalesGA_RangedAttack::GetCurrentChargeRatio() const
{
	if (!bIsCharging || !ActiveSlingshot || !GetWorld()) return 0.0f;

	float Elapsed = GetWorld()->GetTimeSeconds() - ChargeStartTime;
	return FMath::Clamp((Elapsed - ActiveSlingshot->MinChargeTime) / (ActiveSlingshot->MaxChargeTime - ActiveSlingshot->MinChargeTime), 0.0f, 1.0f);
}

void UThreadTalesGA_RangedAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	bIsCharging = false;
	ActiveSlingshot = nullptr;

	if (WaitInputReleaseTask)
	{
		WaitInputReleaseTask->EndTask();
		WaitInputReleaseTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
