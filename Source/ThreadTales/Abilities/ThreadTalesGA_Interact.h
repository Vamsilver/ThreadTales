// Copyright ThreadTales. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/ThreadTalesGameplayAbility.h"
#include "ThreadTalesGA_Interact.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UThreadTalesInteractableComponent;
class AThreadTalesCharacter;
class UThreadTalesInteractionConfig;

/**
 * Gameplay Ability driving contextual world interaction (chests, levers, doors).
 * Leverages Motion Warping to align avatar position and rotation to the Level Designer's
 * InteractionPoint arrow before/during interaction montage playback.
 */
UCLASS()
class THREADTALES_API UThreadTalesGA_Interact : public UThreadTalesGameplayAbility
{
	GENERATED_BODY()

public:
	UThreadTalesGA_Interact();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Callback when montage completes normally */
	UFUNCTION()
	void OnMontageCompleted();

	/** Callback when montage is blended out early */
	UFUNCTION()
	void OnMontageBlendOut();

	/** Callback when montage is interrupted */
	UFUNCTION()
	void OnMontageInterrupted();

	/** Callback when montage is cancelled */
	UFUNCTION()
	void OnMontageCancelled();

	/** Callback when Anim Notify fires GameplayEvent to trigger action in sync with hands */
	UFUNCTION()
	void OnInteractEventReceived(FGameplayEventData Payload);

	/** Callback when Anim Notify State ends physical interaction window */
	UFUNCTION()
	void OnInteractMotionEndReceived(FGameplayEventData Payload);

	/** Callback when player releases hold key before completion */
	UFUNCTION()
	void OnHoldInputReleased(float TimeHeld);

	// UActorComponent: Timer tick updating radial hold progress
	void UpdateHoldProgress();

	// UActorComponent: Timer tick walking character into InteractionPoint before montage starts
	void UpdateApproachMovement();

	/** Executes interaction alignment/walk and montage after Tap or completed Hold */
	void ExecuteInteractionAction(AThreadTalesCharacter* TTChar, const UThreadTalesInteractionConfig* Config);

	/** Starts the interaction montage once character reaches InteractionPoint */
	void PlayInteractionMontage(AThreadTalesCharacter* TTChar, const UThreadTalesInteractionConfig* Config);

	/** Toggles GASPALS WantsToWalk flag inside CharacterInputState so approach uses Walk gait instead of Run */
	void SetWalkGaitForInteraction(AThreadTalesCharacter* TTChar, bool bWalk);

private:
	/** Active montage task */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask = nullptr;

	/** Active wait gameplay event task for hand sync */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitEventTask = nullptr;

	/** Active wait gameplay event task for physical motion window end */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitEndEventTask = nullptr;

	/** Active wait input release task for hold interactions */
	UPROPERTY(Transient)
	TObjectPtr<class UAbilityTask_WaitInputRelease> WaitInputReleaseTask = nullptr;

	/** Interactable component currently being processed by this ability instance */
	UPROPERTY(Transient)
	TWeakObjectPtr<UThreadTalesInteractableComponent> ActiveInteractable = nullptr;

	// UActorComponent: Raw non-reflected state members
	FTimerHandle HoldTimerHandle;
	FTimerHandle ApproachTimerHandle;
	FVector ApproachMoveDir = FVector::ZeroVector;
	uint64 LastApproachFrame = 0;
	float HoldElapsedTime = 0.0f;
	float ApproachElapsedTime = 0.0f;
	float ApproachTangentSign = 1.0f;
	bool bIsHolding = false;
	bool bIsApproaching = false;

	/** Configures body and hand warp targets on the character */
	void ConfigureMotionWarping(AThreadTalesCharacter* TTChar, const UThreadTalesInteractionConfig* Config);
};
