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

	/** Initiates mantle/climb up onto the ledge (triggered by Jump while hanging) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	void ClimbUp();

	/** Releases the ledge and drops down into falling mode (triggered by Crouch/Drop key) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Traversal")
	void ReleaseLedge();

protected:
	virtual void BeginPlay() override;

	/** Sets the traversal state locally and manages GAS tag */
	void SetTraversalState(EThreadTalesTraversalState NewState);

	/** Callback when mantle climb up montage finishes */
	UFUNCTION()
	void OnClimbMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// ==========================================
	// NETWORKING & RPCs
	// ==========================================

	UPROPERTY(ReplicatedUsing = OnRep_TraversalState, BlueprintReadOnly, Category = "ThreadTales|Traversal")
	EThreadTalesTraversalState CurrentTraversalState;

	UFUNCTION()
	virtual void OnRep_TraversalState(EThreadTalesTraversalState PrevState);

	UFUNCTION(Server, Reliable)
	void Server_StartLedgeClimb(const FVector& LedgeLocation, const FVector& WallNormal);

	UFUNCTION(Server, Reliable)
	void Server_ClimbUp();

	UFUNCTION(Server, Reliable)
	void Server_ReleaseLedge();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StartLedgeClimb(const FVector& LedgeLocation, const FVector& WallNormal);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayClimbMontage(UAnimMontage* MontageToPlay);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ReleaseLedge();

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

	/** Horizontal distance from wall surface when hanging */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Offsets")
	float HangWallOffset;

	/** Vertical distance below ledge edge when hanging */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Offsets")
	float HangHeightOffset;

	/** Minimum time before character can regrab a ledge after dropping */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Tuning")
	float RegrabCooldownDuration;

	/** Root motion AnimMontage played when climbing on top of a ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages")
	TObjectPtr<UAnimMontage> ClimbOnTopMontage;

	/** Optional AnimMontage played while hanging (looped or single pose) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Traversal|Montages")
	TObjectPtr<UAnimMontage> HangMontage;

	/** Draw debug visual lines, spheres and normals for ledge detection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Traversal|Debug")
	bool bDrawDebugTraces;

private:
	UPROPERTY(Transient)
	TObjectPtr<AThreadTalesCharacter> CharacterOwner;

	/** Timestamp or timer tracking regrab cooldown */
	float RegrabCooldownTimer;

	/** True while climb up mantle montage is executing */
	bool bIsClimbingUp;

	/** Cached target ledge position and wall normal */
	FVector CachedLedgeLocation;
	FVector CachedWallNormal;
	FVector CachedHangLocation;
	FRotator CachedHangRotation;
};
