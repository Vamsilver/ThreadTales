// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/ThreadTalesCharacter.h"
#include "ThreadTalesDummy.generated.h"

class UBoxComponent;
class UAnimMontage;
class UAnimSequenceBase;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UStateTreeComponent;

/**
 * State machine for Training Dummy attack phases
 */
UENUM(BlueprintType)
enum class ECombatDummyAttackState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Windup UMETA(DisplayName = "Windup (Telegraph)"),
	ParryWindow UMETA(DisplayName = "Parry Window"),
	Strike UMETA(DisplayName = "Strike (Active Hitbox)"),
	Recovery UMETA(DisplayName = "Recovery"),
	Staggered UMETA(DisplayName = "Staggered")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDummyAttackStateChanged, ECombatDummyAttackState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDummyWindupStarted, float, WindupDuration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDummyParryWindowOpened, float, ParryWindowDuration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDummyParryWindowClosed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDummyParried, AActor*, ParryingActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDummyAttackStrike, UAnimMontage*, StrikeMontage);

/**
 * Unified Training Dummy (BP_Dummy).
 * Operates as a passive punching bag by default (bEnableAttacks = false),
 * or as an active sparring partner with periodic attacks, windup telegraphing,
 * active hitbox, and calibrated parry window visualization (bEnableAttacks = true).
 */
UCLASS()
class THREADTALES_API AThreadTalesDummy : public AThreadTalesCharacter
{
	GENERATED_BODY()

public:
	AThreadTalesDummy();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ==========================================
	// HEALTH & RESPAWN
	// ==========================================

	/** If true, dummy cannot die (health is restored on hit) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Combat", meta = (DisplayName = "Is Immortal"))
	bool bIsImmortal;

	/** If true, automatically respawns and resets health after death */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Combat", meta = (DisplayName = "Auto Respawn"))
	bool bAutoRespawn;

	/** Delay in seconds before respawning after death */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Combat", meta = (DisplayName = "Respawn Delay", EditCondition = "bAutoRespawn", ClampMin = "1.0"))
	float RespawnDelay;

	/** Resets dummy back to initial spawn state and full health */
	UFUNCTION(BlueprintCallable, Category = "Dummy|Combat")
	virtual void Respawn();

	/** Alias for Respawn */
	UFUNCTION(BlueprintCallable, Category = "Dummy|Combat")
	void ResetDummy() { Respawn(); }

	// IThreadTalesTargetableInterface
	virtual bool CanBeTargeted_Implementation(const AActor* TargetingActor) const override;
	virtual FVector GetTargetLockLocation_Implementation(const AActor* TargetingActor) const override;

	// ==========================================
	// CONFIGURATION: ATTACK PARAMETERS
	// ==========================================

	/** If true, dummy executes periodic attacks against nearby players */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Attacks", meta = (DisplayName = "Enable Attacks"))
	bool bEnableAttacks;

	/** Base interval in seconds between attacks */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Attacks", meta = (ClampMin = "1.0", EditCondition = "bEnableAttacks"))
	float AttackInterval;

	/** Random deviation (+/-) in seconds applied to AttackInterval */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Attacks", meta = (ClampMin = "0.0", EditCondition = "bEnableAttacks"))
	float AttackIntervalVariance;

	/** Snappy attack delay in seconds after recovering from a parry stagger (instead of waiting full AttackInterval) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Attacks", meta = (ClampMin = "0.2", EditCondition = "bEnableAttacks", DisplayName = "Post Parry Attack Delay"))
	float PostParryAttackDelay;

	/** Maximum distance to player to initiate attack */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Attacks", meta = (ClampMin = "50.0", EditCondition = "bEnableAttacks"))
	float AttackRange;

	/** If true, attacks only when a player is within AttackRange. If false, swings automatically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Attacks", meta = (EditCondition = "bEnableAttacks"))
	bool bRequireTargetInRange;

	/** If true, turns smoothly to face the target player during the windup phase */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Attacks", meta = (EditCondition = "bEnableAttacks"))
	bool bFaceTargetDuringWindup;

