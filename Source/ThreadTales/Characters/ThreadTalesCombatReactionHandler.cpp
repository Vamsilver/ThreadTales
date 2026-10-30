// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesCombatReactionHandler.h"
#include "ThreadTales.h"
#include "Characters/ThreadTalesBaseCharacter.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Abilities/ThreadTalesAbilitySystemComponent.h"
#include "Abilities/ThreadTalesAttributeSet.h"
#include "Abilities/ThreadTalesGameplayEffects.h"
#include "Abilities/ThreadTalesPostureConfig.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
#include "Combat/ThreadTalesWeapon.h"
#include "Combat/ThreadTalesWeapon_Bow.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

UThreadTalesCombatReactionHandler::UThreadTalesCombatReactionHandler()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// Default Combat Montages
	static ConstructorHelpers::FObjectFinder<UAnimMontage> GuardHitObj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/AM_Guard_Hit.AM_Guard_Hit"));
	if (GuardHitObj.Succeeded())
	{
		GuardHitMontage = GuardHitObj.Object;
	}
	static ConstructorHelpers::FObjectFinder<UAnimMontage> GuardHitHeavyObj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/AM_Guard_Hit_Heavy.AM_Guard_Hit_Heavy"));
	if (GuardHitHeavyObj.Succeeded())
	{
		GuardHitHeavyMontage = GuardHitHeavyObj.Object;
	}
	static ConstructorHelpers::FObjectFinder<UAnimMontage> ParryObj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/AM_Parry.AM_Parry"));
	if (ParryObj.Succeeded())
	{
		ParryMontage = ParryObj.Object;
	}
	static ConstructorHelpers::FObjectFinder<UAnimMontage> StaggerObj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/AM_Stagger.AM_Stagger"));
	if (StaggerObj.Succeeded())
	{
		StaggerMontage = StaggerObj.Object;
	}
	static ConstructorHelpers::FObjectFinder<UAnimMontage> HitReactObj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/AM_HitReact.AM_HitReact"));
	if (HitReactObj.Succeeded())
	{
		HitReactMontage = HitReactObj.Object;
	}
	static ConstructorHelpers::FObjectFinder<UAnimMontage> PostureBreakObj(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Montages/AM_PostureBreak.AM_PostureBreak"));
	if (PostureBreakObj.Succeeded())
	{
		PostureBreakMontage = PostureBreakObj.Object;
	}
}

AThreadTalesBaseCharacter* UThreadTalesCombatReactionHandler::GetOwningCharacter() const
{
	return Cast<AThreadTalesBaseCharacter>(GetOwner());
}

void UThreadTalesCombatReactionHandler::ApplyHitStop(float Duration)
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character || Character->IsStaggered() || Character->IsEvading())
	{
		return;
	}

	const float ClampedDuration = FMath::Clamp(Duration, 0.02f, 0.15f);
	if (Character->HasAuthority())
	{
		Multicast_ApplyHitStop(ClampedDuration);
	}
	else
	{
		Multicast_ApplyHitStop_Implementation(ClampedDuration);
	}
}

void UThreadTalesCombatReactionHandler::Multicast_ApplyHitStop_Implementation(float Duration)
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character || !GetWorld() || !Character->GetMesh() || Character->IsStaggered() || Character->IsEvading())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(HitStopTimerHandle);

	if (UAnimInstance* AnimInst = Character->GetMesh()->GetAnimInstance())
	{
		if (UAnimMontage* CurrentMontage = AnimInst->GetCurrentActiveMontage())
		{
			AnimInst->Montage_Pause(CurrentMontage);
		}
	}
	Character->GetMesh()->bPauseAnims = true;

	GetWorld()->GetTimerManager().SetTimer(
		HitStopTimerHandle,
		this,
		&UThreadTalesCombatReactionHandler::ResumeFromHitStop,
		Duration,
		false
	);
}

