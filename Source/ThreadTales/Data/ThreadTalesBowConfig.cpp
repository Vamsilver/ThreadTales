// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/ThreadTalesBowConfig.h"
#include "Animation/AnimMontage.h"

int32 UThreadTalesBowConfig::EvaluateChargeTier(float HoldDurationSeconds) const
{
	if (HoldDurationSeconds >= LV3ChargeTimeSeconds)
	{
		return 3;
	}
	if (HoldDurationSeconds >= LV2ChargeTimeSeconds)
	{
		return 2;
	}
	return 1;
}

float UThreadTalesBowConfig::GetSpeedForTier(int32 ChargeTier) const
{
	switch (ChargeTier)
	{
	case 3:
		return LV3ArrowSpeed;
	case 2:
		return LV2ArrowSpeed;
	default:
		return LV1ArrowSpeed;
	}
}

float UThreadTalesBowConfig::GetGravityForTier(int32 ChargeTier) const
{
	switch (ChargeTier)
	{
	case 3:
		return LV3GravityScale;
	case 2:
		return LV2GravityScale;
	default:
		return LV1GravityScale;
	}
}

float UThreadTalesBowConfig::GetHealthDamageForTier(int32 ChargeTier) const
{
	switch (ChargeTier)
	{
	case 3:
		return LV3HealthDamage;
	case 2:
		return LV2HealthDamage;
	default:
		return LV1HealthDamage;
	}
}

float UThreadTalesBowConfig::GetPoiseDamageForTier(int32 ChargeTier) const
{
	switch (ChargeTier)
	{
	case 3:
		return LV3PoiseDamage;
	case 2:
		return LV2PoiseDamage;
	default:
		return LV1PoiseDamage;
	}
}

UAnimMontage* UThreadTalesBowConfig::GetReleaseMontageForTier(int32 ChargeTier) const
{
	switch (ChargeTier)
	{
	case 3:
		return ReleaseMontageLV3.LoadSynchronous();
	case 2:
		return ReleaseMontageLV2.LoadSynchronous();
	default:
		return ReleaseMontageLV1.LoadSynchronous();
	}
}

UAnimMontage* UThreadTalesBowConfig::GetAimReadyMontage() const
{
	return AimReadyMontage.LoadSynchronous();
}

UAnimMontage* UThreadTalesBowConfig::GetDrawHoldMontageForTier(int32 ChargeTier) const
{
	switch (ChargeTier)
	{
	case 3:
		if (UAnimMontage* MontageLV3 = DrawHoldMontageLV3.LoadSynchronous())
		{
			return MontageLV3;
		}
		break;
	case 2:
		if (UAnimMontage* MontageLV2 = DrawHoldMontageLV2.LoadSynchronous())
		{
			return MontageLV2;
		}
		break;
	default:
		if (UAnimMontage* MontageLV1 = DrawHoldMontageLV1.LoadSynchronous())
		{
			return MontageLV1;
		}
		break;
	}
	return DrawHoldMontage.LoadSynchronous();
}

UAnimMontage* UThreadTalesBowConfig::GetDrawHoldMontage() const
{
	return DrawHoldMontage.LoadSynchronous();
}

UAnimMontage* UThreadTalesBowConfig::GetParryMontage(int32 DirectionIndex) const
{
	if (BowParryMontages.IsValidIndex(DirectionIndex))
	{
		return BowParryMontages[DirectionIndex].LoadSynchronous();
	}
	return BowParryMontages.Num() > 0 ? BowParryMontages[0].LoadSynchronous() : nullptr;
}

UAnimMontage* UThreadTalesBowConfig::GetParryCounterMontage(int32 DirectionIndex) const
{
	if (BowParryCounterMontages.IsValidIndex(DirectionIndex))
	{
		return BowParryCounterMontages[DirectionIndex].LoadSynchronous();
	}
	return BowParryCounterMontages.Num() > 0 ? BowParryCounterMontages[0].LoadSynchronous() : nullptr;
}

