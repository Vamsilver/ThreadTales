// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ThreadTalesBowTrajectoryComponent.h"
#include "Combat/ThreadTalesWeapon_Bow.h"
#include "Data/ThreadTalesBowConfig.h"
#include "Characters/ThreadTalesCharacter.h"
#include "Combat/ThreadTalesTargetLockComponent.h"
#include "ThreadTales.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

static TAutoConsoleVariable<bool> CVarEnableSplineMeshRibbon(
	TEXT("r.ThreadTales.Bow.EnableSplineMeshRibbon"),
	true,
	TEXT("Master toggle for C++ Spline Mesh ribbon trajectory rendering.\n0: Disabled (Niagara only), 1: Enabled (allow component setting)"),
	ECVF_Cheat
);

static TAutoConsoleVariable<bool> CVarEnableViewportArcRender(
	TEXT("r.ThreadTales.Bow.EnableViewportArcRender"),
	true,
	TEXT("Master toggle for C++ debug DrawDebugLine trajectory rendering.\n0: Disabled, 1: Enabled (allow component setting)"),
	ECVF_Cheat
);

UThreadTalesBowTrajectoryComponent::UThreadTalesBowTrajectoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Trajectory Niagara system is assigned via Blueprint / DataAsset (Data-Driven Architecture)
	TrajectoryNiagaraSystemAsset = nullptr;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderFinder.Succeeded())
	{
		SplineMeshAsset = CylinderFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMatFinder(TEXT("/Game/LevelPrototyping/Interactable/JumpPad/Assets/Materials/M_SimpleGlow.M_SimpleGlow"));
	if (GlowMatFinder.Succeeded())
	{
		SplineMeshMaterial = GlowMatFinder.Object;
	}
}

void UThreadTalesBowTrajectoryComponent::BeginPlay()
{
	Super::BeginPlay();
	ClearTrajectoryArc();
}

void UThreadTalesBowTrajectoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTrajectoryArc();
	for (TObjectPtr<USplineMeshComponent>& Comp : SplineMeshPool)
	{
		if (Comp) { Comp->DestroyComponent(); }
	}
	SplineMeshPool.Empty();
	RibbonMID = nullptr;

	if (TrajectoryNiagaraComponent)
	{
		TrajectoryNiagaraComponent->DestroyComponent();
		TrajectoryNiagaraComponent = nullptr;
	}
	if (TrajectorySplineComponent)
	{
		TrajectorySplineComponent->DestroyComponent();
		TrajectorySplineComponent = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

AThreadTalesWeapon_Bow* UThreadTalesBowTrajectoryComponent::GetOwningBow() const
{
	return Cast<AThreadTalesWeapon_Bow>(GetOwner());
}

const UThreadTalesBowConfig* UThreadTalesBowTrajectoryComponent::ResolveBowConfig() const
{
	if (const AThreadTalesWeapon_Bow* Bow = GetOwningBow())
	{
		return Bow->GetBowConfig();
	}
	return nullptr;
}

bool UThreadTalesBowTrajectoryComponent::CalculateAimConvergence(
	const AActor* AvatarActor,
	const FVector& MuzzleOrigin,
	FVector& OutTargetLocation,
	FVector& OutLaunchDirection) const
{
	if (const AThreadTalesCharacter* TTChar = Cast<AThreadTalesCharacter>(AvatarActor))
	{
		if (const UThreadTalesTargetLockComponent* TargetLock = TTChar->GetTargetLockComponent())
		{
			if (TargetLock->IsTargetLocked())
			{
				OutTargetLocation = TargetLock->GetTargetLocation();
				OutLaunchDirection = (OutTargetLocation - MuzzleOrigin).GetSafeNormal();
				return true;
			}
		}
	}

	FVector CameraLoc = AvatarActor ? AvatarActor->GetActorLocation() : GetOwner()->GetActorLocation();
	FRotator CameraRot = AvatarActor ? AvatarActor->GetActorRotation() : GetOwner()->GetActorRotation();

	if (const APawn* Pawn = Cast<APawn>(AvatarActor))
	{
		if (const APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			CameraLoc = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : CameraLoc;
			CameraRot = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation() : PC->GetControlRotation();
		}
		else
		{
			Pawn->GetActorEyesViewPoint(CameraLoc, CameraRot);
		}
	}

	const FVector CameraFwd = CameraRot.Vector();
	const FVector TraceEnd = CameraLoc + CameraFwd * MaxConvergenceDistance;

	FCollisionQueryParams QueryParams(TEXT("BowAimConvergenceTrace"), false, AvatarActor);
	QueryParams.AddIgnoredActor(GetOwner());
	if (AvatarActor)
	{
		TArray<AActor*> AttachedActors;
		AvatarActor->GetAttachedActors(AttachedActors, true, true);
		QueryParams.AddIgnoredActors(AttachedActors);
	}

	FHitResult HitResult;
	const UWorld* World = GetWorld();
	const bool bHit = World && World->LineTraceSingleByChannel(HitResult, CameraLoc, TraceEnd, ECC_Visibility, QueryParams);

	if (bHit && HitResult.bBlockingHit)
	{
		const FVector AvatarLoc = AvatarActor ? AvatarActor->GetActorLocation() : CameraLoc;
		const float ForwardProj = (HitResult.ImpactPoint - AvatarLoc) | CameraFwd;
		OutTargetLocation = (ForwardProj > BlindSpotForwardThreshold) ? HitResult.ImpactPoint : (CameraLoc + CameraFwd * BlindSpotSafeDistance);
	}
	else
	{
		OutTargetLocation = CameraLoc + CameraFwd * (MaxConvergenceDistance * 0.60f);
	}

	OutLaunchDirection = (OutTargetLocation - MuzzleOrigin).GetSafeNormal();
	if (OutLaunchDirection.IsNearlyZero()) { OutLaunchDirection = CameraFwd; }
	return true;
}

bool UThreadTalesBowTrajectoryComponent::PredictTrajectory(
	const AActor* AvatarActor,
	const FVector& MuzzleOrigin,
	float DrawRatio,
	int32 ChargeTier,
	TArray<FVector>& OutPathPoints,
	FHitResult& OutImpactHit) const
{
	FVector AimTarget, LaunchDirection;
	CalculateAimConvergence(AvatarActor, MuzzleOrigin, AimTarget, LaunchDirection);

	const UThreadTalesBowConfig* BowConfig = ResolveBowConfig();
	const float Speed = BowConfig ? BowConfig->GetSpeedForTier(ChargeTier) : 4500.0f;
	const float GravityScale = BowConfig ? BowConfig->GetGravityForTier(ChargeTier) : 0.15f;

	FPredictProjectilePathParams PredictParams;
	PredictParams.StartLocation = MuzzleOrigin;
	PredictParams.LaunchVelocity = LaunchDirection * Speed;
	PredictParams.bTraceWithCollision = true;
	PredictParams.ProjectileRadius = ProjectileRadius;
	PredictParams.MaxSimTime = MaxSimTime;
	PredictParams.bTraceWithChannel = true;
	PredictParams.TraceChannel = ECC_Visibility;
	PredictParams.SimFrequency = SimFrequency;
	PredictParams.ActorsToIgnore.Add(const_cast<AActor*>(AvatarActor));
	PredictParams.ActorsToIgnore.Add(const_cast<AActor*>(GetOwner()));
	if (AvatarActor)
	{
		TArray<AActor*> AttachedActors;
		AvatarActor->GetAttachedActors(AttachedActors, true, true);
		PredictParams.ActorsToIgnore.Append(AttachedActors);
	}

	const UWorld* CurrentWorld = GetWorld();
	const float WorldGravityZ = CurrentWorld ? CurrentWorld->GetGravityZ() : -980.0f;
	PredictParams.OverrideGravityZ = WorldGravityZ * GravityScale;

	FPredictProjectilePathResult PredictResult;
	const bool bSuccess = UGameplayStatics::PredictProjectilePath(this, PredictParams, PredictResult);

	OutPathPoints.Reset(PredictResult.PathData.Num());
	for (const FPredictProjectilePathPointData& Point : PredictResult.PathData)
	{
		OutPathPoints.Add(Point.Location);
	}
	OutImpactHit = PredictResult.HitResult;
	return bSuccess;
}

bool UThreadTalesBowTrajectoryComponent::EnsureSplineComponent()
{
	if (TrajectorySplineComponent) { return true; }
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) { return false; }

	TrajectorySplineComponent = NewObject<USplineComponent>(OwnerActor, TEXT("TrajectorySplineComp"));
	if (!TrajectorySplineComponent) { return false; }

	TrajectorySplineComponent->SetMobility(EComponentMobility::Movable);
	TrajectorySplineComponent->RegisterComponent();
	TrajectorySplineComponent->AttachToComponent(OwnerActor->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	TrajectorySplineComponent->bDrawDebug = false;
	return true;
}

bool UThreadTalesBowTrajectoryComponent::EnsureNiagaraComponent()
{
	if (TrajectoryNiagaraComponent) { return true; }
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) { return false; }

	TrajectoryNiagaraComponent = NewObject<UNiagaraComponent>(OwnerActor, TEXT("TrajectoryNiagaraComp"));
	if (!TrajectoryNiagaraComponent) { return false; }

	TrajectoryNiagaraComponent->SetMobility(EComponentMobility::Movable);
	TrajectoryNiagaraComponent->bAutoActivate = false;
	if (TrajectoryNiagaraSystemAsset)
	{
		TrajectoryNiagaraComponent->SetAsset(TrajectoryNiagaraSystemAsset);
	}
	TrajectoryNiagaraComponent->RegisterComponent();
	TrajectoryNiagaraComponent->AttachToComponent(OwnerActor->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	return true;
}

void UThreadTalesBowTrajectoryComponent::UpdateSplineFromPoints(const TArray<FVector>& PathPoints)
{
	if (!EnsureSplineComponent() || PathPoints.Num() < 2)
	{
		return;
	}

	TrajectorySplineComponent->ClearSplinePoints(false);
	const int32 NumPoints = PathPoints.Num();
	for (int32 Index = 0; Index < NumPoints; ++Index)
	{
		TrajectorySplineComponent->AddSplinePoint(PathPoints[Index], ESplineCoordinateSpace::World, false);
		TrajectorySplineComponent->SetSplinePointType(Index, ESplinePointType::Curve, false);
	}
	TrajectorySplineComponent->UpdateSpline();
}

void UThreadTalesBowTrajectoryComponent::UpdateNiagaraParameters(int32 ChargeTier)
{
	if (!TrajectoryNiagaraComponent || !TrajectoryNiagaraSystemAsset || LastPathPoints.Num() < 2) { return; }
	if (TrajectoryNiagaraComponent->GetAsset() != TrajectoryNiagaraSystemAsset) { TrajectoryNiagaraComponent->SetAsset(TrajectoryNiagaraSystemAsset); }
	if (!TrajectoryNiagaraComponent->IsActive()) { TrajectoryNiagaraComponent->Activate(true); }

	if (TrajectorySplineComponent)
	{
		TrajectoryNiagaraComponent->SetVariableObject(FName(TEXT("User.Spline")), TrajectorySplineComponent);
		TrajectoryNiagaraComponent->SetVariableFloat(FName(TEXT("User.SplineLength")), TrajectorySplineComponent->GetSplineLength());
	}

	const FVector BeamEndPos = LastImpactHit.bBlockingHit ? LastImpactHit.ImpactPoint : LastPathPoints.Last();
	TrajectoryNiagaraComponent->SetVariablePosition(FName(TEXT("User.BeamStart")), LastPathPoints[0]);
	TrajectoryNiagaraComponent->SetVariablePosition(FName(TEXT("User.BeamEnd")), BeamEndPos);

	const FLinearColor TierLinearColor = GetLinearColorForTier(ChargeTier);
	const float ScaledRibbonWidth = BaseRibbonWidth * (GetThicknessForTier(ChargeTier) / 2.5f);

	TrajectoryNiagaraComponent->SetVariableLinearColor(FName(TEXT("User.ArcColor")), TierLinearColor);
	TrajectoryNiagaraComponent->SetVariableFloat(FName(TEXT("User.RibbonWidth")), ScaledRibbonWidth);
	TrajectoryNiagaraComponent->SetVariableInt(FName(TEXT("User.ChargeTier")), ChargeTier);

	if (!RibbonMID && SplineMeshMaterial)
	{
		RibbonMID = UMaterialInstanceDynamic::Create(SplineMeshMaterial, this);
	}
	if (RibbonMID)
	{
		RibbonMID->SetScalarParameterValue(FName(TEXT("Custom Prim Switch")), 1.0f);
		RibbonMID->SetVectorParameterValue(FName(TEXT("Color")), TierLinearColor);
		TrajectoryNiagaraComponent->SetMaterial(0, RibbonMID);
	}

	if (LastImpactHit.bBlockingHit)
	{
		TrajectoryNiagaraComponent->SetVariablePosition(FName(TEXT("User.ImpactPoint")), LastImpactHit.ImpactPoint);
		TrajectoryNiagaraComponent->SetVariableVec3(FName(TEXT("User.ImpactNormal")), LastImpactHit.ImpactNormal);
		TrajectoryNiagaraComponent->SetVariableBool(FName(TEXT("User.HasHit")), true);
	}
	else
	{
		TrajectoryNiagaraComponent->SetVariableBool(FName(TEXT("User.HasHit")), false);
	}
}

void UThreadTalesBowTrajectoryComponent::UpdateSplineMeshRibbon(int32 ChargeTier)
{
	if (!TrajectorySplineComponent || LastPathPoints.Num() < 2 || !SplineMeshAsset)
	{
		ClearSplineMeshes();
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) { return; }

	if (!RibbonMID && SplineMeshMaterial)
	{
		RibbonMID = UMaterialInstanceDynamic::Create(SplineMeshMaterial, this);
	}

	const FLinearColor TierColor = GetLinearColorForTier(ChargeTier);
	if (RibbonMID)
	{
		// Critical for M_SimpleGlow: Custom Prim Switch = 1.0 routes Lerp to VectorParameter "Color" instead of ParticleColor
		RibbonMID->SetScalarParameterValue(FName(TEXT("Custom Prim Switch")), 1.0f);
		RibbonMID->SetVectorParameterValue(FName(TEXT("Color")), TierColor);
	}

	const int32 NumSegments = LastPathPoints.Num() - 1;
	const float ScaleFactor = (RibbonMeshDiameter * (GetThicknessForTier(ChargeTier) / 2.5f)) / 100.0f;
	const FVector2D MeshScale(ScaleFactor, ScaleFactor);

	for (int32 Index = 0; Index < NumSegments; ++Index)
	{
		USplineMeshComponent* SplineMeshComp = nullptr;
		if (Index < SplineMeshPool.Num())
		{
			SplineMeshComp = SplineMeshPool[Index];
		}
		else
		{
			SplineMeshComp = NewObject<USplineMeshComponent>(OwnerActor);
			if (SplineMeshComp)
			{
				SplineMeshComp->SetMobility(EComponentMobility::Movable);
				SplineMeshComp->RegisterComponent();
				SplineMeshComp->AttachToComponent(OwnerActor->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
				SplineMeshComp->SetAbsolute(true, true, true);
				SplineMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				SplineMeshComp->SetCastShadow(false);
				SplineMeshComp->SetForwardAxis(ESplineMeshAxis::Z);
				SplineMeshComp->SetStaticMesh(SplineMeshAsset);
				if (RibbonMID) { SplineMeshComp->SetMaterial(0, RibbonMID); }
				SplineMeshPool.Add(SplineMeshComp);
			}
		}

		if (SplineMeshComp)
		{
			if (RibbonMID && SplineMeshComp->GetMaterial(0) != RibbonMID)
			{
				SplineMeshComp->SetMaterial(0, RibbonMID);
			}
			SplineMeshComp->SetWorldTransform(FTransform::Identity);
			FVector StartPos, StartTangent, EndPos, EndTangent;
			TrajectorySplineComponent->GetLocationAndTangentAtSplinePoint(Index, StartPos, StartTangent, ESplineCoordinateSpace::World);
			TrajectorySplineComponent->GetLocationAndTangentAtSplinePoint(Index + 1, EndPos, EndTangent, ESplineCoordinateSpace::World);

			SplineMeshComp->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent, true);
			SplineMeshComp->SetStartScale(MeshScale, true);
			SplineMeshComp->SetEndScale(MeshScale, true);
			SplineMeshComp->SetVisibility(true);
			SplineMeshComp->UpdateMesh();
		}
	}

	for (int32 Index = NumSegments; Index < SplineMeshPool.Num(); ++Index)
	{
		if (SplineMeshPool[Index]) { SplineMeshPool[Index]->SetVisibility(false); }
	}
}

void UThreadTalesBowTrajectoryComponent::ClearSplineMeshes()
{
	for (TObjectPtr<USplineMeshComponent>& Comp : SplineMeshPool)
	{
		if (Comp) { Comp->SetVisibility(false); }
	}
}

void UThreadTalesBowTrajectoryComponent::DrawDebugFallback(int32 ChargeTier)
{
	UWorld* World = GetWorld();
	if (!bEnableViewportArcRender || !World || LastPathPoints.Num() < 2)
	{
		return;
	}

	const FColor ArcColor = GetColorForTier(ChargeTier);
	const float Thickness = GetThicknessForTier(ChargeTier);
	constexpr float DisplayDuration = 0.04f;

	for (int32 Index = 1; Index < LastPathPoints.Num(); ++Index)
	{
		DrawDebugLine(World, LastPathPoints[Index - 1], LastPathPoints[Index], ArcColor, false, DisplayDuration, 0, Thickness);
	}

	if (LastImpactHit.bBlockingHit)
	{
		const FVector Center = LastImpactHit.ImpactPoint + LastImpactHit.ImpactNormal * 1.5f;
		const FVector UpAxis = LastImpactHit.ImpactNormal;
		FVector FwdAxis = FVector::CrossProduct(UpAxis, FVector::RightVector);
		if (FwdAxis.IsNearlyZero()) { FwdAxis = FVector::CrossProduct(UpAxis, FVector::ForwardVector); }
		FwdAxis.Normalize();
		const FVector RightAxis = FVector::CrossProduct(UpAxis, FwdAxis).GetSafeNormal();

		DrawDebugCircle(World, Center, 14.0f, 16, ArcColor, false, DisplayDuration, 0, Thickness, FwdAxis, RightAxis, false);
		DrawDebugCircle(World, Center, 4.0f, 8, ArcColor, false, DisplayDuration, 0, Thickness * 0.8f, FwdAxis, RightAxis, false);
		DrawDebugLine(World, Center - FwdAxis * 18.0f, Center + FwdAxis * 18.0f, ArcColor, false, DisplayDuration, 0, Thickness * 0.7f);
		DrawDebugLine(World, Center - RightAxis * 18.0f, Center + RightAxis * 18.0f, ArcColor, false, DisplayDuration, 0, Thickness * 0.7f);
	}
}

void UThreadTalesBowTrajectoryComponent::UpdateTrajectoryArc(
	const AActor* AvatarActor,
	const FVector& MuzzleOrigin,
	float DrawRatio,
	int32 ChargeTier)
{
	TArray<FVector> PathPoints;
	FHitResult ImpactHit;
	PredictTrajectory(AvatarActor, MuzzleOrigin, DrawRatio, ChargeTier, PathPoints, ImpactHit);

	LastPathPoints = MoveTemp(PathPoints);
	LastImpactHit = ImpactHit;
	bIsArcActive = true;

	UpdateSplineFromPoints(LastPathPoints);

	const bool bSplineMeshAllowed = bEnableSplineMeshRibbon && CVarEnableSplineMeshRibbon.GetValueOnGameThread();
	if (bSplineMeshAllowed) { UpdateSplineMeshRibbon(ChargeTier); }
	else { ClearSplineMeshes(); }

	if (TrajectoryNiagaraSystemAsset && LastPathPoints.Num() > 1 && EnsureNiagaraComponent())
	{
		UpdateNiagaraParameters(ChargeTier);
	}

	const bool bNiagaraActive = TrajectoryNiagaraComponent && TrajectoryNiagaraComponent->IsActive();
	const bool bDebugAllowed = bEnableViewportArcRender && CVarEnableViewportArcRender.GetValueOnGameThread();
	if (bDebugAllowed && !bSplineMeshAllowed && !bNiagaraActive)
	{
		DrawDebugFallback(ChargeTier);
	}

	OnTrajectoryUpdated.Broadcast(LastPathPoints, LastImpactHit, ChargeTier);
}

void UThreadTalesBowTrajectoryComponent::ClearTrajectoryArc()
{
	const bool bWasActive = bIsArcActive;
	bIsArcActive = false;
	LastPathPoints.Reset();
	LastImpactHit.Init();
	ClearSplineMeshes();

	if (TrajectorySplineComponent)
	{
		TrajectorySplineComponent->ClearSplinePoints(true);
	}
	if (TrajectoryNiagaraComponent && TrajectoryNiagaraComponent->IsActive())
	{
		TrajectoryNiagaraComponent->Deactivate();
	}

	if (bWasActive)
	{
		OnTrajectoryCleared.Broadcast();
	}
}

float UThreadTalesBowTrajectoryComponent::GetSplineLength() const
{
	return TrajectorySplineComponent ? TrajectorySplineComponent->GetSplineLength() : 0.0f;
}

FVector UThreadTalesBowTrajectoryComponent::GetSplineLocationAtDistance(float Distance) const
{
	return TrajectorySplineComponent ? TrajectorySplineComponent->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) : FVector::ZeroVector;
}

