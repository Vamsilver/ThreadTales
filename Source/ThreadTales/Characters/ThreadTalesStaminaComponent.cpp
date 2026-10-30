// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesStaminaComponent.h"
#include "Characters/ThreadTalesBaseCharacter.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesDummy.h"
#include "Characters/ThreadTalesTraversalInputHelper.h"
#include "Core/ThreadTalesPlayerController.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Abilities/ThreadTalesAbilitySystemComponent.h"
#include "Abilities/ThreadTalesAttributeSet.h"
#include "Abilities/ThreadTalesStaminaConfig.h"
#include "Abilities/ThreadTalesPostureConfig.h"
#include "UI/ThreadTalesHUDWidget.h"
#include "UI/ThreadTalesStatBarWidget.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "ThreadTales.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#endif

UThreadTalesStaminaComponent::UThreadTalesStaminaComponent()
{
	PrimaryComponentTick.bCanEverTick = true; // [TICK-ALWAYS-OK]
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UThreadTalesStaminaComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializeDefaultConfigs();

	if (AThreadTalesBaseCharacter* OwnerChar = GetCharacterOwner())
	{
		if (OwnerChar->HasAuthority() && PostureConfig && PostureConfig->MaxPosture > 0.0f)
		{
			if (UThreadTalesAttributeSet* AttrSet = GetOwnerAttributeSetMutable())
			{
				AttrSet->SetMaxPoise(PostureConfig->MaxPosture);
				AttrSet->SetPoise(PostureConfig->MaxPosture);
			}
		}
	}
}

void UThreadTalesStaminaComponent::InitializeDefaultConfigs()
{
	if (!StaminaConfig)
	{
		StaminaConfig = LoadObject<UThreadTalesStaminaConfig>(nullptr, TEXT("/Game/Blueprints/Data/DA_StaminaConfig.DA_StaminaConfig"));
#if WITH_EDITOR
		if (!StaminaConfig && !IsRunningCommandlet())
		{
			const FString PackageName = TEXT("/Game/Blueprints/Data/DA_StaminaConfig");
			UPackage* Package = CreatePackage(*PackageName);
			if (Package)
			{
				Package->FullyLoad();
				UThreadTalesStaminaConfig* NewConfig = NewObject<UThreadTalesStaminaConfig>(
					Package,
					UThreadTalesStaminaConfig::StaticClass(),
					TEXT("DA_StaminaConfig"),
					RF_Public | RF_Standalone | RF_Transactional);
				if (NewConfig)
				{
					FAssetRegistryModule::AssetCreated(NewConfig);
					Package->MarkPackageDirty();
					const FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
					FSavePackageArgs SaveArgs;
					SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
					UPackage::SavePackage(Package, NewConfig, *PackageFileName, SaveArgs);
					StaminaConfig = NewConfig;
					UE_LOG(LogThreadTales, Display, TEXT("[Stamina] Auto-created and saved DataAsset: /Game/Blueprints/Data/DA_StaminaConfig"));
				}
			}
		}
#endif
	}

	if (!PostureConfig)
	{
		PostureConfig = LoadObject<UThreadTalesPostureConfig>(nullptr, TEXT("/Game/Blueprints/Data/DA_PostureConfig.DA_PostureConfig"));
#if WITH_EDITOR
		if (!PostureConfig && !IsRunningCommandlet())
		{
			const FString PackageName = TEXT("/Game/Blueprints/Data/DA_PostureConfig");
			UPackage* Package = CreatePackage(*PackageName);
			if (Package)
			{
				Package->FullyLoad();
				UThreadTalesPostureConfig* NewConfig = NewObject<UThreadTalesPostureConfig>(
					Package,
					UThreadTalesPostureConfig::StaticClass(),
					TEXT("DA_PostureConfig"),
					RF_Public | RF_Standalone | RF_Transactional);
				if (NewConfig)
				{
					FAssetRegistryModule::AssetCreated(NewConfig);
					Package->MarkPackageDirty();
					const FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
					FSavePackageArgs SaveArgs;
					SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
					UPackage::SavePackage(Package, NewConfig, *PackageFileName, SaveArgs);
					PostureConfig = NewConfig;
					UE_LOG(LogThreadTales, Display, TEXT("[Posture] Auto-created and saved DataAsset: /Game/Blueprints/Data/DA_PostureConfig"));
				}
			}
		}
#endif
	}
}

