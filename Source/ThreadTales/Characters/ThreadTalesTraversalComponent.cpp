// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ThreadTalesTraversalComponent.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Characters/ThreadTalesClimbingConfig.h"
#include "Characters/ThreadTalesTraversalInputHelper.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "InputCoreTypes.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"

static TAutoConsoleVariable<int32> CVarTraversalDebugLogs(
	TEXT("tt.TraversalDebugLogs"),
	1,
	TEXT("Enable detailed diagnostic logging for ThreadTales Traversal system.\n0: Disabled\n1: Enabled"),
	ECVF_Cheat
);

static TAutoConsoleVariable<int32> CVarTraversalDrawDebug(
	TEXT("tt.TraversalDrawDebug"),
	0,
	TEXT("Enable real-time visual debug drawing for ThreadTales Traversal traces and ledges.\n0: Disabled\n1: Enabled"),
	ECVF_Cheat
);

bool UThreadTalesTraversalComponent::ShouldLogTraversal() const
{
	return DebugConfig.bEnableDetailedTraversalLogs || (CVarTraversalDebugLogs.GetValueOnGameThread() > 0);
}

bool UThreadTalesTraversalComponent::IsGlobalTraversalLoggingEnabled(const ACharacter* Character)
{
	if (CVarTraversalDebugLogs.GetValueOnGameThread() > 0)
	{
		return true;
	}
	if (Character)
	{
		if (const UThreadTalesTraversalComponent* Comp = Character->FindComponentByClass<UThreadTalesTraversalComponent>())
		{
			return Comp->DebugConfig.bEnableDetailedTraversalLogs;
		}
	}
	return false;
}

bool UThreadTalesTraversalComponent::IsDrawDebugTracesEnabled() const
{
	return DebugConfig.bDrawDebugTraces || (CVarTraversalDrawDebug.GetValueOnGameThread() > 0);
}

bool UThreadTalesTraversalComponent::IsGlobalDrawDebugTracesEnabled(const ACharacter* Character)
{
	if (CVarTraversalDrawDebug.GetValueOnGameThread() > 0)
	{
		return true;
	}
	if (Character)
	{
		if (const UThreadTalesTraversalComponent* Comp = Character->FindComponentByClass<UThreadTalesTraversalComponent>())
		{
			return Comp->IsDrawDebugTracesEnabled();
		}
	}
	return false;
}

UThreadTalesTraversalComponent::UThreadTalesTraversalComponent()
{
	PrimaryComponentTick.bCanEverTick = true; // [TICK-ALWAYS-OK]
	SetIsReplicatedByDefault(true);

	CurrentTraversalState = EThreadTalesTraversalState::None;
	CurrentClimbingSubState = EThreadTalesClimbingSubState::None;
	bIsFreehang = false;
	CurrentShimmyDirectionSign = 0.0f;
	DebugConfig.bDrawDebugTraces = false;
	DebugConfig.bEnableDetailedTraversalLogs = true;
	DebugConfig.PhaseDesyncThreshold = 0.15f;
}

void UThreadTalesTraversalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UThreadTalesTraversalComponent, CurrentTraversalState);
	DOREPLIFETIME(UThreadTalesTraversalComponent, CurrentClimbingSubState);
	DOREPLIFETIME(UThreadTalesTraversalComponent, bIsFreehang);
	DOREPLIFETIME(UThreadTalesTraversalComponent, CurrentShimmyDirectionSign);
}

void UThreadTalesTraversalComponent::BeginPlay()
{
	Super::BeginPlay();

	CharacterOwner = Cast<AThreadTalesCharacter>(GetOwner());
	if (CharacterOwner)
	{
		if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
		{
			AddTickPrerequisiteComponent(MoveComp);
		}
	}
	if (!ClimbingConfig)
	{
		ClimbingConfig = LoadObject<UThreadTalesClimbingConfig>(nullptr, TEXT("/Game/Data/DA_ClimbingConfig_Default.DA_ClimbingConfig_Default"));
	}
	LedgeHandler.Initialize(ClimbingConfig, GetActiveHangMontage(false));
}

void UThreadTalesTraversalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CharacterOwner)
	{
		return;
	}

	// On-screen debug for ground movement input verification
	if (GEngine && CharacterOwner->IsLocallyControlled() && CurrentTraversalState == EThreadTalesTraversalState::None)
	{
		const APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
		const FVector2D MoveIn = FThreadTalesTraversalInputHelper::GetMovementInputVector(PC, CharacterOwner);
		GEngine->AddOnScreenDebugMessage(100, 0.0f, FColor::Cyan,
			FString::Printf(TEXT("[Traversal Ground] In2D:(X:%.2f, Y:%.2f) | A:%d D:%d W:%d S:%d Space:%d"),
				MoveIn.X, MoveIn.Y,
				(PC && PC->IsInputKeyDown(EKeys::A)) ? 1 : 0,
				(PC && PC->IsInputKeyDown(EKeys::D)) ? 1 : 0,
				(PC && PC->IsInputKeyDown(EKeys::W)) ? 1 : 0,
				(PC && PC->IsInputKeyDown(EKeys::S)) ? 1 : 0,
				(PC && PC->IsInputKeyDown(EKeys::SpaceBar)) ? 1 : 0));
	}

	// Always tick cooldown and debounce timers regardless of current state
	LedgeHandler.TickTimers(DeltaTime);

	// 1. Auto-detect ledges when in locomotion / airborne
	if (CurrentTraversalState == EThreadTalesTraversalState::None && LedgeHandler.CanAttemptGrab(ClimbingConfig))
	{
		if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
		{
			const float MaxGrabVelZ = ClimbingConfig ? ClimbingConfig->Sensing.Detection.MaxGrabVelocityZ : 400.0f;
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
			else if (IsDrawDebugTracesEnabled() && CharacterOwner->IsLocallyControlled())
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

	ResetClimbingMovementInput();
}

// ==========================================
// WORLD SENSING DELEGATION
// ==========================================

void UThreadTalesTraversalComponent::RecordClimbingMovementInput(const FVector& WorldDirection, float ScaleValue)
{
	CachedClimbingInputVector += (WorldDirection * ScaleValue);
	bHasClimbingInputThisFrame = true;
}

float UThreadTalesTraversalComponent::ConsumeClimbingLateralInput()
{
	const APlayerController* PC = CharacterOwner ? Cast<APlayerController>(CharacterOwner->GetController()) : nullptr;
	return FThreadTalesTraversalInputHelper::GetLateralInputSign(PC, CharacterOwner);
}

bool UThreadTalesTraversalComponent::DetectLedge(FVector& OutLedgeLocation, FVector& OutWallNormal)
{
	bool bDetectedFreehang = false;
	const bool bFound = FThreadTalesLedgeDetector::DetectLedge(CharacterOwner, ClimbingConfig, IsDrawDebugTracesEnabled(), OutLedgeLocation, OutWallNormal, &bDetectedFreehang);
	if (bFound)
	{
		SetIsFreehang(bDetectedFreehang);
	}
	return bFound;
}

bool UThreadTalesTraversalComponent::FindUpperLedge(FVector& OutLedgeLoc, FVector& OutWallNormal)
{
	return FThreadTalesLedgeDetector::FindUpperLedge(CharacterOwner, ClimbingConfig, IsDrawDebugTracesEnabled(), LedgeHandler.GetCachedLedgeLocation(), LedgeHandler.GetCachedWallNormal(), OutLedgeLoc, OutWallNormal);
}

bool UThreadTalesTraversalComponent::FindSideLedge(float DirectionSign, FVector& OutLedgeLoc, FVector& OutWallNormal, bool& bOutIsContinuous, bool bForJump)
{
	bool bSideIsFreehang = false;
	const bool bFound = FThreadTalesLedgeDetector::FindSideLedge(CharacterOwner, ClimbingConfig, IsDrawDebugTracesEnabled(), LedgeHandler.GetCachedLedgeLocation(), LedgeHandler.GetCachedWallNormal(), DirectionSign, bForJump, OutLedgeLoc, OutWallNormal, bOutIsContinuous, &bSideIsFreehang);
	if (bFound && bOutIsContinuous)
	{
		SetIsFreehang(bSideIsFreehang);
	}
	return bFound;
}

bool UThreadTalesTraversalComponent::FindLedgeInDirection(EThreadTalesLedgeJumpDirection Direction, EThreadTalesLedgeJumpDistanceType DistanceType, FVector& OutLedgeLoc, FVector& OutWallNormal)
{
	return FThreadTalesLedgeDetector::FindLedgeInDirection(CharacterOwner, ClimbingConfig, IsDrawDebugTracesEnabled(), LedgeHandler.GetCachedLedgeLocation(), LedgeHandler.GetCachedWallNormal(), Direction, DistanceType, LedgeHandler.GetJumpConfigTable(), OutLedgeLoc, OutWallNormal);
}

bool UThreadTalesTraversalComponent::CanClimbUp(FVector& OutStandingLoc) const
{
	return FThreadTalesLedgeDetector::CanClimbUp(CharacterOwner, ClimbingConfig, IsDrawDebugTracesEnabled(), LedgeHandler.GetCachedLedgeLocation(), LedgeHandler.GetCachedWallNormal(), OutStandingLoc);
}

bool UThreadTalesTraversalComponent::CheckIsFreehangLedge(const FVector& EdgeLoc, const FVector& WallNormal2D) const
{
	return FThreadTalesLedgeDetector::CheckIsFreehangLedge(CharacterOwner, ClimbingConfig, IsDrawDebugTracesEnabled(), EdgeLoc, WallNormal2D);
}

bool UThreadTalesTraversalComponent::ValidateLedgeLipAndClearance(const FVector& EdgeLoc, const FVector& WallNormal2D)
{
	return FThreadTalesLedgeDetector::ValidateLedgeLipAndClearance(CharacterOwner, ClimbingConfig, IsDrawDebugTracesEnabled(), EdgeLoc, WallNormal2D);
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
	if (!CharacterOwner)
	{
		CharacterOwner = Cast<AThreadTalesCharacter>(GetOwner());
	}
	LedgeHandler.ReleaseLedge(this, CharacterOwner);
}

void UThreadTalesTraversalComponent::ProcessClimbingInput(float DeltaTime)
{
	if (!CharacterOwner || CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	LedgeHandler.ProcessClimbingMovement(DeltaTime, PC, this, CharacterOwner);
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
	SetShimmyDirectionSign(DirectionSign);
	LedgeHandler.StartShimmy(DirectionSign, TargetLedgeLoc, TargetWallNormal, this, CharacterOwner);
}

void UThreadTalesTraversalComponent::ClimbUp()
{
	LedgeHandler.ClimbUp(this, CharacterOwner);
}

bool UThreadTalesTraversalComponent::TryClimbDown()
{
	if (!CharacterOwner)
	{
		CharacterOwner = Cast<AThreadTalesCharacter>(GetOwner());
	}
	return LedgeHandler.TryClimbDown(this, CharacterOwner);
}

bool UThreadTalesTraversalComponent::HandleCrouchOrClimbDownInput()
{
	if (IsTransitioning() || CurrentClimbingSubState == EThreadTalesClimbingSubState::ClimbDown)
	{
		return true;
	}

	if (IsLedgeClimbing())
	{
		ReleaseLedge();
		return true;
	}
	return TryClimbDown();
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
		// 1. Short Distance Freehang (8 directions with diagonal fallback to lateral/vertical)
		if (DistanceType == EThreadTalesLedgeJumpDistanceType::Short)
		{
			switch (Direction)
			{
			case EThreadTalesLedgeJumpDirection::Up:
				if (UAnimMontage* M = ClimbingConfig->Montages.ShortJump.BarClimbHopUpMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::UpRight:
				if (UAnimMontage* M = ClimbingConfig->Montages.ShortJump.BarClimbJumpUpRightMontage.LoadSynchronous()) return M;
				if (UAnimMontage* M = ClimbingConfig->Montages.ShortJump.BarClimbJumpRightMontage.LoadSynchronous()) return M;
				if (UAnimMontage* M = ClimbingConfig->Montages.ShortJump.BarClimbHopUpMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::Right:
				if (UAnimMontage* M = ClimbingConfig->Montages.ShortJump.BarClimbJumpRightMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::DownRight:
				if (UAnimMontage* M = ClimbingConfig->Montages.ShortJump.BarClimbJumpDownRightMontage.LoadSynchronous()) return M;
				if (UAnimMontage* M = ClimbingConfig->Montages.ShortJump.BarClimbJumpRightMontage.LoadSynchronous()) return M;
				if (UAnimMontage* M = ClimbingConfig->Montages.ShortJump.BarClimbHopDownMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::Down:
				if (UAnimMontage* M = ClimbingConfig->Montages.ShortJump.BarClimbHopDownMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::DownLeft:
				if (UAnimMontage* M = ClimbingConfig->Montages.ShortJump.BarClimbJumpDownLeftMontage.LoadSynchronous()) return M;
				if (UAnimMontage* M = ClimbingConfig->Montages.ShortJump.BarClimbJumpLeftMontage.LoadSynchronous()) return M;
				if (UAnimMontage* M = ClimbingConfig->Montages.ShortJump.BarClimbHopDownMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::Left:
				if (UAnimMontage* M = ClimbingConfig->Montages.ShortJump.BarClimbJumpLeftMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::UpLeft:
				if (UAnimMontage* M = ClimbingConfig->Montages.ShortJump.BarClimbJumpUpLeftMontage.LoadSynchronous()) return M;
				if (UAnimMontage* M = ClimbingConfig->Montages.ShortJump.BarClimbJumpLeftMontage.LoadSynchronous()) return M;
				if (UAnimMontage* M = ClimbingConfig->Montages.ShortJump.BarClimbHopUpMontage.LoadSynchronous()) return M;
				break;
			default:
				break;
			}
		}
		// 2. Long Distance Freehang (8 directions with fallback to Short Bar or Braced Long)
		else if (DistanceType == EThreadTalesLedgeJumpDistanceType::Long)
		{
			switch (Direction)
			{
			case EThreadTalesLedgeJumpDirection::Up:
				if (UAnimMontage* M = ClimbingConfig->Montages.LongJump.BarLongHopUpMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::UpRight:
				if (UAnimMontage* M = ClimbingConfig->Montages.LongJump.BarLongJumpUpRightMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::Right:
				if (UAnimMontage* M = ClimbingConfig->Montages.LongJump.BarLongJumpRightMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::DownRight:
				if (UAnimMontage* M = ClimbingConfig->Montages.LongJump.BarLongJumpDownRightMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::Down:
				if (UAnimMontage* M = ClimbingConfig->Montages.LongJump.BarLongHopDownMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::DownLeft:
				if (UAnimMontage* M = ClimbingConfig->Montages.LongJump.BarLongJumpDownLeftMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::Left:
				if (UAnimMontage* M = ClimbingConfig->Montages.LongJump.BarLongJumpLeftMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::UpLeft:
				if (UAnimMontage* M = ClimbingConfig->Montages.LongJump.BarLongJumpUpLeftMontage.LoadSynchronous()) return M;
				break;
			default:
				break;
			}

			// Cascade: If dedicated BarLong montage is not assigned, fall back to Bar Short montage
			UAnimMontage* BarShortFallback = GetJumpMontage(Direction, EThreadTalesLedgeJumpDistanceType::Short, true);
			if (BarShortFallback)
			{
				return BarShortFallback;
			}
		}
		// 3. Leap Distance Freehang (8 directions with fallback to Long Bar or Short Bar)
		else if (DistanceType == EThreadTalesLedgeJumpDistanceType::Leap)
		{
			switch (Direction)
			{
			case EThreadTalesLedgeJumpDirection::Up:
				if (UAnimMontage* M = ClimbingConfig->Montages.LeapJump.BarLeapHopUpMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::UpRight:
				if (UAnimMontage* M = ClimbingConfig->Montages.LeapJump.BarLeapJumpUpRightMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::Right:
				if (UAnimMontage* M = ClimbingConfig->Montages.LeapJump.BarLeapJumpRightMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::DownRight:
				if (UAnimMontage* M = ClimbingConfig->Montages.LeapJump.BarLeapJumpDownRightMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::Down:
				if (UAnimMontage* M = ClimbingConfig->Montages.LeapJump.BarLeapHopDownMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::DownLeft:
				if (UAnimMontage* M = ClimbingConfig->Montages.LeapJump.BarLeapJumpDownLeftMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::Left:
				if (UAnimMontage* M = ClimbingConfig->Montages.LeapJump.BarLeapJumpLeftMontage.LoadSynchronous()) return M;
				break;
			case EThreadTalesLedgeJumpDirection::UpLeft:
				if (UAnimMontage* M = ClimbingConfig->Montages.LeapJump.BarLeapJumpUpLeftMontage.LoadSynchronous()) return M;
				break;
			default:
				break;
			}

			// Cascade: If dedicated BarLeap montage is not assigned, fall back to Bar Long or Bar Short montage
			UAnimMontage* BarLongFallback = GetJumpMontage(Direction, EThreadTalesLedgeJumpDistanceType::Long, true);
			if (BarLongFallback)
			{
				return BarLongFallback;
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
		return bFreehang ? ClimbingConfig->Montages.Base.BarHangMontage.LoadSynchronous() : ClimbingConfig->Montages.Base.HangMontage.LoadSynchronous();
	}
	return nullptr;
}

UAnimMontage* UThreadTalesTraversalComponent::GetActiveClimbOnTopMontage(bool bFreehang) const
{
	if (ClimbingConfig)
	{
		return bFreehang ? ClimbingConfig->Montages.Base.BarClimbOnTopMontage.LoadSynchronous() : ClimbingConfig->Montages.Base.ClimbOnTopMontage.LoadSynchronous();
	}
	return nullptr;
}

UAnimMontage* UThreadTalesTraversalComponent::GetActiveClimbDownMontage(bool bFreehang) const
{
	if (ClimbingConfig)
	{
		return bFreehang ? ClimbingConfig->Montages.Base.BarClimbDownMontage.LoadSynchronous() : ClimbingConfig->Montages.Base.ClimbDownMontage.LoadSynchronous();
	}
	return nullptr;
}

UAnimMontage* UThreadTalesTraversalComponent::GetActiveBackJumpMontage(bool bFreehang) const
{
	if (ClimbingConfig)
	{
		if (bFreehang)
		{
			if (UAnimMontage* BarMontage = ClimbingConfig->Montages.Base.BarClimbBackJumpMontage.LoadSynchronous())
			{
				return BarMontage;
			}
		}
		return ClimbingConfig->Montages.Base.ClimbBackJumpMontage.LoadSynchronous();
	}
	return nullptr;
}

UAnimMontage* UThreadTalesTraversalComponent::GetActiveBackJumpUpMontage(bool bFreehang) const
{
	return GetActiveBackJumpUpMontage(bFreehang, 0.0f);
}

UAnimMontage* UThreadTalesTraversalComponent::GetActiveBackJumpUpMontage(bool bFreehang, float LateralOffset) const
{
	if (!ClimbingConfig)
	{
		return nullptr;
	}
	const bool bLeft = (LateralOffset < -2.0f);
	if (bLeft)
	{
		if (UAnimMontage* Left = ClimbingConfig->Montages.Base.ClimbBackJumpUpLeftMontage.LoadSynchronous())
		{
			return Left;
		}
	}
	if (UAnimMontage* Up = ClimbingConfig->Montages.Base.ClimbBackJumpUpMontage.LoadSynchronous())
	{
		return Up;
	}
	return GetActiveBackJumpMontage(bFreehang);
}

UAnimMontage* UThreadTalesTraversalComponent::GetActiveShimmyMontage(bool bLeft, bool bFreehang) const
{
	if (ClimbingConfig)
	{
		if (bFreehang)
		{
			UAnimMontage* BarMontage = bLeft ? ClimbingConfig->Montages.Base.BarShimmyLeftMontage.LoadSynchronous() : ClimbingConfig->Montages.Base.BarShimmyRightMontage.LoadSynchronous();
			if (BarMontage)
			{
				return BarMontage;
			}
		}
		return bLeft ? ClimbingConfig->Montages.Base.ShimmyLeftMontage.LoadSynchronous() : ClimbingConfig->Montages.Base.ShimmyRightMontage.LoadSynchronous();
	}
	return nullptr;
}

float UThreadTalesTraversalComponent::GetHangWallOffset() const { return ClimbingConfig ? ClimbingConfig->Kinematics.Offsets.HangWallOffset : 24.0f; }
float UThreadTalesTraversalComponent::GetHangHeightOffset() const { return ClimbingConfig ? ClimbingConfig->Kinematics.Offsets.HangHeightOffset : 73.0f; }
float UThreadTalesTraversalComponent::GetClimbAnimPlayRateMultiplier() const { return ClimbingConfig ? ClimbingConfig->Kinematics.Offsets.ClimbAnimPlayRateMultiplier : 1.0f; }
float UThreadTalesTraversalComponent::GetJumpPlayRateMultiplier() const { return ClimbingConfig ? ClimbingConfig->Kinematics.Offsets.JumpPlayRateMultiplier : 1.25f; }
float UThreadTalesTraversalComponent::GetClimbBackJumpPlayRate() const { return ClimbingConfig ? ClimbingConfig->Kinematics.Offsets.ClimbBackJumpPlayRate : 1.35f; }
float UThreadTalesTraversalComponent::GetGrabPlayRate() const { return ClimbingConfig ? ClimbingConfig->Kinematics.Offsets.GrabPlayRate : 1.4f; }
float UThreadTalesTraversalComponent::GetBarGrabPlayRate() const { return ClimbingConfig ? ClimbingConfig->Kinematics.Offsets.BarGrabPlayRate : 2.3f; }
float UThreadTalesTraversalComponent::GetShimmyCooldownDuration() const { return ClimbingConfig ? ClimbingConfig->Traversal.Shimmy.ShimmyCooldownDuration : 0.08f; }
float UThreadTalesTraversalComponent::GetShimmyInputDebounceDuration() const { return ClimbingConfig ? ClimbingConfig->Traversal.Shimmy.ShimmyInputDebounceDuration : 0.15f; }
float UThreadTalesTraversalComponent::GetJumpInputDebounceDuration() const { return ClimbingConfig ? ClimbingConfig->Traversal.General.JumpInputDebounceDuration : 0.25f; }
float UThreadTalesTraversalComponent::GetRegrabCooldownDuration() const { return ClimbingConfig ? ClimbingConfig->Traversal.General.RegrabCooldownDuration : 0.4f; }
float UThreadTalesTraversalComponent::GetWallEjectRegrabCooldown() const { return ClimbingConfig ? ClimbingConfig->Traversal.WallEject.WallEjectRegrabCooldown : 0.55f; }

bool UThreadTalesTraversalComponent::IsRegrabCooldownActive() const
{
	return LedgeHandler.GetRegrabCooldownTimer() > 0.0f;
}

bool UThreadTalesTraversalComponent::IsWallEjectActive() const
{
	return LedgeHandler.GetRegrabCooldownTimer() > 0.0f;
}

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

	if (CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing)
	{
		SetClimbingSubState(EThreadTalesClimbingSubState::None);
		SetIsFreehang(false);
	}

	UpdateClimbingGameplayTags();
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

	UpdateClimbingGameplayTags();
	OnClimbingSubStateChanged.Broadcast(NewSubState, OldSubState);
}

void UThreadTalesTraversalComponent::SetIsFreehang(bool bInFreehang)
{
	if (bIsFreehang != bInFreehang)
	{
		bIsFreehang = bInFreehang;
		LedgeHandler.SetIsFreehang(bInFreehang);
		UpdateClimbingGameplayTags();
	}
}

void UThreadTalesTraversalComponent::SetShimmyDirectionSign(float InSign)
{
	if (CurrentShimmyDirectionSign != InSign)
	{
		CurrentShimmyDirectionSign = InSign;
		UpdateClimbingGameplayTags();
	}
}

void UThreadTalesTraversalComponent::OnRep_TraversalState(EThreadTalesTraversalState OldState)
{
	UpdateClimbingGameplayTags();
	OnTraversalStateChanged.Broadcast(CurrentTraversalState, OldState);
}

void UThreadTalesTraversalComponent::OnRep_ClimbingSubState(EThreadTalesClimbingSubState OldSubState)
{
	LedgeHandler.SetClimbingSubState(CurrentClimbingSubState);
	UpdateClimbingGameplayTags();
	OnClimbingSubStateChanged.Broadcast(CurrentClimbingSubState, OldSubState);
}

void UThreadTalesTraversalComponent::OnRep_IsFreehang()
{
	LedgeHandler.SetIsFreehang(bIsFreehang);
	UpdateClimbingGameplayTags();
}

void UThreadTalesTraversalComponent::OnRep_ShimmyDirectionSign()
{
	UpdateClimbingGameplayTags();
}

void UThreadTalesTraversalComponent::UpdateClimbingGameplayTags()
{
	if (!CharacterOwner)
	{
		return;
	}

	UAbilitySystemComponent* ASC = CharacterOwner->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	auto SafeRemoveTag = [ASC](const FGameplayTag& Tag)
	{
		if (Tag.IsValid() && ASC->HasMatchingGameplayTag(Tag))
		{
			ASC->RemoveLooseGameplayTag(Tag);
		}
	};

	auto SafeAddTag = [ASC](const FGameplayTag& Tag)
	{
		if (Tag.IsValid() && !ASC->HasMatchingGameplayTag(Tag))
		{
			ASC->AddLooseGameplayTag(Tag);
		}
	};

	if (CurrentTraversalState != EThreadTalesTraversalState::LedgeClimbing)
	{
		SafeRemoveTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing);
		SafeRemoveTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Hang_Braced);
		SafeRemoveTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Hang_Freehang);
		SafeRemoveTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Shimmy);
		SafeRemoveTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Shimmy_Left);
		SafeRemoveTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Shimmy_Right);
		return;
	}

	// In Ledge Climbing:
	SafeAddTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing);

	if (CurrentClimbingSubState == EThreadTalesClimbingSubState::IdleHang)
	{
		// Clear Shimmy tags
		SafeRemoveTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Shimmy);
		SafeRemoveTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Shimmy_Left);
		SafeRemoveTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Shimmy_Right);

		if (bIsFreehang)
		{
			SafeAddTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Hang_Freehang);
			SafeRemoveTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Hang_Braced);
		}
		else
		{
			SafeAddTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Hang_Braced);
			SafeRemoveTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Hang_Freehang);
		}
	}
	else if (CurrentClimbingSubState == EThreadTalesClimbingSubState::Shimmying)
	{
		if (bIsFreehang)
		{
			SafeAddTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Hang_Freehang);
			SafeRemoveTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Hang_Braced);
		}
		else
		{
			SafeAddTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Hang_Braced);
			SafeRemoveTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Hang_Freehang);
		}

		SafeAddTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Shimmy);

		if (CurrentShimmyDirectionSign < 0.0f)
		{
			SafeAddTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Shimmy_Left);
			SafeRemoveTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Shimmy_Right);
		}
		else
		{
			SafeAddTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Shimmy_Right);
			SafeRemoveTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Shimmy_Left);
		}
	}
	else
	{
		// Dynamic transitions (Jump, Corner, Mantle, Drop, Catch):
		// Clear Hang and Shimmy tags so DefaultSlot montage takes precedence cleanly
		SafeRemoveTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Hang_Braced);
		SafeRemoveTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Hang_Freehang);
		SafeRemoveTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Shimmy);
		SafeRemoveTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Shimmy_Left);
		SafeRemoveTag(ThreadTalesGameplayTags::TAG_State_Movement_Climbing_Shimmy_Right);
	}
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

	SetClimbingSubState(EThreadTalesClimbingSubState::JumpTransition);
	bIsFreehang = CheckIsFreehangLedge(TargetLedgeLoc, TargetWallNormal);
	LedgeHandler.SetIsFreehang(bIsFreehang);

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

	SetClimbingSubState(EThreadTalesClimbingSubState::JumpTransition);
	bIsFreehang = CheckIsFreehangLedge(TargetLedgeLoc, TargetWallNormal);
	LedgeHandler.SetIsFreehang(bIsFreehang);

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

	SetClimbingSubState(EThreadTalesClimbingSubState::JumpTransition);

	bIsFreehang = CheckIsFreehangLedge(TargetLedgeLoc, TargetWallNormal);
	LedgeHandler.SetIsFreehang(bIsFreehang);

	UAnimMontage* Montage = nullptr;
	if (Direction == EThreadTalesLedgeJumpDirection::Down)
	{
		const float HeightDelta = TargetLedgeLoc.Z - LedgeHandler.GetCachedLedgeLocation().Z;
		if (HeightDelta > 50.0f)
		{
			const FVector ActorRight = CharacterOwner ? CharacterOwner->GetActorRightVector() : FVector::CrossProduct(TargetWallNormal.GetSafeNormal2D(), FVector::UpVector).GetSafeNormal();
			const float LateralOffset = FVector::DotProduct(TargetLedgeLoc - LedgeHandler.GetCachedLedgeLocation(), ActorRight);
			Montage = GetActiveBackJumpUpMontage(bIsFreehang, LateralOffset);
		}
		else
		{
			Montage = GetActiveBackJumpMontage(bIsFreehang);
		}
	}
	if (!Montage)
	{
		Montage = GetJumpMontage(Direction, DistanceType, bIsFreehang);
	}
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

	SetShimmyDirectionSign(DirectionSign);
	SetClimbingSubState(DirectionSign != 0.0f ? EThreadTalesClimbingSubState::Shimmying : EThreadTalesClimbingSubState::IdleHang);
	SetIsFreehang(CheckIsFreehangLedge(TargetLedgeLoc, TargetWallNormal));

	UAnimMontage* Montage = GetActiveShimmyMontage(DirectionSign < 0.0f, bIsFreehang);
	LedgeHandler.Internal_StartTransition(Montage, TargetLedgeLoc, TargetWallNormal, this, CharacterOwner);
}

