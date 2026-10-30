// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ThreadTalesWeapon_Bow.h"
#include "Combat/ThreadTalesBowTrajectoryComponent.h"
#include "Data/ThreadTalesBowConfig.h"
#include "Abilities/ThreadTalesGA_BowDraw.h"
#include "Abilities/ThreadTalesGA_BowRelease.h"
#include "Abilities/ThreadTalesGA_BowQuickCombo.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "ThreadTales.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "InputMappingContext.h"
#include "GameFramework/Character.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Net/UnrealNetwork.h"
#include "Curves/CurveFloat.h"
#include "Engine/SkeletalMeshSocket.h"
#include "UObject/ConstructorHelpers.h"

AThreadTalesWeapon_Bow::AThreadTalesWeapon_Bow()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	TrajectoryComponent = CreateDefaultSubobject<UThreadTalesBowTrajectoryComponent>(TEXT("TrajectoryComponent"));

	BowSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BowSkeletalMesh"));
	BowSkeletalMesh->SetupAttachment(RootComponent);
	BowSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BowMeshFinder(TEXT("/Game/Archer/Demo/Characters/Mannequins/Meshes/Bow.Bow"));
	if (BowMeshFinder.Succeeded()) { BowSkeletalMesh->SetSkeletalMeshAsset(BowMeshFinder.Object); }

	static ConstructorHelpers::FObjectFinder<UAnimSequence> BowDrawAnimFinder(TEXT("/Game/Animations/Archer_Retarget/AS_ArcherBow_Draw.AS_ArcherBow_Draw"));
	if (BowDrawAnimFinder.Succeeded()) { BowDrawAnimSequence = BowDrawAnimFinder.Object; }

	ArrowSpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("ArrowSpawnPoint"));
	ArrowSpawnPoint->SetupAttachment(BowSkeletalMesh, FName("Bow_String"));
	ArrowSpawnPoint->ArrowSize = 0.5f;

	NockedArrowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NockedArrowMesh"));
	NockedArrowMesh->SetupAttachment(BowSkeletalMesh, FName("Bow_String"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ArrowFinder(TEXT("/Game/Archer/Demo/Characters/Mannequins/Meshes/Arrow.Arrow"));
	if (ArrowFinder.Succeeded()) { NockedArrowMesh->SetStaticMesh(ArrowFinder.Object); }
	NockedArrowMesh->SetRelativeRotation(NockedArrowRelativeRotation);
	NockedArrowMesh->SetRelativeScale3D(FVector::OneVector);
	NockedArrowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NockedArrowMesh->SetVisibility(false);
	NockedArrowMesh->SetHiddenInGame(true);

	QuiverMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("QuiverMesh"));
	QuiverMesh->SetupAttachment(RootComponent);
	QuiverMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> QuiverFinder(TEXT("/Game/Archer/Demo/Characters/Mannequins/Meshes/Holder_Arrow.Holder_Arrow"));
	if (QuiverFinder.Succeeded()) { QuiverMesh->SetStaticMesh(QuiverFinder.Object); }

	// validate socket defaults if not overridden
	HandSocketName = FName("overlay_bow");
	SheathSocketName = FName("bow_holder1");
	QuiverSocketName = FName("arrow_holder_01");
	OverlayPoseName = FName("Bow");
	bHasSheath = false;
	bEnableSheathClipPlane = false;
	bStartDrawn = false;

	BowHandRelativeTransform = FTransform(FRotator(0.0f, 0.0f, -90.0f), FVector::ZeroVector);
	BowSheathRelativeTransform = FTransform(FRotator(0.0f, 0.0f, -90.0f), FVector::ZeroVector);
	QuiverRelativeTransform = FTransform::Identity;

	PrimaryAttackTag = ThreadTalesGameplayTags::TAG_Ability_Ranged_Fire;
	GrantedAbilities = { UThreadTalesGA_BowDraw::StaticClass(), UThreadTalesGA_BowRelease::StaticClass(), UThreadTalesGA_BowQuickCombo::StaticClass() };

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> BowIMCFinder(TEXT("/Game/Input/IMC/IMC_Katana.IMC_Katana"));
	if (BowIMCFinder.Succeeded()) { WeaponMappingContext = BowIMCFinder.Object; }
	static ConstructorHelpers::FObjectFinder<UAnimMontage> EquipMontageFinder(TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Equip.AM_Bow_Equip"));
	if (EquipMontageFinder.Succeeded()) { DrawMontage = EquipMontageFinder.Object; }
	static ConstructorHelpers::FObjectFinder<UAnimMontage> UnequipMontageFinder(TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Unequip.AM_Bow_Unequip"));
	if (UnequipMontageFinder.Succeeded()) { SheathMontage = UnequipMontageFinder.Object; }
	static ConstructorHelpers::FObjectFinder<UThreadTalesBowConfig> BowConfigFinder(TEXT("/Game/Data/DA_BowConfig.DA_BowConfig"));
	if (BowConfigFinder.Succeeded()) { BowConfig = BowConfigFinder.Object; }
}

#include "Animation/ThreadTalesBowMontageHelper.h"

void AThreadTalesWeapon_Bow::BeginPlay()
{
	Super::BeginPlay();

	if (OverlayPoseName.IsNone()) { OverlayPoseName = FName("Bow"); }
	UThreadTalesBowMontageHelper::EnsureBowCombatMontagesConfigured();

	if (!BowConfig) { BowConfig = LoadObject<UThreadTalesBowConfig>(nullptr, TEXT("/Game/Data/DA_BowConfig.DA_BowConfig")); }
	if (!BowDrawAnimSequence) { BowDrawAnimSequence = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Animations/Archer_Retarget/AS_ArcherBow_Draw.AS_ArcherBow_Draw")); }
	if (!WeaponMappingContext) { WeaponMappingContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC/IMC_Katana.IMC_Katana")); }
	if (GrantedAbilities.IsEmpty())
	{
		GrantedAbilities = { UThreadTalesGA_BowDraw::StaticClass(), UThreadTalesGA_BowRelease::StaticClass(), UThreadTalesGA_BowQuickCombo::StaticClass() };
	}
	if (!PrimaryAttackTag.IsValid()) { PrimaryAttackTag = ThreadTalesGameplayTags::TAG_Ability_Ranged_Fire; }

	if (BowSkeletalMesh && BowDrawAnimSequence)
	{
		BowSkeletalMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		BowSkeletalMesh->SetAnimation(BowDrawAnimSequence);
		BowSkeletalMesh->SetPlayRate(0.0f);
		BowSkeletalMesh->SetPosition(0.0f, false);
	}
	if (NockedArrowMesh && !NockedArrowMesh->GetStaticMesh())
	{
		if (UStaticMesh* ArrowSM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Archer/Demo/Characters/Mannequins/Meshes/Arrow.Arrow")))
		{
			NockedArrowMesh->SetStaticMesh(ArrowSM);
			NockedArrowMesh->SetRelativeScale3D(FVector::OneVector);
			NockedArrowMesh->SetRelativeRotation(NockedArrowRelativeRotation);
		}
	}
	if (WeaponMesh) { WeaponMesh->SetVisibility(false); WeaponMesh->SetHiddenInGame(true); }
	if (SheathMesh) { SheathMesh->SetVisibility(false); SheathMesh->SetHiddenInGame(true); }
	if (QuiverMesh && !QuiverMesh->GetStaticMesh()) { QuiverMesh->SetVisibility(false); QuiverMesh->SetHiddenInGame(true); }

	ClearTrajectoryArc();
	ApplyDrawRatioToAnimInstance();
}

void AThreadTalesWeapon_Bow::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsChargingTension)
	{
		DrawElapsed = GetWorld() ? FMath::Max(0.0f, GetWorld()->GetTimeSeconds() - DrawInterpolationSpeed) : 0.0f;
		const float Alpha = FMath::Clamp(DrawElapsed / FMath::Max(DrawDuration, 0.05f), 0.0f, 1.0f);
		SetDrawRatio(EvaluateTensionCurve(Alpha));
	}
	else if (bDriveDrawFromAnimCurve && IsDrawn())
	{
		if (const ACharacter* Char = Cast<ACharacter>(GetOwner()))
		{
			if (const USkeletalMeshComponent* CharMesh = Char->GetMesh())
			{
				if (const UAnimInstance* AnimInst = CharMesh->GetAnimInstance())
				{
					static const FName CurveDrawName(TEXT("Draw"));
					const float CurveVal = AnimInst->GetCurveValue(CurveDrawName);
					if (CurveVal > 0.0f) { SetDrawRatio(CurveVal); }
					else if (CurrentDrawRatio > 0.0f) { SetDrawRatio(0.0f); }
				}
			}
		}
	}
	else if (bDriveDrawFromAnimCurve && CurrentDrawRatio > 0.0f) { SetDrawRatio(0.0f); }
}

void AThreadTalesWeapon_Bow::Equip(AActor* NewOwner)
{
	// validate if not authority
	if (!HasAuthority()) { return; }

	SetOwner(NewOwner);
	if (GrantedAbilities.IsEmpty())
	{
		GrantedAbilities = { UThreadTalesGA_BowDraw::StaticClass(), UThreadTalesGA_BowRelease::StaticClass(), UThreadTalesGA_BowQuickCombo::StaticClass() };
	}
	if (!WeaponMappingContext) { WeaponMappingContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC/IMC_Katana.IMC_Katana")); }

	if (ACharacter* Character = Cast<ACharacter>(NewOwner))
	{
		if (Character->GetMesh())
		{
			if (bStartDrawn) { AttachToHand(); }
			else { AttachToSheath(); }
		}
	}

	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(NewOwner))
	{
		OwnerASC = ASI->GetAbilitySystemComponent();
		if (OwnerASC)
		{
			for (TSubclassOf<UGameplayAbility> AbilityClass : GrantedAbilities)
			{
				if (AbilityClass) { GrantedAbilityHandles.Add(OwnerASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this))); }
			}
		}
	}

	AddWeaponMappingContext();
	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Bow] Equip on '%s': Granted %d abilities, IMC: '%s'"),
		*GetNameSafe(NewOwner), GrantedAbilities.Num(), WeaponMappingContext ? *WeaponMappingContext->GetName() : TEXT("None"));
}

