// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesTraversalComponent.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesClimbingConfig.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogThreadTalesTraversal, Log, All);

UThreadTalesTraversalComponent::UThreadTalesTraversalComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);

	CurrentTraversalState = EThreadTalesTraversalState::None;
	ClimbingSubState = EThreadTalesClimbingSubState::None;
	bIsFreehang = false;
	bDrawDebugTraces = false;

	CachedLedgeLocation = FVector::ZeroVector;
	CachedWallNormal = FVector::ZeroVector;
	CachedHangLocation = FVector::ZeroVector;
	CachedHangRotation = FRotator::ZeroRotator;

	TransitionStartLocation = FVector::ZeroVector;
	TransitionTargetLocation = FVector::ZeroVector;
	TransitionStartRotation = FRotator::ZeroRotator;
	TransitionTargetRotation = FRotator::ZeroRotator;

	PendingTargetLedgeLocation = FVector::ZeroVector;
	PendingTargetWallNormal = FVector::ZeroVector;

	TransitionDistanceType = EThreadTalesLedgeJumpDistanceType::Short;
	TransitionTimer = 0.0f;
	TransitionDuration = 0.8f;

	RegrabCooldownTimer = 0.0f;
	ClimbingInputDebounceTimer = 0.0f;
	ShimmyCooldownTimer = 0.0f;
}

void UThreadTalesTraversalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UThreadTalesTraversalComponent, CurrentTraversalState);
	DOREPLIFETIME(UThreadTalesTraversalComponent, bIsFreehang);
}

void UThreadTalesTraversalComponent::BeginPlay()
{
	Super::BeginPlay();

	CharacterOwner = Cast<AThreadTalesCharacter>(GetOwner());
	JumpConfigTable.Build(ClimbingConfig, GetActiveHangMontage(false));
}

void UThreadTalesTraversalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (RegrabCooldownTimer > 0.0f)
	{
		RegrabCooldownTimer -= DeltaTime;
	}
	if (ClimbingInputDebounceTimer > 0.0f)
	{
		ClimbingInputDebounceTimer -= DeltaTime;
	}
	if (ShimmyCooldownTimer > 0.0f)
	{
		ShimmyCooldownTimer -= DeltaTime;
	}

	if (!CharacterOwner)
	{
		return;
	}

	// 1. If currently transitioning between ledges (Hop Up, Side Jump, Shimmy), interpolate character
	if (CurrentTraversalState == EThreadTalesTraversalState::LedgeClimbing && IsTransitioning())
	{
		TransitionTimer += DeltaTime;
		const float NormalizedTime = FMath::Clamp(TransitionTimer / FMath::Max(TransitionDuration, 0.01f), 0.0f, 1.0f);

		FVector NewLoc;
		FRotator NewRot;
		FThreadTalesClimbingKinematics::CalculateTransitionTransform(
			NormalizedTime,
			ClimbingSubState == EThreadTalesClimbingSubState::JumpTransition,
			TransitionDistanceType,
			ClimbingConfig,
			TransitionStartLocation,
			TransitionTargetLocation,
			TransitionStartRotation,
			TransitionTargetRotation,
			NewLoc,
			NewRot
		);

		CharacterOwner->SetActorLocationAndRotation(NewLoc, NewRot, false, nullptr, ETeleportType::TeleportPhysics);

		if (NormalizedTime >= 1.0f)
		{
			ClimbingSubState = EThreadTalesClimbingSubState::IdleHang;
			CachedLedgeLocation = PendingTargetLedgeLocation;
			CachedWallNormal = PendingTargetWallNormal;
			CachedHangLocation = TransitionTargetLocation;
			CachedHangRotation = TransitionTargetRotation;

			bool bChainedShimmy = false;
			if (CharacterOwner->IsLocallyControlled())
			{
				float NextDirSign = 0.0f;
				if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
				{
					if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Right))
					{
						NextDirSign = 1.0f;
					}
					else if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Left))
					{
						NextDirSign = -1.0f;
					}
				}

				if (NextDirSign != 0.0f)
				{
					FVector NextLedgeLoc;
					FVector NextWallNormal;
					bool bNextContinuous = false;
					if (FindSideLedge(NextDirSign, NextLedgeLoc, NextWallNormal, bNextContinuous, false) && bNextContinuous)
					{
						bChainedShimmy = true;
						StartShimmy(NextDirSign, NextLedgeLoc, NextWallNormal);
					}
				}
			}

			if (!bChainedShimmy)
			{
				ShimmyCooldownTimer = GetShimmyCooldownDuration();
				ClimbingInputDebounceTimer = 0.35f;
				CrossfadeToHangMontage();
			}
		}
		return;
	}

	// 2. While hanging on ledge (IdleHang), process WASD climbing input and hard-anchor
	if (CurrentTraversalState == EThreadTalesTraversalState::LedgeClimbing && ClimbingSubState == EThreadTalesClimbingSubState::IdleHang)
	{
		if (CharacterOwner->IsLocallyControlled())
		{
			ProcessClimbingInput(DeltaTime);
		}

		if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
		{
			MoveComp->Velocity = FVector::ZeroVector;
		}
		CharacterOwner->SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation, false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	// 3. Auto-detect ledges when airborne
	const bool bAutoGrab = ClimbingConfig ? ClimbingConfig->bAutoGrabLedge : true;
	const float MaxGrabVelZ = ClimbingConfig ? ClimbingConfig->MaxGrabVelocityZ : 150.0f;

	if (CurrentTraversalState == EThreadTalesTraversalState::None && bAutoGrab && RegrabCooldownTimer <= 0.0f)
	{
		UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
		if (MoveComp && MoveComp->IsFalling() && MoveComp->Velocity.Z <= MaxGrabVelZ)
		{
			if (CharacterOwner->IsLocallyControlled() || CharacterOwner->HasAuthority())
			{
				FVector DetectedLedgeLocation;
				FVector DetectedWallNormal;
				if (DetectLedge(DetectedLedgeLocation, DetectedWallNormal))
				{
					StartLedgeClimb(DetectedLedgeLocation, DetectedWallNormal);
				}
			}
		}
	}
}

// ==========================================
// DELEGATED SENSING CALLS (FThreadTalesLedgeDetector)
// ==========================================

bool UThreadTalesTraversalComponent::DetectLedge(FVector& OutLedgeLocation, FVector& OutWallNormal)
{
	return FThreadTalesLedgeDetector::DetectLedge(CharacterOwner, ClimbingConfig, bDrawDebugTraces, OutLedgeLocation, OutWallNormal);
}

