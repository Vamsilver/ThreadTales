// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/ThreadTalesBaseCharacter.h"
#include "AI/ThreadTalesEnemyConfig.h"
#include "ThreadTalesEnemyCharacter.generated.h"

class UStateTreeComponent;
class UThreadTalesEnemyConfig;
class AThreadTalesWeapon;
class UWidgetComponent;
class UThreadTalesEnemyOverheadWidget;
class USkeletalMeshComponent;
class USphereComponent;

/**
 * Base enemy character class driven by AI Controller and StateTree.
 * Inherits full GAS attributes, posture/poise economy, and combat reactions from AThreadTalesBaseCharacter.
 * Contains zero player-specific traversal, target lock, or input components.
 */
UCLASS()
class THREADTALES_API AThreadTalesEnemyCharacter : public AThreadTalesBaseCharacter
{
	GENERATED_BODY()

public:
	AThreadTalesEnemyCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	virtual void HandleHitReact(AActor* AttackerActor) override;
	virtual void HandleGuardHit(AActor* AttackerActor) override;
	virtual void HandleStaggered(AActor* AttackerActor) override;
	virtual void HandleParried(AActor* ParryingActor) override;
	virtual void HandlePostureBreak(AActor* ParryingActor, float StunDuration = -1.0f) override;
	virtual void Die(AActor* KillerActor) override;
	virtual void Landed(const FHitResult& Hit) override;

	virtual UAnimMontage* GetHitReactMontage() const override;
	virtual UAnimMontage* GetStaggerMontage() const override;
	virtual UAnimMontage* GetPostureBreakMontage() const override;
	virtual UAnimMontage* GetAirHitReactMontage() const override;
	virtual UAnimMontage* GetAirStaggerMontage() const override;
	virtual UAnimMontage* GetAirPostureBreakMontage() const override;
	virtual UAnimMontage* GetRecoverMontage() const override;
	virtual bool IsStaggered() const override;

	/** Activates or deactivates natural attack hitbox (for enemies without equipped weapons), optionally attaching to a specific limb socket/bone */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Enemy|Combat")
	virtual void SetHitboxActive(bool bActive, FName SocketOrBoneName = NAME_None);

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Enemy|Combat")
	USphereComponent* GetAttackHitbox() const { return AttackHitbox; }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Enemy|Combat")
	bool IsPendingAirKnockdown() const { return bPendingAirKnockdown; }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Enemy|Combat")
	bool IsPendingAirParryStagger() const { return bPendingAirParryStagger; }

	// Targetable Interface Callbacks
	virtual void OnTargetLocked_Implementation(AActor* TargetingActor) override;
	virtual void OnTargetUnlocked_Implementation(AActor* TargetingActor) override;

	/** Returns the enemy configuration data asset */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Enemy|Config")
	const UThreadTalesEnemyConfig* GetEnemyConfig() const { return EnemyConfig; }

	/** Returns the state tree component driving this enemy's logic */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Enemy|AI")
	UStateTreeComponent* GetStateTreeComponent() const { return StateTreeComponent; }

	/** Returns the overhead widget component */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Enemy|UI")
	UWidgetComponent* GetOverheadWidgetComponent() const { return OverheadWidgetComponent; }

	/** Returns the overhead widget instance if initialized */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Enemy|UI")
	UThreadTalesEnemyOverheadWidget* GetOverheadWidget() const;

	/** Returns the enemy difficulty settings from config or default fallback */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Enemy|Difficulty")
	const FThreadTalesEnemyDifficultySettings& GetDifficultySettings() const;

	/** Sets difficulty multiplier at runtime (scales tracking speed, evade reactivity, and combo chaining) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Enemy|Difficulty")
	void SetDifficultyMultiplier(float NewMultiplier);

	/** Returns the optional retarget follower mesh component */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Enemy|Mesh")
	USkeletalMeshComponent* GetRetargetMeshComponent() const { return RetargetMeshComponent; }

	/** Blueprint presentation event triggered when this enemy dies (ideal for spawning loot fountains) */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|Enemy|Combat", meta = (DisplayName = "OnDeath"))
	void ReceiveOnDeath(AActor* KillerActor);

protected:
	/** Optional follower mesh attached to primary Mesh for live retargeting (bugs/creatures) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Mesh", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> RetargetMeshComponent;

	/** StateTree AI Component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|StateTree", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStateTreeComponent> StateTreeComponent;

	/** Configuration data asset for this enemy archetype */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Config", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const UThreadTalesEnemyConfig> EnemyConfig;

	/** Overhead Screen-Space Widget Component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> OverheadWidgetComponent;

	/** Class of the overhead widget to spawn in OverheadWidgetComponent */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> OverheadWidgetClass;

	/** Relative vertical offset for the overhead widget above the capsule */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|UI", meta = (AllowPrivateAccess = "true"))
	FVector OverheadWidgetRelativeLocation;

	/** Runtime override of difficulty settings if modified via SetDifficultyMultiplier */
	UPROPERTY(Transient)
	FThreadTalesEnemyDifficultySettings RuntimeDifficultySettings;

	UPROPERTY(Transient)
	bool bHasCustomDifficulty = false;

	/** Natural Melee Attack Hitbox for creature / unweaponed strikes */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> AttackHitbox;

	UFUNCTION()
	void InitializeStateTreeAI();

	UFUNCTION()
	void OnAttackHitboxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Set of actors damaged during current attack swing to avoid multi-hitting */
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<AActor>> HitActorsThisStrike;

	/** True if enemy was knocked down / posture broken in mid-air and is falling toward the ground */
	UPROPERTY(Transient)
	bool bPendingAirKnockdown = false;

	/** True if enemy attack was parried in mid-air and is recoiling/falling toward the ground */
	UPROPERTY(Transient)
	bool bPendingAirParryStagger = false;

	/** Cached posture break stun duration to apply upon touching the floor */
	UPROPERTY(Transient)
	float CachedPostureBreakDuration = 0.0f;

	/** Cached parrying actor who caused the mid-air interrupt */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CachedParryingActor;

	/** If true, automatically draws weapon to hand on spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat", meta = (AllowPrivateAccess = "true"))
	bool bAutoDrawWeapon = true;

	/** If true, automatically activates and starts StateTree AI logic on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|StateTree", meta = (AllowPrivateAccess = "true"))
	bool bAutoStartAI = true;

	/** If true, filters out rotational root motion from animations so enemy orientation is purely code/steering driven */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat", meta = (AllowPrivateAccess = "true"))
	bool bIgnoreRootMotionRotation = false;

	/** Strips baked rotational root motion from animation sequences so enemy orientation is code/steering driven */
	FTransform FilterRootMotionPreConvert(const FTransform& LocalRootMotionTransform, class UCharacterMovementComponent* MoveComp, float DeltaSeconds);

private:
	void DisarmAndHaltAI();
	bool TryHandleAirCombatInterrupt(bool bIsKnockdown, AActor* ParryingActor, float StunDuration);
};
