// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_WeaponHitbox.generated.h"

/**
 * AnimNotifyState placed during the active swing frames of an attack.
 * Enables the weapon hitbox at NotifyBegin and disables it at NotifyEnd.
 */
UCLASS(const, hidecategories=Object, collapsecategories, meta=(DisplayName="Weapon Hitbox (Damage)"))
class THREADTALES_API UAnimNotifyState_WeaponHitbox : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_WeaponHitbox();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	/** If true, this hitbox operates on an unarmed body limb instead of an equipped weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitbox", meta = (DisplayName = "Unarmed Strike"))
	bool bUnarmedStrike = false;

	/** Socket or bone name driving the attack volume (e.g. Socket_Hand_R, ladybug-r-hand). Only used if Unarmed Strike is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitbox", meta = (DisplayName = "Hitbox Socket / Bone Name", EditCondition = "bUnarmedStrike"))
	FName HitboxSocketName = NAME_None;
};
