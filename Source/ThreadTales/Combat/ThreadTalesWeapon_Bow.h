// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/ThreadTalesWeapon.h"
#include "ThreadTalesWeapon_Bow.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class UArrowComponent;
class UCurveFloat;
class UThreadTalesBowConfig;
class UAnimSequence;
class UThreadTalesBowTrajectoryComponent;

/**
 * Archer archetype bow weapon with skeletal mesh string deformation,
 * dynamic draw tension control, arrow spawn reference, and quiver mounting.
 */
UCLASS(BlueprintType, Blueprintable)
class THREADTALES_API AThreadTalesWeapon_Bow : public AThreadTalesWeapon
{
	GENERATED_BODY()

public:
	AThreadTalesWeapon_Bow();

	virtual void Equip(AActor* NewOwner) override;
	virtual void Unequip() override;

	virtual void AttachToHand() override;
	virtual void AttachToSheath() override;

	/** Sets the dynamic string draw tension ratio (0.0 = relaxed, 1.0 = fully drawn) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Weapon|Bow", meta = (ToolTip = "Sets the dynamic bowstring draw ratio (0.0 = relaxed, 1.0 = fully drawn)"))
	void SetDrawRatio(float InDrawRatio);

	/** Starts smooth frame-by-frame bowstring draw tension progression toward target (1.0) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Weapon|Bow", meta = (ToolTip = "Starts smooth continuous bowstring draw tension progression"))
	void StartDrawingTension(float InDuration = 0.65f);

	/** Instantly snaps bowstring draw tension and right hand back to rest (0.0) upon release */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Weapon|Bow", meta = (ToolTip = "Snaps bowstring draw tension back to rest position"))
	void SnapDrawTensionToRest();

	/** Returns current dynamic string draw tension ratio */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon|Bow", meta = (ToolTip = "Returns current bowstring draw tension ratio (0.0 to 1.0)"))
	float GetDrawRatio() const { return CurrentDrawRatio; }

	/** Data-Driven Bow Configuration asset (DA_BowConfig) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Balancing", meta = (ToolTip = "Data-Driven Bow Configuration asset (DA_BowConfig) defining 3-tier charge parameters and montages."))
	TObjectPtr<UThreadTalesBowConfig> BowConfig;

	/** Returns the assigned UThreadTalesBowConfig data asset */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Bow|Balancing", meta = (ToolTip = "Returns the assigned UThreadTalesBowConfig data asset."))
	const UThreadTalesBowConfig* GetBowConfig() const { return BowConfig; }

	/** Returns the close-quarters bow parry deflection montage from assigned config (0 = Left, 1 = Right) */
	virtual UAnimMontage* GetParryMontage(int32 DirectionIndex = 0) const override;

	/** Returns the point-blank parry counter-shot montage from assigned config (0 = Left, 1 = Right) */
	UAnimMontage* GetParryCounterMontage(int32 DirectionIndex = 0) const;

	/** Returns the UpperBody aim ready montage from assigned config */
	UAnimMontage* GetAimReadyMontage() const;

	/** Returns the UpperBody draw hold montage from assigned config */
	UAnimMontage* GetDrawHoldMontage() const;

	/** Optional tension curve (0.0 to 1.0) overriding default mathematical ease curve */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Balancing", meta = (ToolTip = "Optional tension curve (0.0 to 1.0) overriding default ease curve"))
	TObjectPtr<UCurveFloat> TensionCurve;

	/** Evaluates dynamic tension curve (or smooth mathematical ease) for normalized progress alpha (0.0 to 1.0) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Bow|Balancing", meta = (ToolTip = "Evaluates tension curve or mathematical ease for progress alpha"))
	float EvaluateTensionCurve(float InAlpha) const;

	/** Total duration in seconds configured for current draw tension charge */
	/** Total duration in seconds configured for current draw tension charge */
	float GetDrawDuration() const { return DrawDuration; }

	/** Elapsed time in seconds since draw tension charge began */
	float GetDrawElapsed() const { return DrawElapsed; }

