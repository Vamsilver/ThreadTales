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
	CurrentClimbingSubState = EThreadTalesClimbingSubState::None;
	bIsFreehang = false;
	bDrawDebugTraces = false;
}

void UThreadTalesTraversalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UThreadTalesTraversalComponent, CurrentTraversalState);
	DOREPLIFETIME(UThreadTalesTraversalComponent, CurrentClimbingSubState);
	DOREPLIFETIME(UThreadTalesTraversalComponent, bIsFreehang);
}

void UThreadTalesTraversalComponent::BeginPlay()
{
	Super::BeginPlay();

	CharacterOwner = Cast<AThreadTalesCharacter>(GetOwner());
	LedgeHandler.Initialize(ClimbingConfig, GetActiveHangMontage(false));
}

void UThreadTalesTraversalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CharacterOwner)
	{
		return;
	}

	// Always tick cooldown and debounce timers regardless of current state
	LedgeHandler.TickTimers(DeltaTime);

	// 1. Auto-detect ledges when in locomotion / airborne
	if (CurrentTraversalState == EThreadTalesTraversalState::None && LedgeHandler.CanAttemptGrab(ClimbingConfig))
	{
		if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
		{
			const float MaxGrabVelZ = ClimbingConfig ? ClimbingConfig->MaxGrabVelocityZ : 400.0f;
			if (MoveComp->IsFalling() && MoveComp->Velocity.Z <= MaxGrabVelZ)
			{
				if (CharacterOwner->IsLocallyControlled() || CharacterOwner->HasAuthority())
				{
					FVector DetectedLedgeLocation;
					FVector DetectedWallNormal;
					if (DetectLedge(DetectedLedgeLocation, DetectedWallNormal))
					{
						StartLedgeClimb(DetectedLedgeLocation, DetectedWallNormal);
						return;
					}
				}
			}
			else if (bDrawDebugTraces && CharacterOwner->IsLocallyControlled())
			{
				// Keep debug visualization active while walking near walls in debug mode
				FVector DebugLedgeLoc;
				FVector DebugWallNorm;
				DetectLedge(DebugLedgeLoc, DebugWallNorm);
			}
		}
	}

	// 2. Dispatch tick to the active traversal mode handler
	switch (CurrentTraversalState)
	{
	case EThreadTalesTraversalState::LedgeClimbing:
		LedgeHandler.Tick(DeltaTime, this, CharacterOwner);
		break;
	case EThreadTalesTraversalState::Ladder:
		// Reserved for Future Milestone: LadderHandler.Tick(DeltaTime, this, CharacterOwner);
		break;
	case EThreadTalesTraversalState::Rope:
		// Reserved for Future Milestone: RopeHandler.Tick(DeltaTime, this, CharacterOwner);
		break;
	default:
		break;
	}
}

// ==========================================
// WORLD SENSING DELEGATION
// ==========================================

bool UThreadTalesTraversalComponent::DetectLedge(FVector& OutLedgeLocation, FVector& OutWallNormal)
{
	return FThreadTalesLedgeDetector::DetectLedge(CharacterOwner, ClimbingConfig, bDrawDebugTraces, OutLedgeLocation, OutWallNormal);
}

bool UThreadTalesTraversalComponent::FindUpperLedge(FVector& OutLedgeLoc, FVector& OutWallNormal)
{
	return FThreadTalesLedgeDetector::FindUpperLedge(CharacterOwner, ClimbingConfig, bDrawDebugTraces, LedgeHandler.GetCachedLedgeLocation(), LedgeHandler.GetCachedWallNormal(), OutLedgeLoc, OutWallNormal);
}

bool UThreadTalesTraversalComponent::FindSideLedge(float DirectionSign, FVector& OutLedgeLoc, FVector& OutWallNormal, bool& bOutIsContinuous, bool bForJump)
{
	return FThreadTalesLedgeDetector::FindSideLedge(CharacterOwner, ClimbingConfig, bDrawDebugTraces, LedgeHandler.GetCachedLedgeLocation(), LedgeHandler.GetCachedWallNormal(), DirectionSign, bForJump, OutLedgeLoc, OutWallNormal, bOutIsContinuous);
}

