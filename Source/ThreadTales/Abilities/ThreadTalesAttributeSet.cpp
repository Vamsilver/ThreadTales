#include "Abilities/ThreadTalesAttributeSet.h"
#include "Abilities/ThreadTalesStaminaConfig.h"
#include "Abilities/ThreadTalesPostureConfig.h"
#include "ThreadTales.h"
#include "AbilitySystemComponent.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Characters/ThreadTalesBaseCharacter.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesStaminaComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

UThreadTalesAttributeSet::UThreadTalesAttributeSet()
{
	// Initialize default values
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitStamina(100.0f);
	InitMaxStamina(100.0f);
	InitPoise(100.0f);
	InitMaxPoise(100.0f);
	InitDamage(0.0f);
	InitPoiseDamage(0.0f);
}

void UThreadTalesAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UThreadTalesAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UThreadTalesAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UThreadTalesAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UThreadTalesAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UThreadTalesAttributeSet, Poise, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UThreadTalesAttributeSet, MaxPoise, COND_None, REPNOTIFY_Always);
}

void UThreadTalesAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Clamp attributes before changing
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxStamina());
	}
	else if (Attribute == GetPoiseAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxPoise());
	}
}

void UThreadTalesAttributeSet::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		HandleIncomingDamage(Data);
	}
	else if (Data.EvaluatedData.Attribute == GetPoiseDamageAttribute())
	{
		HandleIncomingPoiseDamage(Data);
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));

		ACharacter* TargetCharacter = Data.Target.AbilityActorInfo.IsValid() ? Cast<ACharacter>(Data.Target.AbilityActorInfo->AvatarActor.Get()) : nullptr;
		if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(TargetCharacter))
		{
			TTChar->HandleHealthChanged(GetHealth(), GetMaxHealth());
			if (GetHealth() <= 0.0f && TTChar->IsAlive() && TTChar->CanDie())
			{
				TTChar->Die(Data.EffectSpec.GetContext().GetInstigator());
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(), 0.0f, GetMaxStamina()));

		ACharacter* TargetCharacter = Data.Target.AbilityActorInfo.IsValid() ? Cast<ACharacter>(Data.Target.AbilityActorInfo->AvatarActor.Get()) : nullptr;
		if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(TargetCharacter))
		{
			TTChar->HandleStaminaChanged(GetStamina(), GetMaxStamina());
		}
	}
	else if (Data.EvaluatedData.Attribute == GetPoiseAttribute())
	{
		SetPoise(FMath::Clamp(GetPoise(), 0.0f, GetMaxPoise()));

		ACharacter* TargetCharacter = Data.Target.AbilityActorInfo.IsValid() ? Cast<ACharacter>(Data.Target.AbilityActorInfo->AvatarActor.Get()) : nullptr;
		if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(TargetCharacter))
		{
			TTChar->HandlePoiseChanged(GetPoise(), GetMaxPoise());
		}
	}
}

