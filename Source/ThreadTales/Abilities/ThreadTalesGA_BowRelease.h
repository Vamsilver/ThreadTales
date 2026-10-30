// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/ThreadTalesGameplayAbility.h"
#include "ThreadTalesGA_BowRelease.generated.h"

class AThreadTalesWeapon_Bow;
class AThreadTalesProjectile_Arrow;
class UThreadTalesGA_BowDraw;
class UThreadTalesBowConfig;
class UAnimMontage;
class USoundBase;
class UCameraShakeBase;

/**
 * Arrow Release & Fire Gameplay Ability for Archer Archetype.
 * Triggered on Primary Attack input (LMB) while Bow is equipped and aiming.
 * Performs:
 * - 3-Phase UpperBody draw (AM_Bow_Draw_Hold Start -> Loop on LMB hold, NockedArrowMesh visible).
 * - 3-Tier power charge evaluation (LV1/LV2/LV3 thresholds, speed, gravity, damage, poise via UThreadTalesBowConfig).
 * - Spawning ballistic AThreadTalesProjectile_Arrow scaled by charge tier.
 * - Bowstring snap audio, tier-specific release recoil montage (AM_Bow_Release_LV1/LV2/LV3), and camera shake.
 * - String tension reset on bow and rapid follow-up shot transition on active BowDraw.
 */
UCLASS()
class THREADTALES_API UThreadTalesGA_BowRelease : public UThreadTalesGameplayAbility
{
	GENERATED_BODY()

public:
	UThreadTalesGA_BowRelease();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Projectile arrow asset to spawn upon release */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Combat", meta = (ToolTip = "Projectile arrow asset to spawn upon release"))
	TSubclassOf<AThreadTalesProjectile_Arrow> ArrowClass;

	/** Sound played when arrow is released (bowstring snap) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Audio", meta = (ToolTip = "Sound played upon bowstring release / snap"))
	TObjectPtr<USoundBase> ReleaseSound;

	/** Optional audio played when draw is released below minimum threshold (dry release / abort) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Audio", meta = (ToolTip = "Sound played upon aborted release below threshold"))
	TObjectPtr<USoundBase> DryReleaseSound;

	/** Optional upper body release / follow-through montage fallback */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Montages", meta = (ToolTip = "Optional upper body release / follow-through montage fallback"))
	TObjectPtr<UAnimMontage> FireMontage;

	/** Optional camera shake played upon firing */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Camera", meta = (ToolTip = "Optional camera shake played upon firing"))
	TSubclassOf<UCameraShakeBase> FireCameraShake;

	/** Base health damage at full draw tension (1.0). Scaled by draw ratio if BowConfig is absent. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Balancing", meta = (ClampMin = "1.0", ToolTip = "Base health damage at full tension (1.0)"))
	float BaseDamage = 50.0f;

	/** Base poise/posture damage at full draw tension (1.0). Contributes to Sekiro posture break. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Balancing", meta = (ClampMin = "0.0", ToolTip = "Base poise damage at full tension (1.0)"))
	float PoiseDamage = 25.0f;

	/** Minimum draw tension ratio (0.0 to 1.0) required to release an arrow. Releases below this threshold cancel the shot to prevent blaster-like spamming. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Balancing", meta = (ClampMin = "0.05", ClampMax = "0.5", ToolTip = "Minimum draw tension ratio (0.0 to 1.0) required to release an arrow."))
	float MinReleaseThreshold = 0.10f;

	/** Minimum time interval in seconds between consecutive shots to prevent blaster-like rapid click spamming */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Balancing", meta = (ClampMin = "0.1", ToolTip = "Minimum time interval in seconds between consecutive shots"))
	float RefireCooldown = 0.35f;

	/** Continuous stamina drained per second while holding LMB tension */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Balancing", meta = (ClampMin = "0.0", ToolTip = "Continuous stamina drain per second while holding LMB tension"))
	float HoldStaminaDrainRate = 12.0f;

	/** Initial stamina cost deducted when initiating bow draw via LMB */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Balancing", meta = (ClampMin = "0.0", ToolTip = "Stamina consumed upon initiating bow draw via LMB"))
	float InitialStaminaCost = 10.0f;