bool UThreadTalesTraversalComponent::FindLedgeInDirection(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, FVector& OutLedgeLoc, FVector& OutWallNormal)
{
	return FThreadTalesLedgeDetector::FindLedgeInDirection(CharacterOwner, ClimbingConfig, bDrawDebugTraces, LedgeHandler.GetCachedLedgeLocation(), LedgeHandler.GetCachedWallNormal(), Direction, DistanceType, LedgeHandler.GetJumpConfigTable(), OutLedgeLoc, OutWallNormal);
}

bool UThreadTalesTraversalComponent::CanClimbUp(FVector& OutStandingLoc) const
{
	return FThreadTalesLedgeDetector::CanClimbUp(CharacterOwner, ClimbingConfig, bDrawDebugTraces, LedgeHandler.GetCachedLedgeLocation(), LedgeHandler.GetCachedWallNormal(), OutStandingLoc);
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
// ACTIONS DELEGATION TO LEDGE HANDLER
// ==========================================

void UThreadTalesTraversalComponent::StartLedgeClimb(const FVector& LedgeLocation, const FVector& WallNormal)
{
	LedgeHandler.StartLedgeClimb(LedgeLocation, WallNormal, this, CharacterOwner);
}

void UThreadTalesTraversalComponent::ReleaseLedge()
{
	LedgeHandler.ReleaseLedge(this, CharacterOwner);
}

void UThreadTalesTraversalComponent::ProcessClimbingInput(float DeltaTime)
{
	if (!CharacterOwner || CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	LedgeHandler.ProcessInput(PC, this, CharacterOwner);
}

void UThreadTalesTraversalComponent::HandleLedgeJumpInput(bool bFromSpace)
{
	if (!CharacterOwner || CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	LedgeHandler.HandleJumpInput(PC, this, CharacterOwner);
}

void UThreadTalesTraversalComponent::HopUp(const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	LedgeHandler.HopUp(TargetLedgeLoc, TargetWallNormal, this, CharacterOwner);
}

void UThreadTalesTraversalComponent::SideJump(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	LedgeHandler.SideJump(DirectionSign, TargetLedgeLoc, TargetWallNormal, this, CharacterOwner);
}

void UThreadTalesTraversalComponent::StartShimmy(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	LedgeHandler.StartShimmy(DirectionSign, TargetLedgeLoc, TargetWallNormal, this, CharacterOwner);
}

void UThreadTalesTraversalComponent::ClimbUp()
{
	LedgeHandler.ClimbUp(this, CharacterOwner);
}

bool UThreadTalesTraversalComponent::TryClimbDown()
{
	return LedgeHandler.TryClimbDown(this, CharacterOwner);
}

void UThreadTalesTraversalComponent::PerformBackJump()
{
	LedgeHandler.PerformBackJump(this, CharacterOwner);
}

void UThreadTalesTraversalComponent::WallEject()
{
	LedgeHandler.WallEject(this, CharacterOwner);
}

void UThreadTalesTraversalComponent::PerformLedgeJump(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal)
{
	LedgeHandler.PerformLedgeJump(Direction, DistanceType, TargetLedgeLoc, TargetWallNormal, this, CharacterOwner);
}

// ==========================================
// JUMP TABLE & KINEMATICS QUERIES
// ==========================================

const FThreadTalesLedgeJumpConfig& UThreadTalesTraversalComponent::GetJumpConfig(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const
{
	return LedgeHandler.GetJumpConfig(Direction, DistanceType);
}

bool UThreadTalesTraversalComponent::GetJumpDistanceBounds(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, float& OutMinDist, float& OutMaxDist) const
{
	return LedgeHandler.GetJumpDistanceBounds(Direction, DistanceType, OutMinDist, OutMaxDist);
}

bool UThreadTalesTraversalComponent::IsLedgeDistanceValid(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, float Distance) const
{
	return LedgeHandler.IsLedgeDistanceValid(Direction, DistanceType, Distance);
}

float UThreadTalesTraversalComponent::GetNominalJumpDistance(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType) const
{
	return LedgeHandler.GetNominalJumpDistance(Direction, DistanceType);
}

UAnimMontage* UThreadTalesTraversalComponent::GetJumpMontage(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, bool bFreehang) const
{
	if (bFreehang && ClimbingConfig)
	{
		if (DistanceType == EThreadTalesLedgeJumpDistanceType::Short)
		{
			switch (Direction)
			{
			case EThreadTalesLedgeJumpDirection::Up:
				if (ClimbingConfig->BarClimbHopUpMontage) return ClimbingConfig->BarClimbHopUpMontage.Get();
				break;
			case EThreadTalesLedgeJumpDirection::Down:
				if (ClimbingConfig->BarClimbHopDownMontage) return ClimbingConfig->BarClimbHopDownMontage.Get();
				break;
			case EThreadTalesLedgeJumpDirection::Left:
			case EThreadTalesLedgeJumpDirection::DownLeft:
			case EThreadTalesLedgeJumpDirection::UpLeft:
				if (ClimbingConfig->BarClimbJumpLeftMontage) return ClimbingConfig->BarClimbJumpLeftMontage.Get();
				break;
			case EThreadTalesLedgeJumpDirection::Right:
			case EThreadTalesLedgeJumpDirection::DownRight:
			case EThreadTalesLedgeJumpDirection::UpRight:
				if (ClimbingConfig->BarClimbJumpRightMontage) return ClimbingConfig->BarClimbJumpRightMontage.Get();
				break;
			default:
				break;
			}
		}
	}

	return LedgeHandler.GetJumpMontage(Direction, DistanceType);
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
		return bFreehang ? ClimbingConfig->BarHangMontage.Get() : ClimbingConfig->HangMontage.Get();
	}
	return nullptr;
}

UAnimMontage* UThreadTalesTraversalComponent::GetActiveClimbOnTopMontage(bool bFreehang) const
{
	if (ClimbingConfig)
	{
		return bFreehang ? ClimbingConfig->BarClimbOnTopMontage.Get() : ClimbingConfig->ClimbOnTopMontage.Get();
	}
	return nullptr;
}

UAnimMontage* UThreadTalesTraversalComponent::GetActiveClimbDownMontage(bool bFreehang) const
{
	if (ClimbingConfig)
	{
		return bFreehang ? ClimbingConfig->BarClimbDownMontage.Get() : ClimbingConfig->ClimbDownMontage.Get();
	}
	return nullptr;
}

UAnimMontage* UThreadTalesTraversalComponent::GetActiveBackJumpMontage(bool bFreehang) const
{
	if (ClimbingConfig)
	{
		if (bFreehang && ClimbingConfig->BarClimbBackJumpMontage)
		{
			return ClimbingConfig->BarClimbBackJumpMontage.Get();
		}
		return ClimbingConfig->ClimbBackJumpMontage.Get();
	}
	return nullptr;
}

UAnimMontage* UThreadTalesTraversalComponent::GetActiveShimmyMontage(bool bLeft, bool bFreehang) const
{
	if (ClimbingConfig)
	{
		if (bFreehang)
		{
			UAnimMontage* BarMontage = bLeft ? ClimbingConfig->BarShimmyLeftMontage.Get() : ClimbingConfig->BarShimmyRightMontage.Get();
			if (BarMontage)
			{
				return BarMontage;
			}
		}
		return bLeft ? ClimbingConfig->ShimmyLeftMontage.Get() : ClimbingConfig->ShimmyRightMontage.Get();
	}
	return nullptr;
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
// STATE MANAGEMENT
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
			}
			else if (OldState == EThreadTalesTraversalState::LedgeClimbing)
			{
				ASC->RemoveLooseGameplayTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing);
				SetClimbingSubState(EThreadTalesClimbingSubState::None);
			}
		}
	}

	OnTraversalStateChanged.Broadcast(NewState, OldState);
}

