// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ThreadTalesCombatReactionHandler.generated.h"

class AThreadTalesBaseCharacter;
class UAnimMontage;
class UThreadTalesAbilitySystemComponent;
class UThreadTalesAttributeSet;

/**
 * Dedicated component handling combat impact reactions, poise breaks, hit-stop,
 * parry counters, and network replication for AThreadTalesCharacter.
 * Extracted from AThreadTalesCharacter to decouple reaction logic from pawn lifecycle.
 */
UCLASS(ClassGroup=(ThreadTales), meta=(BlueprintSpawnableComponent, DisplayName="ThreadTales Combat Reaction Handler"))
class THREADTALES_API UThreadTalesCombatReactionHandler : public UActorComponent
{
	GENERATED_BODY()

public:
	UThreadTalesCombatReactionHandler();

	/** Montage to play when Poise breaks and character is staggered */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Combat|Montages")
	TObjectPtr<UAnimMontage> StaggerMontage;

	/** Dedicated montage to play when Posture breaks completely (3.5s stun) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Combat|Montages")
	TObjectPtr<UAnimMontage> PostureBreakMontage;

	/** Montage to play when recovering from posture break (get-up) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Combat|Montages")
	TObjectPtr<UAnimMontage> RecoverMontage;

	/** Montage to play on standard hit reaction */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Combat|Montages")
	TObjectPtr<UAnimMontage> HitReactMontage;

	/** Montage to play on guard hit when blocking an attack */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Combat|Montages")
	TObjectPtr<UAnimMontage> GuardHitMontage;

	/** Montage to play on heavy guard hit with pushback */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Combat|Montages")
	TObjectPtr<UAnimMontage> GuardHitHeavyMontage;

	/** Montage to play on successful perfect parry deflection */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Combat|Montages")
	TObjectPtr<UAnimMontage> ParryMontage;

	UAnimMontage* GetStaggerMontage() const { return StaggerMontage; }
	UAnimMontage* GetPostureBreakMontage() const { return PostureBreakMontage; }
	UAnimMontage* GetRecoverMontage() const { return RecoverMontage; }
	UAnimMontage* GetHitReactMontage() const { return HitReactMontage; }
	UAnimMontage* GetGuardHitMontage() const { return GuardHitMontage; }
	UAnimMontage* GetGuardHitHeavyMontage() const { return GuardHitHeavyMontage; }
	UAnimMontage* GetParryMontage() const { return ParryMontage; }

	/** Optional airborne stagger montage (fallback to StaggerMontage if null) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Combat|Montages|Airborne")
	TObjectPtr<UAnimMontage> AirStaggerMontage;

	/** Optional airborne posture break montage (fallback to PostureBreakMontage if null) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Combat|Montages|Airborne")
	TObjectPtr<UAnimMontage> AirPostureBreakMontage;

	/** Optional airborne hit react montage (fallback to HitReactMontage if null) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Combat|Montages|Airborne")
	TObjectPtr<UAnimMontage> AirHitReactMontage;

	UAnimMontage* GetAirStaggerMontage() const { return AirStaggerMontage; }
	UAnimMontage* GetAirPostureBreakMontage() const { return AirPostureBreakMontage; }
	UAnimMontage* GetAirHitReactMontage() const { return AirHitReactMontage; }

	/** Applies network-safe local animation pause (Hit-Stop) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Combat")
	void ApplyHitStop(float Duration = 0.06f);

	/** Resumes animation playback and clears mesh animation pause from Hit-Stop */
	void ResumeFromHitStop();

	/** Handles stagger reaction when poise breaks */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Combat")
	void HandleStaggered(AActor* AttackerActor);

	/** Handles light hit reaction */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Combat")
	void HandleHitReact(AActor* AttackerActor);

	/** Handles guard hit reaction when blocking an attack */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Combat")
	void HandleGuardHit(AActor* AttackerActor);

	/** Handles parried reaction when this character's attack is parried */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Combat")
	void HandleParried(AActor* ParryingActor);

	/** Handles extended Posture Break stun when Poise reaches 0 */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Combat")
	void HandlePostureBreak(AActor* ParryingActor, float StunDuration = -1.0f);

	/** Recovers character from extended Posture Break stun and restores Poise */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Combat")
	void FinishPostureBreak();

	/** Recovers character from standard parried stagger recoil */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Combat")
	void FinishParriedStagger();

	/** Handles successful perfect parry deflection when this character deflects an incoming attack */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Combat")
	void HandleSuccessfulParry(AActor* AttackingActor);

	/** Opens timed parry counter-shot window */
	void OpenParryCounterWindow(float Duration = 1.25f);

	/** Clears parry counter window tag from ASC */
	void CloseParryCounterWindow();

	/** Returns true if hit-stop or stun is currently active */
	bool IsHitStopActive() const { return HitStopTimerHandle.IsValid(); }

	// Network Replication RPCs
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ApplyHitStop(float Duration);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayGuardHit(AActor* AttackerActor, bool bHeavyHit);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlaySuccessfulParry(AActor* AttackingActor);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitReact(AActor* AttackerActor);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayStaggered(AActor* AttackerActor);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayParried(AActor* ParryingActor);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayPostureBreak(AActor* ParryingActor, float StunDuration);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayRecover();

	/** Triggered when PostureBreak montage starts blending out on floor */
	UFUNCTION()
	void OnPostureBreakMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	/** Triggered when PostureBreak montage ends */
	UFUNCTION()
	void OnPostureBreakMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Triggered when Parried Stagger montage starts blending out */
	UFUNCTION()
	void OnParriedMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	/** Triggered when Parried Stagger montage ends */
	UFUNCTION()
	void OnParriedMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Clears all active combat reaction and stun timers (e.g. upon death) */
	void ClearReactionTimers();

protected:
	/** Helper to obtain owning AThreadTalesBaseCharacter */
	AThreadTalesBaseCharacter* GetOwningCharacter() const;

	/** Timer handle for Hit-Stop micro-pause */
	FTimerHandle HitStopTimerHandle;

	/** Timer handle for parried stagger recoil */
	FTimerHandle ParriedStaggerTimerHandle;

	/** Timer handle for Posture Break stun duration */
	FTimerHandle PostureBreakTimerHandle;

	/** Timer handle for parry counter window duration */
	FTimerHandle ParryCounterWindowTimerHandle;
};
