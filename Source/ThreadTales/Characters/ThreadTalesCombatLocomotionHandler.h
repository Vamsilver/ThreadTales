// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThreadTalesCombatLocomotionHandler.generated.h"

class AThreadTalesBaseCharacter;

/**
 * Dedicated component handling GASPALS overlay poses, aim stances, strafing modes,
 * and camera alignment replication for AThreadTalesCharacter.
 * Extracted from AThreadTalesCharacter to decouple locomotion stances from pawn lifecycle.
 */
UCLASS(ClassGroup=(ThreadTales), meta=(BlueprintSpawnableComponent, DisplayName="ThreadTales Combat Locomotion Handler"))
class THREADTALES_API UThreadTalesCombatLocomotionHandler : public UActorComponent
{
	GENERATED_BODY()

public:
	UThreadTalesCombatLocomotionHandler();

	/** Sets the GASPALS Overlay Pose by raw byte value (0 = Default, 11 = Katana, 4 = Bow) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Overlay")
	void SetOverlayPose(uint8 NewOverlayPose);

	/** Sets the GASPALS Overlay Pose by name ("Default", "Katana", "Bow") */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Overlay")
	void SetOverlayPoseByName(FName PoseName);

	/** Returns current GASPALS Overlay Pose byte value */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Overlay")
	uint8 GetOverlayPose() const;

	/** Static helper to map pose name to byte value (cached or resolved from Enum_OverlayPose) */
	static uint8 GetOverlayPoseValueByName(FName PoseName);

	/** Configures aim stance and updates rotation modes */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Stance")
	void SetAimInputDown(bool bAiming);

	/** Configures strafe locomotion mode */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Stance")
	void SetStrafeInputDown(bool bStrafing);

	// Network Replication RPCs
	UFUNCTION(Server, Reliable)
	void ServerSetOverlayPose(uint8 NewOverlayPose);

	UFUNCTION(Server, Reliable)
	void ServerSetAimInputDown(bool bAiming);

	UFUNCTION(Server, Reliable)
	void ServerSetStrafeInputDown(bool bStrafing);

protected:
	/** Helper to obtain owning AThreadTalesBaseCharacter */
	AThreadTalesBaseCharacter* GetOwningCharacter() const;
};
