// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesEnemyCharacter.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Components/StateTreeComponent.h"
#include "Components/WidgetComponent.h"
#include "UI/ThreadTalesEnemyOverheadWidget.h"
#include "StateTree.h"
#include "AI/ThreadTalesAIController.h"
#include "AI/ThreadTalesEnemyConfig.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
#include "Abilities/ThreadTalesAbilitySystemComponent.h"
#include "Abilities/ThreadTalesAttributeSet.h"
#include "Abilities/ThreadTalesGameplayEffects.h"
#include "Components/SphereComponent.h"
#include "Combat/ThreadTalesWeapon.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "AI/ThreadTalesStateTreeTasks.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "ThreadTales.h"

AThreadTalesEnemyCharacter::AThreadTalesEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AIControllerClass = AThreadTalesAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bRequestedMoveUseAcceleration = true;
		MoveComp->bOrientRotationToMovement = true;
		MoveComp->RotationRate = FRotator(0.0f, 360.0f, 0.0f);
		MoveComp->MaxWalkSpeed = 500.0f;
	}

	StateTreeComponent = CreateDefaultSubobject<UStateTreeComponent>(TEXT("StateTreeComponent"));
	StateTreeComponent->PrimaryComponentTick.bCanEverTick = true;
	StateTreeComponent->PrimaryComponentTick.bStartWithTickEnabled = true;
	StateTreeComponent->PrimaryComponentTick.TickInterval = 0.05f;
	StateTreeComponent->bAutoActivate = true;
	StateTreeComponent->SetStartLogicAutomatically(false);

	OverheadWidgetRelativeLocation = FVector(0.0f, 0.0f, 115.0f);
	OverheadWidgetClass = UThreadTalesEnemyOverheadWidget::StaticClass();

	OverheadWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidgetComponent"));
	OverheadWidgetComponent->SetupAttachment(GetRootComponent());
	OverheadWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	OverheadWidgetComponent->SetDrawAtDesiredSize(true);
	OverheadWidgetComponent->SetRelativeLocation(OverheadWidgetRelativeLocation);
	if (OverheadWidgetClass)
	{
		OverheadWidgetComponent->SetWidgetClass(OverheadWidgetClass);
	}

	RetargetMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RetargetMeshComponent"));
	RetargetMeshComponent->SetupAttachment(GetMesh());
	RetargetMeshComponent->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	RetargetMeshComponent->SetCollisionProfileName(TEXT("NoCollision"));

	AttackHitbox = CreateDefaultSubobject<USphereComponent>(TEXT("AttackHitbox"));
	AttackHitbox->SetupAttachment(GetRootComponent());
	AttackHitbox->SetSphereRadius(65.0f);
	AttackHitbox->SetRelativeLocation(FVector(75.0f, 0.0f, 0.0f));
	AttackHitbox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	AttackHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AttackHitbox->OnComponentBeginOverlap.AddDynamic(this, &AThreadTalesEnemyCharacter::OnAttackHitboxOverlap);
}

void AThreadTalesEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (AttackHitbox && AttackHitbox->GetCollisionEnabled() != ECollisionEnabled::NoCollision && GetWorld())
	{
		static const auto CVarDebugTrace = IConsoleManager::Get().FindConsoleVariable(TEXT("tt.Combat.DebugTrace"));
		if (CVarDebugTrace && CVarDebugTrace->GetInt() > 0)
		{
			DrawDebugSphere(
				GetWorld(),
				AttackHitbox->GetComponentLocation(),
				AttackHitbox->GetScaledSphereRadius(),
				16,
				FColor::Orange,
				false,
				-1.0f,
				0,
				2.0f
			);
		}
	}
}

void AThreadTalesEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (AttackHitbox && EnemyConfig)
	{
		AttackHitbox->SetSphereRadius(EnemyConfig->HitboxSettings.SphereRadius);
		AttackHitbox->SetRelativeLocation(EnemyConfig->HitboxSettings.RelativeOffset);
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		const float ConfigTurnRate = EnemyConfig ? EnemyConfig->DifficultySettings.TrackingTurnRate : 360.0f;
		MoveComp->RotationRate = FRotator(0.0f, ConfigTurnRate, 0.0f);
		MoveComp->bAllowPhysicsRotationDuringAnimRootMotion = false;
		MoveComp->ProcessRootMotionPreConvertToWorld.BindUObject(this, &AThreadTalesEnemyCharacter::FilterRootMotionPreConvert);
	}

	if (bAutoDrawWeapon && EquipmentComponent && HasAuthority())
	{
		if (AThreadTalesWeapon* Weapon = EquipmentComponent->GetCurrentWeapon())
		{
			Weapon->AttachToHand();
			Weapon->SetWeaponState(EWeaponCombatState::Drawn);
			SetOverlayPoseByName(Weapon->GetOverlayPoseName());
		}
	}

	if (bAutoStartAI && StateTreeComponent)
	{
		InitializeStateTreeAI();
	}

	// CDO Self-Healing Fallback [DO_NOT_DO: 16]
	if (!OverheadWidgetComponent || OverheadWidgetComponent->GetOwner() != this)
	{
		OverheadWidgetComponent = FindComponentByClass<UWidgetComponent>();
	}

	if (OverheadWidgetComponent)
	{
		OverheadWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
		OverheadWidgetComponent->SetDrawAtDesiredSize(true);
		OverheadWidgetComponent->SetRelativeLocation(OverheadWidgetRelativeLocation);

		if (OverheadWidgetClass && OverheadWidgetComponent->GetWidgetClass() != OverheadWidgetClass)
		{
			OverheadWidgetComponent->SetWidgetClass(OverheadWidgetClass);
		}

		if (!OverheadWidgetComponent->GetUserWidgetObject())
		{
			OverheadWidgetComponent->InitWidget();
		}

		if (UThreadTalesEnemyOverheadWidget* OverheadWidget = Cast<UThreadTalesEnemyOverheadWidget>(OverheadWidgetComponent->GetUserWidgetObject()))
		{
			OverheadWidget->InitializeForCharacter(this);
		}
	}

	// CDO Self-Healing for RetargetMeshComponent [DO_NOT_DO: 16]
	if (!RetargetMeshComponent || RetargetMeshComponent->GetOwner() != this)
	{
		TArray<USkeletalMeshComponent*> SkelComps;
		GetComponents<USkeletalMeshComponent>(SkelComps);
		for (USkeletalMeshComponent* Comp : SkelComps)
		{
			if (Comp != GetMesh())
			{
				RetargetMeshComponent = Comp;
				break;
			}
		}
	}

	// Instance fallback from CDO if level instance lost its mesh assignment
	if (RetargetMeshComponent && !RetargetMeshComponent->GetSkeletalMeshAsset())
	{
		UClass* ActualClass = GetClass();
		if (ActualClass)
		{
			UObject* DefaultObj = ActualClass->GetDefaultObject();
			const AThreadTalesEnemyCharacter* EnemyCDO = Cast<AThreadTalesEnemyCharacter>(DefaultObj);
			if (EnemyCDO && EnemyCDO->RetargetMeshComponent && EnemyCDO->RetargetMeshComponent->GetSkeletalMeshAsset())
			{
				RetargetMeshComponent->SetSkeletalMeshAsset(EnemyCDO->RetargetMeshComponent->GetSkeletalMeshAsset());
				RetargetMeshComponent->SetAnimInstanceClass(EnemyCDO->RetargetMeshComponent->GetAnimClass());
			}
		}
	}

	if (RetargetMeshComponent && RetargetMeshComponent->GetSkeletalMeshAsset())
	{
		// Live Retargeting Active: hide human mannequin, show follower creature mesh
		GetMesh()->SetHiddenInGame(true);
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		RetargetMeshComponent->SetHiddenInGame(false);
		RetargetMeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
	else if (RetargetMeshComponent)
	{
		RetargetMeshComponent->SetHiddenInGame(true);
	}

	// CDO Self-Healing for AttackHitbox
	if (!AttackHitbox || AttackHitbox->GetOwner() != this)
	{
		AttackHitbox = FindComponentByClass<USphereComponent>();
	}
	if (AttackHitbox)
	{
		AttackHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AttackHitbox->OnComponentBeginOverlap.RemoveDynamic(this, &AThreadTalesEnemyCharacter::OnAttackHitboxOverlap);
		AttackHitbox->OnComponentBeginOverlap.AddDynamic(this, &AThreadTalesEnemyCharacter::OnAttackHitboxOverlap);
	}
}

void AThreadTalesEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->ProcessRootMotionPreConvertToWorld.Unbind();
	}

	Super::EndPlay(EndPlayReason);
}

FTransform AThreadTalesEnemyCharacter::FilterRootMotionPreConvert(const FTransform& LocalRootMotionTransform, UCharacterMovementComponent* MoveComp, float DeltaSeconds)
{
	if (bIgnoreRootMotionRotation)
	{
		return FTransform(FQuat::Identity, LocalRootMotionTransform.GetTranslation(), LocalRootMotionTransform.GetScale3D());
	}
	return LocalRootMotionTransform;
}

void AThreadTalesEnemyCharacter::InitializeStateTreeAI()
{
	if (!StateTreeComponent)
	{
		return;
	}

#if WITH_EDITOR
	UStateTree* TreeAsset = nullptr;
	if (EnemyConfig && !EnemyConfig->StateTreeAsset.IsNull())
	{
		TreeAsset = EnemyConfig->StateTreeAsset.LoadSynchronous();
	}

	const bool bIsLadybug = (EnemyConfig && !EnemyConfig->AttackComboMontages.IsEmpty() &&
		EnemyConfig->AttackComboMontages[0].GetAssetName().Contains(TEXT("Ladybug")));

	if (!TreeAsset)
	{
		const FString DefaultTreePath = bIsLadybug
			? TEXT("/Game/AI/StateTree/ST_Enemy_Ladybug.ST_Enemy_Ladybug")
			: TEXT("/Game/AI/StateTree/ST_Enemy_Samurai.ST_Enemy_Samurai");

		TreeAsset = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *DefaultTreePath));
	}

	if (TreeAsset && StateTreeComponent->GetStateTreeRunStatus() != EStateTreeRunStatus::Running)
	{
		StateTreeComponent->SetStateTree(TreeAsset);
	}
#endif

	StateTreeComponent->SetComponentTickEnabled(true);
	StateTreeComponent->Activate(true);
	StateTreeComponent->StartLogic();

	const EStateTreeRunStatus RunStatus = StateTreeComponent->GetStateTreeRunStatus();
	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Enemy '%s'] StateTree StartLogic called! Status: %d (0=Running, 1=Failed, 2=Succeeded, 3=Stopped)"),
		*GetName(), static_cast<int32>(RunStatus));
}


static AThreadTalesBaseCharacter* ResolveAttackerCharacter(AActor* AttackerActor, AController* EventInstigator = nullptr)
{
	if (AThreadTalesBaseCharacter* DirectChar = Cast<AThreadTalesBaseCharacter>(AttackerActor))
	{
		return DirectChar;
	}
	if (AttackerActor && AttackerActor->GetOwner())
	{
		if (AThreadTalesBaseCharacter* OwnerChar = Cast<AThreadTalesBaseCharacter>(AttackerActor->GetOwner()))
		{
			return OwnerChar;
		}
	}
	if (EventInstigator)
	{
		if (AThreadTalesBaseCharacter* InstigatorPawn = Cast<AThreadTalesBaseCharacter>(EventInstigator->GetPawn()))
		{
			return InstigatorPawn;
		}
	}
	return nullptr;
}

float AThreadTalesEnemyCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (HasAuthority() && DamageAmount > 0.0f)
	{
		if (AThreadTalesAIController* AICon = Cast<AThreadTalesAIController>(GetController()))
		{
			if (AThreadTalesCharacter* AttackerChar = Cast<AThreadTalesCharacter>(ResolveAttackerCharacter(DamageCauser, EventInstigator)))
			{
				const float ThreatMult = EnemyConfig ? EnemyConfig->ThreatSettings.DamageThreatMultiplier : 1.5f;
				AICon->RegisterThreat(AttackerChar, DamageAmount * ThreatMult);
			}
		}
	}

	return ActualDamage;
}