bool UThreadTalesTraversalComponent::FindUpperLedge(FVector& OutLedgeLoc, FVector& OutWallNormal)
{
	return FThreadTalesLedgeDetector::FindUpperLedge(CharacterOwner, ClimbingConfig, bDrawDebugTraces, CachedLedgeLocation, CachedWallNormal, OutLedgeLoc, OutWallNormal);
}

bool UThreadTalesTraversalComponent::FindSideLedge(float DirectionSign, FVector& OutLedgeLoc, FVector& OutWallNormal, bool& bOutIsContinuous, bool bForJump)
{
	return FThreadTalesLedgeDetector::FindSideLedge(CharacterOwner, ClimbingConfig, bDrawDebugTraces, CachedLedgeLocation, CachedWallNormal, DirectionSign, bForJump, OutLedgeLoc, OutWallNormal, bOutIsContinuous);
}

bool UThreadTalesTraversalComponent::FindLedgeInDirection(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, FVector& OutLedgeLoc, FVector& OutWallNormal)
{
	return FThreadTalesLedgeDetector::FindLedgeInDirection(CharacterOwner, ClimbingConfig, bDrawDebugTraces, CachedLedgeLocation, CachedWallNormal, Direction, DistanceType, JumpConfigTable, OutLedgeLoc, OutWallNormal);
}

bool UThreadTalesTraversalComponent::CanClimbUp(FVector& OutStandingLoc) const
{
	return FThreadTalesLedgeDetector::CanClimbUp(CharacterOwner, ClimbingConfig, bDrawDebugTraces, CachedLedgeLocation, CachedWallNormal, OutStandingLoc);
}

bool UThreadTalesTraversalComponent::CheckIsFreehangLedge(const FVector& EdgeLoc, const FVector& WallNormal2D) const
{
	return FThreadTalesLedgeDetector::CheckIsFreehangLedge(CharacterOwner, ClimbingConfig, bDrawDebugTraces, EdgeLoc, WallNormal2D);
}

bool UThreadTalesTraversalComponent::ValidateLedgeLipAndClearance(const FVector& EdgeLoc, const FVector& WallNormal2D)
{
	return FThreadTalesLedgeDetector::ValidateLedgeLipAndClearance(CharacterOwner, ClimbingConfig, bDrawDebugTraces, EdgeLoc, WallNormal2D);
}

// ==========================================
// DELEGATED JUMP TABLE & KINEMATICS CALLS
// ==========================================

const FThreadTalesLedgeJumpConfig& UThreadTalesTraversalComponent::GetJumpConfig(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const
{
	return JumpConfigTable.GetConfig(Direction, DistanceType);
}

bool UThreadTalesTraversalComponent::GetJumpDistanceBounds(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, float& OutMinDist, float& OutMaxDist) const
{
	return JumpConfigTable.GetDistanceBounds(Direction, DistanceType, OutMinDist, OutMaxDist);
}

bool UThreadTalesTraversalComponent::IsLedgeDistanceValid(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, float Distance) const
{
	return JumpConfigTable.IsDistanceValid(Direction, DistanceType, Distance);
}

float UThreadTalesTraversalComponent::GetNominalJumpDistance(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const
{
	return JumpConfigTable.GetNominalDistance(Direction, DistanceType);
}

UAnimMontage* UThreadTalesTraversalComponent::GetJumpMontage(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const
{
	return JumpConfigTable.GetJumpMontage(Direction, DistanceType);
}

TArray<EThreadTalesLedgeJumpDistanceType> UThreadTalesTraversalComponent::GetJumpDistancePriorities(bool bIsSprint) const
{
	return FThreadTalesJumpConfigTable::GetDistancePriorities(bIsSprint);
}

void UThreadTalesTraversalComponent::GetJumpKinematicPhases(EThreadTalesLedgeJumpDistanceType DistanceType, float& OutAnticipationEnd, float& OutFlightEnd) const
{
	FThreadTalesJumpConfigTable::GetKinematicPhases(DistanceType, ClimbingConfig, OutAnticipationEnd, OutFlightEnd);
}

// ==========================================
// ACTIVE MONTAGES & TUNING RESOLVERS
// ==========================================

UAnimMontage* UThreadTalesTraversalComponent::GetActiveHangMontage(bool bFreehang) const
{
	if (ClimbingConfig)
	{
		UAnimMontage* CfgMont = bFreehang ? ClimbingConfig->BarHangMontage.Get() : ClimbingConfig->HangMontage.Get();
		if (CfgMont) return CfgMont;
	}
	return bFreehang ? BarHangMontage.Get() : HangMontage.Get();
}

UAnimMontage* UThreadTalesTraversalComponent::GetActiveClimbOnTopMontage(bool bFreehang) const
{
	if (ClimbingConfig)
	{
		UAnimMontage* CfgMont = bFreehang ? ClimbingConfig->BarClimbOnTopMontage.Get() : ClimbingConfig->ClimbOnTopMontage.Get();
		if (CfgMont) return CfgMont;
	}
	return bFreehang ? BarClimbOnTopMontage.Get() : ClimbOnTopMontage.Get();
}

UAnimMontage* UThreadTalesTraversalComponent::GetActiveClimbDownMontage(bool bFreehang) const
{
	if (ClimbingConfig)
	{
		UAnimMontage* CfgMont = bFreehang ? ClimbingConfig->BarClimbDownMontage.Get() : ClimbingConfig->ClimbDownMontage.Get();
		if (CfgMont) return CfgMont;
	}
	return bFreehang ? BarClimbDownMontage.Get() : ClimbDownMontage.Get();
}

UAnimMontage* UThreadTalesTraversalComponent::GetActiveBackJumpMontage() const
{
	if (ClimbingConfig && ClimbingConfig->ClimbBackJumpMontage)
	{
		return ClimbingConfig->ClimbBackJumpMontage.Get();
	}
	return ClimbBackJumpMontage.Get();
}

UAnimMontage* UThreadTalesTraversalComponent::GetActiveShimmyMontage(bool bLeft) const
{
	if (ClimbingConfig)
	{
		UAnimMontage* CfgMont = bLeft ? ClimbingConfig->ShimmyLeftMontage.Get() : ClimbingConfig->ShimmyRightMontage.Get();
		if (CfgMont) return CfgMont;
	}
	return bLeft ? ShimmyLeftMontage.Get() : ShimmyRightMontage.Get();
}

float UThreadTalesTraversalComponent::GetHangWallOffset() const { return ClimbingConfig ? ClimbingConfig->HangWallOffset : 24.0f; }
float UThreadTalesTraversalComponent::GetHangHeightOffset() const { return ClimbingConfig ? ClimbingConfig->HangHeightOffset : 73.0f; }
float UThreadTalesTraversalComponent::GetClimbAnimPlayRateMultiplier() const { return ClimbingConfig ? ClimbingConfig->ClimbAnimPlayRateMultiplier : 1.0f; }
float UThreadTalesTraversalComponent::GetClimbBackJumpPlayRate() const { return ClimbingConfig ? ClimbingConfig->ClimbBackJumpPlayRate : 1.35f; }
float UThreadTalesTraversalComponent::GetShimmyCooldownDuration() const { return ClimbingConfig ? ClimbingConfig->ShimmyCooldownDuration : 0.08f; }
float UThreadTalesTraversalComponent::GetShimmyInputDebounceDuration() const { return ClimbingConfig ? ClimbingConfig->ShimmyInputDebounceDuration : 0.15f; }
float UThreadTalesTraversalComponent::GetJumpInputDebounceDuration() const { return ClimbingConfig ? ClimbingConfig->JumpInputDebounceDuration : 0.25f; }
float UThreadTalesTraversalComponent::GetRegrabCooldownDuration() const { return ClimbingConfig ? ClimbingConfig->RegrabCooldownDuration : 0.4f; }
float UThreadTalesTraversalComponent::GetWallEjectRegrabCooldown() const { return ClimbingConfig ? ClimbingConfig->WallEjectRegrabCooldown : 0.55f; }

// ==========================================
// STATE MANAGEMENT & ACTIONS
// ==========================================

void UThreadTalesTraversalComponent::SetTraversalState(EThreadTalesTraversalState NewState)
{
	if (CurrentTraversalState == NewState)
	{
		return;
	}

	const EThreadTalesTraversalState OldState = CurrentTraversalState;
	CurrentTraversalState = NewState;

	if (CharacterOwner)
	{
		if (UAbilitySystemComponent* ASC = CharacterOwner->GetAbilitySystemComponent())
		{
			if (NewState == EThreadTalesTraversalState::LedgeClimbing)
			{
				ASC->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing);
				if (ClimbingSubState != EThreadTalesClimbingSubState::ClimbDown && ClimbingSubState != EThreadTalesClimbingSubState::MantleUp)
				{
					ClimbingSubState = EThreadTalesClimbingSubState::IdleHang;
				}
			}
			else if (OldState == EThreadTalesTraversalState::LedgeClimbing)
			{
				ASC->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing);
				ClimbingSubState = EThreadTalesClimbingSubState::None;
			}
		}
	}

	OnTraversalStateChanged.Broadcast(NewState, OldState);
}

void UThreadTalesTraversalComponent::SetClimbingSubState(EThreadTalesClimbingSubState NewSubState)
{
	ClimbingSubState = NewSubState;
}

void UThreadTalesTraversalComponent::OnRep_TraversalState(EThreadTalesTraversalState OldState)
{
	if (CharacterOwner)
	{
		if (UAbilitySystemComponent* ASC = CharacterOwner->GetAbilitySystemComponent())
		{
			if (CurrentTraversalState == EThreadTalesTraversalState::LedgeClimbing)
			{
				ASC->AddLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing);
			}
			else if (OldState == EThreadTalesTraversalState::LedgeClimbing)
			{
				ASC->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing);
			}
		}
	}
	OnTraversalStateChanged.Broadcast(CurrentTraversalState, OldState);
}