	/** Returns the world transform of the arrow spawn point */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon|Bow", meta = (ToolTip = "Returns world transform where arrows should be spawned / nocked"))
	FTransform GetArrowSpawnTransform() const;

	/** Returns the world location of the trajectory start point (arrow nock / bowstring socket) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon|Bow", meta = (ToolTip = "Returns world location of the arrow nock on the bowstring"))
	FVector GetTrajectoryStartLocation() const;

	/** Accessor for the bow skeletal mesh component */
	USkeletalMeshComponent* GetBowSkeletalMesh() const { return BowSkeletalMesh; }

	/** Accessor for the quiver static mesh component */
	UStaticMeshComponent* GetQuiverMesh() const { return QuiverMesh; }

	/** Accessor for the arrow spawn point component */
	UArrowComponent* GetArrowSpawnPoint() const { return ArrowSpawnPoint; }

	/** Accessor for the cosmetic nocked arrow static mesh component */
	UStaticMeshComponent* GetNockedArrowMesh() const { return NockedArrowMesh; }

	/** Sets visibility of the nocked cosmetic arrow while aiming */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Weapon|Bow", meta = (ToolTip = "Shows or hides the nocked cosmetic arrow on the bow / hand"))
	void SetNockedArrowVisible(bool bVisible);

	/** Returns whether nocked arrow is currently visible */
	bool IsNockedArrowVisible() const;

	/** Phase 1: Attaches nocked arrow to character right hand bone (quiver pull) */
	void AttachNockedArrowToHand();

	/** Phase 2: Attaches nocked arrow to bowstring bone on BowSkeletalMesh (draw) */
	void AttachNockedArrowToString();

	/** Accessor for the ballistic trajectory predictor and tracer arc visualizer component */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon|Bow")
	UThreadTalesBowTrajectoryComponent* GetTrajectoryComponent() const { return TrajectoryComponent; }

	/** Calculates TPS aim convergence target point and normalized launch direction from muzzle origin */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Weapon|Bow", meta = (ToolTip = "Calculates TPS aim convergence target point and normalized launch direction."))
	bool GetAimConvergenceTargetAndDirection(const AActor* AvatarActor, const FVector& MuzzleOrigin, FVector& OutTargetLocation, FVector& OutLaunchDirection) const;

	/** Updates dynamic tracer arc and impact reticle for current draw state */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Weapon|Bow", meta = (ToolTip = "Updates dynamic tracer arc for current draw state."))
	void UpdateTrajectoryArc(const AActor* AvatarActor, float DrawRatio, int32 ChargeTier);

	/** Clears and hides active tracer arc and impact marker */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Weapon|Bow", meta = (ToolTip = "Clears and hides active tracer arc."))
	void ClearTrajectoryArc();

	/** Toggles the physical C++ spline mesh trajectory ribbon on or off */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Weapon|Bow", meta = (ToolTip = "Toggles the physical C++ spline mesh trajectory ribbon on or off."))
	void SetSplineMeshRibbonEnabled(bool bInEnable);

	/** Returns whether the physical C++ spline mesh ribbon is currently enabled */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon|Bow", meta = (ToolTip = "Returns whether the physical C++ spline mesh ribbon is currently enabled."))
	bool IsSplineMeshRibbonEnabled() const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Applies the current draw ratio to the underlying Bow_AnimBP */
	void ApplyDrawRatioToAnimInstance();

	UFUNCTION()
	void OnRep_CurrentDrawRatio();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	/** Bow skeletal mesh displaying deformation and string flex */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Weapon|Components", meta = (ToolTip = "Skeletal mesh for bow string and limb animation"))
	TObjectPtr<USkeletalMeshComponent> BowSkeletalMesh;

	/** Quiver static mesh attached to character back / spine */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Weapon|Components", meta = (ToolTip = "Optional static mesh for arrow quiver worn on torso/back"))
	TObjectPtr<UStaticMeshComponent> QuiverMesh;

	/** Arrow indicating projectile launch location and direction */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Weapon|Components", meta = (ToolTip = "Origin and forward trajectory vector for spawned arrows"))
	TObjectPtr<UArrowComponent> ArrowSpawnPoint;

