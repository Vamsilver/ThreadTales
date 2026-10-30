// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesBaseCharacter.h"
#include "ThreadTales.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Abilities/ThreadTalesAbilitySystemComponent.h"
#include "Abilities/ThreadTalesAttributeSet.h"
#include "Abilities/ThreadTalesGameplayEffects.h"
#include "Abilities/ThreadTalesPostureConfig.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
#include "Characters/ThreadTalesStaminaComponent.h"
#include "Characters/ThreadTalesCombatReactionHandler.h"
#include "Characters/ThreadTalesCombatLocomotionHandler.h"
#include "Combat/ThreadTalesWeapon.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "UI/ThreadTalesTargetLockWidget.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Animation/AnimInstance.h"

AThreadTalesBaseCharacter::AThreadTalesBaseCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true; // [TICK-ALWAYS-OK]

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	MaxHealth = 100.0f;
	bIsAlive = true;

	AbilitySystemComponent = CreateDefaultSubobject<UThreadTalesAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UThreadTalesAttributeSet>(TEXT("AttributeSet"));

	DefaultPassiveEffects.Add(UThreadTalesGE_StaminaRegen::StaticClass());
	DefaultPassiveEffects.Add(UThreadTalesGE_PoiseRegen::StaticClass());

	EquipmentComponent = CreateDefaultSubobject<UThreadTalesEquipmentComponent>(TEXT("EquipmentComponent"));
	StaminaComponent = CreateDefaultSubobject<UThreadTalesStaminaComponent>(TEXT("StaminaComponent"));
	CombatReactionHandler = CreateDefaultSubobject<UThreadTalesCombatReactionHandler>(TEXT("CombatReactionHandler"));
	CombatLocomotionHandler = CreateDefaultSubobject<UThreadTalesCombatLocomotionHandler>(TEXT("CombatLocomotionHandler"));

	StimuliSourceComponent = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("StimuliSourceComponent"));
	StimuliSourceComponent->RegisterForSense(UAISense_Sight::StaticClass());

	TargetLockMinScale = 0.45f;

	TargetLockWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("TargetLockWidgetComponent"));
	TargetLockWidgetComponent->SetupAttachment(GetMesh(), TEXT("spine_02"));
	TargetLockWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	TargetLockWidgetComponent->SetDrawAtDesiredSize(true);
	TargetLockWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
	TargetLockWidgetComponent->SetVisibility(false);
	StimuliSourceComponent->RegisterForSense(UAISense_Hearing::StaticClass());
	StimuliSourceComponent->bAutoRegister = true;

	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		GetCapsuleComponent()->CanCharacterStepUpOn = ECB_No;
	}
}

void AThreadTalesBaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AThreadTalesBaseCharacter, bIsAlive);
}

UAbilitySystemComponent* AThreadTalesBaseCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AThreadTalesBaseCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		UpdateMovementModeGameplayTags(GetCharacterMovement() ? GetCharacterMovement()->MovementMode.GetValue() : MOVE_None);
	}
}

void AThreadTalesBaseCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void AThreadTalesBaseCharacter::InitializeDefaultAbilitiesAndPassives()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass && !AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass))
		{
			FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);
			AbilitySystemComponent->GiveAbility(Spec);
		}
	}

	// Fallback migration: if legacy EvadeAbilityClass is still set on any archetype, ensure it is granted
	if (EvadeAbilityClass && !AbilitySystemComponent->FindAbilitySpecFromClass(EvadeAbilityClass))
	{
		FGameplayAbilitySpec Spec(EvadeAbilityClass, 1, INDEX_NONE, this);
		AbilitySystemComponent->GiveAbility(Spec);
	}

	for (const TSubclassOf<UGameplayEffect>& EffectClass : DefaultPassiveEffects)
	{
		if (EffectClass)
		{
			FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
			Context.AddInstigator(this, this);
			AbilitySystemComponent->ApplyGameplayEffectToSelf(EffectClass->GetDefaultObject<UGameplayEffect>(), 1.0f, Context);
		}
	}

	if (EquipmentComponent && DefaultWeaponClass)
	{
		EquipmentComponent->EquipWeapon(DefaultWeaponClass);
	}
}