void AThreadTalesWeapon_Bow::Unequip()
{
	if (!HasAuthority()) { return; }
	if (BowSkeletalMesh) { BowSkeletalMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform); }
	if (QuiverMesh) { QuiverMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform); }
	Super::Unequip();
}

void AThreadTalesWeapon_Bow::AttachToHand()
{
	// validate socket if not null
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character && Character->GetMesh() && BowSkeletalMesh)
	{
		USkeletalMeshComponent* CharMesh = Character->GetMesh();
		const FName TargetHandSocket = CharMesh->DoesSocketExist(FName("overlay_bow")) ? FName("overlay_bow") : HandSocketName;
		const FTransform RefPoseCompTransform(FRotator(0.0f, 0.0f, -90.0f), FVector::ZeroVector);

		BowSkeletalMesh->AttachToComponent(CharMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TargetHandSocket);
		BowSkeletalMesh->SetRelativeTransform(RefPoseCompTransform);
		BowSkeletalMesh->SetVisibility(true);
		BowSkeletalMesh->SetHiddenInGame(false);
		BowSkeletalMesh->UpdateComponentToWorld();

		if (QuiverMesh && QuiverMesh->GetStaticMesh() && CharMesh->DoesSocketExist(QuiverSocketName))
		{
			QuiverMesh->AttachToComponent(CharMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, QuiverSocketName);
			QuiverMesh->SetRelativeTransform(QuiverRelativeTransform);
			QuiverMesh->SetVisibility(true);
			QuiverMesh->SetHiddenInGame(false);
			QuiverMesh->UpdateComponentToWorld();
		}

		if (WeaponState != EWeaponCombatState::Drawing) { SetWeaponState(EWeaponCombatState::Drawn); }
		AddWeaponMappingContext();
		ClearTrajectoryArc();
		SetActorTickEnabled(true);

		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Bow] Attached BowSkeletalMesh to hand socket '%s' on '%s'"),
			*TargetHandSocket.ToString(), *Character->GetName());
	}
}

