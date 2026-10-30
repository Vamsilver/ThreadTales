// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ThreadTalesMirrorAnimHelper.generated.h"

class USkeleton;
class UMirrorDataTable;

/**
 * Helper function library for procedural MirrorDataTable creation.
 */
UCLASS()
class THREADTALES_API UThreadTalesMirrorAnimHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Builds and populates an in-memory or persistent package UMirrorDataTable for the given skeleton */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Animation|Mirror")
	static UMirrorDataTable* CreateMannequinMirrorTable(USkeleton* Skeleton, const FString& PackagePath);

	/** Configures start/end playback window and blend times for a single-sequence montage */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Animation|Mirror")
	static bool ConfigureMontagePlayWindow(UAnimMontage* Montage, float StartTime, float EndTime, float BlendInTime = 0.15f, float BlendOutTime = 0.20f);
};