void AThreadTalesBaseCharacter::BeginPlay()
{
	if (GetMesh() && !GetMesh()->ComponentTags.Contains(FName("AttachOverlayItem")))
	{
		GetMesh()->ComponentTags.AddUnique(FName("AttachOverlayItem"));
	}

	if (TargetLockWidgetComponent)
	{
		TargetLockWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
		TargetLockWidgetComponent->SetDrawAtDesiredSize(true);
		TargetLockWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
		if (TargetLockWidgetClass)
		{
			TargetLockWidgetComponent->SetWidgetClass(TargetLockWidgetClass);
		}
		TargetLockWidgetComponent->SetVisibility(false);

		if (GetMesh())
		{
			FName SocketToAttach = TEXT("spine_02");
			if (GetMesh()->DoesSocketExist(TEXT("TargetLockSocket")))
			{
				SocketToAttach = TEXT("TargetLockSocket");
			}
			else if (!GetMesh()->DoesSocketExist(TEXT("spine_02")))
			{
				if (GetMesh()->DoesSocketExist(TEXT("spine_01")))
				{
					SocketToAttach = TEXT("spine_01");
				}
				else if (GetMesh()->DoesSocketExist(TEXT("spine_03")))
				{
					SocketToAttach = TEXT("spine_03");
				}
			}

			if (GetMesh()->DoesSocketExist(SocketToAttach))
			{
				TargetLockWidgetComponent->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketToAttach);
			}
			else
			{
				TargetLockWidgetComponent->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				TargetLockWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 20.0f));
			}
		}
	}

	Super::BeginPlay();

	if (!CombatReactionHandler || CombatReactionHandler->GetOwner() != this)
	{
		CombatReactionHandler = FindComponentByClass<UThreadTalesCombatReactionHandler>();
	}
	if (!CombatLocomotionHandler || CombatLocomotionHandler->GetOwner() != this)
	{
		CombatLocomotionHandler = FindComponentByClass<UThreadTalesCombatLocomotionHandler>();
	}
	if (!EquipmentComponent || EquipmentComponent->GetOwner() != this)
	{
		EquipmentComponent = FindComponentByClass<UThreadTalesEquipmentComponent>();
	}
	if (!StaminaComponent || StaminaComponent->GetOwner() != this)
	{
		StaminaComponent = FindComponentByClass<UThreadTalesStaminaComponent>();
	}
	if (!StimuliSourceComponent || StimuliSourceComponent->GetOwner() != this)
	{
		StimuliSourceComponent = FindComponentByClass<UAIPerceptionStimuliSourceComponent>();
	}
	if (StimuliSourceComponent)
	{
		StimuliSourceComponent->RegisterWithPerceptionSystem();
	}

	if (AbilitySystemComponent && (!AbilitySystemComponent->AbilityActorInfo.IsValid() || AbilitySystemComponent->AbilityActorInfo->OwnerActor == nullptr))
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}

	InitializeDefaultAbilitiesAndPassives();

	const uint8 CurrentPose = GetOverlayPose();
	if (CurrentPose > 0)
	{
		SetOverlayPose(CurrentPose);
	}
	UpdateMovementModeGameplayTags(GetCharacterMovement() ? GetCharacterMovement()->MovementMode.GetValue() : MOVE_None);
}

void AThreadTalesBaseCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateTargetLockWidgetScaling();
}

void AThreadTalesBaseCharacter::UpdateTargetLockWidgetScaling()
{
	if (!TargetLockWidgetComponent || !TargetLockWidgetComponent->IsVisible())
	{
		return;
	}

	APlayerCameraManager* CamManager = nullptr;
	if (CurrentTargetingActor.IsValid())
	{
		if (const APawn* TargetingPawn = Cast<APawn>(CurrentTargetingActor.Get()))
		{
			if (const APlayerController* PC = Cast<APlayerController>(TargetingPawn->GetController()))
			{
				CamManager = PC->PlayerCameraManager;
			}
		}
	}

	if (!CamManager)
	{
		if (const UWorld* World = GetWorld())
		{
			if (const APlayerController* LocalPC = World->GetFirstPlayerController())
			{
				CamManager = LocalPC->PlayerCameraManager;
			}
		}
	}

	if (CamManager)
	{
		const FVector CamLoc = CamManager->GetCameraLocation();
		const FVector AnchorLoc = GetTargetLockLocation_Implementation(CurrentTargetingActor.Get());
		const float Distance = FVector::Dist(CamLoc, AnchorLoc);

		const float Scale = FMath::GetMappedRangeValueClamped(
			FVector2D(350.0f, 2200.0f),
			FVector2D(1.0f, TargetLockMinScale),
			Distance
		);

		if (UUserWidget* UserWidget = TargetLockWidgetComponent->GetUserWidgetObject())
		{
			UserWidget->SetRenderScale(FVector2D(Scale, Scale));
		}
	}
}