	/** Rotation speed towards player during windup */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Attacks", meta = (EditCondition = "bFaceTargetDuringWindup"))
	float FaceTargetRotationSpeed;

	/** Optional dedicated windup telegraph montage (e.g. raised blade posture) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Attacks", meta = (EditCondition = "bEnableAttacks"))
	TObjectPtr<UAnimMontage> WindupMontage;

	/** Fallback windup sequence if WindupMontage is not set */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Attacks", meta = (EditCondition = "bEnableAttacks"))
	TObjectPtr<UAnimSequenceBase> WindupSequence;

	/** Attack montages to play during the strike phase */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Attacks", meta = (EditCondition = "bEnableAttacks"))
	TArray<TObjectPtr<UAnimMontage>> AttackMontages;

	/** If true, chooses randomly from AttackMontages. If false, sequences through them in order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Attacks", meta = (EditCondition = "bEnableAttacks"))
	bool bRandomizeAttacks;

	// ==========================================
	// CONFIGURATION: TIMING & PARRY WINDOW
	// ==========================================

	/** Total windup duration (telegraph phase) before the strike begins */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Timing", meta = (ClampMin = "0.2", EditCondition = "bEnableAttacks"))
	float WindupDuration;

	/** Delay after windup start when the parry window opens (must be < WindupDuration) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Timing", meta = (ClampMin = "0.0", EditCondition = "bEnableAttacks"))
	float ParryWindowStartTime;

	/** Duration of the active parry opportunity window */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Timing", meta = (ClampMin = "0.05", EditCondition = "bEnableAttacks"))
	float ParryWindowDuration;

	/** Duration of the active damage strike / hitbox phase */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Timing", meta = (ClampMin = "0.1", EditCondition = "bEnableAttacks"))
	float StrikeActiveDuration;

	/** Delay from strike montage start before hitbox becomes active (aligns with forward swing arc) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Timing", meta = (ClampMin = "0.0", EditCondition = "bEnableAttacks"))
	float StrikeHitboxDelay;

	/** Duration of recovery cooldown after strike before returning to Idle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Timing", meta = (ClampMin = "0.1", EditCondition = "bEnableAttacks"))
	float RecoveryDuration;

	/** Duration of stagger / deflection stun after dummy attack is parried */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Timing", meta = (ClampMin = "0.2"))
	float StaggerDuration;

	// ==========================================
	// CONFIGURATION: DAMAGE & HITBOX
	// ==========================================

	/** Base damage dealt to players on unblocked hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Damage", meta = (EditCondition = "bEnableAttacks"))
	float AttackDamage;

	/** Base poise damage dealt to players */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Damage", meta = (EditCondition = "bEnableAttacks"))
	float AttackPoiseDamage;

	/** Box hitbox used for attack collision */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy|Collision")
	TObjectPtr<UBoxComponent> AttackHitbox;

	/** If true, incoming player hits will interrupt windup attack if poise is damaged */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Poise")
	bool bInterruptOnHit;

	// ==========================================
	// VISUALIZATION & TELEMETRY
	// ==========================================

	/** If true, draws real-time in-world 3D debug visualizations for windup, parry window, and hitbox */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Debug")
	bool bShowDebugVisualization;

	/** Visual indicator mesh component (e.g. overhead sphere) displaying telegraph and parry state colors */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy|Visual")
	TObjectPtr<UStaticMeshComponent> ParryIndicatorMesh;

	/** Dynamic material instance used to drive indicator glow and colors */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> IndicatorMID;

	/** Emissive color during initial windup / charge telegraph */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Visual")
	FLinearColor WindupIndicatorColor;

	/** Emissive color during active parry opportunity window (flash) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Visual")
	FLinearColor ParryWindowIndicatorColor;

	/** Emissive color during active hitbox strike phase */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Visual")
	FLinearColor StrikeIndicatorColor;

	// ==========================================
	// STATETREE AI COMPONENT
	// ==========================================

