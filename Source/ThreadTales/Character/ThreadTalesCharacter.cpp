// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesCharacter.h"
#include "ThreadTales.h"
#include "ThreadTalesAbilitySystemComponent.h"
#include "ThreadTalesAttributeSet.h"
#include "ThreadTalesEquipmentComponent.h"
#include "ThreadTalesPlayerController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

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

	// Ensure solid character collision between all pawns
	if (GetCapsuleComponent() && bIsAlive)
	{
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	// Ensure ASC is initialized on BeginPlay
	if (AbilitySystemComponent && (!AbilitySystemComponent->AbilityActorInfo.IsValid() || AbilitySystemComponent->AbilityActorInfo->OwnerActor == nullptr))
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void AThreadTalesCharacter::ActivateAbilityByTag(FGameplayTag ContainerTag)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(ContainerTag));
	}
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

	if (HasAuthority() && AttributeSet)
	{
		const float OldHealth = AttributeSet->GetHealth();
		const float NewHealth = FMath::Clamp(OldHealth - DamageAmount, 0.0f, AttributeSet->GetMaxHealth());

		const_cast<UThreadTalesAttributeSet*>(AttributeSet)->SetHealth(NewHealth);

		UE_LOG(LogThreadTalesCombat, Display, TEXT("[AttributeSet] %s took %.1f damage. Remaining Health: %.1f / %.1f"),
			*GetName(), DamageAmount, NewHealth, AttributeSet->GetMaxHealth());

		HandleHealthChanged(NewHealth, AttributeSet->GetMaxHealth());

		if (NewHealth <= 0.0f)
		{
			Die(DamageCauser);
		}
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
