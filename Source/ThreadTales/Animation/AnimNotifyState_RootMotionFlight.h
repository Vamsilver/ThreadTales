// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_RootMotionFlight.generated.h"

/**
 * AnimNotifyState that temporarily switches character movement mode to MOVE_Flying
 * during a jump or airborne attack, allowing vertical Root Motion (Z-axis) to drive
 * the capsule into the air without being clamped to the floor by PhysWalking.
 * Restores MOVE_Walking (or MOVE_Falling) at NotifyEnd.
 */
UCLASS(const, hidecategories = Object, collapsecategories, meta = (DisplayName = "Root Motion Flight (Airborne Z)"))
class THREADTALES_API UAnimNotifyState_RootMotionFlight : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_RootMotionFlight();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	/** If true, restores MOVE_Falling instead of MOVE_Walking if character is still high above ground at NotifyEnd */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (DisplayName = "Safe Ground Check On End"))
	bool bSafeGroundCheckOnEnd = true;

	/** If true, character remains in MOVE_Flying at NotifyEnd (e.g. for Takeoff transitions into sustained flight) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (DisplayName = "Maintain Flight On End"))
	bool bMaintainFlightOnEnd = false;
};