bool UThreadTalesAttributeSet::ProcessParryDeflect(AThreadTalesBaseCharacter* TTChar, AActor* EffectiveAttacker, AActor* TargetActor, AThreadTalesBaseCharacter* AttackerChar, bool bFacingAttacker)
{
	if (!TTChar || !TTChar->IsParrying() || !bFacingAttacker)
	{
		return false;
	}

	UThreadTalesStaminaComponent* StamComp = TTChar->GetStaminaComponent();
	const UThreadTalesPostureConfig* PostureCfg = StamComp ? StamComp->GetPostureConfig() : nullptr;
	const UThreadTalesStaminaConfig* StaminaCfg = StamComp ? StamComp->GetStaminaConfig() : nullptr;
	const float ParryRelief = PostureCfg ? PostureCfg->ParryPostureRelief : (StaminaCfg ? StaminaCfg->ParryPoiseRelief : 15.0f);
	if (ParryRelief > 0.0f && GetPoise() < GetMaxPoise())
	{
		const float RelievedPoise = FMath::Clamp(GetPoise() + ParryRelief, 0.0f, GetMaxPoise());
		SetPoise(RelievedPoise);
		TTChar->HandlePoiseChanged(RelievedPoise, GetMaxPoise());
	}

	if (AttackerChar)
	{
		UThreadTalesAttributeSet* AttackerAttrSet = AttackerChar->GetAttributeSet();
		const float DeflectPostureDmg = PostureCfg ? PostureCfg->ParryDeflectPostureDamage : 60.0f;
		if (AttackerAttrSet)
		{
			const float CurrentAttackerPoise = AttackerAttrSet->GetPoise();
			const float NewAttackerPoise = FMath::Max(0.0f, CurrentAttackerPoise - DeflectPostureDmg);
			AttackerAttrSet->SetPoise(NewAttackerPoise);
			AttackerChar->HandlePoiseChanged(NewAttackerPoise, AttackerAttrSet->GetMaxPoise());

			if (UThreadTalesStaminaComponent* AttackerStamComp = AttackerChar->GetStaminaComponent())
			{
				AttackerStamComp->ApplyPoiseRegenDelay();
			}

			const bool bPostureBroken = (NewAttackerPoise <= 0.0f);
			if (bPostureBroken)
			{
				AttackerChar->HandlePostureBreak(TargetActor);
			}
			else
			{
				AttackerChar->HandleParried(TargetActor);
			}
		}
		else
		{
			AttackerChar->HandleParried(TargetActor);
		}

		AttackerChar->ApplyHitStop(PostureCfg ? PostureCfg->ParryAttackerHitStop : 0.08f);
	}

	TTChar->HandleSuccessfulParry(EffectiveAttacker);
	TTChar->ApplyHitStop(PostureCfg ? PostureCfg->ParryDefenderHitStop : 0.07f);
	LastParryDeflectTimestamp = FPlatformTime::Seconds();
	return true;
}

bool UThreadTalesAttributeSet::ProcessBlockMitigation(AThreadTalesBaseCharacter* TTChar, AActor* EffectiveAttacker, AThreadTalesBaseCharacter* AttackerChar, float LocalDamageDone, bool bFacingAttacker)
{
	if (!TTChar || !TTChar->IsBlocking() || !bFacingAttacker)
	{
		return false;
	}

	UThreadTalesStaminaComponent* StamComp = TTChar->GetStaminaComponent();
	const UThreadTalesPostureConfig* PostureCfg = StamComp ? StamComp->GetPostureConfig() : nullptr;
	const UThreadTalesStaminaConfig* StaminaCfg = StamComp ? StamComp->GetStaminaConfig() : nullptr;

	const float BlockPoiseRatio = PostureCfg ? PostureCfg->BlockPostureDamageRatio : (StaminaCfg ? StaminaCfg->BlockPoiseDamageRatio : 0.50f);
	const float MinBlockPoise = PostureCfg ? PostureCfg->BlockMinPostureCost : (StaminaCfg ? StaminaCfg->BlockMinPoiseCost : 15.0f);
	const float EffectivePoiseDmg = FMath::Max(MinBlockPoise, LocalDamageDone * BlockPoiseRatio);
	const float NewPoise = GetPoise() - EffectivePoiseDmg;
	if (StamComp)
	{
		StamComp->ApplyPoiseRegenDelay();
	}
	if (NewPoise <= 0.0f)
	{
		TTChar->HandlePostureBreak(EffectiveAttacker);
		return true;
	}
	SetPoise(NewPoise);
	TTChar->HandlePoiseChanged(NewPoise, GetMaxPoise());

	const float MinBlockCost = StaminaCfg ? StaminaCfg->BlockHitMinCost : 0.0f;
	const float BlockDamageRatio = StaminaCfg ? StaminaCfg->BlockHitDamageRatio : 0.0f;
	const float CurrentStamina = GetStamina();
	const float StaminaCost = FMath::Max(MinBlockCost, LocalDamageDone * BlockDamageRatio);

	if (StaminaCost > 0.0f)
	{
		const float NewStamina = FMath::Max(0.0f, CurrentStamina - StaminaCost);
		SetStamina(NewStamina);
		TTChar->HandleStaminaChanged(NewStamina, GetMaxStamina());
		if (StamComp)
		{
			StamComp->ApplyStaminaRegenDelay(NewStamina <= 0.0f);
		}
	}

	const float HpAbsorption = PostureCfg ? FMath::Clamp(PostureCfg->BlockHpDamageAbsorption, 0.0f, 1.0f) : 1.0f;
	const float ChipDamage = LocalDamageDone * (1.0f - HpAbsorption);
	if (ChipDamage > 0.0f)
	{
		const float NewHealth = FMath::Clamp(GetHealth() - ChipDamage, 0.0f, GetMaxHealth());
		SetHealth(NewHealth);
		TTChar->HandleHealthChanged(NewHealth, GetMaxHealth());
	}

	const float BlockHitStop = PostureCfg ? PostureCfg->BlockHitStopDuration : 0.05f;
	TTChar->HandleGuardHit(EffectiveAttacker);
	TTChar->ApplyHitStop(BlockHitStop);
	if (AttackerChar)
	{
		AttackerChar->ApplyHitStop(BlockHitStop);
	}
	return true;
}