AThreadTalesBaseCharacter* UThreadTalesStaminaComponent::GetCharacterOwner() const
{
	return Cast<AThreadTalesBaseCharacter>(GetOwner());
}

UThreadTalesAbilitySystemComponent* UThreadTalesStaminaComponent::GetOwnerASC() const
{
	const AThreadTalesBaseCharacter* OwnerChar = GetCharacterOwner();
	return OwnerChar ? Cast<UThreadTalesAbilitySystemComponent>(OwnerChar->GetAbilitySystemComponent()) : nullptr;
}

const UThreadTalesAttributeSet* UThreadTalesStaminaComponent::GetOwnerAttributeSet() const
{
	const AThreadTalesBaseCharacter* OwnerChar = GetCharacterOwner();
	return OwnerChar ? OwnerChar->GetAttributeSet() : nullptr;
}

UThreadTalesAttributeSet* UThreadTalesStaminaComponent::GetOwnerAttributeSetMutable() const
{
	AThreadTalesBaseCharacter* OwnerChar = GetCharacterOwner();
	return OwnerChar ? const_cast<UThreadTalesAttributeSet*>(OwnerChar->GetAttributeSet()) : nullptr;
}

bool UThreadTalesStaminaComponent::CanSpendStamina() const
{
	const AThreadTalesBaseCharacter* OwnerChar = GetCharacterOwner();
	return OwnerChar ? (OwnerChar->GetStamina() > 0.0f) : false;
}

bool UThreadTalesStaminaComponent::TryConsumeStamina(float CostAmount, bool bApplyRegenDelay)
{
	AThreadTalesBaseCharacter* OwnerChar = GetCharacterOwner();
	UThreadTalesAttributeSet* AttrSet = GetOwnerAttributeSetMutable();
	if (!OwnerChar || !AttrSet)
	{
		return false;
	}

	if (!CanSpendStamina())
	{
		NotifyInsufficientStamina();
		return false;
	}

	const float CurrentStaminaVal = OwnerChar->GetStamina();
	const float NewStaminaVal = FMath::Max(0.0f, CurrentStaminaVal - CostAmount);
	const bool bExhausted = (NewStaminaVal <= 0.0f);

	AttrSet->SetStamina(NewStaminaVal);
	OwnerChar->HandleStaminaChanged(NewStaminaVal, OwnerChar->GetMaxStamina());

	if (bApplyRegenDelay)
	{
		ApplyStaminaRegenDelay(bExhausted);
	}

	if (bExhausted)
	{
		if (const AThreadTalesPlayerController* TTPC = Cast<AThreadTalesPlayerController>(OwnerChar->GetController()))
		{
			if (UThreadTalesHUDWidget* HUD = TTPC->GetPlayerHUDWidget())
			{
				if (UThreadTalesStatBarWidget* StaminaBar = HUD->GetPlayerStaminaBar())
				{
					StaminaBar->TriggerFlash(FLinearColor(1.0f, 0.45f, 0.15f, 0.75f), 0.25f);
				}
			}
		}
	}

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Stamina] %s consumed %.1f stamina (%.1f -> %.1f / %.1f)%s"),
		*GetNameSafe(OwnerChar), CostAmount, CurrentStaminaVal, NewStaminaVal, OwnerChar->GetMaxStamina(),
		bExhausted ? TEXT(" [EXHAUSTED]") : TEXT(""));

	return true;
}

void UThreadTalesStaminaComponent::ApplyStaminaRegenDelay(bool bExhausted)
{
	UThreadTalesAbilitySystemComponent* ASC = GetOwnerASC();
	UWorld* World = GetWorld();
	if (!ASC || !World)
	{
		return;
	}

	const float DelaySeconds = bExhausted
		? (StaminaConfig ? StaminaConfig->ExhaustedRegenDelay : 1.25f)
		: (StaminaConfig ? StaminaConfig->NormalRegenDelay : 0.75f);

	if (DelaySeconds <= 0.0f)
	{
		return;
	}

	if (!ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_StaminaRegenBlocked))
	{
		ASC->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_StaminaRegenBlocked);
	}

	World->GetTimerManager().ClearTimer(StaminaRegenDelayTimerHandle);
	World->GetTimerManager().SetTimer(
		StaminaRegenDelayTimerHandle,
		this,
		&UThreadTalesStaminaComponent::OnStaminaRegenDelayExpired,
		DelaySeconds,
		false);
}

