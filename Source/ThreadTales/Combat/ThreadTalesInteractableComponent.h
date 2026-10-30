// Copyright ThreadTales. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ThreadTalesInteractableComponent.generated.h"

class UThreadTalesInteractionConfig;
class UThreadTalesInteractionPromptWidget;
class UWidgetComponent;
class UArrowComponent;
class ACharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionTriggered, ACharacter*, InstigatingCharacter, UThreadTalesInteractableComponent*, InteractableComp);

/**
 * Universal component attached to world actors (chests, levers, doors, shrines).
 * Features a visual InteractionPoint arrow in the editor viewport for zero-code Level Designer alignment.
 * Includes a designer-friendly 3D floating Screen-Space prompt widget with Fade In/Out animations.
 */
UCLASS(ClassGroup = (ThreadTales), meta = (BlueprintSpawnableComponent, DisplayName = "ThreadTales Interactable"))
class THREADTALES_API UThreadTalesInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UThreadTalesInteractableComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// UActorComponent: replication registration
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Returns true if interaction is currently enabled and valid for the player */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Interaction")
	bool CanInteract(const ACharacter* Character) const;

	/** Returns true if the object requires a tag (RequiredTag) that the character currently lacks */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Interaction|Requirements")
	bool IsLockedForCharacter(const ACharacter* Character) const;

	/** Plays locked sound and shakes prompt UI when character attempts to interact while locked */
	bool TryRejectLockedInteraction(ACharacter* Character);

	/** Initiates interaction flow from the given character */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Interaction")
	bool TriggerInteraction(ACharacter* Character);

	/** Marks interaction as finished and executes completion logic (loot drop, door open) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Interaction")
	void FinishInteraction(ACharacter* Character);

	/** Cancels interaction in progress, clearing busy state and restoring prompt */
	void CancelInteraction(ACharacter* Character);

	/** Triggers cosmetic interaction action (e.g. lid opening, lever pulling) taking toggle state into account */
	void TriggerCosmeticAction();

	// UActorComponent: Triggers smooth animation on the child component (e.g. lid opening, lever pulling)
	void StartLidAnimation(bool bReverse = false);

	/** Returns whether this interactable is currently in its active/toggled state */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Interaction|State")
	bool IsActivated() const { return bIsActivated; }
	/** Returns whether this interactable functions in toggle mode (explicit bToggleMode or derived from Config) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Interaction|State")
	bool IsToggleMode() const;
	/** Resets the interactable back to its initial inactive state */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Interaction|State")
	void ResetInteractionState();
	/** Toggles the interactable state between active and inactive */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Interaction|State")
	void ToggleState(ACharacter* InstigatorCharacter = nullptr);

	/** Configuration asset assigned by Level Designer */
	const UThreadTalesInteractionConfig* GetConfig() const { return InteractionConfig; }

	/** World location where character should stand to perform interaction */
	FVector GetInteractionLocation() const;

	/** World rotation character should face when interacting */
	FRotator GetInteractionRotation() const;

	/** Transform matching the Level Designer's interaction arrow (InteractionPoint) */
	FTransform GetInteractionTransform() const;

	/** Transform where hands should reach during interaction (e.g. LidPivot, lock, handle) */
	FTransform GetHandInteractionTransform() const;

	/** World location where the floating UI prompt should hover (centered over the actor/chest rather than player feet arrow) */
	FVector GetPromptLocation() const;

	bool IsFocused() const { return bIsFocused; }

	bool IsBusy() const { return bIsBusy; }

	void SetFocused(bool bInFocused, const ACharacter* FocusedCharacter = nullptr);

	UWidgetComponent* GetPromptWidgetComponent() const { return PromptWidgetComponent; }

	UThreadTalesInteractionPromptWidget* GetPromptWidget() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction", meta = (ToolTip = "Data asset preset defining interaction behavior"))
	TObjectPtr<UThreadTalesInteractionConfig> InteractionConfig = nullptr;

	UPROPERTY()
	TObjectPtr<AActor> TargetActorToTrigger = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|Trigger", meta = (ToolTip = "Level actors triggered simultaneously when interaction finishes (e.g. doors, gates, bridges, traps)"))
	TArray<TObjectPtr<AActor>> TargetActorsToTrigger;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction", meta = (ToolTip = "Name of child Arrow/Scene component acting as alignment point. Default: InteractionPoint"))
	FName InteractionPointComponentName = FName("InteractionPoint");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction", meta = (ToolTip = "Name of child anchor where hands align. Default: HandTarget"))
	FName HandTargetComponentName = FName("HandTarget");

	/** Maximum radius in cm for player to be considered in range */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction", meta = (ClampMin = "50.0", ClampMax = "500.0", ToolTip = "Interaction detection radius in cm"))
	float InteractionRadius = 200.0f;

	/** Maximum horizontal angle in degrees from facing vector allowed to interact (180 = all angles) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction", meta = (ClampMin = "30.0", ClampMax = "180.0", ToolTip = "Allowable horizontal interaction angle in degrees"))
	float InteractionAngleDegrees = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction|Rules", meta = (ToolTip = "If true, interaction toggles state repeatedly rather than disabling after first use"))
	bool bToggleMode = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsActivated, VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|State")
	bool bIsActivated = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction|Rules", meta = (ToolTip = "If true, automatically resets back to default state after delay"))
	bool bAutoReset = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction|Rules", meta = (EditCondition = "bAutoReset", ClampMin = "0.1", ClampMax = "60.0", ToolTip = "Delay in seconds before auto-resetting"))
	float AutoResetDelay = 5.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|State")
	bool bHasBeenTriggered = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsBusy, VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|State")
	bool bIsBusy = false;

	// UActorComponent: RepNotify handlers for replicated state
	UFUNCTION()
	void OnRep_IsActivated();

	UFUNCTION()
	void OnRep_IsBusy();

	/** Name of the child component rotated or translated on interaction (e.g. LidPivot, ChestLid, LeverHandle, SlidingDoor) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction|Cosmetics", meta = (ToolTip = "Name of child component to animate smoothly on interaction (e.g. LidPivot, DoorHinge, LeverHandle)"))
	FName AnimatedComponentName = FName("LidPivot");

	/** Relative rotation offset applied when opened/activated (e.g. Pitch = 85 for chest lid, Yaw = 90 for door, Pitch = 45 for lever) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction|Cosmetics", meta = (ToolTip = "Relative rotation offset applied when fully opened/activated"))
	FRotator OpenRotationOffset = FRotator(85.0f, 0.0f, 0.0f);

	/** Relative translation offset applied when opened/activated (e.g. Z = 50 for rising gate, X = 80 for sliding drawer) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction|Cosmetics", meta = (ToolTip = "Relative translation offset applied when fully opened/activated (for sliding doors, drawers, gates)"))
	FVector OpenTranslationOffset = FVector::ZeroVector;

	/** Duration of forward opening/activation in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction|Cosmetics", meta = (ClampMin = "0.05", ClampMax = "10.0", ToolTip = "Duration of forward activation animation in seconds"))
	float OpenDuration = 0.6f;

	/** Duration of reverse closing/reset animation in seconds (if <= 0.0f, OpenDuration is used) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction|Cosmetics", meta = (ClampMin = "0.0", ClampMax = "10.0", ToolTip = "Optional duration of reverse closing animation in seconds (0 = uses OpenDuration)"))
	float CloseDuration = 0.0f;

	/** Optional custom easing curve (0.0 to 1.0) overriding default smooth cubic EaseInOut */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction|Cosmetics", meta = (ToolTip = "Optional float curve driving normalized animation progress [0.0..1.0]"))
	TObjectPtr<class UCurveFloat> KinematicsCurve = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction|Cosmetics", meta = (ToolTip = "Optional sound played when interaction completes and prop moves"))
	TObjectPtr<USoundBase> OpenSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction|Cosmetics", meta = (ToolTip = "Optional sound played when resetting back to original state"))
	TObjectPtr<USoundBase> ResetSound = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction|UI")
	TObjectPtr<UWidgetComponent> PromptWidgetComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction|UI", meta = (ToolTip = "Widget archetype for floating prompt"))
	TSubclassOf<UThreadTalesInteractionPromptWidget> PromptWidgetClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction|UI", meta = (ToolTip = "Position offset for floating UI above interaction point"))
	FVector PromptWidgetRelativeOffset = FVector(0.0f, 0.0f, 60.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction|UI", meta = (ToolTip = "Prompt text displayed when object is currently in active/toggled state"))
	FText ToggledPromptText;

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Interaction|Events")
	FOnInteractionTriggered OnInteractionStarted{};

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Interaction|Events")
	FOnInteractionTriggered OnInteractionCompleted{};

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Interaction|Events")
	FOnInteractionTriggered OnInteractionReset{};

private:
	/** Resolves and caches the alignment component and animated lid */
	void CacheInteractionPoint();

	/** Resolves and caches the component to animate */
	void CacheAnimatedComponent();

	/** Resolves and caches the hand alignment component */
	void CacheHandTargetComponent();

	/** Ticks the smooth lid rotation timer */
	void UpdateLidRotation();

	/** Initializes or prepares the floating 3D prompt widget */
	void SetupPromptWidget();

	/** Updates prompt widget visibility and presentation */
	void UpdatePromptVisibility(bool bVisible);

	/** Broadcasts trigger execution (or reset/toggle state) to TargetActorToTrigger and TargetActorsToTrigger array */
	void NotifyLinkedTriggers(AActor* InstigatorActor, bool bActiveState = true);

	/** Cached pointer to the LD alignment point (arrow or scene component) */
	TWeakObjectPtr<USceneComponent> CachedInteractionPoint;

	/** Cached pointer to the animated lid/handle component */
	TWeakObjectPtr<USceneComponent> CachedAnimatedComponent;

	/** Cached pointer to the hand alignment component */
	TWeakObjectPtr<USceneComponent> CachedHandTargetComponent;

	/** Cached focused character for contextual prompt resolution */
	TWeakObjectPtr<const ACharacter> CachedFocusedCharacter;

	/** Local focus flag */
	bool bIsFocused = false;

	/** Initial rotation before opening */
	FRotator InitialLidRotation = FRotator::ZeroRotator;

	/** Target rotation when opened */
	FRotator TargetLidRotation = FRotator::ZeroRotator;

	/** Initial translation before opening */
	FVector InitialLidLocation = FVector::ZeroVector;

	/** Target translation when opened */
	FVector TargetLidLocation = FVector::ZeroVector;

	/** Timer handle driving smooth opening interpolation */
	FTimerHandle LidInterpolationTimerHandle;

	/** Timer handle driving auto-reset delay */
	FTimerHandle AutoResetTimerHandle;

	/** Whether the active rotation interpolation is moving in reverse back to initial rotation */
	bool bIsAnimatingReverse = false;

	/** Opening animation elapsed time */
	float LidAnimElapsedTime = 0.0f;

	/** Flag preventing duplicate animation triggering between AnimNotify and FinishInteraction */
	bool bLidAnimationStarted = false;

	// UActorComponent: Flag tracking whether cosmetic animation was executed in the current interaction session
	bool bCosmeticActionExecutedThisInteraction = false;
};
