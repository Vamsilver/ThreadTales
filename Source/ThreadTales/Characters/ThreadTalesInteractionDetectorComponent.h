// Copyright ThreadTales. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThreadTalesInteractionDetectorComponent.generated.h"

class UThreadTalesInteractableComponent;
class ACharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFocusedInteractableChanged, UThreadTalesInteractableComponent*, NewFocusedInteractable);

/**
 * Lightweight detector component attached to player characters.
 * Runs an optimized sweep/cone trace at 10 Hz to select the best interactable object
 * in front of the avatar and notify UI prompts.
 */
UCLASS(ClassGroup = (ThreadTales), meta = (BlueprintSpawnableComponent, DisplayName = "ThreadTales Interaction Detector"))
class THREADTALES_API UThreadTalesInteractionDetectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UThreadTalesInteractionDetectorComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Returns the currently focused interactable object (nullptr if none) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Interaction")
	UThreadTalesInteractableComponent* GetCurrentInteractable() const { return CurrentInteractable.Get(); }

	/** Manually triggers interaction on the currently focused object */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Interaction")
	bool TryTriggerCurrentInteraction();

	/** Procedural Hand IK target and blend weight for interaction props */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Interaction|IK")
	void SetInteractionHandIKTarget(const FVector& TargetLocation, float Alpha);

	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Interaction|IK")
	void ClearInteractionHandIK();

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Interaction|IK")
	FVector GetInteractionHandIKLocation() const { return InteractionHandIKLocation; }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Interaction|IK")
	float GetInteractionHandIKAlpha() const { return InteractionHandIKAlpha; }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Interaction|IK")
	bool IsInteractionHandIKEnabled() const { return bEnableInteractionHandIK; }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Interaction|IK")
	FName GetInteractionHandIKBoneName() const { return InteractionHandIKBoneName; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction|IK")
	FVector InteractionHandIKLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction|IK", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InteractionHandIKAlpha = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Interaction|IK")
	bool bEnableInteractionHandIK = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|IK")
	FName InteractionHandIKBoneName = FName("hand_r");
	/** Detection radius in front of character (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction", meta = (ClampMin = "100.0", ClampMax = "600.0", ToolTip = "Search radius in cm"))
	float DetectionRadius = 250.0f;

	/** Detection cone half-angle in degrees (from character facing vector) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction", meta = (ClampMin = "15.0", ClampMax = "90.0", ToolTip = "Half-angle of forward detection cone"))
	float ConeHalfAngle = 60.0f;

	/** Frequency of world queries in seconds (0.1s = 10 Hz) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction", meta = (ClampMin = "0.05", ClampMax = "0.5", ToolTip = "Interval between sweep updates in seconds"))
	float QueryInterval = 0.1f;

	/** Collision channel used for line of sight verification */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction", meta = (ToolTip = "Collision channel for wall occlusion line trace"))
	TEnumAsByte<ECollisionChannel> OcclusionTraceChannel = ECC_Visibility;

	/** Weak reference to currently focused interactable object */
	UPROPERTY(Transient)
	TWeakObjectPtr<UThreadTalesInteractableComponent> CurrentInteractable;

	/** Timer handle driving the periodic 10 Hz search */
	FTimerHandle DetectionTimerHandle;

public:
	/** Broadcast when the best candidate interactable changes (for HUD prompts) */
	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Interaction|Events")
	FOnFocusedInteractableChanged OnFocusedInteractableChanged;

private:
	/** Performs periodic detection query */
	void PerformDetectionSweep();

	/** Updates the focused object and dispatches delegates */
	void SetCurrentInteractable(UThreadTalesInteractableComponent* NewCandidate);
};