void UThreadTalesStaminaComponent::OnStaminaRegenDelayExpired()
{
	if (UThreadTalesAbilitySystemComponent* ASC = GetOwnerASC())
	{
		ASC->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_StaminaRegenBlocked);
	}
}

void UThreadTalesStaminaComponent::ApplyPoiseRegenDelay(float CustomDelay)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float DelaySeconds = (CustomDelay >= 0.0f)
		? CustomDelay
		: (PostureConfig ? PostureConfig->PostureRegenDelay : (StaminaConfig ? StaminaConfig->PoiseRegenDelay : 1.10f));

	if (DelaySeconds <= 0.0f)
	{
		bPoiseRegenBlockedByDelay = false;
		return;
	}

	bPoiseRegenBlockedByDelay = true;
	World->GetTimerManager().ClearTimer(PoiseRegenDelayTimerHandle);
	World->GetTimerManager().SetTimer(
		PoiseRegenDelayTimerHandle,
		this,
		&UThreadTalesStaminaComponent::OnPoiseRegenDelayExpired,
		DelaySeconds,
		false);
}

void UThreadTalesStaminaComponent::OnPoiseRegenDelayExpired()
{
	bPoiseRegenBlockedByDelay = false;
}

void UThreadTalesStaminaComponent::NotifyInsufficientStamina()
{
	AThreadTalesBaseCharacter* OwnerChar = GetCharacterOwner();
	if (!OwnerChar)
	{
		return;
	}

	if (const AThreadTalesPlayerController* TTPC = Cast<AThreadTalesPlayerController>(OwnerChar->GetController()))
	{
		if (UThreadTalesHUDWidget* HUD = TTPC->GetPlayerHUDWidget())
		{
			if (UThreadTalesStatBarWidget* StaminaBar = HUD->GetPlayerStaminaBar())
			{
				StaminaBar->TriggerFlash(FLinearColor(1.0f, 0.25f, 0.20f, 0.88f), 0.30f);
			}
		}
	}
}

void UThreadTalesStaminaComponent::ForceClearGaspalsSprint()
{
	AThreadTalesBaseCharacter* OwnerChar = GetCharacterOwner();
	if (!OwnerChar)
	{
		return;
	}

	if (FBoolProperty* SprintProp = CastField<FBoolProperty>(OwnerChar->GetClass()->FindPropertyByName(TEXT("bIsSprintInputDown"))))
	{
		SprintProp->SetPropertyValue_InContainer(OwnerChar, false);
	}

	if (FProperty* InputStateProp = OwnerChar->GetClass()->FindPropertyByName(TEXT("CharacterInputState")))
	{
		if (const FStructProperty* StructProp = CastField<FStructProperty>(InputStateProp))
		{
			void* StructAddr = StructProp->ContainerPtrToValuePtr<void>(OwnerChar);
			if (StructAddr)
			{
				for (TFieldIterator<FBoolProperty> It(StructProp->Struct); It; ++It)
				{
					if (It->GetName().Contains(TEXT("WantsToSprint")))
					{
						It->SetPropertyValue_InContainer(StructAddr, false);
					}
				}

				if (!OwnerChar->HasAuthority())
				{
					if (UFunction* UpdateFunc = OwnerChar->FindFunction(TEXT("UpdateInputState_Server")))
					{
						OwnerChar->ProcessEvent(UpdateFunc, StructAddr);
					}
				}
			}
		}
	}
}

