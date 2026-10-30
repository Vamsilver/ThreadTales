// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThreadTalesTargetLockComponent.generated.h"

class AThreadTalesCharacter;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetLockChangedDelegate, AActor*, NewTarget);

/**
 * Manages tactical combat target lock-on, candidate selection, camera tracking, and strafe locomotion.
 * Designed for symmetrical 2-Player Co-op (per-controller execution, zero GetPlayerController(0)).
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class THREADTALES_API UThreadTalesTargetLockComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UThreadTalesTargetLockComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ==========================================
	// CONFIGURATION & TUNING
	// ==========================================

	/** Maximum search distance for acquiring a target lock */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|TargetLock")
	float LockSearchRadius;

	/** Distance at which an active lock breaks automatically */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|TargetLock")
	float BreakDistance;

	/** Continuous occlusion time (in seconds) behind geometry before lock breaks */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|TargetLock")
	float MaxOcclusionTime;

	/** Camera interpolation speed towards the locked target */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|TargetLock")
	float CameraInterpSpeed;

	/** Minimum allowed pitch (looking down) during lock-on tracking */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|TargetLock")
	float MinPitchAngle;

	/** Maximum allowed pitch (looking up) during lock-on tracking */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|TargetLock")
	float MaxPitchAngle;

	/** Weight of angle-to-camera-center when scoring target candidates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|TargetLock")
	float AngleWeight;

	/** Weight of distance when scoring target candidates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|TargetLock")
	float DistanceWeight;

	/** Minimum time interval between target switches to prevent rapid cycling on stick tilt or scroll wheel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|TargetLock")
	float TargetSwitchCooldown;

	/** Enable debug traces and candidate score spheres in the viewport */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|TargetLock")
	bool bDrawDebug;

	// ==========================================
	// PUBLIC API
	// ==========================================

	/** Toggles target lock: acquires closest valid target or disengages if already locked */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|TargetLock")
	void ToggleTargetLock();

	/** Engages target lock onto a specific actor */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|TargetLock")
	bool LockOn(AActor* NewTarget);

	/** Disengages target lock and restores normal free locomotion */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|TargetLock")
	void Unlock();

	/** Cycles to the next valid target candidate to the left or right */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|TargetLock")
	void SwitchTarget(bool bRight);

	/** Returns the currently locked target actor, or nullptr if none */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|TargetLock")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	/** Returns true if currently locked onto a valid target */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|TargetLock")
	bool IsTargetLocked() const { return CurrentTarget.IsValid(); }

	/** Returns true if owner character is eligible to lock onto targets (in combat stance / has InCombat tag, alive, not climbing) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|TargetLock")
	bool CanLockOn() const;

	/** Returns the 3D aim/camera focus location of the current target */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|TargetLock")
	FVector GetTargetLocation() const;

	/** Sets character movement mode between Free Locomotion and Strafe */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|TargetLock")
	void SetCharacterStrafeMode(bool bEnableStrafe);

	/** Delegate fired whenever the locked target changes */
	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|TargetLock")
	FOnTargetLockChangedDelegate OnTargetLockChanged;

protected:
	/** Finds the best target candidate in front of the camera view */
	AActor* FindBestTarget() const;

	/** Collects all currently valid target candidates within SearchRadius */
	void GetTargetCandidates(TArray<AActor*>& OutCandidates) const;

	/** Checks if an actor is alive, targetable, and not the owner */
	bool IsValidTarget(const AActor* TargetActor) const;

	/** Performs line trace to verify line-of-sight between camera and target */
	bool HasLineOfSightToTarget(const AActor* TargetActor) const;

	/** Updates camera controller rotation smoothly towards the target */
	void UpdateCameraTracking(float DeltaTime);

	/** Safely retrieves the PlayerController owning this character (2P Co-op safe) */
	APlayerController* GetPlayerController() const;

private:
	/** Currently locked target */
	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentTarget;

	/** Time accumulated while target has been occluded by geometry */
	float OcclusionTimer;

	/** Anti-debounce timestamp to prevent accidental double-toggle on mouse wheel clicks */
	double LastToggleTime;

	/** Anti-debounce timestamp to prevent rapid target cycling */
	double LastSwitchTime;
};
