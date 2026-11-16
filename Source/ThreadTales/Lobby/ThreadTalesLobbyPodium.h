// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/ThreadTalesPlayerState.h"
#include "ThreadTalesLobbyPodium.generated.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UAnimSequence;
class AThreadTalesPlayerState;
class AThreadTalesWeapon;

/**
 * 3D Character Podium for Lvl_Lobby.
 * Displays character model on a pedestal, spawns real game weapons (BP_Weapon_Katana / BP_Weapon_Bow),
 * and dynamically reacts to player role selection and ready state with equip/unequip animations.
 */
UCLASS()
class THREADTALES_API AThreadTalesLobbyPodium : public AActor
{
	GENERATED_BODY()

public:
	AThreadTalesLobbyPodium();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Configures or queries which player slot this podium represents (0 = P1, 1 = P2) */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Lobby")
	int32 GetAssignedSlotIndex() const { return AssignedSlotIndex; }

	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Lobby")
	void SetAssignedSlotIndex(int32 InSlotIndex);

	/** Forces immediate visual update of character mesh and animations */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Lobby")
	void RefreshPodiumVisuals(bool bForceRestartAnim = false);

	/** Current visual role displayed on this podium */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Lobby")
	EThreadTalesCharacterRole GetCurrentDisplayedRole() const { return CurrentRole; }

	/** Current ready state displayed on this podium */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Lobby")
	bool IsDisplayingReady() const { return bCurrentReady; }

	/** Returns the Render Target used by this podium's Scene Capture */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Lobby")
	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

	/** Sets or changes the Render Target for Scene Capture */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Lobby")
	void SetRenderTarget(UTextureRenderTarget2D* InRenderTarget);

	/** Ensures RenderTarget is assigned to SceneCapture */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Lobby")
	void UpdateCaptureTarget();

	/** Returns currently equipped weapon actor */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Lobby")
	AThreadTalesWeapon* GetCurrentWeapon() const { return CurrentWeapon; }

	// ==========================================
	// Presentation Components
	// ==========================================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Components")
	TObjectPtr<UStaticMeshComponent> BasePedestalMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Components")
	TObjectPtr<USkeletalMeshComponent> CharacterMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ThreadTales|Components")
	TObjectPtr<USceneCaptureComponent2D> SceneCapture;

	/** Render Target asset into which SceneCapture renders this podium */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Lobby")
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	/** Player slot index: 0 = Player 1 (Left), 1 = Player 2 (Right) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Lobby")
	int32 AssignedSlotIndex = 0;

	// ==========================================
	// Weapon Classes
	// ==========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Weapons")
	TSubclassOf<AThreadTalesWeapon> KatanaWeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Weapons")
	TSubclassOf<AThreadTalesWeapon> BowWeaponClass;

	// ==========================================
	// Animation Sequences
	// ==========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Animations")
	TObjectPtr<UAnimSequence> SwordsmanIdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Animations")
	TObjectPtr<UAnimSequence> SwordsmanReadyAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Animations")
	TObjectPtr<UAnimSequence> SwordsmanEquipAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Animations")
	TObjectPtr<UAnimSequence> SwordsmanUnequipAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Animations")
	TObjectPtr<UAnimSequence> ArcherIdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Animations")
	TObjectPtr<UAnimSequence> ArcherReadyAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Animations")
	TObjectPtr<UAnimSequence> ArcherEquipAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Animations")
	TObjectPtr<UAnimSequence> ArcherUnequipAnim;

private:
	UPROPERTY(Transient)
	EThreadTalesCharacterRole CurrentRole = EThreadTalesCharacterRole::None;

	UPROPERTY(Transient)
	bool bCurrentReady = false;

	UPROPERTY(Transient)
	bool bIsPlayingTransition = false;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CurrentlyPlayingAnim = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AThreadTalesWeapon> CurrentWeapon = nullptr;

	FTimerHandle TransitionTimerHandle;
	FTimerHandle WeaponAttachTimerHandle;

	EThreadTalesCharacterRole ResolveSlotRole() const;
	bool ResolveSlotReady() const;
	AThreadTalesPlayerState* ResolvePlayerState() const;

	void UpdateWeaponForRole(EThreadTalesCharacterRole InRole);
	void DestroyWeapon();

	void PlayEquipTransition();
	void PlayUnequipTransition();
	void OnEquipWeaponAttach();
	void OnUnequipWeaponAttach();
	void OnEquipFinished();
	void OnUnequipFinished();

	void ApplySwordsmanVisuals(bool bReady, bool bForceRestartAnim);
	void ApplyArcherVisuals(bool bReady, bool bForceRestartAnim);
	void ApplyEmptyVisuals();
};
