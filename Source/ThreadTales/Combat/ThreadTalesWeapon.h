// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "ThreadTalesWeapon.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UAnimMontage;
class UStaticMeshComponent;
class UInputMappingContext;
class UChooserTable;
class UPoseSearchDatabase;

/**
 * Combat state of the weapon (sheathed on hip/back, being drawn, held in hand, or being sheathed).
 */
UENUM(BlueprintType)
enum class EWeaponCombatState : uint8
{
	Sheathed    UMETA(DisplayName = "Sheathed"),
	Drawing     UMETA(DisplayName = "Drawing"),
	Drawn       UMETA(DisplayName = "Drawn / In Combat"),
	Sheathing   UMETA(DisplayName = "Sheathing")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponStateChanged, EWeaponCombatState, NewState);

UCLASS(Abstract, BlueprintType, Blueprintable)
class THREADTALES_API AThreadTalesWeapon : public AActor
{
	GENERATED_BODY()
	
public:	
	AThreadTalesWeapon();

	/** Called when this weapon is equipped by an actor */
	virtual void Equip(AActor* NewOwner);

	/** Called when this weapon is unequipped */
	virtual void Unequip();

	/** Re-attaches weapon blade mesh to hand socket */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Weapon")
	virtual void AttachToHand();

	/** Re-attaches weapon blade mesh to sheath / hip socket */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Weapon")
	virtual void AttachToSheath();

	/** Re-attaches sheath mesh to hand socket (during equip/unequip) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Weapon")
	virtual void AttachSheathToHand();

	/** Re-attaches sheath mesh to hip/spine socket */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Weapon")
	virtual void AttachSheathToHip();

	/** Updates the weapon combat state */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Weapon")
	void SetWeaponState(EWeaponCombatState NewState);

	/** Returns current weapon combat state */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon")
	EWeaponCombatState GetWeaponState() const { return WeaponState; }

	/** True if weapon is currently drawn and ready in hand */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon")
	bool IsDrawn() const { return WeaponState == EWeaponCombatState::Drawn; }

	/** True if weapon is currently in the process of being drawn */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon")
	bool IsDrawing() const { return WeaponState == EWeaponCombatState::Drawing; }

	/** True if weapon is currently in the process of being sheathed */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon")
	bool IsSheathing() const { return WeaponState == EWeaponCombatState::Sheathing; }

	/** True if weapon is sheathed in scabbard / hip */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon")
	bool IsSheathed() const { return WeaponState == EWeaponCombatState::Sheathed; }

	/** Getters for animation montages */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon")
	UAnimMontage* GetDrawMontage() const { return DrawMontage; }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon")
	UAnimMontage* GetSheathMontage() const { return SheathMontage; }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon")
	UAnimMontage* GetComboAttackMontage() const { return ComboAttackMontage; }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon")
	const TArray<UAnimMontage*>& GetComboStrikeMontages() const { return ComboStrikeMontages; }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon")
	UAnimMontage* GetDodgeAttackMontage() const { return DodgeAttackMontage; }

	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon")
	UAnimMontage* GetSprintAttackMontage() const { return SprintAttackMontage; }

	/** Returns weapon-specific parry deflect montage (if overridden by weapon subclass) */
	virtual UAnimMontage* GetParryMontage(int32 DirectionIndex = 0) const { return nullptr; }

	/** Returns true if dodge attack is enabled for this weapon */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon")
	bool IsDodgeAttackEnabled() const { return bEnableDodgeAttack; }

	/** Returns play rate multiplier for the dodge attack montage */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon")
	float GetDodgeAttackPlayRate() const { return DodgeAttackPlayRate; }

	/** Enables or disables the weapon hitbox (if supported by weapon subclass) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Weapon")
	virtual void SetHitboxEnabled(bool bEnabled) {}

	/** Returns primary attack gameplay tag */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon")
	FGameplayTag GetPrimaryAttackTag() const { return PrimaryAttackTag; }

	/** Returns the Chooser Table configured for this weapon's locomotion */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon")
	class UChooserTable* GetWeaponLocomotionChooserTable() const { return WeaponLocomotionChooserTable; }

	/** Returns the Pose Search Database configured for this weapon's locomotion */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon")
	class UPoseSearchDatabase* GetWeaponLocomotionDatabase() const { return WeaponLocomotionDatabase; }

	/** Name of the GASPALS Overlay Pose used when this weapon is drawn (e.g. "Katana", "Default") */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Overlay")
	FName OverlayPoseName;

	/** Returns overlay pose name for this weapon */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Overlay")
	FName GetOverlayPoseName() const { return OverlayPoseName; }

	/** Event triggered when weapon state changes */
	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Weapon")
	FOnWeaponStateChanged OnWeaponStateChanged;

public:
	virtual void Tick(float DeltaTime) override;

	/** Helper to register weapon's IMC with local player subsystem */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Input")
	void AddWeaponMappingContext();