void UThreadTalesTraversalComponent::CrossfadeToHangMontage()
{
	if (!CharacterOwner || !CharacterOwner->GetMesh())
	{
		return;
	}

	UAnimMontage* ActiveHangMont = GetActiveHangMontage(bIsFreehang);
	if (ActiveHangMont)
	{
		CharacterOwner->PlayAnimMontage(ActiveHangMont);
		if (UAnimInstance* AnimInst = CharacterOwner->GetMesh()->GetAnimInstance())
		{
			AnimInst->Montage_SetNextSection(FName("Default"), FName("Default"), ActiveHangMont);
		}
	}
}

void UThreadTalesTraversalComponent::StopAllClimbingMontages()
{
	if (!CharacterOwner)
	{
		return;
	}

	if (UAnimInstance* AnimInst = CharacterOwner->GetMesh() ? CharacterOwner->GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInst->StopAllMontages(0.15f);
	}
}

void UThreadTalesTraversalComponent::StartLedgeClimb(const FVector& LedgeLocation, const FVector& WallNormal)
{
	if (!CharacterOwner)
	{
		return;
	}

	bIsFreehang = CheckIsFreehangLedge(LedgeLocation, WallNormal);
	ClimbingSubState = EThreadTalesClimbingSubState::IdleHang;
	CachedLedgeLocation = LedgeLocation;
	CachedWallNormal = WallNormal;

	const FRotator FaceWallRotation = (-WallNormal).Rotation();
	CachedHangRotation = FRotator(0.0f, FaceWallRotation.Yaw, 0.0f);
	CachedHangLocation = LedgeLocation + (WallNormal * GetHangWallOffset()) - FVector(0.0f, 0.0f, GetHangHeightOffset());

	SetTraversalState(EThreadTalesTraversalState::LedgeClimbing);

	UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
	if (MoveComp)
	{
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->SetMovementMode(MOVE_Flying);
	}

	CharacterOwner->SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation, false, nullptr, ETeleportType::TeleportPhysics);

	CrossfadeToHangMontage();

	if (!CharacterOwner->HasAuthority())
	{
		Server_StartLedgeClimb(LedgeLocation, WallNormal);
	}
	else
	{
		Multicast_StartLedgeClimb(LedgeLocation, WallNormal);
	}
}

void UThreadTalesTraversalComponent::ReleaseLedge()
{
	if (CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing || !CharacterOwner)
	{
		return;
	}

	StopAllClimbingMontages();

	if (ClimbingSubState == EThreadTalesClimbingSubState::ClimbDown)
	{
		SetClimbDownCollision(false);
	}
	ClimbingSubState = EThreadTalesClimbingSubState::None;
	bIsFreehang = false;

	if (!CharacterOwner->HasAuthority())
	{
		Server_ReleaseLedge();
	}
	else
	{
		Multicast_ReleaseLedge();
	}

	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Falling);
	}

	SetTraversalState(EThreadTalesTraversalState::None);
	RegrabCooldownTimer = GetRegrabCooldownDuration();
}

