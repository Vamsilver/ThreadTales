// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/ThreadTalesTraversalTypes.h"
#include "Characters/ThreadTalesClimbingKinematics.h"

class AThreadTalesCharacter;
class UThreadTalesTraversalComponent;
class UThreadTalesClimbingConfig;
class UAnimMontage;

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

	/** Processes climbing movement input from player controller while hanging on a ledge */
	void ProcessInput(const APlayerController* PC, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);

	/** Evaluates jump input while hanging (Hop Up, Back Jump, or Mantle) */
	void HandleJumpInput(const APlayerController* PC, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);

	/** Sets regrab cooldown timer directly (e.g. from network RPCs) */
	void SetRegrabCooldownTimer(float Cooldown) { RegrabCooldownTimer = Cooldown; }

	// --- Actions ---
	void StartLedgeClimb(const FVector& LedgeLocation, const FVector& WallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void ReleaseLedge(UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void HopUp(const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void SideJump(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void StartShimmy(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void ClimbUp(UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	bool TryClimbDown(UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void PerformBackJump(UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void WallEject(UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void PerformLedgeJump(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);

	// --- Transition & Montages ---
	void Internal_StartTransition(UAnimMontage* MontageToPlay, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner, float CustomDuration = -1.0f, EThreadTalesLedgeJumpDistanceType DistanceType = EThreadTalesLedgeJumpDistanceType::Short);
	void CrossfadeToHangMontage(UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void StopAllClimbingMontages(AThreadTalesCharacter* CharacterOwner);
	void SetClimbDownCollision(bool bEnable, AThreadTalesCharacter* CharacterOwner);

	// --- Montage Callbacks ---
	void OnClimbMontageEnded(UAnimMontage* Montage, bool bInterrupted, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void OnClimbMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void OnClimbDownMontageEnded(UAnimMontage* Montage, bool bInterrupted, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);
	void OnClimbDownMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, UThreadTalesTraversalComponent* OwnerComp, AThreadTalesCharacter* CharacterOwner);

	// --- State Queries ---
	EThreadTalesClimbingSubState GetClimbingSubState() const { return ClimbingSubState; }
	void SetClimbingSubState(EThreadTalesClimbingSubState NewSubState) { ClimbingSubState = NewSubState; }
	bool IsHanging() const { return ClimbingSubState == EThreadTalesClimbingSubState::IdleHang; }
	bool IsTransitioning() const { return ClimbingSubState == EThreadTalesClimbingSubState::JumpTransition || ClimbingSubState == EThreadTalesClimbingSubState::Shimmying; }
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

	// Cooldown & debounce timers
	float RegrabCooldownTimer;
	float ClimbingInputDebounceTimer;
	float ShimmyCooldownTimer;

	// Directional jump table lookup
	FThreadTalesJumpConfigTable JumpConfigTable;
};
