// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_AttachWeapon.generated.h"

/**
 * AnimNotify that triggers weapon re-attachment to Hand or Sheath during Draw/Sheath montage.
 */
UCLASS(const, hidecategories=Object, collapsecategories, meta=(DisplayName="Attach Weapon (Hand/Sheath)"))
class THREADTALES_API UAnimNotify_AttachWeapon : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_AttachWeapon();

	/** If true, attaches weapon to Hand. If false, attaches weapon to Sheath. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	bool bAttachToHand = true;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