void UThreadTalesCombatReactionHandler::ResumeFromHitStop()
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character || !Character->GetMesh())
	{
		return;
	}

	Character->GetMesh()->bPauseAnims = false;
	if (UAnimInstance* AnimInst = Character->GetMesh()->GetAnimInstance())
	{
		if (UAnimMontage* CurrentMontage = AnimInst->GetCurrentActiveMontage())
		{
			AnimInst->Montage_Resume(CurrentMontage);
		}
	}

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(HitStopTimerHandle);
	}
}

void UThreadTalesCombatReactionHandler::HandleStaggered(AActor* AttackerActor)
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return;
	}

	Character->OnStaggered.Broadcast(AttackerActor);

	if (UThreadTalesAbilitySystemComponent* ASC = Cast<UThreadTalesAbilitySystemComponent>(Character->GetAbilitySystemComponent()))
	{
		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(ThreadTalesGameplayTags::TAG_Ability_Melee);
		CancelTags.AddTag(ThreadTalesGameplayTags::TAG_Ability_Ranged);
		CancelTags.AddTag(ThreadTalesGameplayTags::TAG_Ability_Block);
		ASC->CancelAbilities(&CancelTags);

		if (Character->HasAuthority())
		{
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddInstigator(AttackerActor, AttackerActor);
			ASC->ApplyGameplayEffectToSelf(UThreadTalesGE_Stagger::StaticClass()->GetDefaultObject<UGameplayEffect>(), 1.0f, Context);
		}
	}

	if (Character->HasAuthority())
	{
		Multicast_PlayStaggered(AttackerActor);
	}
	else
	{
		Multicast_PlayStaggered_Implementation(AttackerActor);
	}
}

void UThreadTalesCombatReactionHandler::Multicast_PlayStaggered_Implementation(AActor* AttackerActor)
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return;
	}

	ResumeFromHitStop();
	Character->OnStaggered.Broadcast(AttackerActor);

	UAnimMontage* MontageToPlay = Character->GetEffectiveStaggerMontage();
	if (!MontageToPlay)
	{
		MontageToPlay = Character->IsAirborne() ? (AirStaggerMontage ? AirStaggerMontage.Get() : StaggerMontage.Get()) : StaggerMontage.Get();
	}

	if (MontageToPlay)
	{
		Character->PlayAnimMontage(MontageToPlay);
	}
}

void UThreadTalesCombatReactionHandler::HandleHitReact(AActor* AttackerActor)
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character || Character->IsStaggered() || Character->IsEvading() || Character->IsParrying())
	{
		return;
	}

	CloseParryCounterWindow();

	if (Character->HasAuthority())
	{
		Multicast_PlayHitReact(AttackerActor);
	}
	else
	{
		Multicast_PlayHitReact_Implementation(AttackerActor);
	}
}

void UThreadTalesCombatReactionHandler::Multicast_PlayHitReact_Implementation(AActor* AttackerActor)
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character || Character->IsStaggered() || Character->IsEvading() || Character->IsParrying())
	{
		return;
	}

	ResumeFromHitStop();
	CloseParryCounterWindow();
	Character->OnHitReact.Broadcast(AttackerActor);

	if (!Character->IsStaggered() && !Character->IsEvading() && !Character->IsParrying())
	{
		bool bIsBowEquipped = false;
		if (const UThreadTalesEquipmentComponent* EquipComp = Character->GetEquipmentComponent())
		{
			if (const AThreadTalesWeapon* CurWeapon = EquipComp->GetCurrentWeapon())
			{
				if (CurWeapon->IsA<AThreadTalesWeapon_Bow>() || CurWeapon->GetPrimaryAttackTag().MatchesTagExact(ThreadTalesGameplayTags::TAG_Ability_Ranged_Fire))
				{
					bIsBowEquipped = true;
				}
			}
		}

		UAnimMontage* MontageToPlay = Character->GetEffectiveHitReactMontage();
		if (!MontageToPlay)
		{
			MontageToPlay = Character->IsAirborne() ? (AirHitReactMontage ? AirHitReactMontage.Get() : HitReactMontage.Get()) : HitReactMontage.Get();
		}

		if (!bIsBowEquipped && MontageToPlay)
		{
			Character->PlayAnimMontage(MontageToPlay);
		}
	}
}