void AThreadTalesBaseCharacter::ActivateAbilityByTag(FGameplayTag ContainerTag)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(ContainerTag));
	}
}

void AThreadTalesBaseCharacter::PerformEvade()
{
	if (GetCharacterMovement() && (!GetCharacterMovement()->IsMovingOnGround() || GetCharacterMovement()->IsFalling()))
	{
		return;
	}

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(ThreadTalesGameplayTags::TAG_Ability_Evade));
	}
}

void AThreadTalesBaseCharacter::ToggleCombatStance()
{
	if (EquipmentComponent)
	{
		EquipmentComponent->ToggleCombatStance();
	}
}

bool AThreadTalesBaseCharacter::IsInCombatStance() const
{
	return EquipmentComponent && EquipmentComponent->IsInCombatStance();
}

bool AThreadTalesBaseCharacter::IsEvading() const
{
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Evading);
}

bool AThreadTalesBaseCharacter::IsBlocking() const
{
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Blocking);
}

bool AThreadTalesBaseCharacter::IsGrounded() const
{
	if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_Grounded))
	{
		return true;
	}
	return GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround();
}

bool AThreadTalesBaseCharacter::IsAirborne() const
{
	if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_Airborne))
	{
		return true;
	}
	return GetCharacterMovement() && (GetCharacterMovement()->IsFalling() || GetCharacterMovement()->IsFlying());
}

void AThreadTalesBaseCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	if (GetCharacterMovement())
	{
		UpdateMovementModeGameplayTags(GetCharacterMovement()->MovementMode);
	}
}

void AThreadTalesBaseCharacter::UpdateMovementModeGameplayTags(EMovementMode NewMovementMode)
{
	if (!AbilitySystemComponent || !HasAuthority())
	{
		return;
	}

	const bool bGrounded = (NewMovementMode == MOVE_Walking || NewMovementMode == MOVE_NavWalking);
	const bool bAirborne = (NewMovementMode == MOVE_Falling || NewMovementMode == MOVE_Flying);

	if (bGrounded)
	{
		if (!AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_Grounded))
		{
			AbilitySystemComponent->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_Grounded);
		}
		while (AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_Airborne))
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_Airborne);
		}
	}
	else if (bAirborne)
	{
		if (!AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_Airborne))
		{
			AbilitySystemComponent->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_Airborne);
		}
		while (AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_Grounded))
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_Grounded);
		}
	}
}

bool AThreadTalesBaseCharacter::IsParrying() const
{
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Parrying);
}

bool AThreadTalesBaseCharacter::IsFacingAttacker(const AActor* AttackerActor, float MaxHalfAngleDegrees) const
{
	if (!AttackerActor)
	{
		return true;
	}

	FVector ToAttacker = AttackerActor->GetActorLocation() - GetActorLocation();
	ToAttacker.Z = 0.0f;
	if (ToAttacker.IsNearlyZero())
	{
		return true;
	}
	ToAttacker.Normalize();

	FVector Forward = GetActorForwardVector();
	Forward.Z = 0.0f;
	Forward.Normalize();

	const UThreadTalesPostureConfig* PostureCfg = GetPostureConfig();
	const float EffectiveAngle = (PostureCfg && FMath::IsNearlyEqual(MaxHalfAngleDegrees, 95.0f))
		? PostureCfg->FrontalGuardHalfAngleDegrees
		: MaxHalfAngleDegrees;

	const float Dot = FVector::DotProduct(Forward, ToAttacker);
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(EffectiveAngle, 10.0f, 180.0f)));
	return Dot >= MinDot;
}