void AThreadTalesWeapon_Bow::AttachToSheath()
{
	// validate socket if not null
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character && Character->GetMesh() && BowSkeletalMesh)
	{
		USkeletalMeshComponent* CharMesh = Character->GetMesh();
		const FName TargetSheathSocket = CharMesh->DoesSocketExist(FName("bow_holder1")) ? FName("bow_holder1") : SheathSocketName;
		const FTransform RefPoseCompTransform(FRotator(0.0f, 0.0f, -90.0f), FVector::ZeroVector);

		SnapDrawTensionToRest();
		SetNockedArrowVisible(false);
		ClearTrajectoryArc();

		BowSkeletalMesh->AttachToComponent(CharMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TargetSheathSocket);
		BowSkeletalMesh->SetRelativeTransform(RefPoseCompTransform);
		BowSkeletalMesh->SetVisibility(true);
		BowSkeletalMesh->SetHiddenInGame(false);
		BowSkeletalMesh->UpdateComponentToWorld();

		if (QuiverMesh && QuiverMesh->GetStaticMesh() && CharMesh->DoesSocketExist(QuiverSocketName))
		{
			QuiverMesh->AttachToComponent(CharMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, QuiverSocketName);
			QuiverMesh->SetRelativeTransform(QuiverRelativeTransform);
			QuiverMesh->SetVisibility(true);
			QuiverMesh->SetHiddenInGame(false);
			QuiverMesh->UpdateComponentToWorld();
		}

		if (WeaponState != EWeaponCombatState::Sheathing) { SetWeaponState(EWeaponCombatState::Sheathed); }
		SetActorTickEnabled(false);
		if (bAddContextOnlyWhenDrawn) { RemoveWeaponMappingContext(); }

		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Bow] Attached BowSkeletalMesh to sheath socket '%s' on '%s'"),
			*TargetSheathSocket.ToString(), *Character->GetName());
	}
}