void AThreadTalesEnemyCharacter::DisarmAndHaltAI()
{
	if (EquipmentComponent)
	{
		if (AThreadTalesWeapon* Weapon = EquipmentComponent->GetCurrentWeapon())
		{
			Weapon->SetHitboxEnabled(false);
		}
	}
	SetHitboxActive(false);

	if (AThreadTalesAIController* AICon = Cast<AThreadTalesAIController>(GetController()))
	{
		AICon->StopMovement();
	}
}

bool AThreadTalesEnemyCharacter::TryHandleAirCombatInterrupt(bool bIsKnockdown, AActor* ParryingActor, float StunDuration)
{
	const bool bIsAirborne = GetCharacterMovement() && (GetCharacterMovement()->IsFlying() || GetCharacterMovement()->IsFalling());
	if (!bIsAirborne)
	{
		return false;
	}

	StopAnimMontage();
	CachedParryingActor = ParryingActor;
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);

	FVector RecoilDir = ParryingActor ? (GetActorLocation() - ParryingActor->GetActorLocation()).GetSafeNormal2D() : -GetActorForwardVector();
	if (RecoilDir.IsNearlyZero())
	{
		RecoilDir = -GetActorForwardVector();
	}

	if (bIsKnockdown)
	{
		bPendingAirKnockdown = true;
		CachedPostureBreakDuration = (StunDuration > 0.0f) ? StunDuration : GetPostureBreakStunDuration();

		if (AbilitySystemComponent && HasAuthority())
		{
			if (!AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_PostureBroken))
			{
				AbilitySystemComponent->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_PostureBroken);
			}
			if (!AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered))
			{
				AbilitySystemComponent->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered);
			}
		}

		// Gentle horizontal recoil without artificial downward velocity - natural gravity brings the enemy down smoothly
		LaunchCharacter(RecoilDir * 180.0f + FVector(0.0f, 0.0f, 60.0f), true, true);
		if (UAnimMontage* PostureBreakMtg = GetEffectivePostureBreakMontage())
		{
			const float PlayedLen = PlayAnimMontage(PostureBreakMtg, 1.0f, FName("AirFallStart"));
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Enemy Air] '%s' AIR POSTURE BREAK -> PlayAnimMontage '%s' at AirFallStart (ret=%.2fs)"),
				*GetName(), *PostureBreakMtg->GetName(), PlayedLen);
		}
		else if (UAnimMontage* ReactMontage = GetEffectiveHitReactMontage())
		{
			PlayAnimMontage(ReactMontage, 1.0f);
		}
		if (StateTreeComponent)
		{
			StateTreeComponent->SendStateTreeEvent(ThreadTalesGameplayTags::TAG_Event_Combat_PostureBreak);
		}
	}
	else
	{
		bPendingAirParryStagger = true;
		if (AbilitySystemComponent && HasAuthority())
		{
			if (!AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered))
			{
				AbilitySystemComponent->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Staggered);
			}
		}
		// Sekiro-style deflection apex arc: upward pop and recoil back, creating readable hang time before falling
		LaunchCharacter(RecoilDir * 260.0f + FVector(0.0f, 0.0f, 150.0f), true, true);
		if (UAnimMontage* AirStaggerMtg = GetAirStaggerMontage())
		{
			PlayAnimMontage(AirStaggerMtg, 1.0f);
		}
		else if (UAnimMontage* AirReactMtg = GetAirHitReactMontage())
		{
			PlayAnimMontage(AirReactMtg, 1.0f);
		}
		if (StateTreeComponent)
		{
			StateTreeComponent->SendStateTreeEvent(ThreadTalesGameplayTags::TAG_Event_Combat_Parry);
		}
	}
	return true;
}