bool AThreadTalesBaseCharacter::IsSprinting() const
{
	if (!CanSpendStamina())
	{
		return false;
	}
	if (GetVelocity().Size() > 550.0f)
	{
		return true;
	}
	return false;
}

void AThreadTalesBaseCharacter::StopSprinting()
{
	if (StaminaComponent)
	{
		StaminaComponent->ForceClearGaspalsSprint();
	}
}

bool AThreadTalesBaseCharacter::CanAimOrBlock() const
{
	return IsInCombatStance() && !IsStaggered() && !IsEvading() && !IsSprinting();
}

float AThreadTalesBaseCharacter::GetHealth() const { return AttributeSet ? AttributeSet->GetHealth() : 0.0f; }
float AThreadTalesBaseCharacter::GetMaxHealth() const { return AttributeSet ? AttributeSet->GetMaxHealth() : 0.0f; }
float AThreadTalesBaseCharacter::GetStamina() const { return AttributeSet ? AttributeSet->GetStamina() : 0.0f; }
float AThreadTalesBaseCharacter::GetMaxStamina() const { return AttributeSet ? AttributeSet->GetMaxStamina() : 0.0f; }
float AThreadTalesBaseCharacter::GetPoise() const { return AttributeSet ? AttributeSet->GetPoise() : 0.0f; }
float AThreadTalesBaseCharacter::GetMaxPoise() const { return AttributeSet ? AttributeSet->GetMaxPoise() : 0.0f; }

bool AThreadTalesBaseCharacter::CanSpendStamina() const
{
	return StaminaComponent ? StaminaComponent->CanSpendStamina() : (GetStamina() > 0.0f);
}

const UThreadTalesPostureConfig* AThreadTalesBaseCharacter::GetPostureConfig() const { return StaminaComponent ? StaminaComponent->GetPostureConfig() : nullptr; }
float AThreadTalesBaseCharacter::GetPostureBreakStunDuration() const { const UThreadTalesPostureConfig* Cfg = GetPostureConfig(); return Cfg ? Cfg->PostureBreakStunDuration : 3.50f; }
float AThreadTalesBaseCharacter::GetParryRecoilDuration() const { UAnimMontage* StaggerMtg = GetStaggerMontage(); return StaggerMtg ? FMath::Max(0.6f, StaggerMtg->GetPlayLength()) : 0.82f; }
bool AThreadTalesBaseCharacter::TryConsumeStamina(float CostAmount, bool bApplyRegenDelay) { return StaminaComponent ? StaminaComponent->TryConsumeStamina(CostAmount, bApplyRegenDelay) : true; }
void AThreadTalesBaseCharacter::ApplyStaminaRegenDelay(bool bExhausted) { if (StaminaComponent) StaminaComponent->ApplyStaminaRegenDelay(bExhausted); }
void AThreadTalesBaseCharacter::NotifyInsufficientStamina() { if (StaminaComponent) StaminaComponent->NotifyInsufficientStamina(); }
float AThreadTalesBaseCharacter::GetJumpStaminaCost() const { return StaminaComponent ? StaminaComponent->GetJumpStaminaCost() : 8.0f; }

UAnimMontage* AThreadTalesBaseCharacter::GetStaggerMontage() const { return CombatReactionHandler ? CombatReactionHandler->GetStaggerMontage() : nullptr; }
UAnimMontage* AThreadTalesBaseCharacter::GetPostureBreakMontage() const { return CombatReactionHandler ? CombatReactionHandler->GetPostureBreakMontage() : nullptr; }
UAnimMontage* AThreadTalesBaseCharacter::GetHitReactMontage() const { return CombatReactionHandler ? CombatReactionHandler->GetHitReactMontage() : nullptr; }
UAnimMontage* AThreadTalesBaseCharacter::GetGuardHitMontage() const { return CombatReactionHandler ? CombatReactionHandler->GetGuardHitMontage() : nullptr; }
UAnimMontage* AThreadTalesBaseCharacter::GetGuardHitHeavyMontage() const { return CombatReactionHandler ? CombatReactionHandler->GetGuardHitHeavyMontage() : nullptr; }
UAnimMontage* AThreadTalesBaseCharacter::GetParryMontage() const { return CombatReactionHandler ? CombatReactionHandler->GetParryMontage() : nullptr; }

