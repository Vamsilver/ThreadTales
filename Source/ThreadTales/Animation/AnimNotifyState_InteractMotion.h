// Copyright ThreadTales. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "AnimNotifyState_InteractMotion.generated.h"

/**
 * AnimNotifyState marking the physical interaction window in montages (e.g. pulling a lever, lifting a lid).
 * Designers place this state on any interaction montage with arbitrary timings.
 * At NotifyBegin, broadcasts Event.Interact.Motion.Begin to start hand tracking and motion sync.
 * At NotifyEnd, broadcasts Event.Interact.Motion.End to firmly lock the interactable prop in its final rotation.
 */
UCLASS(const, hidecategories = Object, collapsecategories, meta = (DisplayName = "Interact Motion State"))
class THREADTALES_API UAnimNotifyState_InteractMotion : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_InteractMotion();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	/** Name of the hand socket used for interaction contact (defaults to hand_r) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FName HandSocketName = FName("hand_r");

	/** Gameplay tag sent on NotifyBegin. Defaults to Event.Interact.Motion.Begin */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FGameplayTag BeginEventTag;

	/** Gameplay tag sent on NotifyEnd. Defaults to Event.Interact.Motion.End */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FGameplayTag EndEventTag;
};
