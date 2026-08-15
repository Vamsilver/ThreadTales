// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

UThreadTalesAttributeSet::UThreadTalesAttributeSet()
{
	// Initialize default values (can be overridden by data tables later)
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
}

void UThreadTalesAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UThreadTalesAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UThreadTalesAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UThreadTalesAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Clamp health before it changes
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
}

void UThreadTalesAttributeSet::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// Clamp health after a gameplay effect finishes applying
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
}

void UThreadTalesAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UThreadTalesAttributeSet, Health, OldHealth);
}

void UThreadTalesAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UThreadTalesAttributeSet, MaxHealth, OldMaxHealth);
}