UAnimMontage* AThreadTalesBaseCharacter::GetAirStaggerMontage() const { return CombatReactionHandler ? CombatReactionHandler->GetAirStaggerMontage() : nullptr; }
UAnimMontage* AThreadTalesBaseCharacter::GetAirPostureBreakMontage() const { return CombatReactionHandler ? CombatReactionHandler->GetAirPostureBreakMontage() : nullptr; }
UAnimMontage* AThreadTalesBaseCharacter::GetAirHitReactMontage() const { return CombatReactionHandler ? CombatReactionHandler->GetAirHitReactMontage() : nullptr; }
UAnimMontage* AThreadTalesBaseCharacter::GetRecoverMontage() const { return CombatReactionHandler ? CombatReactionHandler->GetRecoverMontage() : nullptr; }

UAnimMontage* AThreadTalesBaseCharacter::GetEffectiveStaggerMontage() const
{
	if (IsAirborne())
	{
		if (UAnimMontage* AirMtg = GetAirStaggerMontage())
		{
			return AirMtg;
		}
	}
	return GetStaggerMontage();
}

UAnimMontage* AThreadTalesBaseCharacter::GetEffectivePostureBreakMontage() const
{
	if (IsAirborne())
	{
		if (UAnimMontage* AirMtg = GetAirPostureBreakMontage())
		{
			return AirMtg;
		}
	}
	return GetPostureBreakMontage();
}

UAnimMontage* AThreadTalesBaseCharacter::GetEffectiveHitReactMontage() const
{
	if (IsAirborne())
	{
		if (UAnimMontage* AirMtg = GetAirHitReactMontage())
		{
			return AirMtg;
		}
	}
	return GetHitReactMontage();
}

bool AThreadTalesBaseCharacter::IsStaggered() const
{
	if (AbilitySystemComponent && 
		(AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered) ||
		 AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_PostureBroken)))
	{
		return true;
	}

	if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		if (UAnimMontage* StaggerMtg = GetEffectiveStaggerMontage())
		{
			if (AnimInst->Montage_IsPlaying(StaggerMtg) || AnimInst->Montage_IsActive(StaggerMtg))
			{
				return true;
			}
		}

		if (UAnimMontage* PostureBreakMtg = GetEffectivePostureBreakMontage())
		{
			if (AnimInst->Montage_IsPlaying(PostureBreakMtg) || AnimInst->Montage_IsActive(PostureBreakMtg))
			{
				return true;
			}
		}

		if (UAnimMontage* RecMtg = GetRecoverMontage())
		{
			if (AnimInst->Montage_IsPlaying(RecMtg) || AnimInst->Montage_IsActive(RecMtg))
			{
				return true;
			}
		}
	}

	return false;
}

void AThreadTalesBaseCharacter::ApplyHitStop(float Duration)
{
	if (CombatReactionHandler)
	{
		CombatReactionHandler->ApplyHitStop(Duration);
	}
}

void AThreadTalesBaseCharacter::ResumeFromHitStop()
{
	if (CombatReactionHandler)
	{
		CombatReactionHandler->ResumeFromHitStop();
	}
}

void AThreadTalesBaseCharacter::HandleHealthChanged(float NewHealth, float MaxHealthVal)
{
	OnHealthChanged.Broadcast(NewHealth, MaxHealthVal);
}

void AThreadTalesBaseCharacter::HandleStaminaChanged(float NewStamina, float MaxStaminaVal)
{
	OnStaminaChanged.Broadcast(NewStamina, MaxStaminaVal);
}

void AThreadTalesBaseCharacter::HandlePoiseChanged(float NewPoise, float MaxPoiseVal)
{
	OnPoiseChanged.Broadcast(NewPoise, MaxPoiseVal);
}

void AThreadTalesBaseCharacter::HandleStaggered(AActor* AttackerActor)
{
	if (CombatReactionHandler)
	{
		CombatReactionHandler->HandleStaggered(AttackerActor);
	}
}

void AThreadTalesBaseCharacter::HandleHitReact(AActor* AttackerActor)
{
	if (CombatReactionHandler)
	{
		CombatReactionHandler->HandleHitReact(AttackerActor);
	}
}

