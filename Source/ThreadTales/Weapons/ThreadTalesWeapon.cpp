#include "ThreadTalesWeapon.h"
#include "ThreadTales.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"

AThreadTalesWeapon::AThreadTalesWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(RootComponent);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SheathMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SheathMesh"));
	SheathMesh->SetupAttachment(RootComponent);
	SheathMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HandSocketName = FName("weapon_hand_r_socket");
	SheathSocketName = FName("weapon_sheath_socket");

	WeaponState = EWeaponCombatState::Sheathed;
}

void AThreadTalesWeapon::BeginPlay()
{
	Super::BeginPlay();
}

void AThreadTalesWeapon::Equip(AActor* NewOwner)
{
	if (!HasAuthority()) return;

	SetOwner(NewOwner);

	ACharacter* Character = Cast<ACharacter>(NewOwner);
	if (Character && Character->GetMesh())
	{
		// Attach Sheath (scabbard) only if weapon uses sheath and mesh is set
		if (bHasSheath && SheathMesh && SheathMesh->GetStaticMesh() && !SheathSocketName.IsNone() && Character->GetMesh()->DoesSocketExist(SheathSocketName))
		{
			SheathMesh->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SheathSocketName);
			SheathMesh->SetVisibility(true);
		}
		else if (SheathMesh)
		{
			SheathMesh->SetVisibility(false);
		}

		if (bStartDrawn || !bHasSheath)
		{
			AttachToHand();
		}
		else
		{
			AttachToSheath();
		}
	}

	// Grant Abilities
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(NewOwner);
	if (ASI)
	{
		OwnerASC = ASI->GetAbilitySystemComponent();
		if (OwnerASC)
		{
			for (TSubclassOf<UGameplayAbility> AbilityClass : GrantedAbilities)
			{
				if (AbilityClass)
				{
					FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);
					FGameplayAbilitySpecHandle Handle = OwnerASC->GiveAbility(Spec);
					GrantedAbilityHandles.Add(Handle);
					UE_LOG(LogThreadTalesCombat, Display, TEXT("[Weapon::Equip] Granted Ability '%s' to ASC of '%s'"), *AbilityClass->GetName(), *NewOwner->GetName());
				}
			}
			if (GrantedAbilityHandles.Num() == 0)
			{
				UE_LOG(LogThreadTalesCombat, Warning, TEXT("[Weapon::Equip] Weapon '%s' has NO GrantedAbilities assigned!"), *GetName());
			}
		}
	}

	if (!bAddContextOnlyWhenDrawn || IsDrawn())
	{
		AddWeaponMappingContext();
	}
}

void AThreadTalesWeapon::Unequip()
{
	RemoveWeaponMappingContext();

	if (!HasAuthority()) return;

	// Remove Abilities
	if (OwnerASC)
	{
		for (const FGameplayAbilitySpecHandle& Handle : GrantedAbilityHandles)
		{
			OwnerASC->ClearAbility(Handle);
		}
	}
	GrantedAbilityHandles.Empty();
	OwnerASC = nullptr;

	if (WeaponMesh)
	{
		WeaponMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}
	if (SheathMesh)
	{
		SheathMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetOwner(nullptr);
}

void AThreadTalesWeapon::AttachToHand()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character && Character->GetMesh() && WeaponMesh)
	{
		WeaponMesh->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, HandSocketName);
		SetWeaponState(EWeaponCombatState::Drawn);
		AddWeaponMappingContext();
	}
}

void AThreadTalesWeapon::AttachToSheath()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character && Character->GetMesh() && WeaponMesh)
	{
		if (bHasSheath && !SheathSocketName.IsNone() && Character->GetMesh()->DoesSocketExist(SheathSocketName))
		{
			WeaponMesh->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SheathSocketName);
			SetWeaponState(EWeaponCombatState::Sheathed);
			if (bAddContextOnlyWhenDrawn)
			{
				RemoveWeaponMappingContext();
			}
		}
		else
		{
			AttachToHand();
		}
	}
}

void AThreadTalesWeapon::SetWeaponState(EWeaponCombatState NewState)
{
	if (WeaponState != NewState)
	{
		WeaponState = NewState;
		OnWeaponStateChanged.Broadcast(WeaponState);
	}
}

void AThreadTalesWeapon::OnRep_WeaponState(EWeaponCombatState OldState)
{
	OnWeaponStateChanged.Broadcast(WeaponState);

	if (bAddContextOnlyWhenDrawn)
	{
		if (WeaponState == EWeaponCombatState::Drawn)
		{
			AddWeaponMappingContext();
		}
		else if (WeaponState == EWeaponCombatState::Sheathed)
		{
			RemoveWeaponMappingContext();
		}
	}
}

void AThreadTalesWeapon::OnRep_Owner()
{
	Super::OnRep_Owner();

	if (!bAddContextOnlyWhenDrawn || IsDrawn())
	{
		AddWeaponMappingContext();
	}
}

void AThreadTalesWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveWeaponMappingContext();
	Super::EndPlay(EndPlayReason);
}

void AThreadTalesWeapon::AddWeaponMappingContext()
{
	if (!WeaponMappingContext) return;

	AActor* CurrentOwner = GetOwner();
	if (!CurrentOwner) return;

	APawn* Pawn = Cast<APawn>(CurrentOwner);
	if (!Pawn) return;

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (PC && PC->IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(WeaponMappingContext, WeaponMappingPriority);
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Weapon] Added WeaponMappingContext '%s' (Priority %d) for '%s'"), 
				*WeaponMappingContext->GetName(), WeaponMappingPriority, *CurrentOwner->GetName());
		}
	}
}

void AThreadTalesWeapon::RemoveWeaponMappingContext()
{
	if (!WeaponMappingContext) return;

	AActor* CurrentOwner = GetOwner();
	if (!CurrentOwner) return;

	APawn* Pawn = Cast<APawn>(CurrentOwner);
	if (!Pawn) return;

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (PC && PC->IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(WeaponMappingContext);
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Weapon] Removed WeaponMappingContext '%s' for '%s'"), 
				*WeaponMappingContext->GetName(), *CurrentOwner->GetName());
		}
	}
}

void AThreadTalesWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AThreadTalesWeapon, WeaponState);
}