void UThreadTalesAttributeSet::HandleIncomingDamage(const struct FGameplayEffectModCallbackData& Data)
{
	const float LocalDamageDone = GetDamage();
	SetDamage(0.0f);
	if (LocalDamageDone <= 0.0f)
	{
		return;
	}

	FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
	AActor* TargetActor = Data.Target.AbilityActorInfo.IsValid() ? Data.Target.AbilityActorInfo->AvatarActor.Get() : nullptr;
	AThreadTalesBaseCharacter* TTChar = Cast<AThreadTalesBaseCharacter>(TargetActor);
	AActor* Attacker = Context.GetInstigator() ? Context.GetInstigator() : Context.GetEffectCauser();
	AThreadTalesBaseCharacter* AttackerChar = Cast<AThreadTalesBaseCharacter>(Attacker);
	if (!AttackerChar && Attacker && Attacker->GetOwner())
	{
		AttackerChar = Cast<AThreadTalesBaseCharacter>(Attacker->GetOwner());
	}
	AActor* EffectiveAttacker = AttackerChar ? static_cast<AActor*>(AttackerChar) : Attacker;
	const bool bFacingAttacker = TTChar ? TTChar->IsFacingAttacker(EffectiveAttacker) : false;

	// 1. Invulnerability / Evade I-Frames: 0 damage
	if (TTChar && (TTChar->IsEvading() || (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AbilitySystemComponent.IsValid() && Data.Target.AbilityActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Invulnerable))))
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[AttributeSet] %s EVADED attack (I-Frames Active)! Zero damage taken."),
			TargetActor ? *TargetActor->GetName() : TEXT("Target"));
		return;
	}

	// 2. Perfect Parry / Deflect Window
	if (ProcessParryDeflect(TTChar, EffectiveAttacker, TargetActor, AttackerChar, bFacingAttacker))
	{
		return;
	}

	// 3. Block / Guard
	if (ProcessBlockMitigation(TTChar, EffectiveAttacker, AttackerChar, LocalDamageDone, bFacingAttacker))
	{
		return;
	}

	// 4. Standard Unblocked Hit
	const float OldHealth = GetHealth();
	const float NewHealth = FMath::Clamp(OldHealth - LocalDamageDone, 0.0f, GetMaxHealth());
	SetHealth(NewHealth);
	if (TTChar)
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[AttributeSet] %s TOOK %.1f Damage! Health: %.1f -> %.1f / %.1f"),
			*TTChar->GetName(), LocalDamageDone, OldHealth, NewHealth, GetMaxHealth());
		TTChar->HandleHealthChanged(NewHealth, GetMaxHealth());
		if (!TTChar->IsStaggered() && GetPoise() > 0.0f)
		{
			TTChar->HandleHitReact(EffectiveAttacker);
			TTChar->ApplyHitStop(0.06f);
		}
		if (AttackerChar)
		{
			AttackerChar->ApplyHitStop(0.06f);
		}

		if (NewHealth <= 0.0f && TTChar->IsAlive() && TTChar->CanDie())
		{
			TTChar->Die(EffectiveAttacker);
		}
	}
}