void UThreadTalesTraversalComponent::SetClimbingSubState(EThreadTalesClimbingSubState NewSubState)
{
	LedgeHandler.SetClimbingSubState(NewSubState);

	if (CurrentClimbingSubState == NewSubState)
	{
		return;
	}

	const EThreadTalesClimbingSubState OldSubState = CurrentClimbingSubState;
	CurrentClimbingSubState = NewSubState;

	OnClimbingSubStateChanged.Broadcast(NewSubState, OldSubState);
}

void UThreadTalesTraversalComponent::OnRep_TraversalState(EThreadTalesTraversalState OldState)
{
	OnTraversalStateChanged.Broadcast(CurrentTraversalState, OldState);
}

void UThreadTalesTraversalComponent::OnRep_ClimbingSubState(EThreadTalesClimbingSubState OldSubState)
{
	LedgeHandler.SetClimbingSubState(CurrentClimbingSubState);
	OnClimbingSubStateChanged.Broadcast(CurrentClimbingSubState, OldSubState);
}

// ==========================================
// MONTAGE CALLBACKS
// ==========================================

void UThreadTalesTraversalComponent::OnClimbMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	LedgeHandler.OnClimbMontageEnded(Montage, bInterrupted, this, CharacterOwner);
}

void UThreadTalesTraversalComponent::OnClimbMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	LedgeHandler.OnClimbMontageBlendingOut(Montage, bInterrupted, this, CharacterOwner);
}

