// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesClimbingConfig.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UThreadTalesClimbingConfig::UThreadTalesClimbingConfig()
{
}

#if WITH_EDITOR
EDataValidationResult UThreadTalesClimbingConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	if (Kinematics.Offsets.HangWallOffset <= 0.0f)
	{
		Context.AddError(FText::FromString(TEXT("HangWallOffset must be greater than zero.")));
		Result = EDataValidationResult::Invalid;
	}

	if (Kinematics.Timings.GrabCatchDuration <= 0.0f)
	{
		Context.AddError(FText::FromString(TEXT("GrabCatchDuration must be greater than zero.")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