void UThreadTalesAttributeSet::HandleIncomingPoiseDamage(const struct FGameplayEffectModCallbackData& Data)
{
	const float LocalPoiseDamage = GetPoiseDamage();
	SetPoiseDamage(0.0f);
	if (LocalPoiseDamage <= 0.0f)
	{
		return;
	}

	FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
	AActor* TargetActor = Data.Target.AbilityActorInfo.IsValid() ? Data.Target.AbilityActorInfo->AvatarActor.Get() : nullptr;
	AThreadTalesBaseCharacter* TTChar = Cast<AThreadTalesBaseCharacter>(TargetActor);
	AActor* Attacker = Context.GetInstigator() ? Context.GetInstigator() : Context.GetEffectCauser();
	if (Attacker && !Cast<AThreadTalesBaseCharacter>(Attacker) && Attacker->GetOwner())
	{
		Attacker = Attacker->GetOwner();
	}
	const bool bFacingAttacker = TTChar ? TTChar->IsFacingAttacker(Attacker) : false;
	const bool bRecentlyParried = (FPlatformTime::Seconds() - LastParryDeflectTimestamp) <= 0.08;

	// 1. Evade I-Frames, Frontal Perfect Parry, or Active Posture Break Stun / Stagger
	if (TTChar && (TTChar->IsEvading() || (TTChar->IsParrying() && bFacingAttacker) || bRecentlyParried || TTChar->IsStaggered() ||
		(Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AbilitySystemComponent.IsValid() &&
			Data.Target.AbilityActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Invulnerable))))
	{
		return;
	}

	// 2. Block vs Unblocked Posture buildup
	UThreadTalesStaminaComponent* StamComp = TTChar ? TTChar->GetStaminaComponent() : nullptr;
	const UThreadTalesPostureConfig* PostureCfg = StamComp ? StamComp->GetPostureConfig() : nullptr;
	const UThreadTalesStaminaConfig* StaminaCfg = StamComp ? StamComp->GetStaminaConfig() : nullptr;
	const bool bBlockedHit = (TTChar && TTChar->IsBlocking() && bFacingAttacker);
	float EffectivePoiseDamage = LocalPoiseDamage * (PostureCfg ? PostureCfg->UnblockedPostureDamageMultiplier : 1.0f);
	if (bBlockedHit)
	{
		const float BlockPoiseRatio = PostureCfg ? PostureCfg->BlockPostureDamageRatio : (StaminaCfg ? StaminaCfg->BlockPoiseDamageRatio : 0.50f);
		const float MinBlockPoise = PostureCfg ? PostureCfg->BlockMinPostureCost : (StaminaCfg ? StaminaCfg->BlockMinPoiseCost : 15.0f);
		EffectivePoiseDamage = FMath::Max(MinBlockPoise, LocalPoiseDamage * BlockPoiseRatio);
	}

	const float NewPoise = GetPoise() - EffectivePoiseDamage;
	if (StamComp)
	{
		StamComp->ApplyPoiseRegenDelay();
	}

	if (NewPoise <= 0.0f)
	{
		SetPoise(0.0f);
		if (TTChar)
		{
			const float BreakHitStop = PostureCfg ? PostureCfg->PostureBreakHitStopDuration : 0.08f;
			TTChar->HandlePostureBreak(Attacker);
			TTChar->ApplyHitStop(BreakHitStop);
		}
	}
	else
	{
		const float ClampedPoise = FMath::Clamp(NewPoise, 0.0f, GetMaxPoise());
		SetPoise(ClampedPoise);
		if (TTChar)
		{
			TTChar->HandlePoiseChanged(ClampedPoise, GetMaxPoise());
			if (!bBlockedHit)
			{
				TTChar->HandleHitReact(Attacker);
				TTChar->ApplyHitStop(0.06f);
			}
		}
	}
}

