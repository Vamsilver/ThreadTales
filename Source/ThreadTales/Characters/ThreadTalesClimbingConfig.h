// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Characters/ThreadTalesClimbingMontageConfig.h"
#include "Characters/ThreadTalesClimbingKinematicsConfig.h"
#include "Characters/ThreadTalesClimbingTraversalConfig.h"
#include "Characters/ThreadTalesClimbingSensingConfig.h"
#include "ThreadTalesClimbingConfig.generated.h"

/**
 * Immutable Primary Data Asset holding all climbing montages, kinematics calibrations,
 * traversal mechanics, and spatial sensing parameters for a character archetype (Tier 2 Archetype Data).
 * Decoupled into four modular composite structures conforming to AAA UE5 C++ architecture.
 */
UCLASS(BlueprintType, Const)
class THREADTALES_API UThreadTalesClimbingConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UThreadTalesClimbingConfig();

	/** Animation montages for base hanging, corners, and directional jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Montages")
	FThreadTalesClimbingMontagesConfig Montages;

	/** Spatial positioning offsets, playback rates, and jump flight kinematics */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kinematics")
	FThreadTalesClimbingKinematicsConfig Kinematics;

	/** Traversal mechanics: shimmy, climb down, mantle, wall eject, and camera tuning */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Traversal")
	FThreadTalesClimbingTraversalConfig Traversal;

	/** Spatial sensing: wall detection, edge validation, freehang probe, and 3D radial scanner */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sensing")
	FThreadTalesClimbingSensingConfig Sensing;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