void UThreadTalesCombatReactionHandler::HandleGuardHit(AActor* AttackerActor)
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return;
	}

	const float CurrentPoise = Character->GetPoise();
	const UThreadTalesPostureConfig* PostureCfg = Character->GetPostureConfig();
	const float HeavyThreshold = PostureCfg ? PostureCfg->HeavyGuardRemainingPoiseThreshold : 35.0f;
	const bool bHeavyHit = (CurrentPoise <= HeavyThreshold);

	if (Character->HasAuthority())
	{
		Multicast_PlayGuardHit(AttackerActor, bHeavyHit);
	}
	else
	{
		Multicast_PlayGuardHit_Implementation(AttackerActor, bHeavyHit);
	}
}

void UThreadTalesCombatReactionHandler::Multicast_PlayGuardHit_Implementation(AActor* AttackerActor, bool bHeavyHit)
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return;
	}

	ResumeFromHitStop();
	Character->OnGuardHit.Broadcast(AttackerActor);

	if (Character->IsStaggered() || Character->IsEvading())
	{
		return;
	}

	UAnimMontage* MontageToPlay = (bHeavyHit && GuardHitHeavyMontage) ? GuardHitHeavyMontage : GuardHitMontage;
	if (MontageToPlay)
	{
		Character->PlayAnimMontage(MontageToPlay, 1.15f);
	}

	const FVector PushDir = -Character->GetActorForwardVector();
	const UThreadTalesPostureConfig* PushPostureCfg = Character->GetPostureConfig();
	const float HeavyPush = PushPostureCfg ? PushPostureCfg->HeavyGuardPushbackForce : 280.0f;
	const float NormalPush = PushPostureCfg ? PushPostureCfg->NormalGuardPushbackForce : 140.0f;
	const float PushForce = bHeavyHit ? HeavyPush : NormalPush;
	Character->LaunchCharacter(PushDir * PushForce, true, false);
}

void UThreadTalesCombatReactionHandler::HandleSuccessfulParry(AActor* AttackingActor)
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return;
	}

	if (Character->HasAuthority())
	{
		Multicast_PlaySuccessfulParry(AttackingActor);
	}
	else
	{
		Multicast_PlaySuccessfulParry_Implementation(AttackingActor);
	}
}

void UThreadTalesCombatReactionHandler::Multicast_PlaySuccessfulParry_Implementation(AActor* AttackingActor)
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return;
	}

	ResumeFromHitStop();
	Character->OnSuccessfulParry.Broadcast(AttackingActor);

	UAnimMontage* MontageToPlay = ParryMontage;
	if (const UThreadTalesEquipmentComponent* EquipComp = Character->GetEquipmentComponent())
	{
		if (const AThreadTalesWeapon* Weapon = EquipComp->GetCurrentWeapon())
		{
			const int32 ParryIdx = (AttackingActor && FVector::DotProduct((AttackingActor->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal2D(), Character->GetActorRightVector()) < 0.0f) ? 0 : 1;
			if (UAnimMontage* WeaponParry = Weapon->GetParryMontage(ParryIdx))
			{
				MontageToPlay = WeaponParry;
			}
		}
	}

	if (!Character->IsStaggered() && !Character->IsEvading() && MontageToPlay)
	{
		Character->PlayAnimMontage(MontageToPlay, 1.25f);
	}
}

void UThreadTalesCombatReactionHandler::OpenParryCounterWindow(float Duration)
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character || !GetWorld())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
	{
		ASC->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_ParryCounterWindow);
	}

	GetWorld()->GetTimerManager().ClearTimer(ParryCounterWindowTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(
		ParryCounterWindowTimerHandle,
		this,
		&UThreadTalesCombatReactionHandler::CloseParryCounterWindow,
		Duration,
		false
	);
}

void UThreadTalesCombatReactionHandler::CloseParryCounterWindow()
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ParryCounterWindowTimerHandle);
	}

	if (Character)
	{
		if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
		{
			if (ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_ParryCounterWindow))
			{
				ASC->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_ParryCounterWindow);
			}
		}
	}
}