	/** Helper to unregister weapon's IMC from local player subsystem */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Input")
	void RemoveWeaponMappingContext();

	/** Updates the blade material clip plane to mask out geometry inside the scabbard */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Weapon|Visuals")
	void UpdateBladeClipPlane();

protected:
	virtual void BeginPlay() override;

	/** Dynamic material instance for blade mesh (used for clip plane masking) */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BladeDynamicMaterial;

	/** Whether to dynamically clip the blade entering the sheath */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Weapon|Visuals")
	bool bEnableSheathClipPlane = true;

	/** Local space offset for the sheath mouth plane */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Weapon|Visuals")
	FVector ClipPlaneMouthOffset = FVector::ZeroVector;

	/** Root Scene Component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** The blade / main mesh of the weapon */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	/** The scabbard / sheath mesh (stays attached to hip/back) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SheathMesh;

	/** Whether this weapon uses a scabbard/sheath mesh and socket */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Weapon")
	bool bHasSheath = true;

	/** Whether this weapon starts directly drawn in hand upon equipping */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Weapon")
	bool bStartDrawn = false;

	/** Socket name on character skeleton where weapon is held in hand */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Sockets")
	FName HandSocketName;

	/** Socket name on character skeleton where scabbard/sheath is attached (can be None if no sheath) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Sockets")
	FName SheathSocketName;

	/** Socket name on character skeleton where scabbard/sheath is held in hand during draw/sheath */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Sockets")
	FName SheathHandSocketName = FName("scabbard_hand_lSocket");

	/** If true, the blade mesh attaches directly to SheathMesh as a child when sheathed, preserving rigid alignment */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Sockets")
	bool bAttachBladeToSheathMesh = true;

	/** Relative transform applied to blade mesh when docked inside SheathMesh (compensating for pivot delta between guard and throat) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Sockets")
	FTransform SheathRelativeTransform = FTransform(FRotator::ZeroRotator, FVector(0.56f, 13.7f, 0.0f), FVector::OneVector);

	/** Montage played when drawing weapon */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Animations")
	TObjectPtr<UAnimMontage> DrawMontage;

	/** Montage played when sheathing weapon */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Animations")
	TObjectPtr<UAnimMontage> SheathMontage;

	/** Single Montage containing the combo attack chain with sections */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Animations")
	TObjectPtr<UAnimMontage> ComboAttackMontage;

	/** Array of individual combo strike montages (e.g. Strike1, Strike2, Strike3, Strike4) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Animations")
	TArray<UAnimMontage*> ComboStrikeMontages;

	/** Montage played when executing a dodge attack */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Animations")
	TObjectPtr<UAnimMontage> DodgeAttackMontage;

	/** If true, character can execute a special dodge attack when attacking out of an evade */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Animations")
	bool bEnableDodgeAttack = true;

	/** Play rate multiplier for the dodge attack montage to adjust speed and snappiness (default: 1.5x) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThreadTales|Animations", meta = (ClampMin = 0.5, ClampMax = 2.5))
	float DodgeAttackPlayRate = 1.5f;

	/** Montage played when executing a sprint attack */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Animations")
	TObjectPtr<UAnimMontage> SprintAttackMontage;

	/** Chooser Table used for Motion Matching locomotion when this weapon is drawn */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|MotionMatching")
	TObjectPtr<class UChooserTable> WeaponLocomotionChooserTable;

	/** Pose Search Database used for Motion Matching locomotion when this weapon is drawn */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|MotionMatching")
	TObjectPtr<class UPoseSearchDatabase> WeaponLocomotionDatabase;

	/** The primary gameplay tag triggered by this weapon's primary attack action */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Abilities")
	FGameplayTag PrimaryAttackTag;

	/** The abilities granted when this weapon is equipped */
	UPROPERTY(EditDefaultsOnly, Category = "ThreadTales|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;

	/** Current weapon combat state */
	UPROPERTY(ReplicatedUsing = OnRep_WeaponState, VisibleInstanceOnly, BlueprintReadOnly, Category = "ThreadTales|Weapon")
	EWeaponCombatState WeaponState;

	UFUNCTION()
	void OnRep_WeaponState(EWeaponCombatState OldState);

	/** Input Mapping Context specific to this weapon (e.g. IMC_Combat, IMC_Katana, IMC_Slingshot) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Input")
	TObjectPtr<UInputMappingContext> WeaponMappingContext;

	/** Priority with which this weapon's IMC is added to EnhancedInputSubsystem */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Input")
	int32 WeaponMappingPriority = 1;

	/** If true, the IMC is only active while weapon is drawn in hand. If false, active whenever weapon is equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Input")
	bool bAddContextOnlyWhenDrawn = false;

	virtual void OnRep_Owner() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Cache of the owner's ASC */
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> OwnerASC;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** Handles to the granted abilities so we can remove them later */
	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;
};