	/** StateTree component driving modular AI combat states and transitions */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy|AI")
	TObjectPtr<UStateTreeComponent> StateTreeComponent;

	/** If true, uses StateTreeComponent instead of C++ timer handles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|AI", meta = (DisplayName = "Use StateTree AI"))
	bool bUseStateTree;

	/** Returns StateTreeComponent */
	UFUNCTION(BlueprintPure, Category = "Dummy|AI")
	UStateTreeComponent* GetStateTreeComponent() const { return StateTreeComponent; }

	// ==========================================
	// DELEGATES & BLUEPRINT EVENTS
	// ==========================================

	UPROPERTY(BlueprintAssignable, Category = "Dummy|Events")
	FOnDummyAttackStateChanged OnAttackStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Dummy|Events")
	FOnDummyWindupStarted OnWindupStarted;

	UPROPERTY(BlueprintAssignable, Category = "Dummy|Events")
	FOnDummyParryWindowOpened OnParryWindowOpened;

	UPROPERTY(BlueprintAssignable, Category = "Dummy|Events")
	FOnDummyParryWindowClosed OnParryWindowClosed;

	UPROPERTY(BlueprintAssignable, Category = "Dummy|Events")
	FOnDummyParried OnDummyParried;

	UPROPERTY(BlueprintAssignable, Category = "Dummy|Events")
	FOnDummyAttackStrike OnAttackStrike;

	// ==========================================
	// PUBLIC STATE ACCESSORS
	// ==========================================

	UFUNCTION(BlueprintPure, Category = "Dummy|State")
	ECombatDummyAttackState GetAttackState() const { return CurrentAttackState; }

	UFUNCTION(BlueprintPure, Category = "Dummy|State")
	bool IsParryWindowActive() const { return bParryWindowOpen; }

	UFUNCTION(BlueprintPure, Category = "Dummy|State")
	bool IsInWindup() const { return CurrentAttackState == ECombatDummyAttackState::Windup; }

	UFUNCTION(BlueprintCallable, Category = "Dummy|Combat")
	void TriggerAttackManual();

	/** Sets attack hitbox active/inactive (invoked synchronously by AnimNotifyState_WeaponHitbox) */
	UFUNCTION(BlueprintCallable, Category = "Dummy|Combat")
	void SetHitboxActive(bool bActive);

	/** Called when a player successfully parries the dummy's attack */
	virtual void HandleParried(AActor* ParryingActor) override;

	/** Called when a player breaks the dummy's posture, inflicting an extended stun */
	virtual void HandlePostureBreak(AActor* ParryingActor, float StunDuration = -1.0f) override;
	virtual void FinishPostureBreak() override;

	// Overrides from AThreadTalesCharacter
	virtual bool IsStaggered() const override;
	virtual bool CanDie() const override { return !bIsImmortal; }
	virtual void HandleHealthChanged(float NewHealth, float MaxHealthVal) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	virtual void HandleStaggered(AActor* AttackerActor) override;
	virtual void HandleHitReact(AActor* AttackerActor) override;
	virtual void Die(AActor* KillerActor) override;

	// ==========================================
	// STATETREE & BLUEPRINT CONTROL HOOKS
	// ==========================================

	UFUNCTION(BlueprintCallable, Category = "Dummy|StateTree")
	void SetAttackState(ECombatDummyAttackState NewState);

	UFUNCTION(BlueprintCallable, Category = "Dummy|StateTree")
	void ScheduleNextAttack();

	UFUNCTION(BlueprintCallable, Category = "Dummy|StateTree")
	void StartAttackSequence();

	UFUNCTION(BlueprintCallable, Category = "Dummy|StateTree")
	void StartWindup();

	UFUNCTION(BlueprintCallable, Category = "Dummy|StateTree")
	void OpenParryWindow();

	UFUNCTION(BlueprintCallable, Category = "Dummy|StateTree")
	void CloseParryWindow();

	UFUNCTION(BlueprintCallable, Category = "Dummy|StateTree")
	void ExecuteStrike();