void AThreadTalesEnemyCharacter::HandleHitReact(AActor* AttackerActor)
{
	if (bPendingAirKnockdown || bPendingAirParryStagger || IsStaggered())
	{
		// Suppress HitReact montage while falling from air interrupt or during posture break stun
		DisarmAndHaltAI();
		return;
	}

	Super::HandleHitReact(AttackerActor);
	DisarmAndHaltAI();

	if (AbilitySystemComponent)
	{
		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(ThreadTalesGameplayTags::TAG_Ability_Melee);
		AbilitySystemComponent->CancelAbilities(&CancelTags);
	}

	if (AThreadTalesAIController* AICon = Cast<AThreadTalesAIController>(GetController()))
	{
		if (AThreadTalesCharacter* AttackerChar = Cast<AThreadTalesCharacter>(ResolveAttackerCharacter(AttackerActor)))
		{
			const float ThreatMult = EnemyConfig ? EnemyConfig->ThreatSettings.DamageThreatMultiplier : 1.5f;
			AICon->RegisterThreat(AttackerChar, 60.0f * ThreatMult);
			AICon->SetCombatTarget(AttackerChar);
			AICon->SetFocus(AttackerChar, EAIFocusPriority::Gameplay);
		}
	}
}

void AThreadTalesEnemyCharacter::HandleGuardHit(AActor* AttackerActor)
{
	Super::HandleGuardHit(AttackerActor);

	if (AThreadTalesAIController* AICon = Cast<AThreadTalesAIController>(GetController()))
	{
		AICon->StopMovement();
		if (AThreadTalesCharacter* AttackerChar = Cast<AThreadTalesCharacter>(ResolveAttackerCharacter(AttackerActor)))
		{
			const float ThreatMult = EnemyConfig ? EnemyConfig->ThreatSettings.DamageThreatMultiplier : 1.5f;
			AICon->RegisterThreat(AttackerChar, 30.0f * ThreatMult);
		}
	}
}

void AThreadTalesEnemyCharacter::HandleStaggered(AActor* AttackerActor)
{
	Super::HandleStaggered(AttackerActor);
	DisarmAndHaltAI();

	if (GetCharacterMovement() && GetCharacterMovement()->IsFlying())
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		GetCharacterMovement()->Velocity = FVector::ZeroVector;
	}

	if (StateTreeComponent)
	{
		StateTreeComponent->SendStateTreeEvent(ThreadTalesGameplayTags::TAG_Event_Combat_Parry);
	}
}

void AThreadTalesEnemyCharacter::HandleParried(AActor* ParryingActor)
{
	DisarmAndHaltAI();
	if (TryHandleAirCombatInterrupt(false, ParryingActor, 0.0f))
	{
		return;
	}

	Super::HandleParried(ParryingActor);
	FVector RecoilDir = ParryingActor ? (GetActorLocation() - ParryingActor->GetActorLocation()).GetSafeNormal2D() : -GetActorForwardVector();
	if (RecoilDir.IsNearlyZero())
	{
		RecoilDir = -GetActorForwardVector();
	}
	LaunchCharacter(RecoilDir * 240.0f, true, false);

	if (StateTreeComponent)
	{
		StateTreeComponent->SendStateTreeEvent(ThreadTalesGameplayTags::TAG_Event_Combat_Parry);
	}
}

void AThreadTalesEnemyCharacter::HandlePostureBreak(AActor* ParryingActor, float StunDuration)
{
	DisarmAndHaltAI();
	if (TryHandleAirCombatInterrupt(true, ParryingActor, StunDuration))
	{
		return;
	}

	Super::HandlePostureBreak(ParryingActor, StunDuration);

	// On ground, jump directly to ground impact / stunned phase
	if (UAnimMontage* PostureBreakMtg = GetEffectivePostureBreakMontage())
	{
		if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInst->Montage_JumpToSection(FName("AirFallEnd"), PostureBreakMtg);
		}
	}

	if (StateTreeComponent)
	{
		StateTreeComponent->SendStateTreeEvent(ThreadTalesGameplayTags::TAG_Event_Combat_PostureBreak);
	}
}

void AThreadTalesEnemyCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (bPendingAirKnockdown || bPendingAirParryStagger)
	{
		const bool bWasKnockdown = bPendingAirKnockdown;
		bPendingAirKnockdown = false;
		bPendingAirParryStagger = false;

		if (GetCharacterMovement())
		{
			GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			GetCharacterMovement()->Velocity = FVector::ZeroVector;
		}

		if (bWasKnockdown)
		{
			Super::HandlePostureBreak(CachedParryingActor.Get(), CachedPostureBreakDuration);
			if (UAnimMontage* PostureBreakMtg = GetEffectivePostureBreakMontage())
			{
				if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
				{
					if (AnimInst->Montage_IsPlaying(PostureBreakMtg))
					{
						AnimInst->Montage_JumpToSection(FName("AirFallEnd"), PostureBreakMtg);
						UE_LOG(LogThreadTalesCombat, Display, TEXT("[Enemy Air] '%s' LANDED FROM KNOCKDOWN -> Montage_JumpToSection 'AirFallEnd'"),
							*GetName());
					}
					else
					{
						const float PlayedLen = PlayAnimMontage(PostureBreakMtg, 1.0f, FName("AirFallEnd"));
						UE_LOG(LogThreadTalesCombat, Display, TEXT("[Enemy Air] '%s' LANDED FROM KNOCKDOWN -> PlayAnimMontage at AirFallEnd (ret=%.2fs)"),
							*GetName(), PlayedLen);
					}
				}
			}
		}
		else
		{
			Super::HandleParried(CachedParryingActor.Get());
		}
		CachedParryingActor.Reset();
	}
}

void AThreadTalesEnemyCharacter::OnTargetLocked_Implementation(AActor* TargetingActor)
{
	Super::OnTargetLocked_Implementation(TargetingActor);

	if (UThreadTalesEnemyOverheadWidget* Widget = GetOverheadWidget())
	{
		Widget->SetTargetLocked(true);
	}
}

void AThreadTalesEnemyCharacter::OnTargetUnlocked_Implementation(AActor* TargetingActor)
{
	Super::OnTargetUnlocked_Implementation(TargetingActor);

	if (UThreadTalesEnemyOverheadWidget* Widget = GetOverheadWidget())
	{
		Widget->SetTargetLocked(false);
	}
}

UThreadTalesEnemyOverheadWidget* AThreadTalesEnemyCharacter::GetOverheadWidget() const
{
	if (!OverheadWidgetComponent)
	{
		return nullptr;
	}

	if (UUserWidget* UserWidget = OverheadWidgetComponent->GetUserWidgetObject())
	{
		return Cast<UThreadTalesEnemyOverheadWidget>(UserWidget);
	}

	UWidgetComponent* MutableWidgetComp = const_cast<UWidgetComponent*>(OverheadWidgetComponent.Get());
	MutableWidgetComp->InitWidget();
	if (UThreadTalesEnemyOverheadWidget* OverheadWidget = Cast<UThreadTalesEnemyOverheadWidget>(MutableWidgetComp->GetUserWidgetObject()))
	{
		OverheadWidget->InitializeForCharacter(const_cast<AThreadTalesEnemyCharacter*>(this));
		return OverheadWidget;
	}

	return nullptr;
}

void AThreadTalesEnemyCharacter::Die(AActor* KillerActor)
{
	if (UThreadTalesEnemyOverheadWidget* Widget = GetOverheadWidget())
	{
		Widget->HandleDeath();
	}

	if (StateTreeComponent)
	{
		StateTreeComponent->StopLogic(TEXT("Character Died"));
	}

	ReceiveOnDeath(KillerActor);

	Super::Die(KillerActor);
}

const FThreadTalesEnemyDifficultySettings& AThreadTalesEnemyCharacter::GetDifficultySettings() const
{
	if (bHasCustomDifficulty)
	{
		return RuntimeDifficultySettings;
	}
	if (EnemyConfig)
	{
		return EnemyConfig->DifficultySettings;
	}
	static const FThreadTalesEnemyDifficultySettings DefaultSettings;
	return DefaultSettings;
}

