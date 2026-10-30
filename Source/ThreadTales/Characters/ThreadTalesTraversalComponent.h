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
#include "ThreadTales.h"
#include "ThreadTalesTraversalComponent.generated.h"

#define TT_TRAVERSAL_LOG(OwnerComp, Verbosity, Format, ...) \
	do { \
		if ((OwnerComp) && (OwnerComp)->ShouldLogTraversal()) { \
			UE_LOG(LogThreadTalesTraversal, Verbosity, Format, ##__VA_ARGS__); \
		} \
	} while(0)

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
	TObjectPtr<const UThreadTalesClimbingConfig> ClimbingConfig;

	const UThreadTalesClimbingConfig* GetClimbingConfig() const { return ClimbingConfig.Get(); }

	/** Debug and diagnostic configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Traversal|Debug")
	FThreadTalesTraversalDebugConfig DebugConfig;

	bool IsDrawDebugTracesEnabled() const;
	static bool IsGlobalDrawDebugTracesEnabled(const ACharacter* Character = nullptr);
	float GetPhaseDesyncThreshold() const { return DebugConfig.PhaseDesyncThreshold; }

	/** Checks whether verbose traversal logging is currently enabled (via property or tt.TraversalDebugLogs CVar) */
	bool ShouldLogTraversal() const;

	/** Static helper to check if traversal logging is enabled for a given character or globally */
	static bool IsGlobalTraversalLoggingEnabled(const ACharacter* Character = nullptr);

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

	bool IsHanging() const { return CurrentTraversalState == EThreadTalesTraversalState::LedgeClimbing && LedgeHandler.IsHanging(); }

	bool IsTransitioning() const { return LedgeHandler.IsTransitioning(); }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	bool IsFreehang() const { return bIsFreehang; }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	bool IsShimmying() const { return CurrentTraversalState == EThreadTalesTraversalState::LedgeClimbing && CurrentClimbingSubState == EThreadTalesClimbingSubState::Shimmying; }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	float GetShimmyDirectionSign() const { return CurrentShimmyDirectionSign; }
	bool IsShimmyLeft() const { return IsShimmying() && CurrentShimmyDirectionSign < 0.0f; }
	bool IsShimmyRight() const { return IsShimmying() && CurrentShimmyDirectionSign > 0.0f; }
	bool IsHangBraced() const { return CurrentTraversalState == EThreadTalesTraversalState::LedgeClimbing && CurrentClimbingSubState == EThreadTalesClimbingSubState::IdleHang && !bIsFreehang; }
	bool IsHangFreehang() const { return CurrentTraversalState == EThreadTalesTraversalState::LedgeClimbing && CurrentClimbingSubState == EThreadTalesClimbingSubState::IdleHang && bIsFreehang; }

	void SetIsFreehang(bool bInFreehang);
	void SetShimmyDirectionSign(float InSign);
	void UpdateClimbingGameplayTags();

	// ==========================================
	// PUBLIC CLIMBING ACTIONS (BLUEPRINT & C++ API)
	// ==========================================

	/** Attempts to detect a grab-able ledge in front of the character */
	bool DetectLedge(FVector& OutLedgeLocation, FVector& OutWallNormal);

	/** Enters the ledge climb/hang state at specified ledge position */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	void StartLedgeClimb(const FVector& LedgeLocation, const FVector& WallNormal);

	/** Records movement input forwarded by character while climbing */
	void RecordClimbingMovementInput(const FVector& WorldDirection, float ScaleValue);

	/** Consumes and returns the lateral input sign (-1.0f = left, 1.0f = right, 0.0f = none) */
	float ConsumeClimbingLateralInput();

	/** Returns true if climbing movement input was received this frame */
	bool HasClimbingMovementInput() const { return bHasClimbingInputThisFrame; }

	/** Resets per-frame climbing movement input */
	void ResetClimbingMovementInput() { CachedClimbingInputVector = FVector::ZeroVector; bHasClimbingInputThisFrame = false; }

	/** Processes direct WASD/stick climbing inputs every tick when hanging on a ledge */
	void ProcessClimbingInput(float DeltaTime);

	/** Handles jump input while hanging: evaluates upper ledge hop, side jump, or top mantle */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	void HandleLedgeJumpInput(bool bFromSpace = false);

	/** Probes for a higher ledge directly above the current one */
	bool FindUpperLedge(FVector& OutLedgeLoc, FVector& OutWallNormal);

	/** Probes for adjacent ledge along wall tangent (-1 = Left, +1 = Right). If bForJump is true, probes for jump distance */
	bool FindSideLedge(float DirectionSign, FVector& OutLedgeLoc, FVector& OutWallNormal, bool& bOutIsContinuous, bool bForJump = false);

	/** Performs a hop to a higher ledge tier */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	void HopUp(const FVector& TargetLedgeLoc, const FVector& TargetWallNormal);

	/** Performs a jump across a gap to an adjacent ledge */
	void SideJump(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal);

	/** Shifts position along a continuous ledge */
	void StartShimmy(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal);

	/** Checks whether character can mantle on top of the current ledge */
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

	/** Handles crouch input: releases ledge if hanging, climbs down if at cliff edge, otherwise returns false */
	bool HandleCrouchOrClimbDownInput();

	/** Initiates back jump: leaps to opposing ledge if present, or performs wall eject kick-off */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	void PerformBackJump();

	/** Powerful kick-off backward from wall into falling locomotion */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	void WallEject();

	/** Probes below a candidate ledge lip to test if solid vertical wall exists */
	bool CheckIsFreehangLedge(const FVector& EdgeLoc, const FVector& WallNormal2D) const;

	/** Validates that a detected candidate ledge edge has a true physical lip and sufficient body clearance */
	bool ValidateLedgeLipAndClearance(const FVector& EdgeLoc, const FVector& WallNormal2D);

	/** Probes for a target ledge in one of the 8 directions with Short or Long distance parameter */
	bool FindLedgeInDirection(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, FVector& OutLedgeLoc, FVector& OutWallNormal);

	/** Performs a directional jump to a target ledge */
	void PerformLedgeJump(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal);

	/** Helper to resolve the best AnimMontage for a given jump direction and distance type */
	UAnimMontage* GetJumpMontage(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, bool bFreehang = false) const;

	/** Returns the compiled jump configuration (Montage, Min/Max distance, Nominal distance) for given direction & distance */
	const FThreadTalesLedgeJumpConfig& GetJumpConfig(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const;

	/** Returns min and max jump distance bounds for a given direction and distance type */
	bool GetJumpDistanceBounds(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, float& OutMinDist, float& OutMaxDist) const;

	/** Checks if a candidate ledge distance is within valid range for the given jump type */
	bool IsLedgeDistanceValid(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, float Distance) const;

	/** Returns nominal distance of the underlying jump animation (Short=48.1cm, Long=96.2cm, Leap=200cm) */
	float GetNominalJumpDistance(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const;

	/** Returns ordered search priorities for ledge jump distance categories based on sprint state */
	TArray<EThreadTalesLedgeJumpDistanceType> GetJumpDistancePriorities(bool bIsSprint) const;

	/** Returns normalized kinematic phase timings (AnticipationEnd and FlightEnd) for the given jump type */
	void GetJumpKinematicPhases(EThreadTalesLedgeJumpDistanceType DistanceType, float& OutAnticipationEnd, float& OutFlightEnd) const;

	// ==========================================
	// ACTIVE MONTAGES & RESOLVERS
	// ==========================================

	UAnimMontage* GetActiveHangMontage(bool bFreehang) const;
	UAnimMontage* GetActiveClimbOnTopMontage(bool bFreehang) const;
	UAnimMontage* GetActiveClimbDownMontage(bool bFreehang) const;
	UAnimMontage* GetActiveBackJumpMontage(bool bFreehang = false) const;
	UAnimMontage* GetActiveBackJumpUpMontage(bool bFreehang = false) const;
	UAnimMontage* GetActiveBackJumpUpMontage(bool bFreehang, float LateralOffset) const;
	UAnimMontage* GetActiveShimmyMontage(bool bLeft, bool bFreehang = false) const;
	UAnimMontage* GetActiveCornerMontage(bool bLeft, EThreadTalesCornerType CornerType, bool bFreehang = false) const;
	UAnimMontage* GetActiveGrabMontage(bool bFreehang = false) const;

	float GetHangWallOffset() const;
	float GetHangHeightOffset() const;
	float GetClimbAnimPlayRateMultiplier() const;
	float GetJumpPlayRateMultiplier() const;
	float GetClimbBackJumpPlayRate() const;
	float GetGrabPlayRate() const;
	float GetBarGrabPlayRate() const;
	float GetShimmyCooldownDuration() const;
	float GetShimmyInputDebounceDuration() const;
	float GetJumpInputDebounceDuration() const;
	float GetRegrabCooldownDuration() const;
	float GetWallEjectRegrabCooldown() const;

	/** Returns true if character is in regrab cooldown after wall eject or dropping from ledge */
	bool IsRegrabCooldownActive() const;

	/** Returns true if character recently performed a wall eject */
	bool IsWallEjectActive() const;

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

	UFUNCTION(Server, Unreliable)
	void Server_UpdateShimmyContinuous(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, bool bInFreehang);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_UpdateShimmyContinuous(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, bool bInFreehang);

	UFUNCTION(Server, Reliable)
	void Server_ClimbUp();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayClimbMontage(UAnimMontage* MontageToPlay);

	UFUNCTION(Server, Reliable)
	void Server_CompleteClimbUp();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_CompleteClimbUp();

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
	void OnClimbMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void OnClimbMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);
	void OnClimbDownMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void OnClimbDownMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(ReplicatedUsing = OnRep_TraversalState, VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Traversal|State", meta = (AllowPrivateAccess = "true"))
	EThreadTalesTraversalState CurrentTraversalState;

	UPROPERTY(ReplicatedUsing = OnRep_ClimbingSubState, VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Traversal|State", meta = (AllowPrivateAccess = "true"))
	EThreadTalesClimbingSubState CurrentClimbingSubState;

	UPROPERTY(ReplicatedUsing = OnRep_IsFreehang, VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Traversal|State", meta = (AllowPrivateAccess = "true"))
	bool bIsFreehang;

	UPROPERTY(ReplicatedUsing = OnRep_ShimmyDirectionSign, VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Traversal|State", meta = (AllowPrivateAccess = "true"))
	float CurrentShimmyDirectionSign;

	UFUNCTION()
	void OnRep_TraversalState(EThreadTalesTraversalState OldState);

	UFUNCTION()
	void OnRep_ClimbingSubState(EThreadTalesClimbingSubState OldSubState);

	UFUNCTION()
	void OnRep_IsFreehang();

	UFUNCTION()
	void OnRep_ShimmyDirectionSign();

private:
	UPROPERTY()
	TObjectPtr<AThreadTalesCharacter> CharacterOwner;

	// Dedicated Ledge Climbing Mode Handler
	FThreadTalesLedgeClimbingHandler LedgeHandler;

	// Per-frame climbing movement input forwarded from AddMovementInput
	FVector CachedClimbingInputVector = FVector::ZeroVector;
	bool bHasClimbingInputThisFrame = false;

	friend class FThreadTalesLedgeClimbingHandler;
};
