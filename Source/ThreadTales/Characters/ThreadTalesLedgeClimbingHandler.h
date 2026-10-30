// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/ThreadTalesTraversalTypes.h"
#include "Characters/ThreadTalesClimbingKinematics.h"
#include "Characters/ThreadTalesClimbingCameraHandler.h"

class AThreadTalesCharacter;
class UThreadTalesTraversalComponent;
class UThreadTalesClimbingConfig;
class UAnimMontage;

/**
 * Telemetry tracker for measuring phase deviation (lead/lag) between
 * animation playback and kinematic capsule interpolation during transitions.
 */
struct THREADTALES_API FThreadTalesPhaseTelemetry
{
	TWeakObjectPtr<UAnimMontage> ActiveMontage = nullptr;
	EThreadTalesClimbingSubState SubState = EThreadTalesClimbingSubState::None;
	EThreadTalesLedgeJumpDistanceType DistanceType = EThreadTalesLedgeJumpDistanceType::Short;
	float TransitionDuration = 0.0f;
	float PlayRate = 1.0f;
	float AnticipationEnd = 0.0f;
	float FlightEnd = 1.0f;
	bool bIsBackJump = false;
	bool bIsBackJumpUp = false;

	// Physical hand tracking
	FVector InitialHandLocation = FVector::ZeroVector;
	FVector TargetHandLocation = FVector::ZeroVector;
	FVector InitialLeftHandLocation = FVector::ZeroVector;
	FVector InitialRightHandLocation = FVector::ZeroVector;
	bool bHandsInitialized = false;

	// Measured physical hand event timestamps (normalized [0..1])
	float ActualHandDetachNormalizedTime = -1.0f; // Recorded when hands first release starting hold (HandProgress >= 0.08)
	float ActualHandCatchNormalizedTime = -1.0f;  // Recorded when hands first contact target hold (HandProgress >= 0.90)

	// Bitmask for slice checkpoints:
	// Bit 0 (0x01): 25% slice
	// Bit 1 (0x02): 50% slice
	// Bit 2 (0x04): 75% slice
	// Bit 3 (0x08): 100% slice
	uint8 EvaluatedSlices = 0;

	// Running telemetry metrics
	float MaxLead = 0.0f; // Max positive PhaseDelta (Anim leads kinematics)
	float MaxLag = 0.0f;  // Max negative PhaseDelta magnitude (Anim lags kinematics)
	float SumPhaseDelta = 0.0f;
	int32 SampleCount = 0;

	void Reset()
	{
		ActiveMontage = nullptr;
		SubState = EThreadTalesClimbingSubState::None;
		DistanceType = EThreadTalesLedgeJumpDistanceType::Short;
		TransitionDuration = 0.0f;
		PlayRate = 1.0f;
		AnticipationEnd = 0.0f;
		FlightEnd = 1.0f;
		bIsBackJump = false;
		bIsBackJumpUp = false;
		InitialHandLocation = FVector::ZeroVector;
		TargetHandLocation = FVector::ZeroVector;
		InitialLeftHandLocation = FVector::ZeroVector;
		InitialRightHandLocation = FVector::ZeroVector;
		bHandsInitialized = false;
		ActualHandDetachNormalizedTime = -1.0f;
		ActualHandCatchNormalizedTime = -1.0f;
		EvaluatedSlices = 0;
		MaxLead = 0.0f;
		MaxLag = 0.0f;
		SumPhaseDelta = 0.0f;
		SampleCount = 0;
	}

	void Start(
		UAnimMontage* InMontage,
		float InDuration,
		float InPlayRate,
		EThreadTalesClimbingSubState InSubState,
		EThreadTalesLedgeJumpDistanceType InDistanceType = EThreadTalesLedgeJumpDistanceType::Short,
		float InAnticipationEnd = 0.0f,
		float InFlightEnd = 1.0f,
		bool bInIsBackJump = false,
		bool bInIsBackJumpUp = false)
	{
		Reset();
		ActiveMontage = InMontage;
		TransitionDuration = InDuration;
		PlayRate = InPlayRate;
		SubState = InSubState;
		DistanceType = InDistanceType;
		AnticipationEnd = InAnticipationEnd;
		FlightEnd = InFlightEnd;
		bIsBackJump = bInIsBackJump;
		bIsBackJumpUp = bInIsBackJumpUp;
	}

	bool IsTracking() const
	{
		return ActiveMontage.IsValid() && TransitionDuration > 0.0f;
	}
};

/**
 * Dedicated state handler and execution logic for Ledge Climbing (hanging, shimmying, directional jumps, mantling, and wall eject).
 * Extracted from UThreadTalesTraversalComponent to keep the coordinator lightweight and modular for future mechanics (Ladders, Ropes).
 */