void AThreadTalesEnemyCharacter::SetDifficultyMultiplier(float NewMultiplier)
{
	RuntimeDifficultySettings = EnemyConfig ? EnemyConfig->DifficultySettings : FThreadTalesEnemyDifficultySettings();
	RuntimeDifficultySettings.DifficultyMultiplier = FMath::Clamp(NewMultiplier, 0.1f, 3.0f);
	RuntimeDifficultySettings.TrackingTurnRate = FMath::Clamp(RuntimeDifficultySettings.TrackingTurnRate * RuntimeDifficultySettings.DifficultyMultiplier, 60.0f, 1200.0f);
	RuntimeDifficultySettings.PostAttackEvadeChance = FMath::Clamp(RuntimeDifficultySettings.PostAttackEvadeChance * RuntimeDifficultySettings.DifficultyMultiplier, 0.0f, 1.0f);
	RuntimeDifficultySettings.ReactiveEvadeChance = FMath::Clamp(RuntimeDifficultySettings.ReactiveEvadeChance * RuntimeDifficultySettings.DifficultyMultiplier, 0.0f, 1.0f);
	bHasCustomDifficulty = true;

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Enemy] '%s' DifficultyMultiplier set to %.2f (TrackingRate: %.1f deg/s, EvadeChance: %.2f)"),
		*GetName(), RuntimeDifficultySettings.DifficultyMultiplier, RuntimeDifficultySettings.TrackingTurnRate, RuntimeDifficultySettings.PostAttackEvadeChance);
}

UAnimMontage* AThreadTalesEnemyCharacter::GetHitReactMontage() const
{
	if (EnemyConfig && !EnemyConfig->HitReactMontage.IsNull())
	{
		if (UAnimMontage* M = EnemyConfig->HitReactMontage.LoadSynchronous())
		{
			return M;
		}
	}
	return Super::GetHitReactMontage();
}

UAnimMontage* AThreadTalesEnemyCharacter::GetStaggerMontage() const
{
	if (EnemyConfig && !EnemyConfig->StaggerMontage.IsNull())
	{
		if (UAnimMontage* M = EnemyConfig->StaggerMontage.LoadSynchronous())
		{
			return M;
		}
	}
	return Super::GetStaggerMontage();
}

UAnimMontage* AThreadTalesEnemyCharacter::GetPostureBreakMontage() const
{
	if (EnemyConfig && !EnemyConfig->PostureBreakMontage.IsNull())
	{
		if (UAnimMontage* M = EnemyConfig->PostureBreakMontage.LoadSynchronous())
		{
			return M;
		}
	}
	return Super::GetPostureBreakMontage();
}

UAnimMontage* AThreadTalesEnemyCharacter::GetAirHitReactMontage() const
{
	if (EnemyConfig && !EnemyConfig->AirHitReactMontage.IsNull())
	{
		if (UAnimMontage* M = EnemyConfig->AirHitReactMontage.LoadSynchronous())
		{
			return M;
		}
	}
	return Super::GetAirHitReactMontage();
}

UAnimMontage* AThreadTalesEnemyCharacter::GetAirStaggerMontage() const
{
	if (EnemyConfig && !EnemyConfig->AirStaggerMontage.IsNull())
	{
		if (UAnimMontage* M = EnemyConfig->AirStaggerMontage.LoadSynchronous())
		{
			return M;
		}
	}
	return Super::GetAirStaggerMontage();
}

UAnimMontage* AThreadTalesEnemyCharacter::GetAirPostureBreakMontage() const
{
	if (EnemyConfig && !EnemyConfig->AirPostureBreakMontage.IsNull())
	{
		if (UAnimMontage* M = EnemyConfig->AirPostureBreakMontage.LoadSynchronous())
		{
			return M;
		}
	}
	return Super::GetAirPostureBreakMontage();
}

UAnimMontage* AThreadTalesEnemyCharacter::GetRecoverMontage() const
{
	if (EnemyConfig && !EnemyConfig->RecoverMontage.IsNull())
	{
		if (UAnimMontage* M = EnemyConfig->RecoverMontage.LoadSynchronous())
		{
			return M;
		}
	}
	return Super::GetRecoverMontage();
}

bool AThreadTalesEnemyCharacter::IsStaggered() const
{
	if (bPendingAirKnockdown || bPendingAirParryStagger)
	{
		return true;
	}
	return Super::IsStaggered();
}