void UThreadTalesStaminaComponent::UpdateSprintStaminaDrain(float DeltaSeconds, bool bSprintKeyHeld)
{
	AThreadTalesBaseCharacter* OwnerChar = GetCharacterOwner();
	UThreadTalesAbilitySystemComponent* ASC = GetOwnerASC();
	UThreadTalesAttributeSet* AttrSet = GetOwnerAttributeSetMutable();
	if (!OwnerChar || !ASC || !AttrSet)
	{
		return;
	}

	const float SprintDrainRate = StaminaConfig ? StaminaConfig->SprintDrainPerSecond : 8.0f;
	const float SprintMinSpeed = StaminaConfig ? StaminaConfig->SprintDrainMinSpeed : 300.0f;

	const bool bGrounded = OwnerChar->GetCharacterMovement() && OwnerChar->GetCharacterMovement()->IsMovingOnGround();
	const bool bAttacking = ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Attacking);
	const AThreadTalesCharacter* PlayerChar = Cast<AThreadTalesCharacter>(OwnerChar);
	const bool bClimbing = PlayerChar && PlayerChar->IsClimbing();
	const bool bActivelySprinting = !bSprintLockedByExhaustion && bGrounded && !OwnerChar->IsEvading() && !OwnerChar->IsBlocking() && !bClimbing && !bAttacking &&
		(bSprintKeyHeld || OwnerChar->IsSprinting()) && (OwnerChar->GetVelocity().Size2D() > SprintMinSpeed) && (OwnerChar->GetStamina() > 0.0f);

	if (bActivelySprinting && SprintDrainRate > 0.0f)
	{
		bWasActivelySprinting = true;
		if (!ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_StaminaRegenBlocked))
		{
			ASC->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_StaminaRegenBlocked);
		}

		const float NewStaminaVal = FMath::Max(0.0f, OwnerChar->GetStamina() - SprintDrainRate * DeltaSeconds);
		AttrSet->SetStamina(NewStaminaVal);
		OwnerChar->HandleStaminaChanged(NewStaminaVal, OwnerChar->GetMaxStamina());

		if (NewStaminaVal <= 0.0f)
		{
			HandleSprintExhaustion();
		}
	}
	else if (bWasActivelySprinting)
	{
		bWasActivelySprinting = false;
		ApplyStaminaRegenDelay(OwnerChar->GetStamina() <= 0.0f);
	}
}

void UThreadTalesStaminaComponent::HandleSprintExhaustion()
{
	AThreadTalesBaseCharacter* OwnerChar = GetCharacterOwner();
	bSprintLockedByExhaustion = true;
	bWasActivelySprinting = false;
	ForceClearGaspalsSprint();
	NotifyInsufficientStamina();
	ApplyStaminaRegenDelay(true);
	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Stamina] %s SPRINT EXHAUSTED -> Stopping sprint."), *GetNameSafe(OwnerChar));
}

void UThreadTalesStaminaComponent::UpdateStaminaRegenDA(float DeltaSeconds)
{
	AThreadTalesBaseCharacter* OwnerChar = GetCharacterOwner();
	UThreadTalesAbilitySystemComponent* ASC = GetOwnerASC();
	UThreadTalesAttributeSet* AttrSet = GetOwnerAttributeSetMutable();
	if (!OwnerChar || !ASC || !AttrSet || !OwnerChar->HasAuthority() || !StaminaConfig || OwnerChar->GetStamina() >= OwnerChar->GetMaxStamina())
	{
		return;
	}

	const bool bBlockedByTag = ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_StaminaRegenBlocked) ||
		ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Attacking) ||
		ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Evading);
	const bool bHoldingBlock = ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Blocking);

	if (!bBlockedByTag)
	{
		if (bHoldingBlock && !StaminaConfig->bPauseRegenWhileBlocking)
		{
			const float NewStam = FMath::Clamp(OwnerChar->GetStamina() + StaminaConfig->StaminaRegenPerSecond * DeltaSeconds, 0.0f, OwnerChar->GetMaxStamina());
			AttrSet->SetStamina(NewStam);
			OwnerChar->HandleStaminaChanged(NewStam, OwnerChar->GetMaxStamina());
		}
		else if (!bHoldingBlock && FMath::Abs(StaminaConfig->StaminaRegenPerSecond - 25.0f) > 0.05f)
		{
			const float RegenDelta = (StaminaConfig->StaminaRegenPerSecond - 25.0f) * DeltaSeconds;
			const float NewStam = FMath::Clamp(OwnerChar->GetStamina() + RegenDelta, 0.0f, OwnerChar->GetMaxStamina());
			AttrSet->SetStamina(NewStam);
			OwnerChar->HandleStaminaChanged(NewStam, OwnerChar->GetMaxStamina());
		}
	}
}