void UThreadTalesCombatReactionHandler::HandleParried(AActor* ParryingActor)
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character || !GetWorld())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
	{
		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(ThreadTalesGameplayTags::TAG_Ability_Melee);
		CancelTags.AddTag(ThreadTalesGameplayTags::TAG_Ability_Ranged);
		CancelTags.AddTag(ThreadTalesGameplayTags::TAG_Ability_Block);
		CancelTags.AddTag(ThreadTalesGameplayTags::TAG_Ability_Evade);
		ASC->CancelAbilities(&CancelTags);

		if (Character->HasAuthority())
		{
			if (!ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered))
			{
				ASC->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered);
			}
		}
	}

	if (Character->GetCharacterMovement())
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
	}

	const float StaggerDuration = Character->GetParryRecoilDuration();
	GetWorld()->GetTimerManager().ClearTimer(ParriedStaggerTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(
		ParriedStaggerTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			FinishParriedStagger();
		}),
		StaggerDuration,
		false
	);

	if (Character->HasAuthority())
	{
		Multicast_PlayParried(ParryingActor);
	}
	else
	{
		Multicast_PlayParried_Implementation(ParryingActor);
	}
}

void UThreadTalesCombatReactionHandler::FinishParriedStagger()
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ParriedStaggerTimerHandle);
	}

	if (Character && Character->HasAuthority())
	{
		if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
		{
			if (!ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_PostureBroken))
			{
				while (ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered))
				{
					ASC->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered);
				}
			}
		}
	}
}

void UThreadTalesCombatReactionHandler::HandlePostureBreak(AActor* ParryingActor, float StunDuration)
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character || !GetWorld())
	{
		return;
	}

	const float EffectiveDuration = (StunDuration > 0.0f) ? StunDuration : Character->GetPostureBreakStunDuration();

	if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
	{
		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(ThreadTalesGameplayTags::TAG_Ability_Melee);
		CancelTags.AddTag(ThreadTalesGameplayTags::TAG_Ability_Ranged);
		CancelTags.AddTag(ThreadTalesGameplayTags::TAG_Ability_Block);
		CancelTags.AddTag(ThreadTalesGameplayTags::TAG_Ability_Evade);
		ASC->CancelAbilities(&CancelTags);

		if (Character->HasAuthority())
		{
			if (!ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_PostureBroken))
			{
				ASC->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_PostureBroken);
			}
			if (!ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered))
			{
				ASC->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered);
			}
		}
	}

	if (Character->GetCharacterMovement())
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
	}

	if (UThreadTalesAttributeSet* AttSet = Character->GetAttributeSet())
	{
		AttSet->SetPoise(0.0f);
		Character->HandlePoiseChanged(0.0f, AttSet->GetMaxPoise());
	}

	GetWorld()->GetTimerManager().ClearTimer(PostureBreakTimerHandle);
	// Fallback safety timeout if montage delegate does not fire (e.g. headless dedicated server)
	const float FallbackDuration = (EffectiveDuration > 0.0f) ? FMath::Max(3.0f, EffectiveDuration) : 4.0f;
	GetWorld()->GetTimerManager().SetTimer(
		PostureBreakTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			FinishPostureBreak();
		}),
		FallbackDuration,
		false
	);

	if (Character->HasAuthority())
	{
		Multicast_PlayPostureBreak(ParryingActor, EffectiveDuration);
	}
	else
	{
		Multicast_PlayPostureBreak_Implementation(ParryingActor, EffectiveDuration);
	}
}

