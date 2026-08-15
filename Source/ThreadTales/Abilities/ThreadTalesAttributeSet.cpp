// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesAttributeSet.h"
#include "ThreadTales.h"
#include "Character/ThreadTalesCharacter.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

UThreadTalesAttributeSet::UThreadTalesAttributeSet()
{
	// Initialize default values
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitDamage(0.0f);
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

	FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
	AActor* TargetActor = nullptr;
	ACharacter* TargetCharacter = nullptr;
	if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
	{
		TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
		TargetCharacter = Cast<ACharacter>(TargetActor);
	}

	AActor* SourceActor = Context.GetInstigator();

	// Process incoming Damage
	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float LocalDamageDone = GetDamage();
		SetDamage(0.0f);

		if (LocalDamageDone > 0.0f)
		{
			const float NewHealth = GetHealth() - LocalDamageDone;
			SetHealth(FMath::Clamp(NewHealth, 0.0f, GetMaxHealth()));

			UE_LOG(LogThreadTalesCombat, Display, TEXT("[AttributeSet] %s took %.1f damage. Remaining Health: %.1f / %.1f"),
				TargetActor ? *TargetActor->GetName() : TEXT("Target"), LocalDamageDone, GetHealth(), GetMaxHealth());

			if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(TargetCharacter))
			{
				TTChar->HandleHealthChanged(GetHealth(), GetMaxHealth());
				if (GetHealth() <= 0.0f && TTChar->IsAlive())
				{
					TTChar->Die(SourceActor);
				}
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));

		if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(TargetCharacter))
		{
			TTChar->HandleHealthChanged(GetHealth(), GetMaxHealth());
			if (GetHealth() <= 0.0f && TTChar->IsAlive())
			{
				TTChar->Die(SourceActor);
			}
		}
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