void AThreadTalesBaseCharacter::HandleGuardHit(AActor* AttackerActor)
{
	if (CombatReactionHandler)
	{
		CombatReactionHandler->HandleGuardHit(AttackerActor);
	}
}

void AThreadTalesBaseCharacter::HandleSuccessfulParry(AActor* AttackingActor)
{
	if (CombatReactionHandler)
	{
		CombatReactionHandler->HandleSuccessfulParry(AttackingActor);
	}
}

void AThreadTalesBaseCharacter::OpenParryCounterWindow(float Duration)
{
	if (CombatReactionHandler)
	{
		CombatReactionHandler->OpenParryCounterWindow(Duration);
	}
}

void AThreadTalesBaseCharacter::CloseParryCounterWindow()
{
	if (CombatReactionHandler)
	{
		CombatReactionHandler->CloseParryCounterWindow();
	}
}

void AThreadTalesBaseCharacter::HandleParried(AActor* ParryingActor)
{
	if (CombatReactionHandler)
	{
		CombatReactionHandler->HandleParried(ParryingActor);
	}
}

void AThreadTalesBaseCharacter::FinishParriedStagger()
{
	if (CombatReactionHandler)
	{
		CombatReactionHandler->FinishParriedStagger();
	}
}

void AThreadTalesBaseCharacter::HandlePostureBreak(AActor* ParryingActor, float StunDuration)
{
	if (CombatReactionHandler)
	{
		CombatReactionHandler->HandlePostureBreak(ParryingActor, StunDuration);
	}
}

void AThreadTalesBaseCharacter::FinishPostureBreak()
{
	if (CombatReactionHandler)
	{
		CombatReactionHandler->FinishPostureBreak();
	}
}

float AThreadTalesBaseCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (!bIsAlive || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	if (AbilitySystemComponent && 
		AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Invulnerable))
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Character] %s EVADED incoming damage of %.1f (I-Frames Active)!"), *GetName(), DamageAmount);
		return 0.0f;
	}

	if (HasAuthority() && AttributeSet)
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Character] %s TakeDamage direct/environmental: %.1f from %s"),
			*GetName(), DamageAmount, DamageCauser ? *DamageCauser->GetName() : TEXT("Environment"));
		AttributeSet->ReceiveDamage(DamageAmount, DamageCauser);
	}

	return ActualDamage;
}

void AThreadTalesBaseCharacter::Die(AActor* KillerActor)
{
	if (!bIsAlive)
	{
		return;
	}

	bIsAlive = false;

	CloseParryCounterWindow();

	if (CombatReactionHandler)
	{
		CombatReactionHandler->ClearReactionTimers();
	}

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Character] %s has DIED! Killed by: %s"), *GetName(), KillerActor ? *KillerActor->GetName() : TEXT("Unknown"));

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->DisableMovement();
		GetCharacterMovement()->StopMovementImmediately();
	}

	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (GetMesh())
	{
		GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
		GetMesh()->SetSimulatePhysics(true);
	}

	OnCharacterDeath.Broadcast(this, KillerActor);
}

