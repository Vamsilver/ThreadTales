#include "ThreadTalesWeapon.h"
#include "ThreadTales.h"
#include "GameplayAbilitySpec.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"

AThreadTalesWeapon::AThreadTalesWeapon()
{
	PrimaryActorTick.bCanEverTick = true; // [TICK-ALWAYS-OK]
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

	HandSocketName = FName("hand_rSocket");
	SheathSocketName = FName("sheath_hip_lSocket");
	OverlayPoseName = FName("Katana");

	WeaponState = EWeaponCombatState::Sheathed;
}

void AThreadTalesWeapon::BeginPlay()
{
	Super::BeginPlay();

	if (WeaponMesh && WeaponMesh->GetStaticMesh())
	{
		BladeDynamicMaterial = WeaponMesh->CreateAndSetMaterialInstanceDynamic(0);
	}
}

void AThreadTalesWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateBladeClipPlane();
}

void AThreadTalesWeapon::UpdateBladeClipPlane()
{
	if (!bEnableSheathClipPlane)
	{
		return;
	}

	if (!BladeDynamicMaterial && WeaponMesh && WeaponMesh->GetStaticMesh())
	{
		BladeDynamicMaterial = WeaponMesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (!BladeDynamicMaterial)
	{
		return;
	}

	// When drawn and ready in hand, disable clipping plane for full visibility
	if (IsDrawn())
	{
		BladeDynamicMaterial->SetScalarParameterValue(FName("bEnableClip"), 0.0f);
		return;
	}

	// When sheathed, drawing, or sheathing: clip any blade vertices entering the scabbard mouth
	if (bHasSheath && SheathMesh && SheathMesh->GetStaticMesh())
	{
		FVector MouthWorldPos;
		FVector MouthWorldNormal;

		if (SheathMesh->DoesSocketExist(FName("MouthSocket")))
		{
			MouthWorldPos = SheathMesh->GetSocketLocation(FName("MouthSocket"));
			MouthWorldNormal = SheathMesh->GetSocketRotation(FName("MouthSocket")).Vector();
		}
		else
		{
			// scabbard.scabbard opening is at local (0, 0, 0) and points outward along local +Y (RightVector)
			const FTransform& SheathTransform = SheathMesh->GetComponentTransform();
			MouthWorldPos = SheathTransform.TransformPosition(ClipPlaneMouthOffset);
			MouthWorldNormal = SheathTransform.TransformVectorNoScale(FVector(0.f, 1.f, 0.f)).GetSafeNormal();
		}

		// When drawing: once the blade has completely cleared the scabbard mouth (distance > 85 cm),
		// transition to Drawn state and turn off clipping to avoid unnecessary GPU masking.
		if (IsDrawing() && WeaponMesh)
		{
			const float DistToMouth = FVector::Dist(WeaponMesh->GetComponentLocation(), MouthWorldPos);
			if (DistToMouth > 85.0f)
			{
				SetWeaponState(EWeaponCombatState::Drawn);
				BladeDynamicMaterial->SetScalarParameterValue(FName("bEnableClip"), 0.0f);
				return;
			}
		}

		BladeDynamicMaterial->SetScalarParameterValue(FName("bEnableClip"), 1.0f);
		BladeDynamicMaterial->SetVectorParameterValue(FName("ClipPlanePosition"), MouthWorldPos);
		BladeDynamicMaterial->SetVectorParameterValue(FName("ClipPlaneNormal"), MouthWorldNormal);
	}
	else
	{
		BladeDynamicMaterial->SetScalarParameterValue(FName("bEnableClip"), 0.0f);
	}
}

void AThreadTalesWeapon::Equip(AActor* NewOwner)
{
	if (!HasAuthority()) return;

	SetOwner(NewOwner);

	ACharacter* Character = Cast<ACharacter>(NewOwner);
	if (Character && Character->GetMesh())
	{
		// Attach Sheath (scabbard) only if weapon uses sheath and mesh is set
		if (bHasSheath && SheathMesh && SheathMesh->GetStaticMesh())
		{
			AttachSheathToHip();
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
					if (OwnerASC->FindAbilitySpecFromClass(AbilityClass))
					{
						UE_LOG(LogThreadTalesCombat, Display, TEXT("[Weapon::Equip] Ability '%s' is already registered on '%s' ASC - skipping duplicate spec"), *AbilityClass->GetName(), *NewOwner->GetName());
					}
					else
					{
						FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);
						FGameplayAbilitySpecHandle Handle = OwnerASC->GiveAbility(Spec);
						GrantedAbilityHandles.Add(Handle);
						UE_LOG(LogThreadTalesCombat, Display, TEXT("[Weapon::Equip] Granted Ability '%s' to ASC of '%s'"), *AbilityClass->GetName(), *NewOwner->GetName());
					}
				}
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
		FName TargetHandSocket = HandSocketName;
		if (!Character->GetMesh()->DoesSocketExist(TargetHandSocket))
		{
			if (Character->GetMesh()->DoesSocketExist(FName("weapon_hand_r_socket")))
			{
				TargetHandSocket = FName("weapon_hand_r_socket");
			}
			else if (Character->GetMesh()->DoesSocketExist(FName("weapon_r")))
			{
				TargetHandSocket = FName("weapon_r");
			}
		}

		WeaponMesh->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TargetHandSocket);
		
		// If we are currently executing the draw montage, maintain Drawing state
		// so that UpdateBladeClipPlane() continues masking blade pixels inside the scabbard until draw completes.
		if (WeaponState != EWeaponCombatState::Drawing)
		{
			SetWeaponState(EWeaponCombatState::Drawn);
		}

		AddWeaponMappingContext();

		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Weapon] '%s' attached to hand socket '%s' on '%s' (State: %d)"),
			*GetName(), *TargetHandSocket.ToString(), *Character->GetName(), static_cast<int32>(WeaponState));
	}
}

