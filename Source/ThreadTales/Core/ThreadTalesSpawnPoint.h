// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "ThreadTalesSpawnPoint.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UArrowComponent;
class USphereComponent;
class UGameplayEffect;
class AThreadTalesCharacter;
class AThreadTalesPlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSpawnPointStateChangedDelegate, AThreadTalesSpawnPoint*, SpawnPoint, bool, bIsActive);

/**
 * AThreadTalesSpawnPoint
 *
 * Souls-like Bonfire / Checkpoint actor.
 * - Proximity zone detects players and sets this spawn point as their active respawn point.
 * - Applies passive health regeneration (GAS GameplayEffect) while the player remains in the zone.
 * - Safe for 2P Co-op: tracks individual PlayerControllers and ASCs.
 */
UCLASS()
class THREADTALES_API AThreadTalesSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AThreadTalesSpawnPoint();

	/** Returns the world transform where the player should spawn */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Spawn")
	FTransform GetSpawnTransform() const;

	/** Sets active state (authority only) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ThreadTales|Spawn")
	void SetSpawnPointActive(bool bNewActive);

	/** Returns true if this spawn point is active */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Spawn")
	bool IsSpawnPointActive() const { return bIsActive; }

	/** Broadcast when the active state changes */
	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Spawn")
	FOnSpawnPointStateChangedDelegate OnSpawnPointStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Visual event called when the bonfire is ignited/activated */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|Spawn")
	void OnSpawnPointActivated();

	/** Visual event called when the bonfire is extinguished/deactivated */
	UFUNCTION(BlueprintImplementableEvent, Category = "ThreadTales|Spawn")
	void OnSpawnPointDeactivated();

	UFUNCTION()
	void OnRep_IsActive();

	UFUNCTION()
	void HandleSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** Root scene component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** Static mesh for the bonfire / altar */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** Directional arrow marker defining player spawn position & rotation */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArrowComponent> SpawnMarker;

	/** Sphere collision for proximity detection and healing aura */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> ProximitySphere;

	/** Periodic health regeneration GameplayEffect applied while in radius */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ThreadTales|Spawn", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> HealthRegenEffectClass;

	/** Active status replicated to all clients for cosmetic effects */
	UPROPERTY(ReplicatedUsing = OnRep_IsActive, BlueprintReadOnly, Category = "ThreadTales|Spawn", meta = (AllowPrivateAccess = "true"))
	bool bIsActive = false;

private:
	/** Tracks active regeneration effect handles per character */
	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<AThreadTalesCharacter>, FActiveGameplayEffectHandle> ActiveRegenEffects;
};