void AThreadTalesEnemyCharacter::SetHitboxActive(bool bActive, FName SocketOrBoneName)
{
	if (AttackHitbox)
	{
		if (bActive)
		{
			if (SocketOrBoneName != NAME_None && GetMesh())
			{
				AttackHitbox->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketOrBoneName);
				AttackHitbox->SetRelativeLocation(FVector::ZeroVector);
				AttackHitbox->SetSphereRadius(42.0f);
			}
			else
			{
				if (AttackHitbox->GetAttachParent() != GetRootComponent())
				{
					AttackHitbox->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				}
				AttackHitbox->SetRelativeLocation(FVector(75.0f, 0.0f, 0.0f));
				AttackHitbox->SetSphereRadius(65.0f);
			}

			AttackHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			AttackHitbox->UpdateOverlaps();
			TArray<AActor*> OverlappingActors;
			AttackHitbox->GetOverlappingActors(OverlappingActors, AThreadTalesCharacter::StaticClass());
			for (AActor* OverlappedActor : OverlappingActors)
			{
				if (OverlappedActor && OverlappedActor != this)
				{
					FHitResult HitResult;
					OnAttackHitboxOverlap(AttackHitbox, OverlappedActor, OverlappedActor->FindComponentByClass<UPrimitiveComponent>(), INDEX_NONE, false, HitResult);
				}
			}
		}
		else
		{
			AttackHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			HitActorsThisStrike.Empty();
		}
	}

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Enemy] '%s' Natural Hitbox (Socket: %s) %s!"),
		*GetName(), *SocketOrBoneName.ToString(), bActive ? TEXT("ACTIVATED") : TEXT("DEACTIVATED"));
}

void AThreadTalesEnemyCharacter::OnAttackHitboxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;
	if (!OtherActor || OtherActor == this || OtherActor->IsA<AThreadTalesEnemyCharacter>() || HitActorsThisStrike.Contains(OtherActor)) return;

	AThreadTalesCharacter* TargetChar = Cast<AThreadTalesCharacter>(OtherActor);
	if (!TargetChar || !TargetChar->IsAlive()) return;

	HitActorsThisStrike.Add(OtherActor);

	UAbilitySystemComponent* TargetASC = TargetChar->GetAbilitySystemComponent();
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponent();

	if (TargetASC && SourceASC)
	{
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddInstigator(this, this);
		Context.AddHitResult(SweepResult);

		FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(UThreadTalesGE_Damage::StaticClass(), 1.0f, Context);
		if (SpecHandle.IsValid())
		{
			const float Dmg = EnemyConfig ? EnemyConfig->AttackDamage : 20.0f;
			const float PoiseDmg = Dmg * 1.2f;

			SpecHandle.Data->SetSetByCallerMagnitude(ThreadTalesGameplayTags::TAG_SetByCaller_Damage, Dmg);
			SpecHandle.Data->SetSetByCallerMagnitude(ThreadTalesGameplayTags::TAG_SetByCaller_PoiseDamage, PoiseDmg);

			SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Enemy Hitbox] '%s' HIT Target '%s' for %.1f Damage, %.1f Poise Damage!"),
				*GetName(), *TargetChar->GetName(), Dmg, PoiseDmg);

			static const auto CVarDebugTrace = IConsoleManager::Get().FindConsoleVariable(TEXT("tt.Combat.DebugTrace"));
			if (CVarDebugTrace && CVarDebugTrace->GetInt() > 0 && GetWorld())
			{
				DrawDebugSphere(GetWorld(), AttackHitbox->GetComponentLocation(), 22.0f, 16, FColor::Red, false, 2.5f, 0, 2.5f);
				DrawDebugString(GetWorld(), AttackHitbox->GetComponentLocation() + FVector(0, 0, 35.0f),
					FString::Printf(TEXT("BEETLE HIT: %s\n-%.0f HP"), *TargetChar->GetName(), Dmg), nullptr, FColor::Orange, 2.5f, true, 1.2f);
			}
		}
	}
}