float AThreadTalesWeapon_Bow::EvaluateTensionCurve(float InAlpha) const
{
	const float ClampedAlpha = FMath::Clamp(InAlpha, 0.0f, 1.0f);
	if (TensionCurve) { return FMath::Clamp(TensionCurve->GetFloatValue(ClampedAlpha), 0.0f, 1.0f); }
	return FMath::InterpEaseInOut(0.0f, 1.0f, ClampedAlpha, 1.35f);
}

void AThreadTalesWeapon_Bow::SetDrawRatio(float InDrawRatio)
{
	const float Clamped = FMath::Clamp(InDrawRatio, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(CurrentDrawRatio, Clamped, 0.005f) && Clamped != 0.0f && Clamped != 1.0f) { return; }
	CurrentDrawRatio = Clamped;
	ApplyDrawRatioToAnimInstance();
}

void AThreadTalesWeapon_Bow::StartDrawingTension(float InDuration)
{
	DrawDuration = FMath::Max(InDuration, 0.05f);
	DrawElapsed = 0.0f;
	DrawInterpolationSpeed = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	TargetDrawRatio = 1.0f;
	bIsChargingTension = true;
	SetActorTickEnabled(true);
}

void AThreadTalesWeapon_Bow::SnapDrawTensionToRest()
{
	bIsChargingTension = false;
	DrawElapsed = 0.0f;
	TargetDrawRatio = 0.0f;
	DrawInterpolationSpeed = 0.0f;
	SetDrawRatio(0.0f);
}

void AThreadTalesWeapon_Bow::SetNockedArrowVisible(bool bVisible)
{
	if (!NockedArrowMesh) { return; }
	if (bVisible) { AttachNockedArrowToString(); }
	else
	{
		NockedArrowMesh->SetVisibility(false);
		NockedArrowMesh->SetHiddenInGame(true);
	}
}

bool AThreadTalesWeapon_Bow::IsNockedArrowVisible() const
{
	return NockedArrowMesh && NockedArrowMesh->IsVisible() && !NockedArrowMesh->bHiddenInGame;
}

void AThreadTalesWeapon_Bow::AttachNockedArrowToHand()
{
	// validate socket if not null
	if (!NockedArrowMesh) { return; }
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !Character->GetMesh()) { return; }

	USkeletalMeshComponent* CharMesh = Character->GetMesh();
	const FName HandBone = CharMesh->DoesSocketExist(FName("hand_r")) ? FName("hand_r") : FName("hand_rSocket");
	NockedArrowMesh->AttachToComponent(CharMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HandBone);
	NockedArrowMesh->SetRelativeLocation(FVector(-11.09f, 1.38f, 2.87f));
	NockedArrowMesh->SetRelativeRotation(FRotator(71.85f, -81.79f, -160.24f));
	NockedArrowMesh->SetRelativeScale3D(FVector::OneVector);
	NockedArrowMesh->SetVisibility(true);
	NockedArrowMesh->SetHiddenInGame(false);
}

void AThreadTalesWeapon_Bow::AttachNockedArrowToString()
{
	// validate socket if not null
	if (!NockedArrowMesh || !BowSkeletalMesh) { return; }
	const FName StringSocket = !NockedArrowSocketName.IsNone() ? NockedArrowSocketName : FName("Bow_String");
	if (BowSkeletalMesh->DoesSocketExist(StringSocket))
	{
		NockedArrowMesh->AttachToComponent(BowSkeletalMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, StringSocket);
		NockedArrowMesh->SetRelativeRotation(NockedArrowRelativeRotation);
		NockedArrowMesh->SetRelativeScale3D(FVector::OneVector);
		NockedArrowMesh->SetRelativeLocation(NockedArrowRelativeOffset);
	}
	NockedArrowMesh->SetVisibility(true);
	NockedArrowMesh->SetHiddenInGame(false);
}

