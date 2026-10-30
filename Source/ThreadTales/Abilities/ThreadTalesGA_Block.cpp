#include "Abilities/ThreadTalesGA_Block.h"
#include "Abilities/ThreadTalesPostureConfig.h"
#include "ThreadTales.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesStaminaComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/Character.h"

UThreadTalesGA_Block::UThreadTalesGA_Block()
{
	AbilityName = TEXT("Block / Guard");
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	SetAssetTags(FGameplayTagContainer(ThreadTalesGameplayTags::TAG_Ability_Block));
	ActivationOwnedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Blocking);

	// Block is impossible while staggered or posture broken
	ActivationBlockedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered);
	ActivationBlockedTags.AddTag(ThreadTalesGameplayTags::TAG_State_Combat_PostureBroken);

	// Blocking cancels and blocks melee attacks
	CancelAbilitiesWithTag.AddTag(ThreadTalesGameplayTags::TAG_Ability_Melee);
	BlockAbilitiesWithTag.AddTag(ThreadTalesGameplayTags::TAG_Ability_Melee);
}

bool UThreadTalesGA_Block::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// Forbid activating block if character state does not permit aiming/blocking
	if (const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		if (!TTChar->CanAimOrBlock())
		{
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Block GA] Cannot activate block: CanAimOrBlock() is false."));
			return false;
		}
	}

	return true;
}

void UThreadTalesGA_Block::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);

	// Fail-safe: if avatar is currently not permitted to aim/block, reject activation
	if (TTChar && !TTChar->CanAimOrBlock())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Block GA] Activated — entering guard stance."));

	// Enter GASPALS aim/strafe mode: drives bIsAimInputDown, routing ABP_Overlay_Katana into Pose_Stand_*_Aim slots.
	// Block operates as a pure strafe overlay: DefaultSlot is NOT blocked by any montage, allowing
	// full 8-way Motion Matching strafe locomotion while upper body maintains the katana guard posture.
	SetAvatarAimInput(true);

	const UThreadTalesPostureConfig* PostureCfg = (TTChar && TTChar->GetStaminaComponent()) ? TTChar->GetStaminaComponent()->GetPostureConfig() : nullptr;
	const float EffectiveParryWindow = PostureCfg ? PostureCfg->ParryWindowDuration : ParryWindowDuration;

	// Open dynamic parry window (TAG_State_Combat_Parrying) for EffectiveParryWindow (default 0.25s)
	bParryActive = true;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Parrying);
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Block GA] Parry window OPEN for %.2fs on '%s'"),
			EffectiveParryWindow, *GetNameSafe(GetAvatarActorFromActorInfo()));
	}

	ParryDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, EffectiveParryWindow);
	if (ParryDelayTask)
	{
		ParryDelayTask->OnFinish.AddDynamic(this, &UThreadTalesGA_Block::OnParryWindowExpired);
		ParryDelayTask->ReadyForActivation();
	}

	// Optional guard montage (if assigned, e.g. for non-overlay weapons or special start flourishes).
	// For Katana, GuardMontage remains nullptr to prevent interrupting DefaultSlot locomotion.
	if (GuardMontage)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, GuardMontage, 1.0f, NAME_None,
			/*bStopWhenAbilityEnds=*/ true, 1.0f);

		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UThreadTalesGA_Block::OnGuardMontageCompleted);
			MontageTask->OnInterrupted.AddDynamic(this, &UThreadTalesGA_Block::OnGuardMontageInterrupted);
			MontageTask->OnCancelled.AddDynamic(this, &UThreadTalesGA_Block::OnGuardMontageCancelled);
			MontageTask->ReadyForActivation();
		}
	}

	// Hold-mode: ability stays active until input is released via InputReleased()
}

void UThreadTalesGA_Block::OnParryWindowExpired()
{
	if (bParryActive)
	{
		bParryActive = false;
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Parrying);
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Block GA] Parry window CLOSED on '%s' — standard guard active."),
				*GetNameSafe(GetAvatarActorFromActorInfo()));
		}
	}
}

void UThreadTalesGA_Block::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Block GA] Input released — ending guard stance."));
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UThreadTalesGA_Block::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Ensure parry tag is cleanly removed if ability ends before window expiry
	if (bParryActive)
	{
		bParryActive = false;
		if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
		{
			ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Parrying);
		}
	}

	// Clear GASPALS aim/strafe mode
	SetAvatarAimInput(false);

	if (bWasCancelled)
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Block GA] Guard cancelled (stagger or external interruption)."));
	}
	else
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Block GA] Guard ended normally."));
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UThreadTalesGA_Block::SetAvatarAimInput(bool bAiming) const
{
	if (AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(GetAvatarActorFromActorInfo()))
	{
		TTChar->SetAimInputDown(bAiming);
	}
}

void UThreadTalesGA_Block::OnGuardMontageCompleted()
{
	// Guard montage looped to end — keep blocking (ability stays active until input release)
	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Block GA] Guard montage completed (loop end). Ability remains active awaiting input release."));
}

void UThreadTalesGA_Block::OnGuardMontageInterrupted()
{
	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Block GA] Guard montage interrupted."));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UThreadTalesGA_Block::OnGuardMontageCancelled()
{
	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Block GA] Guard montage cancelled."));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
