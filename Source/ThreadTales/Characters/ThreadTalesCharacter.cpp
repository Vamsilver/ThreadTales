#include "Characters/ThreadTalesCharacter.h"
#include "ThreadTales.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Core/ThreadTalesPlayerController.h"
#include "Abilities/ThreadTalesAbilitySystemComponent.h"
#include "Abilities/ThreadTalesAttributeSet.h"
#include "Abilities/ThreadTalesGA_Evade.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
#include "Characters/ThreadTalesTraversalComponent.h"
#include "Combat/ThreadTalesWeapon.h"
#include "Input/ThreadTalesInputConfig.h"
#include "Input/ThreadTalesInputComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
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

	// Create traversal manager
	TraversalComponent = CreateDefaultSubobject<UThreadTalesTraversalComponent>(TEXT("TraversalComponent"));

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

		if (EquipmentComponent && DefaultWeaponClass)
		{
			EquipmentComponent->EquipWeapon(DefaultWeaponClass);
		}
	}
}

void AThreadTalesCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (InputConfig)
		{
			if (UThreadTalesInputComponent* TTInputComponent = Cast<UThreadTalesInputComponent>(EnhancedInputComponent))
			{
				TTInputComponent->BindAbilityActions(
					InputConfig,
					this,
					&ThisClass::Input_AbilityInputTagPressed,
					&ThisClass::Input_AbilityInputTagReleased,
					AbilityBindHandles
				);
			}
			else
			{
				// Fallback binding for generic UEnhancedInputComponent
				for (const FThreadTalesInputAction& Action : InputConfig->AbilityInputActions)
				{
					if (Action.InputAction && Action.InputTag.IsValid())
					{
						EnhancedInputComponent->BindAction(Action.InputAction, ETriggerEvent::Started, this, &ThisClass::Input_AbilityInputTagPressed, Action.InputTag);
						EnhancedInputComponent->BindAction(Action.InputAction, ETriggerEvent::Completed, this, &ThisClass::Input_AbilityInputTagReleased, Action.InputTag);
					}
				}
			}

			UE_LOG(LogThreadTales, Display, TEXT("[Character] SetupPlayerInputComponent successfully bound Ability Inputs for '%s'"), *GetName());
		}
	}
}

void AThreadTalesCharacter::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityInputTagPressed(InputTag);
	}
}

void AThreadTalesCharacter::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityInputTagReleased(InputTag);
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
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityInputTagPressed(ThreadTalesGameplayTags::TAG_Input_Ability_Evade);
	}
}

void AThreadTalesCharacter::ToggleCombatStance()
{
	if (EquipmentComponent)
	{
		EquipmentComponent->ToggleCombatStance();
	}
}

bool AThreadTalesCharacter::IsInCombatStance() const
{
	return EquipmentComponent && EquipmentComponent->IsInCombatStance();
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

void AThreadTalesCharacter::RequestReleaseLedge()
{
	if (TraversalComponent && TraversalComponent->IsLedgeClimbing())
	{
		TraversalComponent->ReleaseLedge();
	}
}

void AThreadTalesCharacter::Jump()
{
	if (TraversalComponent && TraversalComponent->IsLedgeClimbing())
	{
		TraversalComponent->HandleLedgeJumpInput();
		return;
	}

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
}

void AThreadTalesCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	// If we were falling and now we are NOT falling (e.g. landed, grabbed a ledge for parkour, started swimming)
	if (PrevMovementMode == MOVE_Falling && GetCharacterMovement()->MovementMode != MOVE_Falling)
	{
		bHasDoubleJumpedInAir = false;

		if (AbilitySystemComponent)
		{
			if (AbilitySystemComponent->HasMatchingGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_HasAirDashed))
			{
				AbilitySystemComponent->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_HasAirDashed);
			}

			// Force-cancel double jump if it's still running (e.g. grabbed a ledge mid-air before montage finished)
			FGameplayTagContainer TagsToCancel;
			TagsToCancel.AddTag(ThreadTalesGameplayTags::TAG_Ability_Movement_DoubleJump);
			AbilitySystemComponent->CancelAbilities(&TagsToCancel);
		}
	}
}

void AThreadTalesCharacter::CheckJumpInput(float DeltaTime)
{
	// Track if we were falling BEFORE native jump logic processes
	bool bWasFallingBeforeJump = GetCharacterMovement() && GetCharacterMovement()->IsFalling();

	// In GASP / Enhanced Input: when jump is pressed in the air, ensure jump transition state is ready for the air jump
	if (bPressedJump && bWasFallingBeforeJump && JumpCurrentCount > 0)
	{
		bWasJumping = false;
	}

	int32 JumpCountBefore = JumpCurrentCount;

	Super::CheckJumpInput(DeltaTime);

	// If a jump actually occurred this frame, and we were ALREADY falling before the jump started,
	// it means we either double-jumped, or we jumped after walking off a ledge.
	// In both cases, it's an air jump (Double Jump).
	if (JumpCurrentCount > JumpCountBefore && bWasFallingBeforeJump && !bHasDoubleJumpedInAir)
	{
		bHasDoubleJumpedInAir = true;
		OnDoubleJump();
	}
}

void AThreadTalesCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	const bool bIsFalling = GetCharacterMovement() && GetCharacterMovement()->IsFalling();
	UE_LOG(LogThreadTales, Display, TEXT("[Jump] %s OnJumped: JumpCurrentCount = %d / %d"), 
		*GetName(), JumpCurrentCount, JumpMaxCount);
}

void AThreadTalesCharacter::OnDoubleJump()
{
	// Try activating Double Jump via GAS (Ability.Movement.DoubleJump)
	bool bActivatedViaGAS = false;
	if (AbilitySystemComponent)
	{
		bActivatedViaGAS = AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(ThreadTalesGameplayTags::TAG_Ability_Movement_DoubleJump));
	}

	if (!bActivatedViaGAS)
	{
		UE_LOG(LogThreadTales, Warning, TEXT("[DoubleJump] GAS failed to activate Double Jump ability for %s! Check DefaultAbilities or stuck abilities."), *GetName());
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