void UThreadTalesTraversalComponent::OnClimbDownMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	LedgeHandler.OnClimbDownMontageEnded(Montage, bInterrupted, this, CharacterOwner);
}

void UThreadTalesTraversalComponent::OnClimbDownMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	LedgeHandler.OnClimbDownMontageBlendingOut(Montage, bInterrupted, this, CharacterOwner);
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

	LedgeHandler.StartLedgeClimb(LedgeLocation, WallNormal, this, CharacterOwner);
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

	LedgeHandler.StopAllClimbingMontages(CharacterOwner);
	if (LedgeHandler.GetClimbingSubState() == EThreadTalesClimbingSubState::ClimbDown)
	{
		LedgeHandler.SetClimbDownCollision(false, CharacterOwner);
	}
	SetClimbingSubState(EThreadTalesClimbingSubState::None);
	bIsFreehang = false;
	LedgeHandler.SetIsFreehang(false);

	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Falling);
	}

	SetTraversalState(EThreadTalesTraversalState::None);
	LedgeHandler.SetRegrabCooldownTimer(GetRegrabCooldownDuration());
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

	LedgeHandler.StopAllClimbingMontages(CharacterOwner);
	SetClimbingSubState(EThreadTalesClimbingSubState::JumpTransition);
	UAnimMontage* Montage = GetJumpMontage(EThreadTalesLedgeJumpDirection::Up, EThreadTalesLedgeJumpDistanceType::Short, bIsFreehang);
	LedgeHandler.Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal, this, CharacterOwner, -1.0f, EThreadTalesLedgeJumpDistanceType::Short);
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

	LedgeHandler.StopAllClimbingMontages(CharacterOwner);
	SetClimbingSubState(EThreadTalesClimbingSubState::JumpTransition);
	const EThreadTalesLedgeJumpDirection Dir = (DirectionSign < 0.0f) ? EThreadTalesLedgeJumpDirection::Left : EThreadTalesLedgeJumpDirection::Right;
	UAnimMontage* Montage = GetJumpMontage(Dir, EThreadTalesLedgeJumpDistanceType::Short, bIsFreehang);
	LedgeHandler.Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal, this, CharacterOwner, -1.0f, EThreadTalesLedgeJumpDistanceType::Short);
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

	LedgeHandler.StopAllClimbingMontages(CharacterOwner);
	SetClimbingSubState(EThreadTalesClimbingSubState::JumpTransition);
	UAnimMontage* Montage = (Direction == EThreadTalesLedgeJumpDirection::Down && DistanceType == EThreadTalesLedgeJumpDistanceType::Leap && GetActiveBackJumpMontage(bIsFreehang))
		? GetActiveBackJumpMontage(bIsFreehang)
		: GetJumpMontage(Direction, DistanceType, bIsFreehang);
	LedgeHandler.Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal, this, CharacterOwner, -1.0f, DistanceType);
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

	LedgeHandler.StopAllClimbingMontages(CharacterOwner);
	SetClimbingSubState(EThreadTalesClimbingSubState::Shimmying);
	UAnimMontage* Montage = GetActiveShimmyMontage(DirectionSign < 0.0f, bIsFreehang);
	LedgeHandler.Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal, this, CharacterOwner);
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

	SetClimbingSubState(EThreadTalesClimbingSubState::MantleUp);
	LedgeHandler.StopAllClimbingMontages(CharacterOwner);

	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->bUseControllerDesiredRotation = false;
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->SetMovementMode(MOVE_Flying);
	}
	CharacterOwner->bUseControllerRotationYaw = false;
	CharacterOwner->SetActorLocationAndRotation(LedgeHandler.GetCachedHangLocation(), LedgeHandler.GetCachedHangRotation(), false, nullptr, ETeleportType::TeleportPhysics);

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
	LedgeHandler.SetIsFreehang(bFreehang);
	SetClimbingSubState(EThreadTalesClimbingSubState::ClimbDown);
	LedgeHandler.SetCachedLedgeLocation(LedgeLocation);
	LedgeHandler.SetCachedWallNormal(WallNormal);

	const FRotator FaceWallRotation = (-WallNormal).Rotation();
	const FRotator HangRot = FRotator(0.0f, FaceWallRotation.Yaw, 0.0f);
	const FVector HangLoc = LedgeLocation + (WallNormal * GetHangWallOffset()) - FVector(0.0f, 0.0f, GetHangHeightOffset());
	LedgeHandler.SetCachedHangRotation(HangRot);
	LedgeHandler.SetCachedHangLocation(HangLoc);

	SetTraversalState(EThreadTalesTraversalState::LedgeClimbing);

	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->bUseControllerDesiredRotation = false;
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->SetMovementMode(MOVE_Flying);
	}
	CharacterOwner->bUseControllerRotationYaw = false;

	LedgeHandler.SetClimbDownCollision(true, CharacterOwner);

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

	LedgeHandler.StopAllClimbingMontages(CharacterOwner);
	SetClimbingSubState(EThreadTalesClimbingSubState::None);
	bIsFreehang = false;
	LedgeHandler.SetIsFreehang(false);

	if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Falling);
		MoveComp->Velocity = FThreadTalesClimbingKinematics::CalculateWallEjectVelocity(LedgeHandler.GetCachedWallNormal(), ClimbingConfig);
	}

	SetTraversalState(EThreadTalesTraversalState::None);
	LedgeHandler.SetRegrabCooldownTimer(GetWallEjectRegrabCooldown());
}