void AThreadTalesWeapon::AttachToSheath()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character && Character->GetMesh() && WeaponMesh)
	{
		FName TargetSheathSocket = SheathSocketName;
		if (!Character->GetMesh()->DoesSocketExist(TargetSheathSocket))
		{
			if (Character->GetMesh()->DoesSocketExist(FName("weapon_sheath_socket")))
			{
				TargetSheathSocket = FName("weapon_sheath_socket");
			}
			else if (Character->GetMesh()->DoesSocketExist(FName("Katana_sheath")))
			{
				TargetSheathSocket = FName("Katana_sheath");
			}
		}

		if (bHasSheath && SheathMesh && SheathMesh->GetStaticMesh() && bAttachBladeToSheathMesh)
		{
			// Rigid parent-child docking directly inside the scabbard mesh
			WeaponMesh->AttachToComponent(SheathMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			WeaponMesh->SetRelativeTransform(SheathRelativeTransform);
			WeaponMesh->UpdateComponentToWorld();
			SetWeaponState(EWeaponCombatState::Sheathed);
			if (bAddContextOnlyWhenDrawn)
			{
				RemoveWeaponMappingContext();
			}

			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Weapon] '%s' SHEATHED -> docked directly to SheathMesh with relative offset (%s)"),
				*GetName(), *SheathRelativeTransform.GetLocation().ToString());
		}
		else if (bHasSheath && !TargetSheathSocket.IsNone() && Character->GetMesh()->DoesSocketExist(TargetSheathSocket))
		{
			WeaponMesh->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TargetSheathSocket);
			WeaponMesh->SetRelativeTransform(SheathRelativeTransform);
			WeaponMesh->UpdateComponentToWorld();
			SetWeaponState(EWeaponCombatState::Sheathed);
			if (bAddContextOnlyWhenDrawn)
			{
				RemoveWeaponMappingContext();
			}

			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Weapon] '%s' SHEATHED -> attached to sheath socket '%s' on '%s' with relative offset (%s)"),
				*GetName(), *TargetSheathSocket.ToString(), *Character->GetName(), *SheathRelativeTransform.GetLocation().ToString());
		}
		else
		{
			UE_LOG(LogThreadTalesCombat, Warning, TEXT("[Weapon] '%s' Sheath socket '%s' not found! Falling back to hand."),
				*GetName(), *TargetSheathSocket.ToString());
			AttachToHand();
		}
	}
}

void AThreadTalesWeapon::AttachSheathToHand()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character && Character->GetMesh() && bHasSheath && SheathMesh && SheathMesh->GetStaticMesh())
	{
		FName TargetSheathHandSocket = SheathHandSocketName;
		if (!Character->GetMesh()->DoesSocketExist(TargetSheathHandSocket))
		{
			if (Character->GetMesh()->DoesSocketExist(FName("scabbard2")))
			{
				TargetSheathHandSocket = FName("scabbard2");
			}
			else if (Character->GetMesh()->DoesSocketExist(FName("hand_l")))
			{
				TargetSheathHandSocket = FName("hand_l");
			}
		}

		SheathMesh->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TargetSheathHandSocket);

		// Re-affirm rigid blade docking inside scabbard if sheathed
		if (WeaponMesh && (WeaponMesh->GetAttachParent() == SheathMesh || IsSheathed()))
		{
			WeaponMesh->AttachToComponent(SheathMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			WeaponMesh->SetRelativeTransform(SheathRelativeTransform);
			WeaponMesh->UpdateComponentToWorld();
		}

		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Weapon] '%s' SHEATH -> attached to hand socket '%s' on '%s'"),
			*GetName(), *TargetSheathHandSocket.ToString(), *Character->GetName());
	}
}

void AThreadTalesWeapon::AttachSheathToHip()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character && Character->GetMesh() && bHasSheath && SheathMesh && SheathMesh->GetStaticMesh())
	{
		FName TargetSheathSocket = SheathSocketName;
		if (!Character->GetMesh()->DoesSocketExist(TargetSheathSocket))
		{
			if (Character->GetMesh()->DoesSocketExist(FName("weapon_sheath_socket")))
			{
				TargetSheathSocket = FName("weapon_sheath_socket");
			}
			else if (Character->GetMesh()->DoesSocketExist(FName("Katana_sheath")))
			{
				TargetSheathSocket = FName("Katana_sheath");
			}
			else if (Character->GetMesh()->DoesSocketExist(FName("pelvis")))
			{
				TargetSheathSocket = FName("pelvis");
			}
			else if (Character->GetMesh()->DoesSocketExist(FName("spine_01")))
			{
				TargetSheathSocket = FName("spine_01");
			}
		}

		SheathMesh->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TargetSheathSocket);

		// Re-affirm rigid blade docking inside scabbard if sheathed
		if (WeaponMesh && (WeaponMesh->GetAttachParent() == SheathMesh || IsSheathed()))
		{
			WeaponMesh->AttachToComponent(SheathMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			WeaponMesh->SetRelativeTransform(SheathRelativeTransform);
			WeaponMesh->UpdateComponentToWorld();
		}

		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Weapon] '%s' SHEATH -> attached to hip socket '%s' on '%s'"),
			*GetName(), *TargetSheathSocket.ToString(), *Character->GetName());
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