void UThreadTalesTraversalComponent::Server_UpdateShimmyContinuous_Implementation(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, bool bInFreehang)
{
	Multicast_UpdateShimmyContinuous(DirectionSign, TargetLedgeLoc, TargetWallNormal, bInFreehang);
}

void UThreadTalesTraversalComponent::Multicast_UpdateShimmyContinuous_Implementation(float DirectionSign, const FVector& TargetLedgeLoc, const FVector& TargetWallNormal, bool bInFreehang)
{
	if (!CharacterOwner || CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	SetShimmyDirectionSign(DirectionSign);
	SetClimbingSubState(DirectionSign != 0.0f ? EThreadTalesClimbingSubState::Shimmying : EThreadTalesClimbingSubState::IdleHang);
	SetIsFreehang(bInFreehang);
	LedgeHandler.UpdateContinuousShimmyTransform(TargetLedgeLoc, TargetWallNormal, bInFreehang, this, CharacterOwner);
}

void UThreadTalesTraversalComponent::Server_ClimbUp_Implementation()
{
	SetClimbingSubState(EThreadTalesClimbingSubState::MantleUp);
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

void UThreadTalesTraversalComponent::Server_CompleteClimbUp_Implementation()
{
	Multicast_CompleteClimbUp();
}

void UThreadTalesTraversalComponent::Multicast_CompleteClimbUp_Implementation()
{
	SetClimbingSubState(EThreadTalesClimbingSubState::None);
	SetIsFreehang(false);
	SetTraversalState(EThreadTalesTraversalState::None);
	LedgeHandler.SetRegrabCooldownTimer(GetRegrabCooldownDuration());

	if (CharacterOwner)
	{
		if (UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
		{
			MoveComp->Velocity = FVector::ZeroVector;
			MoveComp->SetMovementMode(MOVE_Walking);
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

	const FRotator EjectRotation = FRotator(0.0f, LedgeHandler.GetCachedWallNormal().Rotation().Yaw, 0.0f);
	CharacterOwner->SetActorRotation(EjectRotation);

	CharacterOwner->JumpCurrentCount = 1;

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
			Montage = bLeft ? ClimbingConfig->Montages.Corner.BarCornerOuterLeftMontage.LoadSynchronous() : ClimbingConfig->Montages.Corner.BarCornerOuterRightMontage.LoadSynchronous();
		}
		else if (CornerType == EThreadTalesCornerType::Inner)
		{
			Montage = bLeft ? ClimbingConfig->Montages.Corner.BarCornerInnerLeftMontage.LoadSynchronous() : ClimbingConfig->Montages.Corner.BarCornerInnerRightMontage.LoadSynchronous();
		}
	}

	if (!Montage)
	{
		if (CornerType == EThreadTalesCornerType::Outer)
		{
			Montage = bLeft ? ClimbingConfig->Montages.Corner.CornerOuterLeftMontage.LoadSynchronous() : ClimbingConfig->Montages.Corner.CornerOuterRightMontage.LoadSynchronous();
		}
		else if (CornerType == EThreadTalesCornerType::Inner)
		{
			Montage = bLeft ? ClimbingConfig->Montages.Corner.CornerInnerLeftMontage.LoadSynchronous() : ClimbingConfig->Montages.Corner.CornerInnerRightMontage.LoadSynchronous();
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
		if (bFreehang)
		{
			if (UAnimMontage* BarMontage = ClimbingConfig->Montages.Base.BarGrabFromAirMontage.LoadSynchronous())
			{
				return BarMontage;
			}
		}
		return ClimbingConfig->Montages.Base.GrabFromAirMontage.LoadSynchronous();
	}
	return nullptr;
}