FVector UThreadTalesBowTrajectoryComponent::GetSplineDirectionAtDistance(float Distance) const
{
	return TrajectorySplineComponent ? TrajectorySplineComponent->GetDirectionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) : FVector::ForwardVector;
}

FColor UThreadTalesBowTrajectoryComponent::GetColorForTier(int32 ChargeTier) const
{
	switch (ChargeTier)
	{
	case 1: return FColor(20, 190, 255); // Soft Cyan / Azure (LV1)
	case 2: return FColor(255, 180, 10);  // Amber Gold (LV2)
	default: return FColor(255, 20, 30);  // Radiant Crimson (LV3)
	}
}

FLinearColor UThreadTalesBowTrajectoryComponent::GetLinearColorForTier(int32 ChargeTier) const
{
	switch (ChargeTier)
	{
	case 1: return FLinearColor(0.05f, 0.75f, 1.0f, 1.0f) * 4.0f;
	case 2: return FLinearColor(1.0f, 0.70f, 0.02f, 1.0f) * 5.0f;
	default: return FLinearColor(1.0f, 0.04f, 0.08f, 1.0f) * 6.5f;
	}
}

float UThreadTalesBowTrajectoryComponent::GetThicknessForTier(int32 ChargeTier) const
{
	switch (ChargeTier)
	{
	case 1: return 1.8f;
	case 2: return 3.0f;
	default: return 4.5f;
	}
}

void UThreadTalesBowTrajectoryComponent::SetEnableSplineMeshRibbon(bool bInEnable)
{
	if (bEnableSplineMeshRibbon != bInEnable)
	{
		bEnableSplineMeshRibbon = bInEnable;
		if (!bEnableSplineMeshRibbon)
		{
			ClearSplineMeshes();
		}
	}
}

void UThreadTalesBowTrajectoryComponent::SetEnableViewportArcRender(bool bInEnable)
{
	bEnableViewportArcRender = bInEnable;
}