void UThreadTalesTraversalComponent::Internal_StartTransition(UAnimMontage* MontageToPlay, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, float CustomDuration, EThreadTalesLedgeJumpDistanceType DistanceType)
{
	if (!CharacterOwner)
	{
		return;
	}

	TransitionStartLocation = CharacterOwner->GetActorLocation();
	TransitionStartRotation = CharacterOwner->GetActorRotation();

	const FRotator FaceWallRotation = (-TargetWallNormal).Rotation();
	TransitionTargetRotation = FRotator(0.0f, FaceWallRotation.Yaw, 0.0f);
	TransitionTargetLocation = TargetLedgeLoc + (TargetWallNormal * GetHangWallOffset()) - FVector(0.0f, 0.0f, GetHangHeightOffset());

	PendingTargetLedgeLocation = TargetLedgeLoc;
	PendingTargetWallNormal = TargetWallNormal;

	bIsFreehang = CheckIsFreehangLedge(TargetLedgeLoc, TargetWallNormal);
	TransitionDistanceType = DistanceType;
	TransitionTimer = 0.0f;

	if (CustomDuration > 0.0f)
	{
		TransitionDuration = CustomDuration;
	}
	else if (MontageToPlay)
	{
		TransitionDuration = MontageToPlay->GetPlayLength() / FMath::Max(GetClimbAnimPlayRateMultiplier(), 0.1f);
	}
	else
	{
		TransitionDuration = 0.8f;
	}

	if (MontageToPlay)
	{
		CharacterOwner->PlayAnimMontage(MontageToPlay, GetClimbAnimPlayRateMultiplier());
	}
}

void UThreadTalesTraversalComponent::HopUp(const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	if (CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning())
	{
		return;
	}

	ClimbingSubState = EThreadTalesClimbingSubState::JumpTransition;
	StopAllClimbingMontages();

	UAnimMontage* Montage = GetJumpMontage(EThreadTalesLedgeJumpDirection::Up, EThreadTalesLedgeJumpDistanceType::Short);
	Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal, -1.0f, EThreadTalesLedgeJumpDistanceType::Short);

	if (CharacterOwner && !CharacterOwner->HasAuthority())
	{
		Server_HopUp(TargetLedgeLoc, TargetWallNormal);
	}
	else if (CharacterOwner)
	{
		Multicast_HopUp(TargetLedgeLoc, TargetWallNormal);
	}
}

void UThreadTalesTraversalComponent::SideJump(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	if (CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning())
	{
		return;
	}

	ClimbingSubState = EThreadTalesClimbingSubState::JumpTransition;
	StopAllClimbingMontages();

	const EThreadTalesLedgeJumpDirection Dir = (DirectionSign < 0.0f) ? EThreadTalesLedgeJumpDirection::Left : EThreadTalesLedgeJumpDirection::Right;
	UAnimMontage* Montage = GetJumpMontage(Dir, EThreadTalesLedgeJumpDistanceType::Short);
	Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal, -1.0f, EThreadTalesLedgeJumpDistanceType::Short);

	if (CharacterOwner && !CharacterOwner->HasAuthority())
	{
		Server_SideJump(DirectionSign, TargetLedgeLoc, TargetWallNormal);
	}
	else if (CharacterOwner)
	{
		Multicast_SideJump(DirectionSign, TargetLedgeLoc, TargetWallNormal);
	}
}

void UThreadTalesTraversalComponent::PerformLedgeJump(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	if (CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning())
	{
		return;
	}

	ClimbingSubState = EThreadTalesClimbingSubState::JumpTransition;
	StopAllClimbingMontages();

	UAnimMontage* Montage = GetJumpMontage(Direction, DistanceType);
	Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal, -1.0f, DistanceType);

	if (CharacterOwner && !CharacterOwner->HasAuthority())
	{
		Server_LedgeJump(Direction, DistanceType, TargetLedgeLoc, TargetWallNormal);
	}
	else if (CharacterOwner)
	{
		Multicast_LedgeJump(Direction, DistanceType, TargetLedgeLoc, TargetWallNormal);
	}
}

void UThreadTalesTraversalComponent::StartShimmy(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	if (CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning())
	{
		return;
	}

	ClimbingSubState = EThreadTalesClimbingSubState::Shimmying;
	StopAllClimbingMontages();

	UAnimMontage* Montage = GetActiveShimmyMontage(DirectionSign < 0.0f);
	Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal, -1.0f, EThreadTalesLedgeJumpDistanceType::Short);

	if (CharacterOwner && !CharacterOwner->HasAuthority())
	{
		Server_StartShimmy(DirectionSign, TargetLedgeLoc, TargetWallNormal);
	}
	else if (CharacterOwner)
	{
		Multicast_StartShimmy(DirectionSign, TargetLedgeLoc, TargetWallNormal);
	}
}

void UThreadTalesTraversalComponent::ClimbUp()
{
	if (CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning() || !CharacterOwner)
	{
		return;
	}

	FVector TargetStandingLoc;
	if (!CanClimbUp(TargetStandingLoc))
	{
		return;
	}

	ClimbingSubState = EThreadTalesClimbingSubState::MantleUp;
	UAnimMontage* MontageToUse = GetActiveClimbOnTopMontage(bIsFreehang);
	StopAllClimbingMontages();

	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Flying);
	}

	if (!CharacterOwner->HasAuthority())
	{
		Server_ClimbUp();
	}
	else
	{
		Multicast_PlayClimbMontage(MontageToUse);
	}

	if (MontageToUse)
	{
		const float Duration = CharacterOwner->PlayAnimMontage(MontageToUse, GetClimbAnimPlayRateMultiplier());
		if (Duration > 0.0f)
		{
			if (UAnimInstance* AnimInst = CharacterOwner->GetMesh() ? CharacterOwner->GetMesh()->GetAnimInstance() : nullptr)
			{
				FOnMontageBlendingOutStarted BlendingOutDelegate;
				BlendingOutDelegate.BindUObject(this, &UThreadTalesTraversalComponent::OnClimbMontageBlendingOut);
				AnimInst->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontageToUse);

				FOnMontageEnded EndDelegate;
				EndDelegate.BindUObject(this, &UThreadTalesTraversalComponent::OnClimbMontageEnded);
				AnimInst->Montage_SetEndDelegate(EndDelegate, MontageToUse);
			}
		}
		else
		{
			OnClimbMontageEnded(MontageToUse, false);
		}
	}
	else
	{
		const FVector TopLocation = CachedLedgeLocation + FVector(0.0f, 0.0f, 90.0f) + (-CachedWallNormal * 40.0f);
		CharacterOwner->SetActorLocation(TopLocation, false, nullptr, ETeleportType::TeleportPhysics);
		OnClimbMontageEnded(nullptr, false);
	}
}

void UThreadTalesTraversalComponent::OnClimbMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (ClimbingSubState == EThreadTalesClimbingSubState::MantleUp && CharacterOwner)
	{
		if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
		{
			MoveComp->Velocity = FVector::ZeroVector;
			MoveComp->SetMovementMode(MOVE_Walking);
		}
		ClimbingSubState = EThreadTalesClimbingSubState::None;
		SetTraversalState(EThreadTalesTraversalState::None);
		RegrabCooldownTimer = GetRegrabCooldownDuration();
	}
}