void UThreadTalesStaminaComponent::UpdatePostureRecovery(float DeltaSeconds)
{
	AThreadTalesBaseCharacter* OwnerChar = GetCharacterOwner();
	UThreadTalesAbilitySystemComponent* ASC = GetOwnerASC();
	UThreadTalesAttributeSet* AttrSet = GetOwnerAttributeSetMutable();
	if (!OwnerChar || !ASC || !AttrSet || !OwnerChar->HasAuthority())
	{
		return;
	}

	if (PostureConfig && PostureConfig->MaxPosture > 0.0f && !FMath::IsNearlyEqual(OwnerChar->GetMaxPoise(), PostureConfig->MaxPosture))
	{
		AttrSet->SetMaxPoise(PostureConfig->MaxPosture);
		AttrSet->SetPoise(FMath::Min(OwnerChar->GetPoise(), PostureConfig->MaxPosture));
		OwnerChar->HandlePoiseChanged(OwnerChar->GetPoise(), OwnerChar->GetMaxPoise());
	}

	if (OwnerChar->GetPoise() < OwnerChar->GetMaxPoise())
	{
		const bool bPoiseBlockedByState = bPoiseRegenBlockedByDelay ||
			OwnerChar->IsStaggered() ||
			ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered) ||
			ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_HitReact);

		if (!bPoiseBlockedByState)
		{
			const float BasePoiseRegen = PostureConfig ? PostureConfig->PostureRegenPerSecond : (StaminaConfig ? StaminaConfig->PoiseRegenPerSecond : 20.0f);
			const float BlockRegenMult = PostureConfig ? PostureConfig->BlockingPostureRegenMultiplier : (StaminaConfig ? StaminaConfig->BlockingPoiseRegenMultiplier : 2.0f);
			const bool bHoldingBlock = OwnerChar->IsBlocking();
			const float EffectivePoiseRegen = BasePoiseRegen * (bHoldingBlock ? BlockRegenMult : 1.0f);

			if (EffectivePoiseRegen > 0.0f)
			{
				const float NewPoiseVal = FMath::Clamp(OwnerChar->GetPoise() + EffectivePoiseRegen * DeltaSeconds, 0.0f, OwnerChar->GetMaxPoise());
				AttrSet->SetPoise(NewPoiseVal);
				OwnerChar->HandlePoiseChanged(NewPoiseVal, OwnerChar->GetMaxPoise());
			}
		}
	}
}

void UThreadTalesStaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AThreadTalesBaseCharacter* OwnerChar = GetCharacterOwner();
	if (!OwnerChar || !OwnerChar->IsAlive() || OwnerChar->IsA(AThreadTalesDummy::StaticClass()))
	{
		return;
	}

	const APlayerController* LocalPC = Cast<APlayerController>(OwnerChar->GetController());
	const bool bSprintKeyHeld = LocalPC && FThreadTalesTraversalInputHelper::IsSprintHeld(LocalPC, OwnerChar);

	if (bSprintLockedByExhaustion)
	{
		UThreadTalesAbilitySystemComponent* ASC = GetOwnerASC();
		const bool bRegenBlocked = ASC && ASC->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_StaminaRegenBlocked);
		if (!bSprintKeyHeld || (!bRegenBlocked && OwnerChar->GetStamina() >= 25.0f))
		{
			bSprintLockedByExhaustion = false;
		}
		else
		{
			ForceClearGaspalsSprint();
		}
	}
	else if (OwnerChar->GetStamina() <= 0.0f)
	{
		ForceClearGaspalsSprint();
	}

	UpdateSprintStaminaDrain(DeltaTime, bSprintKeyHeld);
	UpdateStaminaRegenDA(DeltaTime);
	UpdatePostureRecovery(DeltaTime);
}

float UThreadTalesStaminaComponent::GetLightAttackStaminaCost() const
{
	return StaminaConfig ? StaminaConfig->LightAttackCost : 10.0f;
}

float UThreadTalesStaminaComponent::GetDodgeAttackStaminaCost() const
{
	return StaminaConfig ? StaminaConfig->DodgeAttackCost : 10.0f;
}

float UThreadTalesStaminaComponent::GetSprintAttackStaminaCost() const
{
	return StaminaConfig ? StaminaConfig->SprintAttackCost : 12.0f;
}

float UThreadTalesStaminaComponent::GetGroundEvadeStaminaCost() const
{
	return StaminaConfig ? StaminaConfig->GroundEvadeCost : 14.0f;
}

float UThreadTalesStaminaComponent::GetAirEvadeStaminaCost() const
{
	return StaminaConfig ? StaminaConfig->AirEvadeCost : 14.0f;
}

float UThreadTalesStaminaComponent::GetJumpStaminaCost() const
{
	return StaminaConfig ? StaminaConfig->JumpCost : 8.0f;
}

float UThreadTalesStaminaComponent::GetDoubleJumpStaminaCost() const
{
	return StaminaConfig ? StaminaConfig->DoubleJumpCost : 8.0f;
}