void AThreadTalesBaseCharacter::OnRep_IsAlive()
{
	if (!bIsAlive)
	{
		if (GetCharacterMovement())
		{
			GetCharacterMovement()->DisableMovement();
			GetCharacterMovement()->StopMovementImmediately();
		}

		if (GetCapsuleComponent())
		{
			GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		if (GetMesh())
		{
			GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
			GetMesh()->SetSimulatePhysics(true);
		}
	}
}

uint8 AThreadTalesBaseCharacter::GetOverlayPose() const
{
	return CombatLocomotionHandler ? CombatLocomotionHandler->GetOverlayPose() : 0;
}

void AThreadTalesBaseCharacter::SetOverlayPose(uint8 NewOverlayPose)
{
	if (CombatLocomotionHandler)
	{
		CombatLocomotionHandler->SetOverlayPose(NewOverlayPose);
	}
}

void AThreadTalesBaseCharacter::SetOverlayPoseByName(FName PoseName)
{
	if (CombatLocomotionHandler)
	{
		CombatLocomotionHandler->SetOverlayPoseByName(PoseName);
	}
}

void AThreadTalesBaseCharacter::SetAimInputDown(bool bAiming)
{
	if (CombatLocomotionHandler)
	{
		CombatLocomotionHandler->SetAimInputDown(bAiming);
	}
}

void AThreadTalesBaseCharacter::ServerSetAimInputDown_Implementation(bool bAiming)
{
	if (CombatLocomotionHandler)
	{
		CombatLocomotionHandler->ServerSetAimInputDown_Implementation(bAiming);
	}
}

void AThreadTalesBaseCharacter::SetStrafeInputDown(bool bStrafing)
{
	if (CombatLocomotionHandler)
	{
		CombatLocomotionHandler->SetStrafeInputDown(bStrafing);
	}
}

void AThreadTalesBaseCharacter::ServerSetStrafeInputDown_Implementation(bool bStrafing)
{
	if (CombatLocomotionHandler)
	{
		CombatLocomotionHandler->ServerSetStrafeInputDown_Implementation(bStrafing);
	}
}

bool AThreadTalesBaseCharacter::CanBeTargeted_Implementation(const AActor* TargetingActor) const
{
	return IsAlive() && (TargetingActor != this);
}

FVector AThreadTalesBaseCharacter::GetTargetLockLocation_Implementation(const AActor* TargetingActor) const
{
	if (const USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (MeshComp->DoesSocketExist(TEXT("TargetLockSocket")))
		{
			return MeshComp->GetSocketLocation(TEXT("TargetLockSocket"));
		}
		if (MeshComp->DoesSocketExist(TEXT("spine_02")))
		{
			return MeshComp->GetSocketLocation(TEXT("spine_02"));
		}
		if (MeshComp->DoesSocketExist(TEXT("spine_01")))
		{
			return MeshComp->GetSocketLocation(TEXT("spine_01"));
		}
		if (MeshComp->DoesSocketExist(TEXT("spine_03")))
		{
			return MeshComp->GetSocketLocation(TEXT("spine_03"));
		}
	}
	return GetActorLocation() + FVector(0.0f, 0.0f, 20.0f);
}

void AThreadTalesBaseCharacter::OnTargetLocked_Implementation(AActor* TargetingActor)
{
	CurrentTargetingActor = TargetingActor;

	// In 2P Co-op, only display the target lock reticle to the player who actually engaged it
	if (const APawn* TargetingPawn = Cast<APawn>(TargetingActor))
	{
		if (!TargetingPawn->IsLocallyControlled())
		{
			return;
		}
	}

	if (TargetLockWidgetComponent)
	{
		if (!TargetLockWidgetComponent->GetWidgetClass())
		{
			if (TargetLockWidgetClass)
			{
				TargetLockWidgetComponent->SetWidgetClass(TargetLockWidgetClass);
			}
			else if (const AThreadTalesBaseCharacter* TargetingBaseChar = Cast<AThreadTalesBaseCharacter>(TargetingActor))
			{
				if (TargetingBaseChar->GetTargetLockWidgetClass())
				{
					TargetLockWidgetComponent->SetWidgetClass(TargetingBaseChar->GetTargetLockWidgetClass());
				}
			}
		}

		UpdateTargetLockWidgetScaling();
		TargetLockWidgetComponent->SetVisibility(true);

		UUserWidget* UserWidget = TargetLockWidgetComponent->GetUserWidgetObject();
		if (!UserWidget && TargetLockWidgetComponent->GetWidgetClass())
		{
			TargetLockWidgetComponent->InitWidget();
			UserWidget = TargetLockWidgetComponent->GetUserWidgetObject();
		}

		if (UThreadTalesTargetLockWidget* LockWidget = Cast<UThreadTalesTargetLockWidget>(UserWidget))
		{
			LockWidget->NotifyTargetLocked(this, TargetingActor);
		}
	}
}

void AThreadTalesBaseCharacter::OnTargetUnlocked_Implementation(AActor* TargetingActor)
{
	CurrentTargetingActor = nullptr;

	if (TargetLockWidgetComponent)
	{
		TargetLockWidgetComponent->SetVisibility(false);

		if (UThreadTalesTargetLockWidget* LockWidget = Cast<UThreadTalesTargetLockWidget>(TargetLockWidgetComponent->GetUserWidgetObject()))
		{
			LockWidget->NotifyTargetUnlocked();
		}
	}
}
