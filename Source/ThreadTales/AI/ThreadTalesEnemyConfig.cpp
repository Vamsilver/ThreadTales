// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ThreadTalesEnemyConfig.h"
#include "Animation/AnimMontage.h"
#include "Combat/ThreadTalesWeapon.h"

UThreadTalesEnemyConfig::UThreadTalesEnemyConfig()
{
	EnemyDisplayName = FText::FromString(TEXT("Противник"));
	AttackComboMontages.Add(TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/Attack/AM_Attack_Combo1.AM_Attack_Combo1"))));
	AttackComboMontages.Add(TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/Attack/AM_Attack_Combo2.AM_Attack_Combo2"))));
	AttackComboMontages.Add(TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/Attack/AM_Attack_Combo3.AM_Attack_Combo3"))));
	AttackComboMontages.Add(TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/Attack/AM_Attack_Combo4.AM_Attack_Combo4"))));

}