static void SetAnimInstanceDrawProp(UAnimInstance* AnimInst, float DrawRatio)
{
	if (!AnimInst) { return; }
	static const FName DrawPropName(TEXT("Draw"));
	if (FProperty* Prop = AnimInst->GetClass()->FindPropertyByName(DrawPropName))
	{
		if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop)) { DoubleProp->SetPropertyValue_InContainer(AnimInst, static_cast<double>(DrawRatio)); }
		else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop)) { FloatProp->SetPropertyValue_InContainer(AnimInst, DrawRatio); }
	}
}

static void UpdateOverlayBowAimPose(const ACharacter* Char, bool bIsDrawingArrow)
{
	if (!Char || !Char->GetMesh()) { return; }
	static UClass* CachedOverlayBowClass = nullptr;
	if (!CachedOverlayBowClass) { CachedOverlayBowClass = LoadClass<UAnimInstance>(nullptr, TEXT("/GASPALS/OverlaySystem/Overlays/Poses/Bow/ABP_Overlay_Bow.ABP_Overlay_Bow_C")); }
	if (!CachedOverlayBowClass) { return; }

	UAnimInstance* LinkedBowOverlay = Char->GetMesh()->GetLinkedAnimLayerInstanceByClass(CachedOverlayBowClass);
	if (!LinkedBowOverlay) { return; }

	static UAnimSequence* CachedAimDrawPose = nullptr;
	static UAnimSequence* CachedAimReadyPose = nullptr;
	if (!CachedAimDrawPose) { CachedAimDrawPose = LoadObject<UAnimSequence>(nullptr, TEXT("/GASPALS/OverlaySystem/Overlays/Poses/Bow/Pose_Bow_Archer_AimDraw_Static.Pose_Bow_Archer_AimDraw_Static")); }
	if (!CachedAimReadyPose) { CachedAimReadyPose = LoadObject<UAnimSequence>(nullptr, TEXT("/GASPALS/OverlaySystem/Overlays/Poses/Bow/Pose_Bow_Archer_AimReady_Static.Pose_Bow_Archer_AimReady_Static")); }

	UAnimSequence* TargetPose = bIsDrawingArrow ? CachedAimDrawPose : CachedAimReadyPose;
	if (!TargetPose) { TargetPose = CachedAimReadyPose ? CachedAimReadyPose : CachedAimDrawPose; }
	if (!TargetPose) { return; }

	static const FName AimPoseProps[] = {
		FName("Pose_Stand_Idle_Aim"), FName("Pose_Stand_Walk_Aim"), FName("Pose_Stand_Run_Aim"),
		FName("Pose_Crouch_Idle_Aim"), FName("Pose_Crouch_Move_Aim")
	};
	for (const FName& PropName : AimPoseProps)
	{
		if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(LinkedBowOverlay->GetClass()->FindPropertyByName(PropName)))
		{
			if (ObjProp->GetObjectPropertyValue_InContainer(LinkedBowOverlay) != TargetPose) { ObjProp->SetObjectPropertyValue_InContainer(LinkedBowOverlay, TargetPose); }
		}
	}
}

void AThreadTalesWeapon_Bow::ApplyDrawRatioToAnimInstance()
{
	if (BowSkeletalMesh)
	{
		if (BowDrawAnimSequence)
		{
			if (BowSkeletalMesh->GetAnimationMode() != EAnimationMode::AnimationSingleNode)
			{
				BowSkeletalMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
				BowSkeletalMesh->SetAnimation(BowDrawAnimSequence);
				BowSkeletalMesh->SetPlayRate(0.0f);
			}
			else if (UAnimSingleNodeInstance* SingleNode = BowSkeletalMesh->GetSingleNodeInstance())
			{
				if (SingleNode->GetCurrentAsset() != BowDrawAnimSequence)
				{
					BowSkeletalMesh->SetAnimation(BowDrawAnimSequence);
					BowSkeletalMesh->SetPlayRate(0.0f);
				}
			}
			BowSkeletalMesh->SetPosition(FMath::Clamp(CurrentDrawRatio, 0.0f, 1.0f) * BowDrawAnimSequence->GetPlayLength(), false);
		}
		else { SetAnimInstanceDrawProp(BowSkeletalMesh->GetAnimInstance(), CurrentDrawRatio); }
	}

	if (const ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		UpdateOverlayBowAimPose(Char, bIsChargingTension || CurrentDrawRatio > 0.02f);
		if (const USkeletalMeshComponent* CharMesh = Char->GetMesh())
		{
			TArray<USceneComponent*> MeshChildren;
			CharMesh->GetChildrenComponents(true, MeshChildren);
			for (USceneComponent* Child : MeshChildren)
			{
				if (USkeletalMeshComponent* ChildSkMesh = Cast<USkeletalMeshComponent>(Child))
				{
					if (ChildSkMesh != BowSkeletalMesh && ChildSkMesh->GetAnimInstance()) { SetAnimInstanceDrawProp(ChildSkMesh->GetAnimInstance(), CurrentDrawRatio); }
				}
			}
		}
	}
}