void UThreadTalesAttributeSet::ReceiveDamage(float DamageAmount, AActor* SourceActor)
{
	if (DamageAmount <= 0.0f)
	{
		return;
	}

	AActor* TargetActor = GetOwningActor();
	AThreadTalesBaseCharacter* TTChar = Cast<AThreadTalesBaseCharacter>(TargetActor);
	if (!TTChar || !TTChar->IsAlive())
	{
		return;
	}

	// 1. Invulnerability / Evade I-Frames: 0 damage
	if (TTChar->IsEvading() || (TTChar->GetAbilitySystemComponent() && TTChar->GetAbilitySystemComponent()->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Invulnerable)))
	{
		return;
	}

	// Environmental / Direct damage path (combat attacks must go through GAS PostGameplayEffectExecute)
	const float OldHealth = GetHealth();
	const float NewHealth = FMath::Clamp(OldHealth - DamageAmount, 0.0f, GetMaxHealth());
	SetHealth(NewHealth);

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[AttributeSet] %s received %.1f direct/environmental damage. Health: %.1f -> %.1f / %.1f"),
		*TTChar->GetName(), DamageAmount, OldHealth, NewHealth, GetMaxHealth());

	TTChar->HandleHealthChanged(NewHealth, GetMaxHealth());
	if (!TTChar->IsStaggered() && GetPoise() > 0.0f)
	{
		TTChar->HandleHitReact(SourceActor);
	}
	if (NewHealth <= 0.0f && TTChar->CanDie())
	{
		TTChar->Die(SourceActor);
	}
}

void UThreadTalesAttributeSet::ResetHealth()
{
	SetHealth(GetMaxHealth());
	AActor* TargetActor = GetOwningActor();
	if (AThreadTalesBaseCharacter* TTChar = Cast<AThreadTalesBaseCharacter>(TargetActor))
	{
		TTChar->HandleHealthChanged(GetMaxHealth(), GetMaxHealth());
	}
}

void UThreadTalesAttributeSet::ResetStamina()
{
	SetStamina(GetMaxStamina());
	AActor* TargetActor = GetOwningActor();
	if (AThreadTalesBaseCharacter* TTChar = Cast<AThreadTalesBaseCharacter>(TargetActor))
	{
		TTChar->HandleStaminaChanged(GetMaxStamina(), GetMaxStamina());
	}
}

void UThreadTalesAttributeSet::ResetPoise()
{
	SetPoise(GetMaxPoise());
	AActor* TargetActor = GetOwningActor();
	if (AThreadTalesBaseCharacter* TTChar = Cast<AThreadTalesBaseCharacter>(TargetActor))
	{
		TTChar->HandlePoiseChanged(GetMaxPoise(), GetMaxPoise());
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

void UThreadTalesAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldStamina)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UThreadTalesAttributeSet, Stamina, OldStamina);
}

void UThreadTalesAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UThreadTalesAttributeSet, MaxStamina, OldMaxStamina);
}

void UThreadTalesAttributeSet::OnRep_Poise(const FGameplayAttributeData& OldPoise)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UThreadTalesAttributeSet, Poise, OldPoise);
}

void UThreadTalesAttributeSet::OnRep_MaxPoise(const FGameplayAttributeData& OldMaxPoise)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UThreadTalesAttributeSet, MaxPoise, OldMaxPoise);
}