class THREADTALES_API FThreadTalesLedgeClimbingHandler
{
public:
	FThreadTalesLedgeClimbingHandler();

	/** Initializes the handler and builds the jump table from configuration */
	void Initialize(const UThreadTalesClimbingConfig* Config, UAnimMontage* FallbackHangMontage);

	/** Updates internal cooldown and debounce timers (called every frame regardless of state) */
	void TickTimers(float DeltaTime);

	/** Checks whether the character can attempt to grab a ledge (not in regrab cooldown and auto-grab enabled) */
	bool CanAttemptGrab(const UThreadTalesClimbingConfig* Config) const;

	/** Per-frame update for ledge transition interpolation, hard anchor, and input handling */
	void Tick(float DeltaTime, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);

	/** Processes climbing movement and lateral continuous shimmy input from player controller while hanging on a ledge */
	void ProcessClimbingMovement(float DeltaTime, const APlayerController* PC, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);

	/** Updates location, rotation, and freehang anchor when continuous shimmy packets arrive from network */
	void UpdateContinuousShimmyTransform(const FVector& NewLedgeLoc, const FVector& NewWallNormal, bool bInFreehang, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);

	/** Evaluates jump input while hanging (Hop Up, Back Jump, or Mantle) */
	void HandleJumpInput(const APlayerController* PC, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);

	/** Sets regrab cooldown timer directly (e.g. from network RPCs) */
	void SetRegrabCooldownTimer(float Cooldown) { RegrabCooldownTimer = Cooldown; }

	/** Returns current regrab cooldown timer in seconds */
	float GetRegrabCooldownTimer() const { return RegrabCooldownTimer; }

