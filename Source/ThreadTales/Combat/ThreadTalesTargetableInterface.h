// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ThreadTalesTargetableInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UThreadTalesTargetableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for any actor that can be targeted or locked onto by combat targeting systems.
 */
class THREADTALES_API IThreadTalesTargetableInterface
{
	GENERATED_BODY()

public:
	/** Returns true if this actor can currently be locked onto by the specified targeting actor */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat|Targeting")
	bool CanBeTargeted(const AActor* TargetingActor) const;

	/** Returns the 3D world location to lock the camera/aim on (e.g., chest/head socket) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat|Targeting")
	FVector GetTargetLockLocation(const AActor* TargetingActor) const;

	/** Called when a targeting actor locks onto this targetable actor */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat|Targeting")
	void OnTargetLocked(AActor* TargetingActor);

	/** Called when a targeting actor unlocks / disengages from this targetable actor */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat|Targeting")
	void OnTargetUnlocked(AActor* TargetingActor);

	virtual bool CanBeTargeted_Implementation(const AActor* TargetingActor) const { return true; }
	virtual FVector GetTargetLockLocation_Implementation(const AActor* TargetingActor) const { return FVector::ZeroVector; }
	virtual void OnTargetLocked_Implementation(AActor* TargetingActor) {}
	virtual void OnTargetUnlocked_Implementation(AActor* TargetingActor) {}
};
