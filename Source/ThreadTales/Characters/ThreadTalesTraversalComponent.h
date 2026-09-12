// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Characters/ThreadTalesTraversalTypes.h"
#include "Characters/ThreadTalesClimbingKinematics.h"
#include "Characters/ThreadTalesLedgeDetector.h"
#include "Characters/ThreadTalesLedgeClimbingHandler.h"
#include "Characters/ThreadTalesTraversalInputHelper.h"
#include "ThreadTalesTraversalComponent.generated.h"

class AThreadTalesCharacter;
class UAnimMontage;
class UAnimInstance;
class UThreadTalesClimbingConfig;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTraversalStateChangedDelegate, EThreadTalesTraversalState, NewState, EThreadTalesTraversalState, OldState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnClimbingSubStateChangedDelegate, EThreadTalesClimbingSubState, NewSubState, EThreadTalesClimbingSubState, OldSubState);

/**
 * Core coordinator component for character traversal and climbing mechanics.
 * Manages character movement state, replication, GAS tags, climbing inputs, and animation RPCs.
 * Delegates world sensing to FThreadTalesLedgeDetector, kinematic interpolation to FThreadTalesClimbingKinematics,
 * and ledge climbing execution to FThreadTalesLedgeClimbingHandler.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class THREADTALES_API UThreadTalesTraversalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UThreadTalesTraversalComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ==========================================
	// CONFIG DATA ASSET & DEBUG
	// ==========================================

	/** Optional Data Asset containing climbing montages and tuning parameters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Traversal|Config")
	TObjectPtr<UThreadTalesClimbingConfig> ClimbingConfig;

	/** Draw debug visual lines, spheres and normals for ledge detection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Traversal|Debug")
	bool bDrawDebugTraces;

	// ==========================================
	// STATE & DELEGATES
	// ==========================================

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Traversal")
	FOnTraversalStateChangedDelegate OnTraversalStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Traversal")
	FOnClimbingSubStateChangedDelegate OnClimbingSubStateChanged;

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	EThreadTalesTraversalState GetTraversalState() const { return CurrentTraversalState; }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	EThreadTalesClimbingSubState GetClimbingSubState() const { return CurrentClimbingSubState; }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	bool IsLedgeClimbing() const { return CurrentTraversalState == EThreadTalesTraversalState::LedgeClimbing; }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	bool IsHanging() const { return CurrentTraversalState == EThreadTalesTraversalState::LedgeClimbing && LedgeHandler.IsHanging(); }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	bool IsTransitioning() const { return LedgeHandler.IsTransitioning(); }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	bool IsFreehang() const { return bIsFreehang; }

	void SetIsFreehang(bool bInFreehang) { bIsFreehang = bInFreehang; LedgeHandler.SetIsFreehang(bInFreehang); }

	// ==========================================
	// PUBLIC CLIMBING ACTIONS (BLUEPRINT API)
	// ==========================================

	/** Attempts to detect a grab-able ledge in front of the character */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	bool DetectLedge(FVector& OutLedgeLocation, FVector& OutWallNormal);

	/** Enters the ledge climb/hang state at specified ledge position */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	void StartLedgeClimb(const FVector& LedgeLocation, const FVector& WallNormal);

	/** Processes direct WASD/stick climbing inputs every tick when hanging on a ledge */
	void ProcessClimbingInput(float DeltaTime);

	/** Handles jump input while hanging: evaluates upper ledge hop, side jump, or top mantle */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	void HandleLedgeJumpInput(bool bFromSpace = false);

	/** Probes for a higher ledge directly above the current one */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	bool FindUpperLedge(FVector& OutLedgeLoc, FVector& OutWallNormal);

	/** Probes for adjacent ledge along wall tangent (-1 = Left, +1 = Right). If bForJump is true, probes for jump distance */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	bool FindSideLedge(float DirectionSign, FVector& OutLedgeLoc, FVector& OutWallNormal, bool& bOutIsContinuous, bool bForJump = false);

	/** Performs a hop to a higher ledge tier */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	void HopUp(const FVector& TargetLedgeLoc, const FVector& TargetWallNormal);

	/** Performs a jump across a gap to an adjacent ledge */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	void SideJump(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal);

	/** Shifts position along a continuous ledge */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	void StartShimmy(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal);

	/** Checks whether character can mantle on top of the current ledge */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	bool CanClimbUp(FVector& OutStandingLoc) const;

	/** Initiates mantle/climb up onto the ledge */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	void ClimbUp();

	/** Releases the ledge and drops down into falling mode */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	void ReleaseLedge();

	/** Attempts to mantle down / climb down from standing at platform edge into hanging */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	bool TryClimbDown();

	/** Initiates back jump: leaps to opposing ledge if present, or performs wall eject kick-off */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	void PerformBackJump();

	/** Powerful kick-off backward from wall into falling locomotion */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	void WallEject();

	/** Probes below a candidate ledge lip to test if solid vertical wall exists */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	bool CheckIsFreehangLedge(const FVector& EdgeLoc, const FVector& WallNormal2D) const;

	/** Validates that a detected candidate ledge edge has a true physical lip and sufficient body clearance */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	bool ValidateLedgeLipAndClearance(const FVector& EdgeLoc, const FVector& WallNormal2D);

	/** Probes for a target ledge in one of the 8 directions with Short or Long distance parameter */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	bool FindLedgeInDirection(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, FVector& OutLedgeLoc, FVector& OutWallNormal);

	/** Performs a directional jump to a target ledge */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	void PerformLedgeJump(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal);

	/** Helper to resolve the best AnimMontage for a given jump direction and distance type */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	UAnimMontage* GetJumpMontage(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, bool bFreehang = false) const;

	/** Returns the compiled jump configuration (Montage, Min/Max distance, Nominal distance) for given direction & distance */
	const FThreadTalesLedgeJumpConfig& GetJumpConfig(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const;

	/** Returns min and max jump distance bounds for a given direction and distance type */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal|MultiLedge")
	bool GetJumpDistanceBounds(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, float& OutMinDist, float& OutMaxDist) const;

	/** Checks if a candidate ledge distance is within valid range for the given jump type */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal|MultiLedge")
	bool IsLedgeDistanceValid(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, float Distance) const;

	/** Returns nominal distance of the underlying jump animation (Short=48.1cm, Long=96.2cm, Leap=200cm) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal|MultiLedge")
	float GetNominalJumpDistance(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const;

	/** Returns ordered search priorities for ledge jump distance categories based on sprint state */
	TArray<EThreadTalesLedgeJumpDistanceType> GetJumpDistancePriorities(bool bIsSprint) const;

	/** Returns normalized kinematic phase timings (AnticipationEnd and FlightEnd) for the given jump type */
	void GetJumpKinematicPhases(EThreadTalesLedgeJumpDistanceType DistanceType, float& OutAnticipationEnd, float& OutFlightEnd) const;

	// ==========================================
	// ACTIVE MONTAGES & RESOLVERS
	// ==========================================

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	UAnimMontage* GetActiveHangMontage(bool bFreehang) const;

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	UAnimMontage* GetActiveClimbOnTopMontage(bool bFreehang) const;

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	UAnimMontage* GetActiveClimbDownMontage(bool bFreehang) const;

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	UAnimMontage* GetActiveBackJumpMontage(bool bFreehang = false) const;

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	UAnimMontage* GetActiveShimmyMontage(bool bLeft, bool bFreehang = false) const;

	/** Returns the appropriate corner wrapping montage for the given direction and corner type.
	 *  Falls back to the shimmy montage if dedicated corner montages are not yet assigned. */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	UAnimMontage* GetActiveCornerMontage(bool bLeft, EThreadTalesCornerType CornerType, bool bFreehang = false) const;

	/** Returns the montage played when grabbing a ledge from the air (falls back to nullptr if not set) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	UAnimMontage* GetActiveGrabMontage(bool bFreehang = false) const;

	float GetHangWallOffset() const;
	float GetHangHeightOffset() const;
	float GetClimbAnimPlayRateMultiplier() const;
	float GetClimbBackJumpPlayRate() const;
	float GetShimmyCooldownDuration() const;
	float GetShimmyInputDebounceDuration() const;
	float GetJumpInputDebounceDuration() const;
	float GetRegrabCooldownDuration() const;
	float GetWallEjectRegrabCooldown() const;

	void SetTraversalState(EThreadTalesTraversalState NewState);
	void SetClimbingSubState(EThreadTalesClimbingSubState NewSubState);

	// Network RPCs (called by LedgeHandler)
	UFUNCTION(Server, Reliable)
	void Server_StartLedgeClimb(const FVector& LedgeLocation, const FVector& WallNormal);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StartLedgeClimb(const FVector& LedgeLocation, const FVector& WallNormal);

	UFUNCTION(Server, Reliable)
	void Server_ReleaseLedge();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ReleaseLedge();

	UFUNCTION(Server, Reliable)
	void Server_HopUp(const FVector& TargetLedgeLoc, const FVector& TargetWallNormal);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_HopUp(const FVector& TargetLedgeLoc, const FVector& TargetWallNormal);

	UFUNCTION(Server, Reliable)
	void Server_SideJump(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SideJump(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal);

	UFUNCTION(Server, Reliable)
	void Server_LedgeJump(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_LedgeJump(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal);

	UFUNCTION(Server, Reliable)
	void Server_StartShimmy(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StartShimmy(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal);

	UFUNCTION(Server, Reliable)
	void Server_ClimbUp();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayClimbMontage(UAnimMontage* MontageToPlay);

	UFUNCTION(Server, Reliable)
	void Server_ClimbDown(const FVector& LedgeLocation, const FVector& WallNormal, bool bFreehang);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ClimbDown(const FVector& LedgeLocation, const FVector& WallNormal, bool bFreehang);

	UFUNCTION(Server, Reliable)
	void Server_WallEject();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_WallEject();

	UFUNCTION(Server, Reliable)
	void Server_StartCornerTransition(float DirectionSign, EThreadTalesCornerType CornerType,
		const FVector& CornerPivotLoc, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StartCornerTransition(float DirectionSign, EThreadTalesCornerType CornerType,
		const FVector& CornerPivotLoc, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal);

	// Montage End / BlendOut Callbacks
	UFUNCTION()
	void OnClimbMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnClimbMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnClimbDownMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnClimbDownMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(ReplicatedUsing = OnRep_TraversalState, VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Traversal|State")
	EThreadTalesTraversalState CurrentTraversalState;

	UPROPERTY(ReplicatedUsing = OnRep_ClimbingSubState, VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Traversal|State")
	EThreadTalesClimbingSubState CurrentClimbingSubState;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Traversal|State")
	bool bIsFreehang;

	UFUNCTION()
	void OnRep_TraversalState(EThreadTalesTraversalState OldState);

	UFUNCTION()
	void OnRep_ClimbingSubState(EThreadTalesClimbingSubState OldSubState);

private:
	UPROPERTY()
	TObjectPtr<AThreadTalesCharacter> CharacterOwner;

	// Dedicated Ledge Climbing Mode Handler
	FThreadTalesLedgeClimbingHandler LedgeHandler;

	friend class FThreadTalesLedgeClimbingHandler;
};