	/** Stamina cost deducted upon releasing an arrow (scaled with draw tension) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Balancing", meta = (ClampMin = "0.0", ToolTip = "Base stamina cost deducted upon arrow release"))
	float ReleaseStaminaCost = 10.0f;

	/** Time in seconds of holding LMB required to reach full bowstring tension (1.0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Balancing", meta = (ClampMin = "0.1", ToolTip = "Time in seconds to reach full draw power"))
	float MaxChargeDuration = 0.90f;

	/** Forward spawn distance offset along launch vector (inverted to pull back along launch vector) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Combat", meta = (ToolTip = "Spawn distance offset along launch vector"))
	float ArrowSpawnForwardOffset = -75.0f;

	/** If true, resets active BowDraw tension to 0.0 for immediate follow-up shot. If false, ends aim. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Balancing", meta = (ToolTip = "If true, resets active BowDraw tension to 0.0 for follow-up shot. If false, ends aim."))
	bool bResetDrawTensionOnFire = true;

	/** If true, completely ends GA_BowDraw and drops aim mode upon release */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Balancing", meta = (ToolTip = "If true, completely cancels GA_BowDraw upon firing"))
	bool bEndAimOnRelease = false;

	/** Returns the currently evaluated charge tier (1 = LV1, 2 = LV2, 3 = LV3) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Bow|State", meta = (ToolTip = "Returns active charge tier (1 = LV1, 2 = LV2, 3 = LV3)"))
	int32 GetCurrentChargeTier() const { return CurrentChargeTier; }

protected:
	/** Timer callback updating bowstring tension deformation and charge tier while holding LMB */
	void OnChargeTick();

	/** Spawns ballistic arrow and executes cosmetic and combat effects */
	void ExecuteFire(float InDrawRatio, float InHoldDurationSeconds);

	/** Resolves camera view and aligns character orientation towards camera look direction */
	void ResolveCameraAndFacing(AActor* AvatarActor, APlayerController* PC, FVector& OutCameraLoc, FRotator& OutCameraRot) const;

	/** Resolves origin location for projectile arrow spawning safely clear of character mesh */
	FVector ResolveMuzzleOrigin(const AThreadTalesWeapon_Bow* Bow, const AActor* AvatarActor) const;

	/** Calculates ballistic launch direction and rotation accounting for Target Lock or optical crosshair convergence */
	FVector CalculateLaunchDirection(const AActor* AvatarActor, const AThreadTalesWeapon_Bow* Bow, const FVector& MuzzleOrigin, const FVector& CameraLoc, const FRotator& CameraRot) const;

	/** Plays tier release montage, audio, camera shake, and handles arrow re-nocking */
	void PlayFireCosmetics(AActor* AvatarActor, APlayerController* PC, const FVector& MuzzleOrigin, int32 ChargeTier, const UThreadTalesBowConfig* BowConfig);

	/** Stops the active DrawHoldMontage if currently playing on the avatar character */
	void StopDrawHoldMontage(AActor* AvatarActor);

	/** Helper spawning and initializing projectile arrow actor with 3-tier charge parameters */
	void SpawnArrowProjectile(AActor* AvatarActor, const FRotator& LaunchRotation, const FVector& MuzzleOrigin, float InDrawRatio, int32 ChargeTier, const UThreadTalesBowConfig* BowConfig);

	/** Resolves the equipped bow weapon from avatar actor */
	AThreadTalesWeapon_Bow* GetEquippedBow() const;

	/** Finds the active UThreadTalesGA_BowDraw instance on avatar ASC if currently drawing */
	UThreadTalesGA_BowDraw* GetActiveBowDrawAbility() const;

	/** Timer handle driving tension progression */
	FTimerHandle ChargeTimerHandle;

	/** Cached pointer to equipped bow */
	TWeakObjectPtr<AThreadTalesWeapon_Bow> CachedBowWeapon;

	/** Cached pointer to active DrawHoldMontage played on LMB hold */
	TWeakObjectPtr<UAnimMontage> ActiveDrawHoldMontage;

	/** Timestamp when LMB draw charging started */
	float ChargeStartTime = 0.0f;

	/** Timestamp of last arrow fire or abort for anti-spam refire cooldown */
	float LastFireTimestamp = -100.0f;

	/** Current normalized draw tension (0.0 .. 1.0) */
	float CurrentChargeRatio = 0.0f;

	/** Current evaluated charge tier (1 = LV1, 2 = LV2, 3 = LV3) */
	int32 CurrentChargeTier = 1;
};
