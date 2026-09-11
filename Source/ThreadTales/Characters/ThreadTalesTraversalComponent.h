// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ThreadTalesTraversalComponent.generated.h"

class AThreadTalesCharacter;
class UAnimMontage;
class UAnimInstance;

/**
 * Traversal states supported by the Traversal Component.
 * Designed to be modular and extended with Ladder and Rope mechanics in future milestones.
 */
UENUM(BlueprintType)
enum class EThreadTalesTraversalState : uint8
{
	None            UMETA(DisplayName = "None"),
	LedgeClimbing   UMETA(DisplayName = "Ledge Climbing"),
	Ladder          UMETA(DisplayName = "Ladder"),
	Rope            UMETA(DisplayName = "Rope")
};

/**
 * 8 directions for jumping between ledges while climbing.
 */
UENUM(BlueprintType)
enum class EThreadTalesLedgeJumpDirection : uint8
{
	None        UMETA(DisplayName = "None"),
	Up          UMETA(DisplayName = "Up"),
	UpRight     UMETA(DisplayName = "Up-Right"),
	Right       UMETA(DisplayName = "Right"),
	DownRight   UMETA(DisplayName = "Down-Right"),
	Down        UMETA(DisplayName = "Down"),
	DownLeft    UMETA(DisplayName = "Down-Left"),
	Left        UMETA(DisplayName = "Left"),
	UpLeft      UMETA(DisplayName = "Up-Left")
};

/**
 * Jump distance categories for climbing traversal.
 */