	/** Cosmetic arrow static mesh nocked on the bow while aiming */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Weapon|Components", meta = (ToolTip = "Cosmetic arrow mesh nocked in the bow while aiming"))
	TObjectPtr<UStaticMeshComponent> NockedArrowMesh;

	/** Ballistic trajectory predictor and tracer arc visualizer component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Weapon|Components", meta = (ToolTip = "Ballistic trajectory predictor and tracer arc visualizer component"))
	TObjectPtr<UThreadTalesBowTrajectoryComponent> TrajectoryComponent;

	/** Replicated string draw ratio (0.0 = relaxed, 1.0 = full tension) */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentDrawRatio, BlueprintReadOnly, Category = "ThreadTales|Weapon|Bow", meta = (ToolTip = "Normalized draw tension ratio (0.0 to 1.0)"))
	float CurrentDrawRatio = 0.0f;

	/** Total time in seconds to reach full bowstring tension (1.0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|Balancing", meta = (ClampMin = "0.1", ToolTip = "Time in seconds to reach full draw tension (1.0)"))
	float DrawDuration = 0.90f;

	/** Elapsed time in seconds during current tension charge */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Bow|State")
	float DrawElapsed = 0.0f;

	/** Whether to drive bow draw tension from character anim curves ("Draw" / "Enable_SpineRotation") in Tick (false when driven by GAS GA_BowDraw) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Weapon|Bow", meta = (ToolTip = "Whether to evaluate anim curves for bow draw in Tick. False when driven by GAS abilities."))
	bool bDriveDrawFromAnimCurve = false;

	/** Relative transform offset applied to BowSkeletalMesh when sheathed on spine_05 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Weapon|Sockets", meta = (ToolTip = "Relative transform applied to BowSkeletalMesh when sheathed on spine_05"))
	FTransform BowSheathRelativeTransform;

	/** Relative transform offset applied to BowSkeletalMesh when held in hand (overlay_bow) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Weapon|Sockets", meta = (ToolTip = "Relative transform applied to BowSkeletalMesh when held in hand"))
	FTransform BowHandRelativeTransform;

	/** Socket name on character skeleton where Quiver is attached */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Weapon|Sockets", meta = (ToolTip = "Socket name on character skeleton for Quiver attachment"))
	FName QuiverSocketName;

	/** Relative transform offset applied to QuiverMesh when attached to character */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Weapon|Sockets", meta = (ToolTip = "Relative transform applied to QuiverMesh on character skeleton"))
	FTransform QuiverRelativeTransform;

	/** Socket name on character mesh or bow mesh where the nocked arrow is attached during draw */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Weapon|Sockets", meta = (ToolTip = "Socket or bone name on bow mesh (Bow_String) where the nocked arrow is attached during draw"))
	FName NockedArrowSocketName = FName("Bow_String");

	/** Relative location offset for nocked arrow mesh so the nock rests on the string socket */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Weapon|Sockets", meta = (ToolTip = "Offset applied to nocked arrow mesh so the nock rests on the string socket"))
	FVector NockedArrowRelativeOffset = FVector::ZeroVector;

	/** Relative rotation applied to nocked arrow mesh so shaft aligns from string through bow riser */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Weapon|Sockets", meta = (ToolTip = "Relative rotation applied to nocked arrow mesh so shaft aligns from string through bow riser"))
	FRotator NockedArrowRelativeRotation = FRotator(0.0f, -90.0f, 0.0f);

	/** Animation sequence driving Bow_String deformation from 0.0s (rest) to 1.0s (full draw) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Weapon|Bow", meta = (ToolTip = "Animation sequence driving Bow_String deformation from 0.0s (rest) to 1.0s (full draw)"))
	TObjectPtr<UAnimSequence> BowDrawAnimSequence;

private:
	/** Target draw ratio when actively pulling string tension */
	float TargetDrawRatio = 0.0f;

	/** Rate of change per second for bowstring tension */
	float DrawInterpolationSpeed = 0.0f;

	/** Whether bowstring tension is actively charging */
	bool bIsChargingTension = false;
};
