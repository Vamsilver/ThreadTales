// Copyright Epic Games, Inc. All Rights Reserved.

#include "Abilities/ThreadTalesGameplayEffects.h"
#include "Abilities/ThreadTalesAttributeSet.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"

UThreadTalesGE_CostMelee::UThreadTalesGE_CostMelee()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo ModInfo;
	ModInfo.Attribute = UThreadTalesAttributeSet::GetStaminaAttribute();
	ModInfo.ModifierOp = EGameplayModOp::Additive;
	ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-15.0f));
	Modifiers.Add(ModInfo);
}

UThreadTalesGE_CostEvade::UThreadTalesGE_CostEvade()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo ModInfo;
	ModInfo.Attribute = UThreadTalesAttributeSet::GetStaminaAttribute();
	ModInfo.ModifierOp = EGameplayModOp::Additive;
	ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-20.0f));
	Modifiers.Add(ModInfo);
}

UThreadTalesGE_StaminaRegen::UThreadTalesGE_StaminaRegen()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = FScalableFloat(0.1f);

	FGameplayModifierInfo ModInfo;
	ModInfo.Attribute = UThreadTalesAttributeSet::GetStaminaAttribute();
	ModInfo.ModifierOp = EGameplayModOp::Additive;
	ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(2.5f)); // +25/s base
	Modifiers.Add(ModInfo);

	UTargetTagRequirementsGameplayEffectComponent* TagReqComp = CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("TargetTagRequirementsComp"));
	TagReqComp->OngoingTagRequirements.IgnoreTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_StaminaRegenBlocked);
	TagReqComp->OngoingTagRequirements.IgnoreTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Attacking);
	TagReqComp->OngoingTagRequirements.IgnoreTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Evading);
	GEComponents.Add(TagReqComp);
}

UThreadTalesGE_StaminaDelay::UThreadTalesGE_StaminaDelay()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.0f));

	UTargetTagsGameplayEffectComponent* TargetTagsComp = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComp"));
	FInheritedTagContainer TagContainerMods;
	TagContainerMods.Added.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_StaminaRegenBlocked);
	TargetTagsComp->SetAndApplyTargetTagChanges(TagContainerMods);
	GEComponents.Add(TargetTagsComp);
}

UThreadTalesGE_StaminaExhaustedDelay::UThreadTalesGE_StaminaExhaustedDelay()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.6f));

	UTargetTagsGameplayEffectComponent* TargetTagsComp = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComp"));
	FInheritedTagContainer TagContainerMods;
	TagContainerMods.Added.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_StaminaRegenBlocked);
	TargetTagsComp->SetAndApplyTargetTagChanges(TagContainerMods);
	GEComponents.Add(TargetTagsComp);
}

UThreadTalesGE_Damage::UThreadTalesGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Health Damage Modifier via SetByCaller
	FGameplayModifierInfo DamageMod;
	DamageMod.Attribute = UThreadTalesAttributeSet::GetDamageAttribute();
	DamageMod.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat DamageCaller;
	DamageCaller.DataTag = ThreadTalesGameplayTags::TAG_SetByCaller_Damage;
	DamageMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(DamageCaller);
	Modifiers.Add(DamageMod);

	// Poise Damage Modifier via SetByCaller
	FGameplayModifierInfo PoiseMod;
	PoiseMod.Attribute = UThreadTalesAttributeSet::GetPoiseDamageAttribute();
	PoiseMod.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat PoiseCaller;
	PoiseCaller.DataTag = ThreadTalesGameplayTags::TAG_SetByCaller_PoiseDamage;
	PoiseMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(PoiseCaller);
	Modifiers.Add(PoiseMod);
}

UThreadTalesGE_Stagger::UThreadTalesGE_Stagger()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.2f));

	UTargetTagsGameplayEffectComponent* TargetTagsComp = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComp"));
	FInheritedTagContainer TagContainerMods;
	TagContainerMods.Added.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered);
	TargetTagsComp->SetAndApplyTargetTagChanges(TagContainerMods);
	GEComponents.Add(TargetTagsComp);
}

UThreadTalesGE_HitReact::UThreadTalesGE_HitReact()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.4f));

	UTargetTagsGameplayEffectComponent* TargetTagsComp = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComp"));
	FInheritedTagContainer TagContainerMods;
	TagContainerMods.Added.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_HitReact);
	TargetTagsComp->SetAndApplyTargetTagChanges(TagContainerMods);
	GEComponents.Add(TargetTagsComp);
}

UThreadTalesGE_PoiseRegen::UThreadTalesGE_PoiseRegen()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = FScalableFloat(0.1f);

	// Driven continuously at 60fps in AThreadTalesCharacter::Tick with Sekiro 2x Block concentration multiplier and PoiseRegenDelay
	FGameplayModifierInfo ModInfo;
	ModInfo.Attribute = UThreadTalesAttributeSet::GetPoiseAttribute();
	ModInfo.ModifierOp = EGameplayModOp::Additive;
	ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.0f));
	Modifiers.Add(ModInfo);

	UTargetTagRequirementsGameplayEffectComponent* TagReqComp = CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("TargetTagRequirementsComp"));
	TagReqComp->OngoingTagRequirements.IgnoreTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered);
	TagReqComp->OngoingTagRequirements.IgnoreTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_HitReact);
	GEComponents.Add(TagReqComp);
}