void UThreadTalesTraversalComponent::OnClimbMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (ClimbingSubState == EThreadTalesClimbingSubState::MantleUp)
	{
		ClimbingSubState = EThreadTalesClimbingSubState::None;
		SetTraversalState(EThreadTalesTraversalState::None);
		RegrabCooldownTimer = GetRegrabCooldownDuration();
	}

	if (CharacterOwner && CurrentTraversalState == EThreadTalesTraversalState::None && ClimbingSubState != EThreadTalesClimbingSubState::ClimbDown)
	{
		if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
		{
			if (MoveComp->MovementMode != MOVE_Walking)
			{
				MoveComp->Velocity = FVector::ZeroVector;
				MoveComp->SetMovementMode(MOVE_Walking);
			}
		}
	}
}

// ==========================================
// CLIMB DOWN
// ==========================================

bool UThreadTalesTraversalComponent::TryClimbDown()
{
	if (!CharacterOwner || CurrentTraversalState != EThreadTalesTraversalState::None || ClimbingSubState != EThreadTalesClimbingSubState::None)
	{
		return false;
	}

	UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
	if (!MoveComp || !MoveComp->IsMovingOnGround())
	{
		return false;
	}

	FVector LedgeLoc = FVector::ZeroVector;
	FVector WallNormal = FVector::ZeroVector;
	bool bFreehang = false;

	if (FThreadTalesLedgeDetector::FindEdgeForClimbDown(CharacterOwner, ClimbingConfig, bDrawDebugTraces, LedgeLoc, WallNormal, bFreehang))
	{
		StartClimbDown(LedgeLoc, WallNormal, bFreehang);
		return true;
	}

	return false;
}

void UThreadTalesTraversalComponent::StartClimbDown(const FVector& LedgeLoc, const FVector& WallNormal2D, bool bFreehang)
{
	if (!CharacterOwner)
	{
		return;
	}

	bIsFreehang = bFreehang;
	ClimbingSubState = EThreadTalesClimbingSubState::ClimbDown;
	CachedLedgeLocation = LedgeLoc;
	CachedWallNormal = WallNormal2D;

	const FRotator FaceWallRotation = (-WallNormal2D).Rotation();
	CachedHangRotation = FRotator(0.0f, FaceWallRotation.Yaw, 0.0f);
	CachedHangLocation = LedgeLoc + (WallNormal2D * GetHangWallOffset()) - FVector(0.0f, 0.0f, GetHangHeightOffset());

	SetTraversalState(EThreadTalesTraversalState::LedgeClimbing);

	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->SetMovementMode(MOVE_Flying);
	}

	SetClimbDownCollision(true);

	FVector ClimbDownStartLocation;
	FRotator ClimbDownStartRotation;
	FThreadTalesClimbingKinematics::CalculateClimbDownPreAlignment(LedgeLoc, WallNormal2D, ClimbingConfig, ClimbDownStartLocation, ClimbDownStartRotation);
	CharacterOwner->SetActorLocationAndRotation(ClimbDownStartLocation, ClimbDownStartRotation, false, nullptr, ETeleportType::TeleportPhysics);

	UAnimMontage* MontageToPlay = GetActiveClimbDownMontage(bFreehang);
	if (MontageToPlay)
	{
		const float Duration = CharacterOwner->PlayAnimMontage(MontageToPlay, GetClimbAnimPlayRateMultiplier());
		if (Duration > 0.0f)
		{
			if (UAnimInstance* AnimInst = CharacterOwner->GetMesh() ? CharacterOwner->GetMesh()->GetAnimInstance() : nullptr)
			{
				FOnMontageBlendingOutStarted BlendingOutDelegate;
				BlendingOutDelegate.BindUObject(this, &UThreadTalesTraversalComponent::OnClimbDownMontageBlendingOut);
				AnimInst->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontageToPlay);

				FOnMontageEnded EndDelegate;
				EndDelegate.BindUObject(this, &UThreadTalesTraversalComponent::OnClimbDownMontageEnded);
				AnimInst->Montage_SetEndDelegate(EndDelegate, MontageToPlay);
			}
		}
		else
		{
			OnClimbDownMontageEnded(MontageToPlay, false);
		}
	}
	else
	{
		OnClimbDownMontageEnded(nullptr, false);
	}

	if (!CharacterOwner->HasAuthority())
	{
		Server_ClimbDown(LedgeLoc, WallNormal2D, bFreehang);
	}
	else
	{
		Multicast_ClimbDown(LedgeLoc, WallNormal2D, bFreehang);
	}
}

void UThreadTalesTraversalComponent::SetClimbDownCollision(bool bEnable)
{
	if (!CharacterOwner)
	{
		return;
	}

	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	if (!Capsule)
	{
		return;
	}

	if (bEnable)
	{
		Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
	}
	else
	{
		Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	}
}

void UThreadTalesTraversalComponent::OnClimbDownMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (ClimbingSubState == EThreadTalesClimbingSubState::ClimbDown)
	{
		SetClimbDownCollision(false);
		CrossfadeToHangMontage();
	}
}

void UThreadTalesTraversalComponent::OnClimbDownMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (ClimbingSubState == EThreadTalesClimbingSubState::ClimbDown)
	{
		SetClimbDownCollision(false);
		if (CharacterOwner && !bInterrupted)
		{
			CharacterOwner->SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation, false, nullptr, ETeleportType::TeleportPhysics);
		}
		ClimbingSubState = EThreadTalesClimbingSubState::IdleHang;
		CrossfadeToHangMontage();
	}
}

// ==========================================
// WALL EJECT & BACK JUMP
// ==========================================

