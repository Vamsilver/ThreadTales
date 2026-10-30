// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ThreadTalesBowTrajectoryComponent.generated.h"

class AThreadTalesWeapon_Bow;
class UThreadTalesBowConfig;
class UNiagaraComponent;
class UNiagaraSystem;
class USplineComponent;
class USplineMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBowTrajectoryUpdated, const TArray<FVector>&, PathPoints, const FHitResult&, ImpactHit, int32, ChargeTier);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBowTrajectoryCleared);

/**
 * AAA Ballistic Trajectory Predictor & Tracer Arc Visualizer for Bow Hunter (ThreadTales).
 * Implements Option 3.3:
 * - TPS Crosshair Convergence from camera to world hit point with 2m blind-spot protection.
 * - Physics prediction via UGameplayStatics::PredictProjectilePathByTraceChannel.
 * - Dynamic 3D tracer arc rendering with charge tier color progression (LV1..LV3).
 * - Impact marker projection on surfaces.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class THREADTALES_API UThreadTalesBowTrajectoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UThreadTalesBowTrajectoryComponent();

	/** Calculates TPS aim convergence target point and normalized launch direction from muzzle origin */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Bow|Aiming", meta = (ToolTip = "Calculates TPS aim convergence target point and normalized launch direction."))
	bool CalculateAimConvergence(const AActor* AvatarActor, const FVector& MuzzleOrigin, FVector& OutTargetLocation, FVector& OutLaunchDirection) const;

	/** Predicts ballistic trajectory of arrow given current draw state */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Bow|Aiming", meta = (ToolTip = "Predicts ballistic flight path of arrow fired with current tension."))
	bool PredictTrajectory(const AActor* AvatarActor, const FVector& MuzzleOrigin, float DrawRatio, int32 ChargeTier, TArray<FVector>& OutPathPoints, FHitResult& OutImpactHit) const;

	/** Updates and renders the dynamic glowing tracer arc and 3D surface impact marker */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Bow|Aiming", meta = (ToolTip = "Updates and displays dynamic tracer arc and impact reticle."))
	void UpdateTrajectoryArc(const AActor* AvatarActor, const FVector& MuzzleOrigin, float DrawRatio, int32 ChargeTier);

	/** Clears and hides active tracer arc and impact marker */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Bow|Aiming", meta = (ToolTip = "Hides active trajectory arc and impact marker."))
	void ClearTrajectoryArc();

	/** Returns whether trajectory arc is currently actively displaying */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Bow|Aiming")
	bool IsArcActive() const { return bIsArcActive; }

	/** Returns last computed impact hit result */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Bow|Aiming")
	const FHitResult& GetLastImpactHit() const { return LastImpactHit; }

	/** Returns last computed path points */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Bow|Aiming")
	const TArray<FVector>& GetLastPathPoints() const { return LastPathPoints; }

	/** Returns the active procedural spline component driving Niagara trajectory */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Bow|Aiming")
	USplineComponent* GetTrajectorySplineComponent() const { return TrajectorySplineComponent; }

	/** Returns length of current predicted trajectory spline in cm */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Bow|Aiming")
	float GetSplineLength() const;

	/** Samples world location along predicted trajectory spline at given distance */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Bow|Aiming")
	FVector GetSplineLocationAtDistance(float Distance) const;

	/** Samples world direction along predicted trajectory spline at given distance */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Bow|Aiming")
	FVector GetSplineDirectionAtDistance(float Distance) const;

	/** Enables or disables physical 3D spline mesh ribbon visualization at runtime */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Bow|Trajectory|Ribbon", meta = (ToolTip = "Enables or disables 3D spline mesh ribbon visualization."))
	void SetEnableSplineMeshRibbon(bool bInEnable);

	/** Enables or disables viewport debug arc visualization at runtime */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Bow|Trajectory", meta = (ToolTip = "Enables or disables debug 3D line drawing."))
	void SetEnableViewportArcRender(bool bInEnable);

	// ==========================================
	// CONFIGURATION & TUNING
	// ==========================================

	/** Maximum distance in cm for camera line trace convergence */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Trajectory", meta = (ClampMin = "1000.0", ToolTip = "Max camera line trace distance in cm for TPS convergence."))
	float MaxConvergenceDistance = 15000.0f;

	/** Blind spot distance forward from avatar in cm below which close hits are projected forward (50m) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Trajectory", meta = (ClampMin = "50.0", ToolTip = "Minimum forward distance in cm to prevent shots digging into ground under avatar."))
	float BlindSpotForwardThreshold = 200.0f;

	/** Safe forward projection distance in cm when hit is inside blind spot */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Trajectory", meta = (ClampMin = "1000.0", ToolTip = "Forward projection distance when blind spot is triggered."))
	float BlindSpotSafeDistance = 5000.0f;

	/** Max simulation duration in seconds for trajectory predictor */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Trajectory", meta = (ClampMin = "0.5", ClampMax = "5.0", ToolTip = "Maximum simulation time in seconds for path prediction."))
	float MaxSimTime = 2.0f;

	/** Simulation frequency (samples per second) for projectile path */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Trajectory", meta = (ClampMin = "10.0", ClampMax = "60.0", ToolTip = "Samples per second along trajectory arc."))
	float SimFrequency = 25.0f;

	/** Collision radius of arrow proxy in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Trajectory", meta = (ClampMin = "1.0", ToolTip = "Collision sphere radius in cm for trajectory sweep."))
	float ProjectileRadius = 4.0f;

	/** Whether to render debug 3D lines and impact reticle in viewport if Niagara is not active */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Bow|Trajectory", meta = (ToolTip = "Whether to render 3D arc lines and impact circle in viewport."))
	bool bEnableViewportArcRender = true;

	/** Procedural Spline component driving high-fidelity curve interpolation for Niagara Ribbon */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ThreadTales|Bow|Trajectory|Spline")
	TObjectPtr<USplineComponent> TrajectorySplineComponent;

	/** Active Niagara component spawned for trajectory arc visualization */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ThreadTales|Bow|Trajectory|VFX")
	TObjectPtr<UNiagaraComponent> TrajectoryNiagaraComponent;

	/** Niagara system asset used for the Silk Thread trajectory arc (NS_BowTrajectory_SilkArc) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Trajectory|VFX", meta = (ToolTip = "Niagara system asset for glowing ribbon tracer arc."))
	TObjectPtr<UNiagaraSystem> TrajectoryNiagaraSystemAsset;

	/** Base ribbon width in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Trajectory|VFX", meta = (ClampMin = "0.5", ClampMax = "50.0", ToolTip = "Base width in cm of the silk ribbon arc."))
	float BaseRibbonWidth = 3.5f;

	/** Emissive intensity multiplier for Niagara material */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Trajectory|VFX", meta = (ClampMin = "1.0", ClampMax = "100.0", ToolTip = "HDR emissive bloom multiplier for Niagara ribbon."))
	float EmissiveIntensity = 20.0f;

	/** Whether to render 3D spline mesh ribbon along trajectory arc */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Bow|Trajectory|Ribbon", meta = (ToolTip = "Whether to render 3D spline mesh ribbon along trajectory arc."))
	bool bEnableSplineMeshRibbon = false;

	/** Static mesh used for spline ribbon segments (defaults to Cylinder) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Trajectory|Ribbon", meta = (ToolTip = "Static mesh deformed along spline segments."))
	TObjectPtr<UStaticMesh> SplineMeshAsset;

	/** Material applied to spline mesh ribbon (defaults to M_SimpleGlow) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Trajectory|Ribbon", meta = (ToolTip = "Material for glowing silk ribbon."))
	TObjectPtr<UMaterialInterface> SplineMeshMaterial;

	/** Physical diameter in cm of the silk ribbon */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Trajectory|Ribbon", meta = (ClampMin = "0.5", ClampMax = "20.0", ToolTip = "Diameter in cm of the silk ribbon arc."))
	float RibbonMeshDiameter = 2.5f;

	// ==========================================
	// EVENT DELEGATES
	// ==========================================

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Bow|Trajectory|Events")
	FOnBowTrajectoryUpdated OnTrajectoryUpdated;

	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Bow|Trajectory|Events")
	FOnBowTrajectoryCleared OnTrajectoryCleared;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Ensures that TrajectorySplineComponent is instantiated and registered on owner */
	bool EnsureSplineComponent();

	/** Ensures that TrajectoryNiagaraComponent is instantiated and registered on owner */
	bool EnsureNiagaraComponent();

	/** Populates procedural spline with predicted trajectory points */
	void UpdateSplineFromPoints(const TArray<FVector>& PathPoints);

	/** Updates and deforms 3D spline mesh components along the active trajectory */
	void UpdateSplineMeshRibbon(int32 ChargeTier);

	/** Hides and resets all pooled spline mesh components */
	void ClearSplineMeshes();

	/** Updates parameters on active Niagara component */
	void UpdateNiagaraParameters(int32 ChargeTier);

	/** Fallback debug render in viewport when Niagara is not active */
	void DrawDebugFallback(int32 ChargeTier);

	/** Resolves the owning bow weapon */
	AThreadTalesWeapon_Bow* GetOwningBow() const;

	/** Resolves bow configuration data asset */
	const UThreadTalesBowConfig* ResolveBowConfig() const;

	/** Resolves arc color for specified charge tier */
	FColor GetColorForTier(int32 ChargeTier) const;

	/** Resolves saturated HDR linear color for specified charge tier */
	FLinearColor GetLinearColorForTier(int32 ChargeTier) const;

	/** Resolves arc line thickness for specified charge tier */
	float GetThicknessForTier(int32 ChargeTier) const;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> SplineMeshPool;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RibbonMID;

	UPROPERTY(Transient)
	TArray<FVector> LastPathPoints;

	UPROPERTY(Transient)
	FHitResult LastImpactHit;

	bool bIsArcActive = false;
};