	// --- Actions ---
	void StartLedgeClimb(const FVector& LedgeLocation, const FVector& WallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void ReleaseLedge(UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void HopUp(const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void SideJump(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void StartShimmy(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void ClimbUp(UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	bool TryClimbDown(UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void PerformBackJump(UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void ExecuteOpposingLedgeJump(const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void WallEject(UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void PerformLedgeJump(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);

	/**
	 * Initiates a Corner Wrapping transition (outer or inner wall corner).
	 * Called when FindSideLedge returns false but ProbeCornerLedge succeeds.
	 * Sets SubState = CornerTransition, caches pivot, launches Internal_StartTransition,
	 * plays corner montage (falls back to shimmy montage if nullptr), and fires RPC.
	 */
	void StartCornerTransition(
		float DirectionSign,
		EThreadTalesCornerType CornerType,
		const FVector& CornerPivotLoc,
		const FVector& TargetLedgeLoc,
		const FVector& TargetWallNormal,
		UThreadTalesTraversalComponent* OwnerComp,
		AThreadTalesCharacter* CharacterOwner);

	// --- Transition & Montages ---
	void Internal_StartTransition(UAnimMontage* MontageToPlay, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner, float CustomDuration = -1.0f, EThreadTalesLedgeJumpDistanceType DistanceType = EThreadTalesLedgeJumpDistanceType::Short);
	void CrossfadeToHangMontage(UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void StopAllClimbingMontages(AThreadTalesCharacter* CharacterOwner);
	void SetClimbDownCollision(bool bEnable, AThreadTalesCharacter* CharacterOwner);

	/** Returns the appropriate corner montage for the given direction and corner type.
	 *  Falls back to ShimmyLeft/RightMontage if the dedicated corner montage is nullptr. */
	UAnimMontage* GetCornerMontage(float DirectionSign, EThreadTalesCornerType CornerType, UThreadTalesTraversalComponent* OwnerComp) const;

	// --- Montage Callbacks ---
	void OnClimbMontageEnded(UAnimMontage* Montage, bool bInterrupted, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void OnClimbMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void OnClimbDownMontageEnded(UAnimMontage* Montage, bool bInterrupted, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void OnClimbDownMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);

	// --- State Queries ---
	EThreadTalesClimbingSubState GetClimbingSubState() const { return ClimbingSubState; }
	void SetClimbingSubState(EThreadTalesClimbingSubState NewSubState) { ClimbingSubState = NewSubState; }
	bool IsHanging() const { return ClimbingSubState == EThreadTalesClimbingSubState::IdleHang; }
	bool IsTransitioning() const
	{
		return ClimbingSubState == EThreadTalesClimbingSubState::JumpTransition
			|| ClimbingSubState == EThreadTalesClimbingSubState::CornerTransition
			|| ClimbingSubState == EThreadTalesClimbingSubState::GrabCatch
			|| ClimbingSubState == EThreadTalesClimbingSubState::Shimmying
			|| ClimbingSubState == EThreadTalesClimbingSubState::MantleUp
			|| ClimbingSubState == EThreadTalesClimbingSubState::ClimbDown;
	}
	bool IsKinematicTransitioning() const
	{
		return ClimbingSubState == EThreadTalesClimbingSubState::JumpTransition
			|| ClimbingSubState == EThreadTalesClimbingSubState::CornerTransition
			|| ClimbingSubState == EThreadTalesClimbingSubState::GrabCatch
			|| ClimbingSubState == EThreadTalesClimbingSubState::Shimmying;
	}
	bool IsFreehang() const { return bIsFreehang; }
	void SetIsFreehang(bool bInFreehang) { bIsFreehang = bInFreehang; }

	// --- Anchors Access ---
	const FVector& GetCachedLedgeLocation() const { return CachedLedgeLocation; }
	const FVector& GetCachedWallNormal() const { return CachedWallNormal; }
	const FVector& GetCachedHangLocation() const { return CachedHangLocation; }
	const FRotator& GetCachedHangRotation() const { return CachedHangRotation; }

	void SetCachedLedgeLocation(const FVector& Loc) { CachedLedgeLocation = Loc; }
	void SetCachedWallNormal(const FVector& Norm) { CachedWallNormal = Norm; }
	void SetCachedHangLocation(const FVector& Loc) { CachedHangLocation = Loc; }
	void SetCachedHangRotation(const FRotator& Rot) { CachedHangRotation = Rot; }

	// --- Table Queries ---
	const FThreadTalesJumpConfigTable& GetJumpConfigTable() const { return JumpConfigTable; }
	const FThreadTalesLedgeJumpConfig& GetJumpConfig(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const;
	bool GetJumpDistanceBounds(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, float& OutMinDist, float& OutMaxDist) const;
	bool IsLedgeDistanceValid(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, float Distance) const;
	float GetNominalJumpDistance(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const;
	UAnimMontage* GetJumpMontage(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const;

private:
	// Sub-state machine
	EThreadTalesClimbingSubState ClimbingSubState;
	bool bIsFreehang;
	bool bSavedUseControllerDesiredRotation;
	bool bSavedOrientRotationToMovement;
	bool bSavedUseControllerRotationYaw;

	// Cached Anchors
	FVector CachedLedgeLocation;
	FVector CachedWallNormal;
	FVector CachedHangLocation;
	FRotator CachedHangRotation;

	// Transition interpolation state
	FVector TransitionStartLocation;
	FVector TransitionTargetLocation;
	FRotator TransitionStartRotation;
	FRotator TransitionTargetRotation;

	FVector PendingTargetLedgeLocation;
	FVector PendingTargetWallNormal;

	EThreadTalesLedgeJumpDistanceType TransitionDistanceType;
	float TransitionTimer;
	float TransitionDuration;
	bool bTransitionIsBackJump = false;
	bool bTransitionIsBackJumpUp = false;

	// Corner Wrapping transition state
	FVector CornerTransitionPivotLocation;   // World position of the corner pivot for two-segment arc
	EThreadTalesCornerType ActiveCornerType; // Type of the corner currently being traversed

	// Dedicated climbing camera alignment handler
	FThreadTalesClimbingCameraHandler CameraHandler;

	// Cooldown & debounce timers
	float RegrabCooldownTimer;
	float ClimbingInputDebounceTimer;
	float ShimmyCooldownTimer;
	float ContinuousScanThrottleTimer;

	// Directional jump table lookup
	FThreadTalesJumpConfigTable JumpConfigTable;

	// --- Movement & Input Processing Helpers ---
	bool TryProcessClimbingJump(
		const APlayerController* PC,
		const FVector& IntentWorldDir,
		const FVector& WallRight,
		EThreadTalesLedgeJumpDirection JumpDir,
		bool bIsDirectJumpPress,
		UThreadTalesTraversalComponent* OwnerComp,
		AThreadTalesCharacter* CharacterOwner);

	bool TryProcessClimbingDirectional(
		const APlayerController* PC,
		EThreadTalesLedgeJumpDirection JumpDir,
		UThreadTalesTraversalComponent* OwnerComp,
		AThreadTalesCharacter* CharacterOwner);

	void ProcessContinuousShimmy(
		float LateralSign,
		bool bIsDirectJumpPress,
		UThreadTalesTraversalComponent* OwnerComp,
		AThreadTalesCharacter* CharacterOwner);

	// --- Phase Desync Telemetry ---
	float CalculateAnimProgress(const AThreadTalesCharacter* CharacterOwner, const UAnimMontage* Montage, float KinematicProgress) const;
	void UpdatePhaseTelemetry(float KinematicProgress, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void LogPhaseSummary(UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner, bool bInterrupted = false);
	void TickTransition(float DeltaTime, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);

	// Animation vs Kinematics Phase Telemetry Tracker
	FThreadTalesPhaseTelemetry PhaseTelemetry;
};