UENUM(BlueprintType)
enum class EThreadTalesLedgeJumpDistanceType : uint8
{
	Short       UMETA(DisplayName = "Short"),
	Long        UMETA(DisplayName = "Long"),
	Leap        UMETA(DisplayName = "Leap")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTraversalStateChangedDelegate, EThreadTalesTraversalState, NewState, EThreadTalesTraversalState, OldState);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class THREADTALES_API UThreadTalesTraversalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UThreadTalesTraversalComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Delegate fired whenever traversal state changes */
	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Traversal")
	FOnTraversalStateChangedDelegate OnTraversalStateChanged;

	// ==========================================
	// STATE ACCESSORS
	// ==========================================

	/** Returns true if character is currently climbing/hanging on a ledge */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	bool IsLedgeClimbing() const { return CurrentTraversalState == EThreadTalesTraversalState::LedgeClimbing; }

	/** Returns true if character is in freehang state (no solid wall beneath hands) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	bool IsFreehang() const { return bIsFreehang; }

	/** Returns true if character is in any active traversal state */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	bool IsTraversing() const { return CurrentTraversalState != EThreadTalesTraversalState::None; }

	/** Returns current traversal state */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	EThreadTalesTraversalState GetCurrentTraversalState() const { return CurrentTraversalState; }

	// ==========================================
	// ACTIONS
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

	/** Checks whether character can mantle on top of the current ledge:
	 * 1. Verifies that the wall does NOT continue upward (i.e. ledge is a true summit/platform edge).
	 * 2. Verifies there is a walkable floor inward on the platform.
	 * 3. Verifies a full character standing capsule can fit without collision (no low ceilings, overhangs, walls).
	 */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	bool CanClimbUp(FVector& OutStandingLoc) const;

	/** Initiates mantle/climb up onto the ledge (triggered by Jump while hanging when no upper ledge exists) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	void ClimbUp();

	/** Releases the ledge and drops down into falling mode (triggered by Crouch/Drop key) */
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

	/** Probes below a candidate ledge lip to test if solid vertical wall exists (false = freehang bar/edge) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	bool CheckIsFreehangLedge(const FVector& EdgeLoc, const FVector& WallNormal2D) const;

	/** Validates that a detected candidate ledge edge has a true physical lip and sufficient body clearance (Section 11.4) */
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
	UAnimMontage* GetJumpMontage(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const;

	/** Returns min and max jump distance bounds for a given direction and distance type */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal|MultiLedge")
	bool GetJumpDistanceBounds(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, float& OutMinDist, float& OutMaxDist) const;

	/** Checks if a candidate ledge distance is within valid range for the given jump type */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal|MultiLedge")
	bool IsLedgeDistanceValid(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, float Distance) const;

	/** Returns nominal distance of the underlying jump animation (Short=48.1cm, Long=96.2cm, Leap=200cm) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal|MultiLedge")
	float GetNominalJumpDistance(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const;

	/** Returns ordered priority list of jump distance types to evaluate based on sprint state */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal|MultiLedge")
	TArray<EThreadTalesLedgeJumpDistanceType> GetJumpDistancePriorities(bool bIsSprint) const;

	/** Returns normalized kinematic phase timings (AnticipationEnd and FlightEnd) for the given jump type */
	void GetJumpKinematicPhases(EThreadTalesLedgeJumpDistanceType DistanceType, float& OutAnticipationEnd, float& OutFlightEnd) const;

protected:
	virtual void BeginPlay() override;

	/** Sets the traversal state locally and manages GAS tag */
	void SetTraversalState(EThreadTalesTraversalState NewState);

	/** Callback when mantle climb up montage begins blending out to locomotion */
	UFUNCTION()
	void OnClimbMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	/** Callback when mantle climb up montage finishes */
	UFUNCTION()
	void OnClimbMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Callback when climb down montage begins blending out into hang */
	UFUNCTION()
	void OnClimbDownMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	/** Callback when climb down montage finishes */
	UFUNCTION()
	void OnClimbDownMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// ==========================================
	// NETWORKING & RPCs
	// ==========================================

	UPROPERTY(ReplicatedUsing = OnRep_TraversalState, BlueprintReadOnly, Category = "ThreadTales|Traversal")
	EThreadTalesTraversalState CurrentTraversalState;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ThreadTales|Traversal")
	bool bIsFreehang;

	UFUNCTION()
	virtual void OnRep_TraversalState(EThreadTalesTraversalState PrevState);

	UFUNCTION(Server, Reliable)
	void Server_StartLedgeClimb(const FVector& LedgeLocation, const FVector& WallNormal);

	UFUNCTION(Server, Reliable)
	void Server_ClimbUp();

	UFUNCTION(Server, Reliable)
	void Server_ReleaseLedge();

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

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StartLedgeClimb(const FVector& LedgeLocation, const FVector& WallNormal);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayClimbMontage(UAnimMontage* MontageToPlay);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ReleaseLedge();

	UFUNCTION(Server, Reliable)
	void Server_ClimbDown(const FVector& LedgeLocation, const FVector& WallNormal, bool bFreehang);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ClimbDown(const FVector& LedgeLocation, const FVector& WallNormal, bool bFreehang);

	UFUNCTION(Server, Reliable)
	void Server_WallEject();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_WallEject();

	// ==========================================
	// CONFIGURATION & TUNING
	// ==========================================

	/** Actor or Component tag required for ledges */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Detection")
	FName LedgeTag;

	/** If true, only surfaces with LedgeTag can be grabbed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Detection")
	bool bRequireLedgeTag;

	/** If true, character automatically grabs detected ledges when airborne */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Detection")
	bool bAutoGrabLedge;

	/** Forward trace distance from character center */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Detection")
	float ForwardTraceDistance;

	/** Vertical offset from character center for forward trace start */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Detection")
	float ForwardTraceZOffset;

	/** Height above forward hit to start downward trace for finding exact top edge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Detection")
	float LedgeDownTraceHeight;

	/** Depth below forward hit for downward trace end */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Detection")
	float LedgeDownTraceDepth;

	/** Minimum ledge height relative to character root for valid grab */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Detection")
	float MinGrabHeight;

	/** Maximum ledge height relative to character root for valid grab */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Detection")
	float MaxGrabHeight;

	/** Half-width distance from center to each hand for 3-point trace */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Detection")
	float HandSpacing;

	/** Maximum approach angle (in degrees) to wall normal for valid grab (up to 60 deg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Detection")
	float MaxApproachAngle;

	/** Maximum vertical speed on upward jump to allow grab (peak of jump) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Detection")
	float MaxGrabVelocityZ;

	/** Multi-depth inward probe distances into wall top to detect narrow shelves vs thick walls (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Detection")
	TArray<float> LedgeProbeDepths;

	/** Maximum forward distance from character to wall at grab moment to prevent teleportation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Detection")
	float MaxGrabWallDistance;

	// --- Ledge Lip & Body Clearance Validation ---

	/** Depth below ledge lip for validating solid vertical wall surface */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Validation")
	float LipBelowWallTraceDepth;

	/** Height above ledge lip for validating open air (flat wall and modular seam rejector) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Validation")
	float LipAboveAirTraceHeight;

	/** Horizontal 2D distance threshold to reject modular mesh seams and continuous flat walls */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Validation")
	float LipModularSeamRejectDistance;

	/** Sphere sweep radius checking clearance for player's torso and head when hanging */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Validation")
	float BodyClearanceRadius;

	/** Vertical upward height checked for body clearance */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Validation")
	float BodyClearanceHeight;

	/** Horizontal distance from wall surface when hanging */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Offsets")
	float HangWallOffset;

	/** Vertical distance below ledge edge when hanging */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Offsets")
	float HangHeightOffset;

	/** Minimum time before character can regrab a ledge after dropping */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Tuning")
	float RegrabCooldownDuration;

	/** If true, character can grab and hang from narrow bars, rails, and edges with no wall underneath */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Tuning")
	bool bAllowFreehang;

	/** Search distance forward/down from feet to locate platform edge for climb down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Tuning")
	float ClimbDownTraceDistance;

	/** Global playback rate multiplier for climbing montages and transitions (e.g. 1.2 = 20% faster) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Tuning", meta = (ClampMin = "0.5", ClampMax = "3.0"))
	float ClimbAnimPlayRateMultiplier;

	/** Root motion AnimMontage played when climbing on top of a ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages")
	TObjectPtr<UAnimMontage> ClimbOnTopMontage;

	/** Optional AnimMontage played while hanging (looped or single pose) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages")
	TObjectPtr<UAnimMontage> HangMontage;

	/** Root motion AnimMontage played when climbing down from platform edge into hang */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages")
	TObjectPtr<UAnimMontage> ClimbDownMontage;

	/** Root motion AnimMontage played when climbing down onto a freehang bar/beam */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages")
	TObjectPtr<UAnimMontage> BarClimbDownMontage;

	/** Optional AnimMontage played while hanging on a freehang bar/beam (no wall) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages")
	TObjectPtr<UAnimMontage> BarHangMontage;

	/** AnimMontage played when climbing up / muscle-up onto a freehang bar/beam */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages")
	TObjectPtr<UAnimMontage> BarClimbOnTopMontage;

	/** AnimMontage played when jumping backwards away from the wall (Back Jump / Wall Eject) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages")
	TObjectPtr<UAnimMontage> ClimbBackJumpMontage;

	/** AnimMontage played when hopping up to a higher ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Short")
	TObjectPtr<UAnimMontage> ClimbHopUpMontage;

	/** AnimMontage played when jumping diagonally up-right */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Short")
	TObjectPtr<UAnimMontage> ClimbJumpUpRightMontage;

	/** AnimMontage played when jumping across a gap to an adjacent ledge on the right */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Short")
	TObjectPtr<UAnimMontage> ClimbJumpRightMontage;

	/** AnimMontage played when jumping diagonally down-right */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Short")
	TObjectPtr<UAnimMontage> ClimbJumpDownRightMontage;

	/** AnimMontage played when hopping down to a lower ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Short")
	TObjectPtr<UAnimMontage> ClimbHopDownMontage;

	/** AnimMontage played when jumping diagonally down-left */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Short")
	TObjectPtr<UAnimMontage> ClimbJumpDownLeftMontage;

	/** AnimMontage played when jumping across a gap to an adjacent ledge on the left */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Short")
	TObjectPtr<UAnimMontage> ClimbJumpLeftMontage;

	/** AnimMontage played when jumping diagonally up-left */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Short")
	TObjectPtr<UAnimMontage> ClimbJumpUpLeftMontage;

	// --- Long Jump Montages ---

	/** AnimMontage played for long hop up */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Long")
	TObjectPtr<UAnimMontage> ClimbLongHopUpMontage;

	/** AnimMontage played for long diagonal jump up-right */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Long")
	TObjectPtr<UAnimMontage> ClimbLongJumpUpRightMontage;

	/** AnimMontage played for long jump to the right */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Long")
	TObjectPtr<UAnimMontage> ClimbLongJumpRightMontage;

	/** AnimMontage played for long diagonal jump down-right */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Long")
	TObjectPtr<UAnimMontage> ClimbLongJumpDownRightMontage;

	/** AnimMontage played for long hop down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Long")
	TObjectPtr<UAnimMontage> ClimbLongHopDownMontage;

	/** AnimMontage played for long diagonal jump down-left */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Long")
	TObjectPtr<UAnimMontage> ClimbLongJumpDownLeftMontage;

	/** AnimMontage played for long jump to the left */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Long")
	TObjectPtr<UAnimMontage> ClimbLongJumpLeftMontage;

	/** AnimMontage played for long diagonal jump up-left */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Long")
	TObjectPtr<UAnimMontage> ClimbLongJumpUpLeftMontage;

	// --- Leap Jump Montages (Chasm / Dynamic Leaps across 100-200cm gaps) ---

	/** AnimMontage played for leap up */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Leap")
	TObjectPtr<UAnimMontage> LeapHopUpMontage;

	/** AnimMontage played for leap up-right */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Leap")
	TObjectPtr<UAnimMontage> LeapJumpUpRightMontage;

	/** AnimMontage played for leap right */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Leap")
	TObjectPtr<UAnimMontage> LeapJumpRightMontage;

	/** AnimMontage played for leap down-right */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Leap")
	TObjectPtr<UAnimMontage> LeapJumpDownRightMontage;

	/** AnimMontage played for leap down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Leap")
	TObjectPtr<UAnimMontage> LeapHopDownMontage;

	/** AnimMontage played for leap down-left */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Leap")
	TObjectPtr<UAnimMontage> LeapJumpDownLeftMontage;

	/** AnimMontage played for leap left */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Leap")
	TObjectPtr<UAnimMontage> LeapJumpLeftMontage;

	/** AnimMontage played for leap up-left */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages|Leap")
	TObjectPtr<UAnimMontage> LeapJumpUpLeftMontage;

	/** AnimMontage played when shifting/shimmying left along a continuous ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages")
	TObjectPtr<UAnimMontage> ShimmyLeftMontage;

	/** AnimMontage played when shifting/shimmying right along a continuous ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages")
	TObjectPtr<UAnimMontage> ShimmyRightMontage;

	// --- Short MultiLedge Tuning ---

	/** Minimum height above current ledge to detect upper ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge")
	float MinHopUpHeight;

	/** Maximum height above current ledge to detect upper ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge")
	float MaxHopUpHeight;

	/** Minimum gap distance for side jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge")
	float MinSideJumpDistance;

	/** Maximum gap distance for side jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge")
	float MaxSideJumpDistance;

	/** Minimum distance for diagonal jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge")
	float MinDiagonalJumpDistance;

	/** Maximum distance for diagonal jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge")
	float MaxDiagonalJumpDistance;

	/** Minimum height below current ledge to detect lower ledge for hop down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge")
	float MinHopDownHeight;

	/** Maximum height below current ledge to detect lower ledge for hop down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge")
	float MaxHopDownHeight;

	// --- Long MultiLedge Tuning ---

	/** Minimum height above current ledge for long hop up */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Long")
	float MinLongHopUpHeight;

	/** Maximum height above current ledge for long hop up */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Long")
	float MaxLongHopUpHeight;

	/** Minimum gap distance for long side jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Long")
	float MinLongSideJumpDistance;

	/** Maximum gap distance for long side jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Long")
	float MaxLongSideJumpDistance;

	/** Minimum distance for long diagonal jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Long")
	float MinLongDiagonalJumpDistance;

	/** Maximum distance for long diagonal jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Long")
	float MaxLongDiagonalJumpDistance;

	/** Minimum height below current ledge for long hop down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Long")
	float MinLongHopDownHeight;

	/** Maximum height below current ledge for long hop down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Long")
	float MaxLongHopDownHeight;

	// --- Leap MultiLedge Tuning (100 - 200 cm) ---

	/** Minimum height above current ledge for leap up */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Leap")
	float MinLeapHopUpHeight;

	/** Maximum height above current ledge for leap up */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Leap")
	float MaxLeapHopUpHeight;

	/** Minimum gap distance for leap side jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Leap")
	float MinLeapSideJumpDistance;

	/** Maximum gap distance for leap side jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Leap")
	float MaxLeapSideJumpDistance;

	/** Minimum distance for diagonal leap */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Leap")
	float MinLeapDiagonalJumpDistance;

	/** Maximum distance for diagonal leap */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Leap")
	float MaxLeapDiagonalJumpDistance;

	/** Minimum height below current ledge for leap down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Leap")
	float MinLeapHopDownHeight;

	/** Maximum height below current ledge for leap down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Leap")
	float MaxLeapHopDownHeight;

	/** Distance to shift along ledge in one shimmy step */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge")
	float ShimmyStepDistance;

	/** Cooldown between consecutive shimmy steps (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Shimmy")
	float ShimmyCooldownDuration;

	/** Inward depth offsets tested when scanning for continuous ledge during shimmy */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Shimmy")
	TArray<float> ShimmyProbeDepths;

	/** Vertical probe half-range for finding top surface of continuous ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Shimmy")
	float ShimmyVerticalTraceRange;

	/** Maximum vertical difference between current ledge and shimmy step target */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Shimmy")
	float ShimmyMaxZDifference;

	// --- Directional Jump Probing Offsets ---

	/** Inward penetration depths tested when scanning for adjacent directional ledges */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Probing")
	TArray<float> DirectionalProbeDepths;

	/** Vertical upward offset for downward trace in Hop Up jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Probing")
	float UpJumpTopTraceOffset;

	/** Vertical upward offset for downward trace in Hop Down jumps (Short/Long) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Probing")
	float DownJumpTopTraceOffset;

	/** Vertical upward offset for downward trace in Hop Down Leap jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Probing")
	float DownJumpLeapTopTraceOffset;

	/** Vertical upward offset for downward trace in Lateral (Left/Right) jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Probing")
	float LateralJumpTopTraceOffset;

	/** Vertical upward offset for downward trace in Diagonal Up jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Probing")
	float DiagonalUpJumpTopTraceOffset;

	/** Vertical upward offset for downward trace in Diagonal Down jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge|Probing")
	float DiagonalDownJumpTopTraceOffset;

	// --- Kinematic Tuning & Timings ---

	/** Full kinematic cycle duration for Short hops (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Kinematics")
	float ShortJumpDuration;

	/** Full kinematic cycle duration for Long hops (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Kinematics")
	float LongJumpDuration;

	/** Full kinematic cycle duration for Leap jumps (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Kinematics")
	float LeapJumpDuration;

	/** Ratio of transition duration where flight movement completes and hands grip target ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Kinematics")
	float JumpFlightEndRatio;

	/** Input debounce duration after triggering a shimmy step */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Kinematics")
	float ShimmyInputDebounceDuration;

	/** Input debounce duration after triggering a directional ledge jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Kinematics")
	float JumpInputDebounceDuration;

	/** Horizontal impulse speed applied when kicking off a wall (Wall Eject) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Kinematics")
	float WallEjectHorizontalSpeed;

	/** Vertical upward impulse speed applied when kicking off a wall (Wall Eject) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Kinematics")
	float WallEjectVerticalSpeed;

	/** Maximum distance behind character to detect and leap to an opposing ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|MultiLedge")
	float BackJumpOpposingLedgeDistance;

	/** Draw debug visual lines, spheres and normals for ledge detection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Traversal|Debug")
	bool bDrawDebugTraces;

private:
	UPROPERTY(Transient)
	TObjectPtr<AThreadTalesCharacter> CharacterOwner;

	/** Starts smooth interpolated movement and plays the corresponding montage to target ledge */
	void Internal_StartTransition(UAnimMontage* MontageToPlay, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, float CustomDuration = -1.0f, EThreadTalesLedgeJumpDistanceType DistanceType = EThreadTalesLedgeJumpDistanceType::Short);

	/** Probes for platform edge near feet for climbing down */
	bool FindEdgeForClimbDown(FVector& OutLedgeLoc, FVector& OutWallNormal, bool& bOutIsFreehang) const;

	/** Executes climb down movement and plays animation */
	void StartClimbDown(const FVector& LedgeLoc, const FVector& WallNormal, bool bFreehang);

	/** Timestamp or timer tracking regrab cooldown */
	float RegrabCooldownTimer;

	/** True while climb up mantle montage is executing */
	bool bIsClimbingUp;

	/** True while climb down mantle montage is executing */
	bool bIsClimbingDown;

	/** True while transitioning between ledges (Hop Up, Side Jump, Shimmy) */
	bool bIsTransitioning;
	bool bIsJumpTransition;
	EThreadTalesLedgeJumpDistanceType TransitionDistanceType;
	float TransitionTimer;
	float TransitionDuration;
	FVector TransitionStartLocation;
	FRotator TransitionStartRotation;
	FVector TransitionTargetLocation;
	FRotator TransitionTargetRotation;
	FVector PendingTargetLedgeLocation;
	FVector PendingTargetWallNormal;

	/** Cooldown between consecutive shimmy steps */
	float ShimmyCooldownTimer;

	/** Debounce timer preventing rapid re-triggering of jumps from continuous WASD press */
	float ClimbingInputDebounceTimer;

	/** Cached target ledge position and wall normal */
	FVector CachedLedgeLocation;
	FVector CachedWallNormal;
	FVector CachedHangLocation;
	FRotator CachedHangRotation;
};
