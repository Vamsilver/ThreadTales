// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_AttachSheath.generated.h"

/**
 * AnimNotify that triggers scabbard / sheath re-attachment to Hand or Hip during Draw/Sheath montage.
 */
UCLASS(const, hidecategories=Object, collapsecategories, meta=(DisplayName="Attach Sheath (Hand/Hip)"))
class THREADTALES_API UAnimNotify_AttachSheath : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_AttachSheath();

	/** If true, attaches sheath to left Hand (scabbard_hand_lSocket). If false, attaches sheath to Hip (sheath_hip_lSocket). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	bool bAttachToHand = true;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
