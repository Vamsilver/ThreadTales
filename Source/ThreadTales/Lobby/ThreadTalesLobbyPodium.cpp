// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesLobbyPodium.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Animation/AnimSequence.h"
#include "Core/ThreadTalesPlayerState.h"
#include "Combat/ThreadTalesWeapon.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "Engine/World.h"

AThreadTalesLobbyPodium::AThreadTalesLobbyPodium()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	BasePedestalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BasePedestalMesh"));
	BasePedestalMesh->SetupAttachment(RootComponent);
	BasePedestalMesh->SetCollisionProfileName(TEXT("BlockAll"));

	CharacterMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh"));
	CharacterMesh->SetupAttachment(RootComponent);
	CharacterMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 155.0f));
	CharacterMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	CharacterMesh->SetCollisionProfileName(TEXT("NoCollision"));

	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
	SceneCapture->SetupAttachment(RootComponent);
	SceneCapture->SetRelativeLocation(FVector(220.0f, 0.0f, 220.0f));
	SceneCapture->SetRelativeRotation(FRotator(-5.0f, 180.0f, 0.0f));
	SceneCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	SceneCapture->bCaptureEveryFrame = true;
	SceneCapture->bCaptureOnMovement = false;
	SceneCapture->FOVAngle = 42.0f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PedestalFinder(TEXT("/Game/ThreadTales/Meshes/PuzzlePodium/SM_PuzzlePodium_StoneThread.SM_PuzzlePodium_StoneThread"));
	if (PedestalFinder.Succeeded())
	{
		BasePedestalMesh->SetStaticMesh(PedestalFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannequinFinder(TEXT("/GASPALS/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin"));
	if (MannequinFinder.Succeeded())
	{
		CharacterMesh->SetSkeletalMeshAsset(MannequinFinder.Object);
	}

	static ConstructorHelpers::FClassFinder<AThreadTalesWeapon> KatanaClassFinder(TEXT("/Game/Blueprints/Weapons/BP_Weapon_Katana.BP_Weapon_Katana_C"));
	if (KatanaClassFinder.Succeeded())
	{
		KatanaWeaponClass = KatanaClassFinder.Class;
	}

	static ConstructorHelpers::FClassFinder<AThreadTalesWeapon> BowClassFinder(TEXT("/Game/Blueprints/Weapons/BP_Weapon_Bow.BP_Weapon_Bow_C"));
	if (BowClassFinder.Succeeded())
	{
		BowWeaponClass = BowClassFinder.Class;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> SamuraiIdleFinder(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Anim_ARPGSamurai_Idle_no_weapon_Anim.Anim_ARPGSamurai_Idle_no_weapon_Anim"));
	if (SamuraiIdleFinder.Succeeded())
	{
		SwordsmanIdleAnim = SamuraiIdleFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> SamuraiReadyFinder(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Anim_ARPGSamurai_Idle1.Anim_ARPGSamurai_Idle1"));
	if (SamuraiReadyFinder.Succeeded())
	{
		SwordsmanReadyAnim = SamuraiReadyFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> SamuraiEquipFinder(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Anim_ARPGSamurai_Equip.Anim_ARPGSamurai_Equip"));
	if (SamuraiEquipFinder.Succeeded())
	{
		SwordsmanEquipAnim = SamuraiEquipFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> SamuraiUnequipFinder(TEXT("/Game/Animations/ARPG_Samurai_Retarget/Anim_ARPGSamurai_Unequip.Anim_ARPGSamurai_Unequip"));
	if (SamuraiUnequipFinder.Succeeded())
	{
		SwordsmanUnequipAnim = SamuraiUnequipFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> ArcherIdleFinder(TEXT("/Game/Animations/Archer_Retarget/01_Idle/AS_Idle_Seq.AS_Idle_Seq"));
	if (ArcherIdleFinder.Succeeded())
	{
		ArcherIdleAnim = ArcherIdleFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> ArcherReadyFinder(TEXT("/Game/Animations/Archer_Retarget/01_Idle/02_Idle_Combat/AS_Idle_Combat_Seq.AS_Idle_Combat_Seq"));
	if (ArcherReadyFinder.Succeeded())
	{
		ArcherReadyAnim = ArcherReadyFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> ArcherEquipFinder(TEXT("/Game/Animations/Archer_Retarget/01_Idle/01_Idle/AS_Idle_to_Idle_Combat_Seq.AS_Idle_to_Idle_Combat_Seq"));
	if (ArcherEquipFinder.Succeeded())
	{
		ArcherEquipAnim = ArcherEquipFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> ArcherUnequipFinder(TEXT("/Game/Animations/Archer_Retarget/01_Idle/02_Idle_Combat/AS_Idle_Combat_to_Idle_Seq.AS_Idle_Combat_to_Idle_Seq"));
	if (ArcherUnequipFinder.Succeeded())
	{
		ArcherUnequipAnim = ArcherUnequipFinder.Object;
	}
}

void AThreadTalesLobbyPodium::UpdateCaptureTarget()
{
	if (SceneCapture)
	{
		if (!RenderTarget)
		{
			const TCHAR* RTPath = (AssignedSlotIndex == 0)
				? TEXT("/Game/UI/RT_LobbyPodium_P1.RT_LobbyPodium_P1")
				: TEXT("/Game/UI/RT_LobbyPodium_P2.RT_LobbyPodium_P2");
			RenderTarget = Cast<UTextureRenderTarget2D>(StaticLoadObject(UTextureRenderTarget2D::StaticClass(), nullptr, RTPath));
		}

		if (RenderTarget)
		{
			SceneCapture->TextureTarget = RenderTarget;
		}
	}
}

void AThreadTalesLobbyPodium::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateCaptureTarget();
	CurrentRole = ResolveSlotRole();
	bCurrentReady = ResolveSlotReady();
	RefreshPodiumVisuals(false);
}

void AThreadTalesLobbyPodium::BeginPlay()
{
	Super::BeginPlay();
	UpdateCaptureTarget();
	CurrentRole = ResolveSlotRole();
	bCurrentReady = ResolveSlotReady();
	UpdateWeaponForRole(CurrentRole);
	RefreshPodiumVisuals(true);
}

void AThreadTalesLobbyPodium::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TransitionTimerHandle);
		World->GetTimerManager().ClearTimer(WeaponAttachTimerHandle);
	}
	DestroyWeapon();
	Super::EndPlay(EndPlayReason);
}

void AThreadTalesLobbyPodium::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const EThreadTalesCharacterRole NewRole = ResolveSlotRole();
	const bool bNewReady = ResolveSlotReady();

	if (NewRole != CurrentRole)
	{
		CurrentRole = NewRole;
		bCurrentReady = bNewReady;
		bIsPlayingTransition = false;
		UpdateWeaponForRole(CurrentRole);
		RefreshPodiumVisuals(true);
		return;
	}

	if (bNewReady != bCurrentReady && !bIsPlayingTransition)
	{
		bCurrentReady = bNewReady;
		if (bCurrentReady)
		{
			PlayEquipTransition();
		}
		else
		{
			PlayUnequipTransition();
		}
	}
}

void AThreadTalesLobbyPodium::SetAssignedSlotIndex(int32 InSlotIndex)
{
	AssignedSlotIndex = FMath::Clamp(InSlotIndex, 0, 1);
	UpdateCaptureTarget();
	CurrentRole = ResolveSlotRole();
	bCurrentReady = ResolveSlotReady();
	UpdateWeaponForRole(CurrentRole);
	RefreshPodiumVisuals(true);
}

void AThreadTalesLobbyPodium::SetRenderTarget(UTextureRenderTarget2D* InRenderTarget)
{
	RenderTarget = InRenderTarget;
	if (SceneCapture)
	{
		SceneCapture->TextureTarget = InRenderTarget;
	}
}

void AThreadTalesLobbyPodium::UpdateWeaponForRole(EThreadTalesCharacterRole InRole)
{
	DestroyWeapon();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TSubclassOf<AThreadTalesWeapon> TargetClass = nullptr;
	if (InRole == EThreadTalesCharacterRole::Swordsman_Male)
	{
		TargetClass = KatanaWeaponClass;
	}
	else if (InRole == EThreadTalesCharacterRole::Archer_Female)
	{
		TargetClass = BowWeaponClass;
	}

	if (TargetClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		CurrentWeapon = World->SpawnActor<AThreadTalesWeapon>(TargetClass, SpawnParams);
		if (CurrentWeapon)
		{
			if (BasePedestalMesh)
			{
				BasePedestalMesh->IgnoreActorWhenMoving(CurrentWeapon, true);
			}
			CurrentWeapon->Equip(this);
			if (bCurrentReady)
			{
				CurrentWeapon->AttachToHand();
			}
			else
			{
				CurrentWeapon->AttachToSheath();
			}
		}
	}
}

void AThreadTalesLobbyPodium::DestroyWeapon()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->Unequip();
		CurrentWeapon->Destroy();
		CurrentWeapon = nullptr;
	}
}

void AThreadTalesLobbyPodium::PlayEquipTransition()
{
	UWorld* World = GetWorld();
	if (!World || !CharacterMesh)
	{
		return;
	}

	bIsPlayingTransition = true;
	World->GetTimerManager().ClearTimer(TransitionTimerHandle);
	World->GetTimerManager().ClearTimer(WeaponAttachTimerHandle);

	UAnimSequence* EquipAnim = (CurrentRole == EThreadTalesCharacterRole::Swordsman_Male) ? SwordsmanEquipAnim : ArcherEquipAnim;
	const float AttachDelay = (CurrentRole == EThreadTalesCharacterRole::Swordsman_Male) ? 0.40f : 0.50f;
	const float TotalDuration = EquipAnim ? EquipAnim->GetPlayLength() : 0.80f;

	if (EquipAnim)
	{
		CurrentlyPlayingAnim = EquipAnim;
		CharacterMesh->PlayAnimation(EquipAnim, false);
	}

	World->GetTimerManager().SetTimer(WeaponAttachTimerHandle, this, &AThreadTalesLobbyPodium::OnEquipWeaponAttach, AttachDelay, false);
	World->GetTimerManager().SetTimer(TransitionTimerHandle, this, &AThreadTalesLobbyPodium::OnEquipFinished, TotalDuration, false);
}

void AThreadTalesLobbyPodium::PlayUnequipTransition()
{
	UWorld* World = GetWorld();
	if (!World || !CharacterMesh)
	{
		return;
	}

	bIsPlayingTransition = true;
	World->GetTimerManager().ClearTimer(TransitionTimerHandle);
	World->GetTimerManager().ClearTimer(WeaponAttachTimerHandle);

	UAnimSequence* UnequipAnim = (CurrentRole == EThreadTalesCharacterRole::Swordsman_Male) ? SwordsmanUnequipAnim : ArcherUnequipAnim;
	const float SheathDelay = (CurrentRole == EThreadTalesCharacterRole::Swordsman_Male) ? 0.35f : 0.50f;
	const float TotalDuration = UnequipAnim ? UnequipAnim->GetPlayLength() : 0.70f;

	if (UnequipAnim)
	{
		CurrentlyPlayingAnim = UnequipAnim;
		CharacterMesh->PlayAnimation(UnequipAnim, false);
	}

	World->GetTimerManager().SetTimer(WeaponAttachTimerHandle, this, &AThreadTalesLobbyPodium::OnUnequipWeaponAttach, SheathDelay, false);
	World->GetTimerManager().SetTimer(TransitionTimerHandle, this, &AThreadTalesLobbyPodium::OnUnequipFinished, TotalDuration, false);
}

void AThreadTalesLobbyPodium::OnEquipWeaponAttach()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->AttachToHand();
	}
}

void AThreadTalesLobbyPodium::OnUnequipWeaponAttach()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->AttachToSheath();
	}
}

void AThreadTalesLobbyPodium::OnEquipFinished()
{
	bIsPlayingTransition = false;
	UAnimSequence* TargetAnim = (CurrentRole == EThreadTalesCharacterRole::Swordsman_Male) ? SwordsmanReadyAnim : ArcherReadyAnim;
	if (TargetAnim && CharacterMesh)
	{
		CurrentlyPlayingAnim = TargetAnim;
		CharacterMesh->PlayAnimation(TargetAnim, true);
	}
	if (CurrentWeapon)
	{
		CurrentWeapon->AttachToHand();
	}
}

void AThreadTalesLobbyPodium::OnUnequipFinished()
{
	bIsPlayingTransition = false;
	UAnimSequence* TargetAnim = (CurrentRole == EThreadTalesCharacterRole::Swordsman_Male) ? SwordsmanIdleAnim : ArcherIdleAnim;
	if (TargetAnim && CharacterMesh)
	{
		CurrentlyPlayingAnim = TargetAnim;
		CharacterMesh->PlayAnimation(TargetAnim, true);
	}
	if (CurrentWeapon)
	{
		CurrentWeapon->AttachToSheath();
	}
}

void AThreadTalesLobbyPodium::RefreshPodiumVisuals(bool bForceRestartAnim)
{
	if (bIsPlayingTransition)
	{
		return;
	}

	switch (CurrentRole)
	{
	case EThreadTalesCharacterRole::Swordsman_Male:
		ApplySwordsmanVisuals(bCurrentReady, bForceRestartAnim);
		break;
	case EThreadTalesCharacterRole::Archer_Female:
		ApplyArcherVisuals(bCurrentReady, bForceRestartAnim);
		break;
	case EThreadTalesCharacterRole::None:
	default:
		ApplyEmptyVisuals();
		break;
	}
}

void AThreadTalesLobbyPodium::ApplySwordsmanVisuals(bool bReady, bool bForceRestartAnim)
{
	if (CharacterMesh)
	{
		CharacterMesh->SetVisibility(true);

		UAnimSequence* TargetAnim = bReady ? SwordsmanReadyAnim : SwordsmanIdleAnim;
		if (TargetAnim && (TargetAnim != CurrentlyPlayingAnim || bForceRestartAnim))
		{
			CurrentlyPlayingAnim = TargetAnim;
			CharacterMesh->PlayAnimation(TargetAnim, true);
		}
	}
}

void AThreadTalesLobbyPodium::ApplyArcherVisuals(bool bReady, bool bForceRestartAnim)
{
	if (CharacterMesh)
	{
		CharacterMesh->SetVisibility(true);

		UAnimSequence* TargetAnim = bReady ? ArcherReadyAnim : ArcherIdleAnim;
		if (TargetAnim && (TargetAnim != CurrentlyPlayingAnim || bForceRestartAnim))
		{
			CurrentlyPlayingAnim = TargetAnim;
			CharacterMesh->PlayAnimation(TargetAnim, true);
		}
	}
}

void AThreadTalesLobbyPodium::ApplyEmptyVisuals()
{
	if (CharacterMesh)
	{
		CharacterMesh->SetVisibility(false);
		CharacterMesh->Stop();
	}
	CurrentlyPlayingAnim = nullptr;
}

AThreadTalesPlayerState* AThreadTalesLobbyPodium::ResolvePlayerState() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	int32 FoundCount = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (FoundCount == AssignedSlotIndex)
			{
				return PC->GetPlayerState<AThreadTalesPlayerState>();
			}
			FoundCount++;
		}
	}
	return nullptr;
}

EThreadTalesCharacterRole AThreadTalesLobbyPodium::ResolveSlotRole() const
{
	if (AThreadTalesPlayerState* PS = ResolvePlayerState())
	{
		return PS->GetSelectedRole();
	}

	return (AssignedSlotIndex == 0) ? EThreadTalesCharacterRole::Swordsman_Male : EThreadTalesCharacterRole::Archer_Female;
}

bool AThreadTalesLobbyPodium::ResolveSlotReady() const
{
	if (AThreadTalesPlayerState* PS = ResolvePlayerState())
	{
		return PS->IsReady();
	}

	return false;
}
