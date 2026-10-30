// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/ThreadTalesBaseCharacter.h"
#include "ThreadTalesCharacter.generated.h"

class UThreadTalesInputConfig;
class UThreadTalesTraversalComponent;
class UThreadTalesCharacterStateConfig;
class UThreadTalesTargetLockComponent;
class UThreadTalesInteractionDetectorComponent;
class UThreadTalesDoubleJumpHandler;
class UThreadTalesAbilityInputDispatcher;
class UMotionWarpingComponent;
class UInputAction;
struct FInputActionValue;

/**
 * Player Character class in ThreadTales.
 * Inherits unified GAS and combat baseline from AThreadTalesBaseCharacter,
 * and adds player-specific systems: Traversal/Parkour, Target Lock,
 * Contextual Interactions, Mid-Air Double Jump, and Enhanced Input routing.
 */
UCLASS(config=Game)
class THREADTALES_API AThreadTalesCharacter : public AThreadTalesBaseCharacter
{
	GENERATED_BODY()

public:
	AThreadTalesCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// ==========================================
	// TRAVERSAL & PARKOUR
	// ==========================================

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	UThreadTalesTraversalComponent* GetTraversalComponent() const { return TraversalComponent; }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Animation")
	UMotionWarpingComponent* GetMotionWarpingComponent() const { return MotionWarpingComponent; }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Traversal")
	bool IsClimbing() const;

	bool IsFreehangClimbing() const;
	bool IsRegrabCooldownActive() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ThreadTales|State")
	bool IsParkourBlocked() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ThreadTales|State")
	bool IsTraversalBlocked() const;

	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	void RequestReleaseLedge();

	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	bool TryClimbDown();

	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	void RequestCrouchOrClimbDown();

	void SetMantleExitDebounce(float Duration) { MantleExitDebounceTimer = FMath::Max(MantleExitDebounceTimer, Duration); }
	bool IsMantleExitDebounceActive() const { return MantleExitDebounceTimer > 0.0f; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement|Parkour", meta = (AllowPrivateAccess = "true"))
	bool bEnableGaspalsParkour = true;

	UFUNCTION(BlueprintNativeEvent, Category = "ThreadTales|Movement|Parkour")
	bool TryGaspalsTraversal();
	virtual bool TryGaspalsTraversal_Implementation();

	// ==========================================
	// TARGET LOCK
	// ==========================================

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Combat")
	UThreadTalesTargetLockComponent* GetTargetLockComponent() const { return TargetLockComponent; }

	// ==========================================
	// CONTEXTUAL INTERACTIONS & HAND IK
	// ==========================================

	UThreadTalesInteractionDetectorComponent* GetInteractionDetectorComponent() const { return InteractionDetectorComponent; }
	void SetInteractionHandIKTarget(const FVector& TargetLocation, float Alpha);
	void ClearInteractionHandIK();
	FVector GetInteractionHandIKLocation() const;
	float GetInteractionHandIKAlpha() const;
	bool IsInteractionHandIKEnabled() const;
	FName GetInteractionHandIKBoneName() const;

	// ==========================================
	// DOUBLE JUMP & INPUT DISPATCHER
	// ==========================================

	UThreadTalesDoubleJumpHandler* GetDoubleJumpHandler() const { return DoubleJumpHandler; }
	UThreadTalesAbilityInputDispatcher* GetAbilityInputDispatcher() const { return AbilityInputDispatcher; }
	const UThreadTalesInputConfig* GetInputConfig() const { return InputConfig; }
	const UThreadTalesCharacterStateConfig* GetStateConfig() const { return StateConfig; }

	virtual void OnDoubleJump();
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;
	bool IsDoubleJumping() const;
	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f, bool bForce = false) override;
	virtual void FaceRotation(FRotator NewControlRotation, float DeltaTime = 0.f) override;
	virtual void Jump() override;
	virtual void StopJumping() override;
	virtual void Crouch(bool bClientSimulation = false) override;
	virtual void UnCrouch(bool bClientSimulation = false) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void CheckJumpInput(float DeltaTime) override;
	virtual bool CanJumpInternal_Implementation() const override;
	virtual void OnJumped_Implementation() override;
	void SetMaxJumpCount(int32 NewMaxJumpCount);
	void PerformDoubleJump();

	// ==========================================
	// CHARACTER DEBUG / GENDER TOGGLE
	// ==========================================

	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Debug")
	void DebugSwitchCharacter(TSubclassOf<AThreadTalesCharacter> NewCharacterClass);

	UFUNCTION(Server, Reliable, Category = "ThreadTales|Debug")
	void ServerDebugSwitchCharacter(TSubclassOf<AThreadTalesCharacter> NewCharacterClass);

	UFUNCTION(Exec, BlueprintCallable, Category = "ThreadTales|Debug")
	void DebugSwitchToMale();

	UFUNCTION(Exec, BlueprintCallable, Category = "ThreadTales|Debug")
	void DebugSwitchToFemale();

	UFUNCTION(Exec, BlueprintCallable, Category = "ThreadTales|Debug")
	void DebugToggleGender();

	/** Alternate hero class for debug hot-swapping / toggle gender (Data-Driven, zero hardcoded paths) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Debug")
	TSubclassOf<AThreadTalesCharacter> AlternateCharacterClass;

	TSubclassOf<AThreadTalesCharacter> GetAlternateCharacterClass() const { return AlternateCharacterClass; }

	virtual bool CanAimOrBlock() const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Debug", meta = (AllowPrivateAccess = "true"))
	bool bEnableOverlayDebugTelemetry = false;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UThreadTalesTraversalComponent> TraversalComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motion Warping", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UThreadTalesTargetLockComponent> TargetLockComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UThreadTalesInteractionDetectorComponent> InteractionDetectorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UThreadTalesDoubleJumpHandler> DoubleJumpHandler;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UThreadTalesAbilityInputDispatcher> AbilityInputDispatcher;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const UThreadTalesInputConfig> InputConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const UInputAction> TargetSwitchAction;

	void Input_TargetSwitch(const FInputActionValue& Value);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|State", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const UThreadTalesCharacterStateConfig> StateConfig;

	void Input_AbilityInputTagPressed(FGameplayTag InputTag);
	void Input_AbilityInputTagReleased(FGameplayTag InputTag);

	TArray<uint32> AbilityBindHandles;

private:
	float MantleExitDebounceTimer = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "ThreadTales|Camera", meta = (ClampMin = "-89.0", ClampMax = "0.0"))
	float CameraPitchMin = -40.0f;

	UPROPERTY(EditDefaultsOnly, Category = "ThreadTales|Camera", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float CameraPitchMax = 45.0f;

	float OverlayTelemetryTimer = 0.0f;
	float LastHandsDistance = 0.0f;
	FRotator LastHandRRot = FRotator::ZeroRotator;
	FRotator LastHandLRot = FRotator::ZeroRotator;
};