void UThreadTalesTraversalComponent::PerformBackJump()
{
	if (CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning() || !CharacterOwner)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Up = FVector::UpVector;
	const FVector WallRight = FVector::CrossProduct(CachedWallNormal, Up).GetSafeNormal();
	const FVector BackDir = CachedWallNormal;

	const float SectorHalfAngle = ClimbingConfig ? ClimbingConfig->WallEjectSectorHalfAngle : 45.0f;
	const int32 SectorRayCount = ClimbingConfig ? ClimbingConfig->WallEjectSectorRayCount : 7;
	const float MinOppHeight = ClimbingConfig ? ClimbingConfig->WallEjectMinOpposingHeight : -100.0f;
	const float MaxOppHeight = ClimbingConfig ? ClimbingConfig->WallEjectMaxOpposingHeight : 150.0f;
	const float OppDist = ClimbingConfig ? ClimbingConfig->BackJumpOpposingLedgeDistance : 450.0f;
	const float NormalTol = ClimbingConfig ? ClimbingConfig->WallEjectNormalToleranceDot : -0.7f;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(CharacterOwner);
	QueryParams.bTraceComplex = false;

	bool bFoundOpposingLedge = false;
	FVector OpposingLedgeLoc = FVector::ZeroVector;
	FVector OpposingWallNormal = FVector::ZeroVector;

	const float AngleStep = (SectorRayCount > 1) ? ((SectorHalfAngle * 2.0f) / (SectorRayCount - 1)) : 0.0f;
	const float HeightOffsets[] = { 0.0f, MinOppHeight, MaxOppHeight, (MinOppHeight + MaxOppHeight) * 0.5f };

	for (float HeightOff : HeightOffsets)
	{
		for (int32 i = 0; i < SectorRayCount; ++i)
		{
			const float Angle = -SectorHalfAngle + (i * AngleStep);
			const FVector RayDir = BackDir.RotateAngleAxis(Angle, Up);

			const FVector RayStart = CachedLedgeLocation + (BackDir * 40.0f) + Up * HeightOff;
			const FVector RayEnd = RayStart + (RayDir * OppDist);

			FHitResult OpposingWallHit;
			if (World->LineTraceSingleByChannel(OpposingWallHit, RayStart, RayEnd, ECC_Visibility, QueryParams))
			{
				const FVector OppNormal2D = FVector(OpposingWallHit.ImpactNormal.X, OpposingWallHit.ImpactNormal.Y, 0.0f).GetSafeNormal();
				if (FVector::DotProduct(OppNormal2D, CachedWallNormal) < NormalTol)
				{
					const FVector TopTraceStart = OpposingWallHit.ImpactPoint - (OppNormal2D * 15.0f) + Up * 60.0f;
					const FVector TopTraceEnd = OpposingWallHit.ImpactPoint - (OppNormal2D * 15.0f) - Up * 40.0f;

					FHitResult OpposingTopHit;
					if (World->LineTraceSingleByChannel(OpposingTopHit, TopTraceStart, TopTraceEnd, ECC_Visibility, QueryParams))
					{
						if (OpposingTopHit.ImpactNormal.Z >= 0.55f)
						{
							const float LedgeTopZ = OpposingTopHit.ImpactPoint.Z;
							const FVector CandidateOpposingLedge = FVector(OpposingWallHit.ImpactPoint.X, OpposingWallHit.ImpactPoint.Y, LedgeTopZ);

							if (ValidateLedgeLipAndClearance(CandidateOpposingLedge, OppNormal2D))
							{
								bFoundOpposingLedge = true;
								OpposingLedgeLoc = CandidateOpposingLedge;
								OpposingWallNormal = OppNormal2D;
								break;
							}
						}
					}
				}
			}
		}
		if (bFoundOpposingLedge)
		{
			break;
		}
	}

	if (bFoundOpposingLedge)
	{
		UAnimMontage* LeapMontage = GetActiveBackJumpMontage();
		if (!LeapMontage)
		{
			LeapMontage = GetJumpMontage(EThreadTalesLedgeJumpDirection::Down, EThreadTalesLedgeJumpDistanceType::Leap);
		}
		const float BackJumpDuration = LeapMontage ? (LeapMontage->GetPlayLength() / GetClimbBackJumpPlayRate()) : 1.5f;
		Internal_StartTransition(LeapMontage, OpposingLedgeLoc, OpposingWallNormal, BackJumpDuration, EThreadTalesLedgeJumpDistanceType::Leap);
		return;
	}

	WallEject();
}

void UThreadTalesTraversalComponent::WallEject()
{
	if (CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing || !CharacterOwner)
	{
		return;
	}

	StopAllClimbingMontages();
	ClimbingSubState = EThreadTalesClimbingSubState::None;
	bIsFreehang = false;

	if (!CharacterOwner->HasAuthority())
	{
		Server_WallEject();
	}
	else
	{
		Multicast_WallEject();
	}

	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Falling);
		MoveComp->Velocity = FThreadTalesClimbingKinematics::CalculateWallEjectVelocity(CachedWallNormal, ClimbingConfig);
	}

	SetTraversalState(EThreadTalesTraversalState::None);
	RegrabCooldownTimer = GetWallEjectRegrabCooldown();
}

// ==========================================
// INPUT PROCESSING
// ==========================================

void UThreadTalesTraversalComponent::HandleLedgeJumpInput(bool bFromSpace)
{
	if (CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning() || !CharacterOwner)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	if (!PC)
	{
		return;
	}

	if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Down))
	{
		PerformBackJump();
		return;
	}

	FVector TargetStandingLoc;
	if (CanClimbUp(TargetStandingLoc))
	{
		ClimbUp();
		return;
	}

	FVector HigherLedgeLoc;
	FVector HigherWallNormal;
	if (FindUpperLedge(HigherLedgeLoc, HigherWallNormal))
	{
		HopUp(HigherLedgeLoc, HigherWallNormal);
	}
}

