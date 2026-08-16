// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesCharacter.h"
#include "ThreadTales.h"
#include "ThreadTalesGameplayTags.h"
#include "ThreadTalesAbilitySystemComponent.h"
#include "ThreadTalesAttributeSet.h"
#include "ThreadTalesEquipmentComponent.h"
#include "ThreadTalesPlayerController.h"
#include "Abilities/ThreadTalesGA_Evade.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

AThreadTalesCharacter::AThreadTalesCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	MaxHealth = 100.0f;
	bIsAlive = true;

	MaxJumpCount = 2;
	JumpMaxCount = 2;
	DoubleJumpZVelocity = 500.0f;
	bHasDoubleJumpedInAir = false;

	// Create ability system component, and set it to be explicitly replicated
	AbilitySystemComponent = CreateDefaultSubobject<UThreadTalesAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// Create the attribute set, this replicates by default
	AttributeSet = CreateDefaultSubobject<UThreadTalesAttributeSet>(TEXT("AttributeSet"));

	// Create equipment manager
	EquipmentComponent = CreateDefaultSubobject<UThreadTalesEquipmentComponent>(TEXT("EquipmentComponent"));

	// Enable solid Pawn-to-Pawn collision blocking so characters cannot walk through each other
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		GetCapsuleComponent()->CanCharacterStepUpOn = ECB_No;
	}
}

void AThreadTalesCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AThreadTalesCharacter, bIsAlive);
	DOREPLIFETIME(AThreadTalesCharacter, MaxJumpCount);
}

UAbilitySystemComponent* AThreadTalesCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AThreadTalesCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Server-side initialization
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void AThreadTalesCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	// Client-side initialization
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void AThreadTalesCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Sync max jumps count
	JumpMaxCount = MaxJumpCount;

	UE_LOG(LogThreadTales, Warning, TEXT("[Character] %s BeginPlay (MaxJumpCount=%d, DoubleJumpMontage=%s)"), 
		*GetName(), MaxJumpCount, DoubleJumpMontage ? *DoubleJumpMontage->GetName() : TEXT("None"));

	// Ensure ASC is initialized on BeginPlay
	if (AbilitySystemComponent && (!AbilitySystemComponent->AbilityActorInfo.IsValid() || AbilitySystemComponent->AbilityActorInfo->OwnerActor == nullptr))
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}

	// Grant default abilities configured in Blueprint
	if (HasAuthority() && AbilitySystemComponent)
	{
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
		{
			if (AbilityClass)
			{
				FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);
				AbilitySystemComponent->GiveAbility(Spec);
			}
		}

		if (EvadeAbilityClass)
		{
			FGameplayAbilitySpec Spec(EvadeAbilityClass, 1, INDEX_NONE, this);
			AbilitySystemComponent->GiveAbility(Spec);
		}
	}
}

void AThreadTalesCharacter::ActivateAbilityByTag(FGameplayTag ContainerTag)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(ContainerTag));
	}
}

void AThreadTalesCharacter::PerformEvade()
{
	if (EquipmentComponent)
	{
		EquipmentComponent->PerformEvade();
	}
	else if (AbilitySystemComponent)
	{
		AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(ThreadTalesGameplayTags::TAG_Ability_Evade));
	}
}

bool AThreadTalesCharacter::IsEvading() const
{
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Evading);
}

float AThreadTalesCharacter::GetHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.0f;
}

float AThreadTalesCharacter::GetMaxHealth() const
{
	return AttributeSet ? AttributeSet->GetMaxHealth() : 0.0f;
}

void AThreadTalesCharacter::HandleHealthChanged(float NewHealth, float MaxHealthVal)
{
	OnHealthChanged.Broadcast(NewHealth, MaxHealthVal);
}

float AThreadTalesCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (!bIsAlive || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	// Check if character is evading (I-Frames)
	if (AbilitySystemComponent && 
		(AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Evading) || 
		 AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Combat_Invulnerable)))
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Character] %s EVADED incoming damage of %.1f (I-Frames Active)!"), *GetName(), DamageAmount);
		return 0.0f;
	}

	if (HasAuthority() && AttributeSet)
	{
		AttributeSet->ReceiveDamage(DamageAmount, DamageCauser);
	}

	return ActualDamage;
}