	UFUNCTION(BlueprintCallable, Category = "Dummy|StateTree")
	void EndStrike();

	UFUNCTION(BlueprintCallable, Category = "Dummy|StateTree")
	void FinishRecovery();

	UFUNCTION(BlueprintCallable, Category = "Dummy|StateTree")
	void FinishParryRecovery();

	/** Begins parry stagger: plays StaggerMontage and sets state to Staggered */
	UFUNCTION(BlueprintCallable, Category = "Dummy|StateTree")
	void StartParryStagger();

	/** Begins extended Posture Break stun */
	UFUNCTION(BlueprintCallable, Category = "Dummy|StateTree")
	void StartPostureBreakStun();

	/** Finishes extended Posture Break stun and restores Poise */
	UFUNCTION(BlueprintCallable, Category = "Dummy|StateTree")
	void FinishPostureBreakStun();

	/** Returns true if a valid player character is alive and within AttackRange */
	UFUNCTION(BlueprintPure, Category = "Dummy|StateTree")
	bool IsTargetInAttackRange() const;

	/** Returns the clamped delay from windup start until the parry window opens */
	UFUNCTION(BlueprintPure, Category = "Dummy|StateTree")
	float GetParryWindowOpenDelay() const;

	/** Returns the duration the parry window remains open prior to strike impact */
	UFUNCTION(BlueprintPure, Category = "Dummy|StateTree")
	float GetParryWindowRemainingDuration() const;

	UFUNCTION(BlueprintPure, Category = "Dummy|StateTree")
	float GetWindupDuration() const { return WindupDuration; }

	UFUNCTION(BlueprintPure, Category = "Dummy|StateTree")
	float GetParryWindowDuration() const { return ParryWindowDuration; }

	UFUNCTION(BlueprintPure, Category = "Dummy|StateTree")
	float GetStrikeActiveDuration() const { return StrikeActiveDuration; }

	UFUNCTION(BlueprintPure, Category = "Dummy|StateTree")
	float GetRecoveryDuration() const { return RecoveryDuration; }

	UFUNCTION(BlueprintPure, Category = "Dummy|StateTree")
	float GetStaggerDuration() const { return GetParryRecoilDuration(); }

	UFUNCTION(BlueprintPure, Category = "Dummy|StateTree")
	float GetPostParryAttackDelay() const { return PostParryAttackDelay; }

	UFUNCTION(BlueprintPure, Category = "Dummy|StateTree")
	float GetAttackInterval() const { return AttackInterval; }

	/** Called by FStateTreeTask_DummyCombatAction::EnterState to confirm StateTree is actively executing tasks */
	void NotifyStateTreeActive() { bStateTreeEnteredFirstState = true; }

protected:

	UFUNCTION()
	void OnAttackHitboxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	AActor* FindTargetPlayer() const;
	void UpdateIndicatorVisuals();

private:
	ECombatDummyAttackState CurrentAttackState;
	bool bParryWindowOpen;
	bool bStateTreeEnteredFirstState = false;
	int32 CurrentAttackMontageIndex;

	FTransform InitialSpawnTransform;
	FTransform InitialMeshRelativeTransform;

	void InitializeCombatDefaults();
	void InitializeHitboxAndVisualIndicators();

	void EnableHitboxFallback();

	FTimerHandle AttackCycleTimerHandle;
	FTimerHandle WindupTimerHandle;
	FTimerHandle ParryWindowOpenTimerHandle;
	FTimerHandle ParryWindowCloseTimerHandle;
	FTimerHandle HitboxActivationTimerHandle;
	FTimerHandle StrikeDurationTimerHandle;
	FTimerHandle RecoveryTimerHandle;
	FTimerHandle RespawnTimerHandle;
	FTimerHandle StaggerRecoveryTimerHandle;

	TSet<TWeakObjectPtr<AActor>> HitActorsThisStrike;
	TWeakObjectPtr<AActor> CurrentTargetActor;
};