void UThreadTalesTraversalComponent::ProcessClimbingInput(float DeltaTime)
{
	if (ClimbingInputDebounceTimer > 0.0f || CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing || IsTransitioning() || !CharacterOwner)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	if (!PC)
	{
		return;
	}

	const bool bW = PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Up);
	const bool bS = PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Down);
	const bool bA = PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Left);
	const bool bD = PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Gamepad_LeftStick_Right);
	const bool bIsSprint = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::Gamepad_LeftThumbstick);

	if (!bW && !bS && !bA && !bD)
	{
		return;
	}

	EThreadTalesLedgeJumpDirection JumpDir = EThreadTalesLedgeJumpDirection::None;
	if (bW && bD) JumpDir = EThreadTalesLedgeJumpDirection::UpRight;
	else if (bW && bA) JumpDir = EThreadTalesLedgeJumpDirection::UpLeft;
	else if (bS && bD) JumpDir = EThreadTalesLedgeJumpDirection::DownRight;
	else if (bS && bA) JumpDir = EThreadTalesLedgeJumpDirection::DownLeft;
	else if (bW) JumpDir = EThreadTalesLedgeJumpDirection::Up;
	else if (bS) JumpDir = EThreadTalesLedgeJumpDirection::Down;
	else if (bD) JumpDir = EThreadTalesLedgeJumpDirection::Right;
	else if (bA) JumpDir = EThreadTalesLedgeJumpDirection::Left;

	const TArray<EThreadTalesLedgeJumpDistanceType> Priorities = GetJumpDistancePriorities(bIsSprint);

	// Case A: Up
	if (JumpDir == EThreadTalesLedgeJumpDirection::Up)
	{
		if (bIsSprint)
		{
			for (EThreadTalesLedgeJumpDistanceType DistType : { EThreadTalesLedgeJumpDistanceType::Long, EThreadTalesLedgeJumpDistanceType::Leap })
			{
				FVector TargetLedgeLoc;
				FVector TargetWallNormal;
				if (FindLedgeInDirection(EThreadTalesLedgeJumpDirection::Up, DistType, TargetLedgeLoc, TargetWallNormal))
				{
					ClimbingInputDebounceTimer = GetJumpInputDebounceDuration();
					PerformLedgeJump(EThreadTalesLedgeJumpDirection::Up, DistType, TargetLedgeLoc, TargetWallNormal);
					return;
				}
			}
		}
		else
		{
			for (EThreadTalesLedgeJumpDistanceType DistType : { EThreadTalesLedgeJumpDistanceType::Short, EThreadTalesLedgeJumpDistanceType::Long, EThreadTalesLedgeJumpDistanceType::Leap })
			{
				FVector TargetLedgeLoc;
				FVector TargetWallNormal;
				if (FindLedgeInDirection(EThreadTalesLedgeJumpDirection::Up, DistType, TargetLedgeLoc, TargetWallNormal))
				{
					ClimbingInputDebounceTimer = GetJumpInputDebounceDuration();
					PerformLedgeJump(EThreadTalesLedgeJumpDirection::Up, DistType, TargetLedgeLoc, TargetWallNormal);
					return;
				}
			}

			FVector TargetStandingLoc;
			if (CanClimbUp(TargetStandingLoc))
			{
				ClimbingInputDebounceTimer = 0.4f;
				ClimbUp();
				return;
			}
		}

		ClimbingInputDebounceTimer = 0.2f;
		return;
	}

	// Case B: Lateral
	if (JumpDir == EThreadTalesLedgeJumpDirection::Right || JumpDir == EThreadTalesLedgeJumpDirection::Left)
	{
		const float SideSign = (JumpDir == EThreadTalesLedgeJumpDirection::Right) ? 1.0f : -1.0f;

		if (!bIsSprint && ShimmyCooldownTimer <= 0.0f)
		{
			FVector SideLedgeLoc;
			FVector SideWallNormal;
			bool bIsContinuous = false;
			if (FindSideLedge(SideSign, SideLedgeLoc, SideWallNormal, bIsContinuous, false) && bIsContinuous)
			{
				ClimbingInputDebounceTimer = GetShimmyInputDebounceDuration();
				StartShimmy(SideSign, SideLedgeLoc, SideWallNormal);
				return;
			}
		}

		for (EThreadTalesLedgeJumpDistanceType DistType : Priorities)
		{
			FVector TargetLedgeLoc;
			FVector TargetWallNormal;
			if (FindLedgeInDirection(JumpDir, DistType, TargetLedgeLoc, TargetWallNormal))
			{
				ClimbingInputDebounceTimer = GetJumpInputDebounceDuration();
				PerformLedgeJump(JumpDir, DistType, TargetLedgeLoc, TargetWallNormal);
				return;
			}
		}

		if (bIsSprint && ShimmyCooldownTimer <= 0.0f)
		{
			FVector SideLedgeLoc;
			FVector SideWallNormal;
			bool bIsContinuous = false;
			if (FindSideLedge(SideSign, SideLedgeLoc, SideWallNormal, bIsContinuous, false) && bIsContinuous)
			{
				ClimbingInputDebounceTimer = GetShimmyInputDebounceDuration();
				StartShimmy(SideSign, SideLedgeLoc, SideWallNormal);
				return;
			}
		}
		return;
	}

	// Case C: Diagonals
	if (JumpDir == EThreadTalesLedgeJumpDirection::UpRight || JumpDir == EThreadTalesLedgeJumpDirection::UpLeft ||
		JumpDir == EThreadTalesLedgeJumpDirection::DownRight || JumpDir == EThreadTalesLedgeJumpDirection::DownLeft)
	{
		for (EThreadTalesLedgeJumpDistanceType DistType : Priorities)
		{
			FVector TargetLedgeLoc;
			FVector TargetWallNormal;
			if (FindLedgeInDirection(JumpDir, DistType, TargetLedgeLoc, TargetWallNormal))
			{
				ClimbingInputDebounceTimer = GetJumpInputDebounceDuration();
				PerformLedgeJump(JumpDir, DistType, TargetLedgeLoc, TargetWallNormal);
				return;
			}
		}
		return;
	}

	// Case D: Down
	if (JumpDir == EThreadTalesLedgeJumpDirection::Down)
	{
		for (EThreadTalesLedgeJumpDistanceType DistType : Priorities)
		{
			FVector TargetLedgeLoc;
			FVector TargetWallNormal;
			if (FindLedgeInDirection(EThreadTalesLedgeJumpDirection::Down, DistType, TargetLedgeLoc, TargetWallNormal))
			{
				ClimbingInputDebounceTimer = GetJumpInputDebounceDuration();
				PerformLedgeJump(EThreadTalesLedgeJumpDirection::Down, DistType, TargetLedgeLoc, TargetWallNormal);
				return;
			}
		}
		return;
	}
}

// ==========================================
// NETWORK RPC IMPLEMENTATIONS
// ==========================================

void UThreadTalesTraversalComponent::Server_StartLedgeClimb_Implementation(const FVector& LedgeLocation, const FVector& WallNormal)
{
	Multicast_StartLedgeClimb(LedgeLocation, WallNormal);
}

void UThreadTalesTraversalComponent::Multicast_StartLedgeClimb_Implementation(const FVector& LedgeLocation, const FVector& WallNormal)
{
	if (!CharacterOwner || CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	bIsFreehang = CheckIsFreehangLedge(LedgeLocation, WallNormal);
	ClimbingSubState = EThreadTalesClimbingSubState::IdleHang;
	CachedLedgeLocation = LedgeLocation;
	CachedWallNormal = WallNormal;

	const FRotator FaceWallRotation = (-WallNormal).Rotation();
	CachedHangRotation = FRotator(0.0f, FaceWallRotation.Yaw, 0.0f);
	CachedHangLocation = LedgeLocation + (WallNormal * GetHangWallOffset()) - FVector(0.0f, 0.0f, GetHangHeightOffset());

	SetTraversalState(EThreadTalesTraversalState::LedgeClimbing);

	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->SetMovementMode(MOVE_Flying);
	}

	CharacterOwner->SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation, false, nullptr, ETeleportType::TeleportPhysics);
	CrossfadeToHangMontage();
}

void UThreadTalesTraversalComponent::Server_ReleaseLedge_Implementation()
{
	Multicast_ReleaseLedge();
}