void AThreadTalesCharacter::Die(AActor* KillerActor)
{
	if (!bIsAlive)
	{
		return;
	}

	bIsAlive = false;

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Character] %s has DIED! Killed by: %s"), *GetName(), KillerActor ? *KillerActor->GetName() : TEXT("Unknown"));

	// Disable movement and capsule collision
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->DisableMovement();
		GetCharacterMovement()->StopMovementImmediately();
	}

	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Enable ragdoll physics on character mesh
	if (GetMesh())
	{
		GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
		GetMesh()->SetSimulatePhysics(true);
	}

	OnCharacterDeath.Broadcast(this, KillerActor);
}

void AThreadTalesCharacter::OnRep_IsAlive()
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

void AThreadTalesCharacter::DebugSwitchCharacter(TSubclassOf<AThreadTalesCharacter> NewCharacterClass)
{
	if (!NewCharacterClass)
	{
		UE_LOG(LogThreadTales, Warning, TEXT("[Debug] DebugSwitchCharacter failed: NewCharacterClass is null!"));
		return;
	}

	if (GetClass() == NewCharacterClass)
	{
		UE_LOG(LogThreadTales, Log, TEXT("[Debug] Already playing as %s!"), *NewCharacterClass->GetName());
		return;
	}

	AController* SavedController = GetController();
	if (!SavedController)
	{
		UE_LOG(LogThreadTales, Warning, TEXT("[Debug] DebugSwitchCharacter failed: Character has no controller!"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	FTransform SpawnTransform = GetActorTransform();
	FRotator ControlRot = SavedController->GetControlRotation();

	// Preserve references to switch back
	TSubclassOf<AThreadTalesCharacter> SavedMaleClass = DebugMaleClass;
	TSubclassOf<AThreadTalesCharacter> SavedFemaleClass = DebugFemaleClass;

	// Unpossess current character
	SavedController->UnPossess();

	// Spawn new character at the exact same transform
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AThreadTalesCharacter* NewChar = World->SpawnActor<AThreadTalesCharacter>(NewCharacterClass, SpawnTransform, SpawnParams);
	if (NewChar)
	{
		// Pass debug references forward if not set on the new instance
		if (!NewChar->DebugMaleClass) NewChar->DebugMaleClass = SavedMaleClass;
		if (!NewChar->DebugFemaleClass) NewChar->DebugFemaleClass = SavedFemaleClass;

		SavedController->Possess(NewChar);
		SavedController->SetControlRotation(ControlRot);
		UE_LOG(LogThreadTales, Display, TEXT("[Debug] Successfully switched character to: %s"), *NewCharacterClass->GetName());
	}

	// Clean up old character's equipment so weapon is destroyed
	if (EquipmentComponent)
	{
		EquipmentComponent->UnequipWeapon();
	}

	// Destroy old character
	Destroy();
}

void AThreadTalesCharacter::DebugSwitchToMale()
{
	if (DebugMaleClass)
	{
		DebugSwitchCharacter(DebugMaleClass);
	}
	else
	{
		UE_LOG(LogThreadTales, Warning, TEXT("[Debug] DebugSwitchToMale: DebugMaleClass is not assigned on character!"));
	}
}

void AThreadTalesCharacter::DebugSwitchToFemale()
{
	if (DebugFemaleClass)
	{
		DebugSwitchCharacter(DebugFemaleClass);
	}
	else
	{
		UE_LOG(LogThreadTales, Warning, TEXT("[Debug] DebugSwitchToFemale: DebugFemaleClass is not assigned on character!"));
	}
}

void AThreadTalesCharacter::DebugToggleGender()
{
	if (DebugMaleClass && IsA(DebugMaleClass))
	{
		DebugSwitchToFemale();
	}
	else
	{
		DebugSwitchToMale();
	}
}

void AThreadTalesCharacter::SetMaxJumpCount(int32 NewMaxJumpCount)
{
	MaxJumpCount = FMath::Max(1, NewMaxJumpCount);
	JumpMaxCount = MaxJumpCount;
}

bool AThreadTalesCharacter::IsDoubleJumping() const
{
	return JumpCurrentCount > 1;
}

void AThreadTalesCharacter::Jump()
{
	if (GetCharacterMovement() && GetCharacterMovement()->IsFalling() && !bHasDoubleJumpedInAir)
	{
		bHasDoubleJumpedInAir = true;
		UE_LOG(LogThreadTales, Warning, TEXT("[DoubleJump] %s Jump() in air -> Triggering OnDoubleJump!"), *GetName());
		OnDoubleJump();
	}

	Super::Jump();
}

void AThreadTalesCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	bHasDoubleJumpedInAir = false;
}

void AThreadTalesCharacter::CheckJumpInput(float DeltaTime)
{
	// In GASP / Enhanced Input: when jump is pressed in the air, ensure jump transition state is ready for the air jump
	if (bPressedJump && GetCharacterMovement() && GetCharacterMovement()->IsFalling() && JumpCurrentCount > 0)
	{
		bWasJumping = false;
	}

	Super::CheckJumpInput(DeltaTime);
}

void AThreadTalesCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	const bool bIsFalling = GetCharacterMovement() && GetCharacterMovement()->IsFalling();
	UE_LOG(LogThreadTales, Warning, TEXT("[Jump] %s OnJumped: JumpCurrentCount = %d / %d (IsFalling = %s)"), 
		*GetName(), JumpCurrentCount, JumpMaxCount, bIsFalling ? TEXT("true") : TEXT("false"));

	if (JumpCurrentCount > 1 && !bHasDoubleJumpedInAir)
	{
		bHasDoubleJumpedInAir = true;
		OnDoubleJump();
	}
}

void AThreadTalesCharacter::OnDoubleJump()
{
	// 1. Try activating Double Jump via GAS (Ability.Movement.DoubleJump)
	bool bActivatedViaGAS = false;
	if (AbilitySystemComponent)
	{
		bActivatedViaGAS = AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(ThreadTalesGameplayTags::TAG_Ability_Movement_DoubleJump));
	}

	// 2. Fallback: if not handled via GAS, execute character's default movement / montage logic
	if (!bActivatedViaGAS)
	{
		if (DoubleJumpZVelocity > 0.0f && GetCharacterMovement())
		{
			FVector CurrentVelocity = GetCharacterMovement()->Velocity;
			CurrentVelocity.Z = DoubleJumpZVelocity;
			GetCharacterMovement()->Velocity = CurrentVelocity;
			UE_LOG(LogThreadTales, Display, TEXT("[DoubleJump] Applied DoubleJumpZVelocity = %.1f"), DoubleJumpZVelocity);
		}

		if (DoubleJumpMontage)
		{
			UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
			const float MontageDuration = PlayAnimMontage(DoubleJumpMontage);
			UE_LOG(LogThreadTales, Warning, TEXT("[DoubleJump] %s DoubleJumpMontage '%s' -> PlayAnimMontage result: %.2f sec (AnimInstance: %s, Mesh: %s)"), 
				*GetName(), 
				*DoubleJumpMontage->GetName(), 
				MontageDuration, 
				AnimInst ? *AnimInst->GetClass()->GetName() : TEXT("NULL"),
				GetMesh() ? *GetMesh()->GetName() : TEXT("NULL"));
		}
		else
		{
			UE_LOG(LogThreadTales, Warning, TEXT("[DoubleJump] %s has NO DoubleJumpMontage assigned!"), *GetName());
		}

		if (DoubleJumpSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, DoubleJumpSound, GetActorLocation());
		}

		if (DoubleJumpEffect)
		{
			const float CapsuleHalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), DoubleJumpEffect, GetActorLocation() - FVector(0.f, 0.f, CapsuleHalfHeight));
		}
	}

	OnDoubleJumped.Broadcast(this);
	BP_OnDoubleJump();
}

void AThreadTalesCharacter::PerformDoubleJump()
{
	if (CanJump())
	{
		Jump();
	}
}

