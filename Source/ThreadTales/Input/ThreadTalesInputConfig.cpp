// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesInputConfig.h"
#include "ThreadTales.h"
#include "InputAction.h"

UThreadTalesInputConfig::UThreadTalesInputConfig()
{
}

const UInputAction* UThreadTalesInputConfig::FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	for (const FThreadTalesInputAction& Action : NativeInputActions)
	{
		if (Action.InputAction && Action.InputTag.MatchesTagExact(InputTag))
		{
			return Action.InputAction;
		}
	}

	if (bLogNotFound)
	{
		UE_LOG(LogThreadTales, Warning, TEXT("[InputConfig] Can't find NativeInputAction for InputTag [%s] on InputConfig [%s]."), 
			*InputTag.ToString(), *GetNameSafe(this));
	}

	return nullptr;
}

const UInputAction* UThreadTalesInputConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	for (const FThreadTalesInputAction& Action : AbilityInputActions)
	{
		if (Action.InputAction && Action.InputTag.MatchesTagExact(InputTag))
		{
			return Action.InputAction;
		}
	}

	if (bLogNotFound)
	{
		UE_LOG(LogThreadTales, Warning, TEXT("[InputConfig] Can't find AbilityInputAction for InputTag [%s] on InputConfig [%s]."), 
			*InputTag.ToString(), *GetNameSafe(this));
	}

	return nullptr;
}