void AThreadTalesWeapon_Bow::OnRep_CurrentDrawRatio() { ApplyDrawRatioToAnimInstance(); }

FTransform AThreadTalesWeapon_Bow::GetArrowSpawnTransform() const
{
	if (BowSkeletalMesh && BowSkeletalMesh->IsVisible())
	{
		FTransform BowTransform = BowSkeletalMesh->GetComponentTransform();
		BowTransform.AddToTranslation(BowTransform.GetRotation().RotateVector(FVector(15.0f, 0.0f, 0.0f)));
		return BowTransform;
	}
	return ArrowSpawnPoint ? ArrowSpawnPoint->GetComponentTransform() : GetActorTransform();
}

void AThreadTalesWeapon_Bow::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AThreadTalesWeapon_Bow, CurrentDrawRatio);
}

UAnimMontage* AThreadTalesWeapon_Bow::GetParryMontage(int32 DirectionIndex) const { return BowConfig ? BowConfig->GetParryMontage(DirectionIndex) : nullptr; }
UAnimMontage* AThreadTalesWeapon_Bow::GetParryCounterMontage(int32 DirectionIndex) const { return BowConfig ? BowConfig->GetParryCounterMontage(DirectionIndex) : nullptr; }
UAnimMontage* AThreadTalesWeapon_Bow::GetAimReadyMontage() const { return BowConfig ? BowConfig->GetAimReadyMontage() : LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Aim_Ready.AM_Bow_Aim_Ready")); }
UAnimMontage* AThreadTalesWeapon_Bow::GetDrawHoldMontage() const { return BowConfig ? BowConfig->GetDrawHoldMontage() : LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Draw_Hold.AM_Bow_Draw_Hold")); }

FVector AThreadTalesWeapon_Bow::GetTrajectoryStartLocation() const
{
	if (BowSkeletalMesh && BowSkeletalMesh->IsVisible())
	{
		const FName StringSocket = !NockedArrowSocketName.IsNone() ? NockedArrowSocketName : FName("Bow_String");
		if (BowSkeletalMesh->DoesSocketExist(StringSocket)) { return BowSkeletalMesh->GetSocketLocation(StringSocket); }
		if (NockedArrowMesh && NockedArrowMesh->IsVisible()) { return NockedArrowMesh->GetComponentLocation(); }
	}
	return GetArrowSpawnTransform().GetLocation();
}

bool AThreadTalesWeapon_Bow::GetAimConvergenceTargetAndDirection(const AActor* AvatarActor, const FVector& MuzzleOrigin, FVector& OutTargetLocation, FVector& OutLaunchDirection) const
{
	return TrajectoryComponent ? TrajectoryComponent->CalculateAimConvergence(AvatarActor, MuzzleOrigin, OutTargetLocation, OutLaunchDirection) : false;
}

void AThreadTalesWeapon_Bow::UpdateTrajectoryArc(const AActor* AvatarActor, float DrawRatio, int32 ChargeTier)
{
	if (TrajectoryComponent) { TrajectoryComponent->UpdateTrajectoryArc(AvatarActor, GetTrajectoryStartLocation(), DrawRatio, ChargeTier); }
}

void AThreadTalesWeapon_Bow::ClearTrajectoryArc()
{
	if (TrajectoryComponent) { TrajectoryComponent->ClearTrajectoryArc(); }
}

void AThreadTalesWeapon_Bow::SetSplineMeshRibbonEnabled(bool bInEnable)
{
	if (TrajectoryComponent)
	{
		TrajectoryComponent->SetEnableSplineMeshRibbon(bInEnable);
	}
}

bool AThreadTalesWeapon_Bow::IsSplineMeshRibbonEnabled() const
{
	return TrajectoryComponent ? TrajectoryComponent->bEnableSplineMeshRibbon : false;
}
