// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_EarlyLocomotionCancel.generated.h"

/**
 * AnimNotifyState placed near the recovery end of combat montages (e.g. Dodge-to-Shot counter, heavy attack recovery).
 * When active, if movement keys (WASD) or movement input are detected,
 * cleanly cancels the montage into locomotion with a smooth blend out.
 */
UCLASS(const, hidecategories=Object, collapsecategories, meta=(DisplayName="Early Locomotion Cancel Window"))
class THREADTALES_API UAnimNotifyState_EarlyLocomotionCancel : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_EarlyLocomotionCancel();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	/** Blend out time in seconds when early cancelling into locomotion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation|LocomotionCancel", meta = (ClampMin = "0.05", ClampMax = "0.50", ToolTip = "Blend out time in seconds when cancelling montage into locomotion."))
	float BlendOutTime = 0.35f;

private:
	bool CheckAndCancelToLocomotion(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) const;
};