void UThreadTalesTraversalComponent::Server_StartCornerTransition_Implementation(
	float DirectionSign,
	EThreadTalesCornerType CornerType,
	const FVector& CornerPivotLoc,
	const FVector& TargetLedgeLoc,
	const FVector& TargetWallNormal)
{
	Multicast_StartCornerTransition(DirectionSign, CornerType, CornerPivotLoc, TargetLedgeLoc, TargetWallNormal);
}

void UThreadTalesTraversalComponent::Multicast_StartCornerTransition_Implementation(
	float DirectionSign,
	EThreadTalesCornerType CornerType,
	const FVector& CornerPivotLoc,
	const FVector& TargetLedgeLoc,
	const FVector& TargetWallNormal)
{
	// Skip on the local client who initiated the transition — they already executed it
	if (!CharacterOwner || CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	SetClimbingSubState(EThreadTalesClimbingSubState::CornerTransition);
	LedgeHandler.StartCornerTransition(DirectionSign, CornerType, CornerPivotLoc, TargetLedgeLoc, TargetWallNormal, this, CharacterOwner);
}

UAnimMontage* UThreadTalesTraversalComponent::GetActiveCornerMontage(bool bLeft, EThreadTalesCornerType CornerType, bool bFreehang) const
{
	if (!ClimbingConfig)
	{
		return nullptr;
	}

	UAnimMontage* Montage = nullptr;

	if (bFreehang)
	{
		if (CornerType == EThreadTalesCornerType::Outer)
		{
			Montage = bLeft ? ClimbingConfig->BarCornerOuterLeftMontage.Get() : ClimbingConfig->BarCornerOuterRightMontage.Get();
		}
		else if (CornerType == EThreadTalesCornerType::Inner)
		{
			Montage = bLeft ? ClimbingConfig->BarCornerInnerLeftMontage.Get() : ClimbingConfig->BarCornerInnerRightMontage.Get();
		}
	}

	if (!Montage)
	{
		if (CornerType == EThreadTalesCornerType::Outer)
		{
			Montage = bLeft ? ClimbingConfig->CornerOuterLeftMontage.Get() : ClimbingConfig->CornerOuterRightMontage.Get();
		}
		else if (CornerType == EThreadTalesCornerType::Inner)
		{
			Montage = bLeft ? ClimbingConfig->CornerInnerLeftMontage.Get() : ClimbingConfig->CornerInnerRightMontage.Get();
		}
	}

	// Fallback to shimmy montage (respecting freehang) so functionality is testable before dedicated corner animations are authored
	if (!Montage)
	{
		Montage = GetActiveShimmyMontage(bLeft, bFreehang);
	}

	return Montage;
}

UAnimMontage* UThreadTalesTraversalComponent::GetActiveGrabMontage(bool bFreehang) const
{
	if (ClimbingConfig)
	{
		if (bFreehang && ClimbingConfig->BarGrabFromAirMontage)
		{
			return ClimbingConfig->BarGrabFromAirMontage.Get();
		}
		return ClimbingConfig->GrabFromAirMontage.Get();
	}
	return nullptr;
}