void UThreadTalesTraversalComponent::Multicast_ReleaseLedge_Implementation()
{
	if (!CharacterOwner || CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	StopAllClimbingMontages();
	if (ClimbingSubState == EThreadTalesClimbingSubState::ClimbDown)
	{
		SetClimbDownCollision(false);
	}
	ClimbingSubState = EThreadTalesClimbingSubState::None;
	bIsFreehang = false;

	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Falling);
	}

	SetTraversalState(EThreadTalesTraversalState::None);
	RegrabCooldownTimer = GetRegrabCooldownDuration();
}

void UThreadTalesTraversalComponent::Server_HopUp_Implementation(const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	Multicast_HopUp(TargetLedgeLoc, TargetWallNormal);
}

void UThreadTalesTraversalComponent::Multicast_HopUp_Implementation(const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	if (!CharacterOwner || CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	StopAllClimbingMontages();
	UAnimMontage* Montage = GetJumpMontage(EThreadTalesLedgeJumpDirection::Up, EThreadTalesLedgeJumpDistanceType::Short);
	Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal);
}

void UThreadTalesTraversalComponent::Server_SideJump_Implementation(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	Multicast_SideJump(DirectionSign, TargetLedgeLoc, TargetWallNormal);
}

void UThreadTalesTraversalComponent::Multicast_SideJump_Implementation(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	if (!CharacterOwner || CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	StopAllClimbingMontages();
	const EThreadTalesLedgeJumpDirection Dir = (DirectionSign < 0.0f) ? EThreadTalesLedgeJumpDirection::Left : EThreadTalesLedgeJumpDirection::Right;
	UAnimMontage* Montage = GetJumpMontage(Dir, EThreadTalesLedgeJumpDistanceType::Short);
	Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal);
}

void UThreadTalesTraversalComponent::Server_LedgeJump_Implementation(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	Multicast_LedgeJump(Direction, DistanceType, TargetLedgeLoc, TargetWallNormal);
}

void UThreadTalesTraversalComponent::Multicast_LedgeJump_Implementation(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	if (!CharacterOwner || CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	StopAllClimbingMontages();
	UAnimMontage* Montage = GetJumpMontage(Direction, DistanceType);
	Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal, -1.0f, DistanceType);
}

void UThreadTalesTraversalComponent::Server_StartShimmy_Implementation(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	Multicast_StartShimmy(DirectionSign, TargetLedgeLoc, TargetWallNormal);
}

void UThreadTalesTraversalComponent::Multicast_StartShimmy_Implementation(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	if (!CharacterOwner || CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	StopAllClimbingMontages();
	UAnimMontage* Montage = GetActiveShimmyMontage(DirectionSign < 0.0f);
	Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal);
}

void UThreadTalesTraversalComponent::Server_ClimbUp_Implementation()
{
	UAnimMontage* MontageToPlay = GetActiveClimbOnTopMontage(bIsFreehang);
	Multicast_PlayClimbMontage(MontageToPlay);
}

void UThreadTalesTraversalComponent::Multicast_PlayClimbMontage_Implementation(UAnimMontage* MontageToPlay)
{
	if (!CharacterOwner || CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	ClimbingSubState = EThreadTalesClimbingSubState::MantleUp;
	StopAllClimbingMontages();

	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Flying);
	}

	if (MontageToPlay)
	{
		const float Duration = CharacterOwner->PlayAnimMontage(MontageToPlay, GetClimbAnimPlayRateMultiplier());
		if (Duration > 0.0f)
		{
			if (UAnimInstance* AnimInst = CharacterOwner->GetMesh() ? CharacterOwner->GetMesh()->GetAnimInstance() : nullptr)
			{
				FOnMontageBlendingOutStarted BlendingOutDelegate;
				BlendingOutDelegate.BindUObject(this, &UThreadTalesTraversalComponent::OnClimbMontageBlendingOut);
				AnimInst->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontageToPlay);

				FOnMontageEnded EndDelegate;
				EndDelegate.BindUObject(this, &UThreadTalesTraversalComponent::OnClimbMontageEnded);
				AnimInst->Montage_SetEndDelegate(EndDelegate, MontageToPlay);
			}
		}
		else
		{
			OnClimbMontageEnded(MontageToPlay, false);
		}
	}
}

void UThreadTalesTraversalComponent::Server_ClimbDown_Implementation(const FVector& LedgeLocation, const FVector& WallNormal, bool bFreehang)
{
	Multicast_ClimbDown(LedgeLocation, WallNormal, bFreehang);
}

void UThreadTalesTraversalComponent::Multicast_ClimbDown_Implementation(const FVector& LedgeLocation, const FVector& WallNormal, bool bFreehang)
{
	if (!CharacterOwner || CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	bIsFreehang = bFreehang;
	ClimbingSubState = EThreadTalesClimbingSubState::ClimbDown;
	CachedLedgeLocation = LedgeLocation;
	CachedWallNormal = WallNormal;

	const FRotator FaceWallRotation = (-WallNormal).Rotation();
	CachedHangRotation = FRotator(0.0f, FaceWallRotation.Yaw, 0.0f);
	CachedHangLocation = LedgeLocation + (WallNormal * GetHangWallOffset()) - FVector(0.0f, 0.0f, GetHangHeightOffset());

	SetTraversalState(EThreadTalesTraversalState::LedgeClimbing);

	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->SetMovementMode(MOVE_Flying);
	}

	SetClimbDownCollision(true);

	FVector ClimbDownStartLocation;
	FRotator ClimbDownStartRotation;
	FThreadTalesClimbingKinematics::CalculateClimbDownPreAlignment(LedgeLocation, WallNormal, ClimbingConfig, ClimbDownStartLocation, ClimbDownStartRotation);
	CharacterOwner->SetActorLocationAndRotation(ClimbDownStartLocation, ClimbDownStartRotation, false, nullptr, ETeleportType::TeleportPhysics);

	UAnimMontage* MontageToPlay = GetActiveClimbDownMontage(bFreehang);
	if (MontageToPlay)
	{
		CharacterOwner->PlayAnimMontage(MontageToPlay, GetClimbAnimPlayRateMultiplier());
	}
}

void UThreadTalesTraversalComponent::Server_WallEject_Implementation()
{
	Multicast_WallEject();
}

void UThreadTalesTraversalComponent::Multicast_WallEject_Implementation()
{
	if (!CharacterOwner || CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	StopAllClimbingMontages();
	ClimbingSubState = EThreadTalesClimbingSubState::None;
	bIsFreehang = false;

	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Falling);
		MoveComp->Velocity = FThreadTalesClimbingKinematics::CalculateWallEjectVelocity(CachedWallNormal, ClimbingConfig);
	}

	SetTraversalState(EThreadTalesTraversalState::None);
	RegrabCooldownTimer = GetWallEjectRegrabCooldown();
}
