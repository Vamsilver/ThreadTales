// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/ThreadTalesCharacter.h"
#include "ThreadTalesDummy.generated.h"

/**
 * Combat Training Dummy that can be placed in the world.
 * Can be made immortal via Details panel or die with ragdoll and optional auto-respawn.
 */
UCLASS()
class THREADTALES_API AThreadTalesDummy : public AThreadTalesCharacter
{
	GENERATED_BODY()

public:
	AThreadTalesDummy();

	/** If true, dummy cannot die (health is never reduced) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Combat", meta = (DisplayName = "Is Immortal"))
	bool bIsImmortal;

	/** If true, automatically respawns and resets health after death */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Combat", meta = (DisplayName = "Auto Respawn"))
	bool bAutoRespawn;

	/** Delay in seconds before respawning after death */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Combat", meta = (DisplayName = "Respawn Delay", EditCondition = "bAutoRespawn", ClampMin = "1.0"))
	float RespawnDelay;

	/** Resets dummy back to initial spawn state and full health */
	UFUNCTION(BlueprintCallable, Category = "Dummy|Combat")
	virtual void Respawn();

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	virtual void Die(AActor* KillerActor) override;

protected:
	virtual void BeginPlay() override;

private:
	FTransform InitialSpawnTransform;
	FTransform InitialMeshRelativeTransform;
	FTimerHandle RespawnTimerHandle;
};