void UThreadTalesCombatReactionHandler::FinishPostureBreak()
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PostureBreakTimerHandle);
	}

	if (!Character)
	{
		return;
	}

	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	const bool bWasPostureBroken = ASC && ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_PostureBroken);

	// Idempotency: only transition once from PostureBroken -> Recover
	if (!bWasPostureBroken)
	{
		return;
	}

	if (Character->HasAuthority())
	{
		if (ASC)
		{
			while (ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_PostureBroken))
			{
				ASC->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_PostureBroken);
			}
		}
	}

	if (UThreadTalesAttributeSet* AttSet = Character->GetAttributeSet())
	{
		AttSet->SetPoise(AttSet->GetMaxPoise());
		Character->HandlePoiseChanged(AttSet->GetMaxPoise(), AttSet->GetMaxPoise());
	}

	float RecoverDuration = 0.0f;
	if (UAnimMontage* RecMontage = Character->GetRecoverMontage())
	{
		RecoverDuration = RecMontage->GetPlayLength();
	}

	if (Character->HasAuthority())
	{
		Multicast_PlayRecover();
	}
	else
	{
		Multicast_PlayRecover_Implementation();
	}

	if (RecoverDuration > 0.0f && GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PostureBreakTimerHandle);
		GetWorld()->GetTimerManager().SetTimer(
			PostureBreakTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				AThreadTalesBaseCharacter* Char = GetOwningCharacter();
				if (Char && Char->HasAuthority())
				{
					if (UAbilitySystemComponent* ASCComp = Char->GetAbilitySystemComponent())
					{
						while (ASCComp->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered))
						{
							ASCComp->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered);
						}
					}
				}
			}),
			RecoverDuration,
			false
		);
	}
	else if (Character->HasAuthority())
	{
		if (ASC)
		{
			while (ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered))
			{
				ASC->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered);
			}
		}
	}
}

void UThreadTalesCombatReactionHandler::OnPostureBreakMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character || !Character->IsAlive() || bInterrupted) return;
	if (Character->IsAirborne() || (Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsFalling())) return;

	UAnimMontage* PostureBreakMtg = Character->GetEffectivePostureBreakMontage();
	if (!PostureBreakMtg) PostureBreakMtg = PostureBreakMontage ? PostureBreakMontage.Get() : StaggerMontage.Get();

	if (Montage == PostureBreakMtg)
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[CombatReactionHandler] '%s' POSTURE BREAK MONTAGE BLENDING OUT -> Seamless Recover transition!"), *Character->GetName());
		if (Character->HasAuthority()) FinishPostureBreak();
	}
	else if (Montage == Character->GetEffectiveStaggerMontage() || Montage == StaggerMontage)
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[CombatReactionHandler] '%s' PARRIED STAGGER MONTAGE BLENDING OUT -> FinishParriedStagger"), *Character->GetName());
		if (Character->HasAuthority()) FinishParriedStagger();
	}
}

void UThreadTalesCombatReactionHandler::OnPostureBreakMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character || !Character->IsAlive() || bInterrupted) return;
	if (Character->IsAirborne() || (Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsFalling())) return;

	UAnimMontage* PostureBreakMtg = Character->GetEffectivePostureBreakMontage();
	if (!PostureBreakMtg) PostureBreakMtg = PostureBreakMontage ? PostureBreakMontage.Get() : StaggerMontage.Get();

	if (Montage == PostureBreakMtg)
	{
		if (Character->HasAuthority()) FinishPostureBreak();
	}
	else if (Montage == Character->GetEffectiveStaggerMontage() || Montage == StaggerMontage)
	{
		if (Character->HasAuthority()) FinishParriedStagger();
	}
}

void UThreadTalesCombatReactionHandler::OnParriedMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	OnPostureBreakMontageBlendingOut(Montage, bInterrupted);
}

void UThreadTalesCombatReactionHandler::OnParriedMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	OnPostureBreakMontageEnded(Montage, bInterrupted);
}

void UThreadTalesCombatReactionHandler::ClearReactionTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitStopTimerHandle);
		World->GetTimerManager().ClearTimer(ParriedStaggerTimerHandle);
		World->GetTimerManager().ClearTimer(PostureBreakTimerHandle);
		World->GetTimerManager().ClearTimer(ParryCounterWindowTimerHandle);
	}
}

