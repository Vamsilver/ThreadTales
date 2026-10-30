// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "ThreadTalesClimbingMontageJumps.generated.h"

class UAnimMontage;

/**
 * Short directional ledge-to-ledge jump montages (braced and freehang).
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingMontagesShortJump
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Short", meta = (ToolTip = "Hop up montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarClimbHopUpMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Short", meta = (ToolTip = "Jump up-right montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarClimbJumpUpRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Short", meta = (ToolTip = "Jump right montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarClimbJumpRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Short", meta = (ToolTip = "Jump down-right montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarClimbJumpDownRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Short", meta = (ToolTip = "Hop down montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarClimbHopDownMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Short", meta = (ToolTip = "Jump down-left montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarClimbJumpDownLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Short", meta = (ToolTip = "Jump left montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarClimbJumpLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Short", meta = (ToolTip = "Jump up-left montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarClimbJumpUpLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Short", meta = (ToolTip = "Hop up montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> ClimbHopUpMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Short", meta = (ToolTip = "Jump up-right montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> ClimbJumpUpRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Short", meta = (ToolTip = "Jump right montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> ClimbJumpRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Short", meta = (ToolTip = "Jump down-right montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> ClimbJumpDownRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Short", meta = (ToolTip = "Hop down montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> ClimbHopDownMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Short", meta = (ToolTip = "Jump down-left montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> ClimbJumpDownLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Short", meta = (ToolTip = "Jump left montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> ClimbJumpLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Short", meta = (ToolTip = "Jump up-left montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> ClimbJumpUpLeftMontage;
};

/**
 * Long directional ledge-to-ledge jump montages (braced and freehang).
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingMontagesLongJump
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Long", meta = (ToolTip = "Long hop up montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarLongHopUpMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Long", meta = (ToolTip = "Long jump up-right montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarLongJumpUpRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Long", meta = (ToolTip = "Long jump right montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarLongJumpRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Long", meta = (ToolTip = "Long jump down-right montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarLongJumpDownRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Long", meta = (ToolTip = "Long hop down montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarLongHopDownMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Long", meta = (ToolTip = "Long jump down-left montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarLongJumpDownLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Long", meta = (ToolTip = "Long jump left montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarLongJumpLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Long", meta = (ToolTip = "Long jump up-left montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarLongJumpUpLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Long", meta = (ToolTip = "Long hop up montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> ClimbLongHopUpMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Long", meta = (ToolTip = "Long jump up-right montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> ClimbLongJumpUpRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Long", meta = (ToolTip = "Long jump right montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> ClimbLongJumpRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Long", meta = (ToolTip = "Long jump down-right montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> ClimbLongJumpDownRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Long", meta = (ToolTip = "Long hop down montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> ClimbLongHopDownMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Long", meta = (ToolTip = "Long jump down-left montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> ClimbLongJumpDownLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Long", meta = (ToolTip = "Long jump left montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> ClimbLongJumpLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Long", meta = (ToolTip = "Long jump up-left montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> ClimbLongJumpUpLeftMontage;
};

/**
 * Leap directional ledge-to-ledge jump montages (braced and freehang).
 */
USTRUCT(BlueprintType)
struct THREADTALES_API FThreadTalesClimbingMontagesLeapJump
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Leap", meta = (ToolTip = "Leap hop up montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarLeapHopUpMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Leap", meta = (ToolTip = "Leap jump up-right montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarLeapJumpUpRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Leap", meta = (ToolTip = "Leap jump right montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarLeapJumpRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Leap", meta = (ToolTip = "Leap jump down-right montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarLeapJumpDownRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Leap", meta = (ToolTip = "Leap hop down montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarLeapHopDownMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Leap", meta = (ToolTip = "Leap jump down-left montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarLeapJumpDownLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Leap", meta = (ToolTip = "Leap jump left montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarLeapJumpLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Leap", meta = (ToolTip = "Leap jump up-left montage for freehang."))
	TSoftObjectPtr<UAnimMontage> BarLeapJumpUpLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Leap", meta = (ToolTip = "Leap hop up montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> LeapHopUpMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Leap", meta = (ToolTip = "Leap jump up-right montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> LeapJumpUpRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Leap", meta = (ToolTip = "Leap jump right montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> LeapJumpRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Leap", meta = (ToolTip = "Leap jump down-right montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> LeapJumpDownRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Leap", meta = (ToolTip = "Leap hop down montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> LeapHopDownMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Leap", meta = (ToolTip = "Leap jump down-left montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> LeapJumpDownLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Leap", meta = (ToolTip = "Leap jump left montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> LeapJumpLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Climbing|Montages|Leap", meta = (ToolTip = "Leap jump up-left montage for braced hang."))
	TSoftObjectPtr<UAnimMontage> LeapJumpUpLeftMontage;
};
