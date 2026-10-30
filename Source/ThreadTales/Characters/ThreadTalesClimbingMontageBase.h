// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "ThreadTalesClimbingMontageBase.generated.h"

class UAnimMontage;

/**
 * Base climbing and idle hang montages (braced and freehang).
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingMontagesBase
{
	GENERATED_BODY()

	/** Looping idle hang montage when braced against wall */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Base", meta = (ToolTip = "Looping idle hang montage when braced against wall."))
	TSoftObjectPtr<UAnimMontage> HangMontage;

	/** Looping idle hang montage when freehanging (feet dangling, no wall) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Base", meta = (ToolTip = "Looping idle hang montage when freehanging (feet dangling, no wall)."))
	TSoftObjectPtr<UAnimMontage> BarHangMontage;

	/** Root motion mantle montage onto solid floor */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Base", meta = (ToolTip = "Root motion mantle montage onto solid floor."))
	TSoftObjectPtr<UAnimMontage> ClimbOnTopMontage;

	/** Root motion mantle montage onto freehang bar/pipe */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Base", meta = (ToolTip = "Root motion mantle montage onto freehang bar/pipe."))
	TSoftObjectPtr<UAnimMontage> BarClimbOnTopMontage;

	/** Root motion climb down montage from standing into braced hang */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Base", meta = (ToolTip = "Root motion climb down montage from standing into braced hang."))
	TSoftObjectPtr<UAnimMontage> ClimbDownMontage;

	/** Root motion climb down montage from standing into freehang */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Base", meta = (ToolTip = "Root motion climb down montage from standing into freehang."))
	TSoftObjectPtr<UAnimMontage> BarClimbDownMontage;

	/** Kick-off from wall / leap to opposing ledge montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Base", meta = (ToolTip = "Kick-off from wall / leap to opposing ledge montage."))
	TSoftObjectPtr<UAnimMontage> ClimbBackJumpMontage;

	/** Kick-off from wall / leap to opposing ledge montage when freehanging (falls back to ClimbBackJumpMontage) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Base", meta = (ToolTip = "Kick-off from wall / leap to opposing ledge montage when freehanging."))
	TSoftObjectPtr<UAnimMontage> BarClimbBackJumpMontage;

	/** Kick-off leap to opposing ledge that is significantly higher than current ledge (Back Jump Up) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Base", meta = (ToolTip = "Kick-off leap to opposing ledge that is significantly higher than current ledge."))
	TSoftObjectPtr<UAnimMontage> ClimbBackJumpUpMontage;

	/** Kick-off leap to opposing ledge higher than current ledge with left-hand wall support (mirrored) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Base", meta = (ToolTip = "Kick-off leap to opposing ledge that is significantly higher than current ledge with left-hand wall support."))
	TSoftObjectPtr<UAnimMontage> ClimbBackJumpUpLeftMontage;

	/** Montage played when grabbing a ledge from the air (braced against wall) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Base", meta = (ToolTip = "Montage played when grabbing a ledge from the air (braced against wall)."))
	TSoftObjectPtr<UAnimMontage> GrabFromAirMontage;

	/** Montage played when grabbing a ledge from the air (freehanging) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Base", meta = (ToolTip = "Montage played when grabbing a ledge from the air (freehanging)."))
	TSoftObjectPtr<UAnimMontage> BarGrabFromAirMontage;

	/** Shimmy step left along continuous ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Base", meta = (ToolTip = "Shimmy step left along continuous ledge."))
	TSoftObjectPtr<UAnimMontage> ShimmyLeftMontage;

	/** Shimmy step right along continuous ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Base", meta = (ToolTip = "Shimmy step right along continuous ledge."))
	TSoftObjectPtr<UAnimMontage> ShimmyRightMontage;

	/** Freehang shimmy step left along continuous bar/pipe (falls back to ShimmyLeftMontage) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Base", meta = (ToolTip = "Freehang shimmy step left along continuous bar/pipe."))
	TSoftObjectPtr<UAnimMontage> BarShimmyLeftMontage;

	/** Freehang shimmy step right along continuous bar/pipe (falls back to ShimmyRightMontage) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Base", meta = (ToolTip = "Freehang shimmy step right along continuous bar/pipe."))
	TSoftObjectPtr<UAnimMontage> BarShimmyRightMontage;
};

/**
 * Corner wrapping animation montages (outer convex and inner concave).
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingMontagesCorner
{
	GENERATED_BODY()

	/** Root motion montage for wrapping around an outer (convex) corner to the left */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Corner", meta = (ToolTip = "Root motion montage for wrapping around an outer (convex) corner to the left."))
	TSoftObjectPtr<UAnimMontage> CornerOuterLeftMontage;

	/** Root motion montage for wrapping around an outer (convex) corner to the right */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Corner", meta = (ToolTip = "Root motion montage for wrapping around an outer (convex) corner to the right."))
	TSoftObjectPtr<UAnimMontage> CornerOuterRightMontage;

	/** Root motion montage for sliding into an inner (concave) corner to the left */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Corner", meta = (ToolTip = "Root motion montage for sliding into an inner (concave) corner to the left."))
	TSoftObjectPtr<UAnimMontage> CornerInnerLeftMontage;

	/** Root motion montage for sliding into an inner (concave) corner to the right */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Corner", meta = (ToolTip = "Root motion montage for sliding into an inner (concave) corner to the right."))
	TSoftObjectPtr<UAnimMontage> CornerInnerRightMontage;

	/** Freehang root motion montage for wrapping around an outer corner to the left */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Corner", meta = (ToolTip = "Freehang root motion montage for wrapping around an outer corner to the left."))
	TSoftObjectPtr<UAnimMontage> BarCornerOuterLeftMontage;

	/** Freehang root motion montage for wrapping around an outer corner to the right */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Corner", meta = (ToolTip = "Freehang root motion montage for wrapping around an outer corner to the right."))
	TSoftObjectPtr<UAnimMontage> BarCornerOuterRightMontage;

	/** Freehang root motion montage for sliding into an inner corner to the left */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Corner", meta = (ToolTip = "Freehang root motion montage for sliding into an inner corner to the left."))
	TSoftObjectPtr<UAnimMontage> BarCornerInnerLeftMontage;

	/** Freehang root motion montage for sliding into an inner corner to the right */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Corner", meta = (ToolTip = "Freehang root motion montage for sliding into an inner corner to the right."))
	TSoftObjectPtr<UAnimMontage> BarCornerInnerRightMontage;
};