void UThreadTalesCombatReactionHandler::Multicast_PlayRecover_Implementation()
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return;
	}

	if (UAnimMontage* RecMontage = Character->GetRecoverMontage())
	{
		// Crossfade directly over running PostureBreak montage on DefaultSlot without StopAnimMontage drop to Idle
		Character->PlayAnimMontage(RecMontage, 1.0f);
	}
	else
	{
		UAnimMontage* MontageToStop = PostureBreakMontage ? PostureBreakMontage.Get() : StaggerMontage.Get();
		if (MontageToStop && Character->GetMesh() && Character->GetMesh()->GetAnimInstance())
		{
			Character->StopAnimMontage(MontageToStop);
		}
		if (UAnimMontage* EffectiveMtg = Character->GetEffectivePostureBreakMontage())
		{
			if (EffectiveMtg != MontageToStop && Character->GetMesh() && Character->GetMesh()->GetAnimInstance())
			{
				Character->StopAnimMontage(EffectiveMtg);
			}
		}
	}
}

void UThreadTalesCombatReactionHandler::Multicast_PlayParried_Implementation(AActor* ParryingActor)
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return;
	}

	ResumeFromHitStop();
	Character->OnParried.Broadcast(ParryingActor);

	UAnimMontage* MontageToPlay = Character->GetEffectiveStaggerMontage();
	if (!MontageToPlay)
	{
		MontageToPlay = StaggerMontage;
	}

	if (MontageToPlay)
	{
		if (UAnimInstance* AnimInst = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
		{
			// Do not restart montage if it is already playing (e.g. air knockdown AirFallStart -> AirFallLoop -> AirFallEnd)
			if (!AnimInst->Montage_IsPlaying(MontageToPlay))
			{
				Character->PlayAnimMontage(MontageToPlay, 1.0f);
			}

			FOnMontageBlendingOutStarted BlendingOutDelegate;
			BlendingOutDelegate.BindUObject(this, &UThreadTalesCombatReactionHandler::OnParriedMontageBlendingOut);
			AnimInst->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontageToPlay);

			FOnMontageEnded EndedDelegate;
			EndedDelegate.BindUObject(this, &UThreadTalesCombatReactionHandler::OnParriedMontageEnded);
			AnimInst->Montage_SetEndDelegate(EndedDelegate, MontageToPlay);
		}
		else
		{
			Character->PlayAnimMontage(MontageToPlay, 1.0f);
		}
	}
}

void UThreadTalesCombatReactionHandler::Multicast_PlayPostureBreak_Implementation(AActor* ParryingActor, float StunDuration)
{
	AThreadTalesBaseCharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return;
	}

	ResumeFromHitStop();
	Character->OnStaggered.Broadcast(ParryingActor);

	UAnimMontage* MontageToPlay = Character->GetEffectivePostureBreakMontage();
	if (!MontageToPlay)
	{
		MontageToPlay = Character->IsAirborne() ? (AirPostureBreakMontage ? AirPostureBreakMontage.Get() : PostureBreakMontage.Get())
		                                        : (PostureBreakMontage ? PostureBreakMontage.Get() : StaggerMontage.Get());
	}
	if (MontageToPlay)
	{
		if (UAnimInstance* AnimInst = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
		{
			// Do not restart montage if it is already playing (e.g. air knockdown AirFallStart -> AirFallLoop -> AirFallEnd)
			if (!AnimInst->Montage_IsPlaying(MontageToPlay))
			{
				Character->PlayAnimMontage(MontageToPlay, 1.0f);
			}

			FOnMontageBlendingOutStarted BlendingOutDelegate;
			BlendingOutDelegate.BindUObject(this, &UThreadTalesCombatReactionHandler::OnPostureBreakMontageBlendingOut);
			AnimInst->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontageToPlay);

			FOnMontageEnded EndedDelegate;
			EndedDelegate.BindUObject(this, &UThreadTalesCombatReactionHandler::OnPostureBreakMontageEnded);
			AnimInst->Montage_SetEndDelegate(EndedDelegate, MontageToPlay);
		}
		else
		{
			Character->PlayAnimMontage(MontageToPlay, 1.0f);
		}
	}
}
