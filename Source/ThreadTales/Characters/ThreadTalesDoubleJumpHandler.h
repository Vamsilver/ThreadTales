// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThreadTalesDoubleJumpHandler.generated.h"

class AThreadTalesCharacter;
class UAnimMontage;
class USoundBase;
class UNiagaraSystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDoubleJumpDelegate, AThreadTalesCharacter*, Character);

/**
 * Component managing character mid-air double jump mechanics, physical jump launches,
 * GAS ability triggering, sound, and particle VFX.
 * Extracted from AThreadTalesCharacter to modularize locomotion capabilities.
 */
UCLASS(ClassGroup=(ThreadTales), meta=(BlueprintSpawnableComponent, DisplayName="ThreadTales Double Jump Handler"))
class THREADTALES_API UThreadTalesDoubleJumpHandler : public UActorComponent
{
	GENERATED_BODY()

public:
	UThreadTalesDoubleJumpHandler();

	/** Maximum allowed consecutive jumps (1 = single jump, 2 = double jump, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement", meta = (ClampMin = 1, ClampMax = 5))
	int32 MaxJumpCount = 2;

	/** Vertical velocity (jump force) for the double jump. Default is 500.0f. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement", meta = (ClampMin = "200.0", ClampMax = "1200.0"))
	float DoubleJumpZVelocity = 500.0f;

	/** Horizontal push strength for directional double jumps. Default is 800.0f. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement", meta = (ClampMin = "0.0", ClampMax = "2000.0"))
	float DoubleJumpHorizontalVelocity = 800.0f;

	/** Optional AnimMontage to play when performing a double jump (fallback / default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement|Montages")
	TObjectPtr<UAnimMontage> DoubleJumpMontage;

	/** Neutral / In-Place double jump montage (when no WASD input) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement|Montages")
	TObjectPtr<UAnimMontage> DoubleJumpNeutralMontage;

	/** Forward double jump montage (W) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement|Montages")
	TObjectPtr<UAnimMontage> DoubleJumpForwardMontage;

	/** Backward double jump montage (S) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement|Montages")
	TObjectPtr<UAnimMontage> DoubleJumpBackwardMontage;

	/** Left double jump montage (A) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement|Montages")
	TObjectPtr<UAnimMontage> DoubleJumpLeftMontage;

	/** Right double jump montage (D) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement|Montages")
	TObjectPtr<UAnimMontage> DoubleJumpRightMontage;

	/** Optional Sound to play on double jump */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement")
	TObjectPtr<USoundBase> DoubleJumpSound;

	/** Optional VFX/Niagara effect to spawn on double jump */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement")
	TObjectPtr<UNiagaraSystem> DoubleJumpEffect;

	/** Blend out time when stopping double jump montage upon early landing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Movement|Animation", meta = (ClampMin = "0.05", ClampMax = "0.5"))
	float LandedMontageStopBlendTime = 0.15f;

	/** Event broadcasted when double jump is performed */
	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Movement")
	FOnDoubleJumpDelegate OnDoubleJumped;

	/** Blueprint event called when double jump occurs */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|Movement")
	void BP_OnDoubleJump();

	/** Returns true if character is currently executing mid-air double jump */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Movement")
	bool IsDoubleJumping() const;

	/** Attempts to perform mid-air double jump */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Movement")
	void PerformDoubleJump();

	/** Triggered when double jump executes (activates GAS ability, broadcasts delegates, plays feedback) */
	bool OnDoubleJump();

	/** Resets double jump state upon character landing */
	void ResetDoubleJumpOnLanded();

	/** Evaluates whether Jump input in air should trigger double jump */
	bool TryTriggerInAirJump();

	/** Returns vertical impulse applied during double jump */
	float GetDoubleJumpZVelocity() const { return DoubleJumpZVelocity; }

	/** Sets vertical impulse applied during double jump */
	void SetDoubleJumpZVelocity(float InZVelocity) { DoubleJumpZVelocity = InZVelocity; }

	/** Returns horizontal impulse applied during directional double jump */
	float GetDoubleJumpHorizontalVelocity() const { return DoubleJumpHorizontalVelocity; }

	/** Sets horizontal impulse applied during directional double jump */
	void SetDoubleJumpHorizontalVelocity(float InHorizontalVelocity) { DoubleJumpHorizontalVelocity = InHorizontalVelocity; }

	/** Returns true if character already used their mid-air jump before landing */
	bool HasDoubleJumpedInAir() const { return bHasDoubleJumpedInAir; }

	/** Sets whether mid-air double jump has been consumed */
	void SetHasDoubleJumpedInAir(bool bInHasDoubleJumped) { bHasDoubleJumpedInAir = bInHasDoubleJumped; }

	/** Returns max allowed jump count */
	int32 GetMaxJumpCount() const { return MaxJumpCount; }

	/** Sets max allowed jump count */
	void SetMaxJumpCount(int32 NewMaxJumpCount) { MaxJumpCount = FMath::Max(1, NewMaxJumpCount); }

protected:
	AThreadTalesCharacter* GetOwningCharacter() const;

	/** Whether the character has already used double jump while airborne */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Movement", meta = (AllowPrivateAccess = "true"))
	bool bHasDoubleJumpedInAir = false;
};
