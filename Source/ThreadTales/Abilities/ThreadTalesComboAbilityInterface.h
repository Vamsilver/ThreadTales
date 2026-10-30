// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ThreadTalesComboAbilityInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UThreadTalesComboAbilityInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Common interface for abilities supporting anim-notify driven combo windows and early recovery cancels.
 */
class THREADTALES_API IThreadTalesComboAbilityInterface
{
	GENERATED_BODY()

public:
	/** Called by UAnimNotifyState_ComboWindow when the combo cancel window opens */
	virtual void NotifyComboWindowBegin() = 0;

	/** Called by UAnimNotifyState_ComboWindow when the combo cancel window closes */
	virtual void NotifyComboWindowEnd() = 0;

	/** Called when attack input is received to buffer or immediately advance combo */
	virtual void NotifyComboInput() = 0;

	/** Attempts to cancel active ability and return cleanly to locomotion if movement input is active */
	virtual bool TryCancelToLocomotion() { return false; }
};
