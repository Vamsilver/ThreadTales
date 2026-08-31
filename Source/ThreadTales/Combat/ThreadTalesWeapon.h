// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayAbilitySpec.h"
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

	/** Updates the weapon combat state */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Weapon")
	void SetWeaponState(EWeaponCombatState NewState);

	/** Returns current weapon combat state */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon")
	EWeaponCombatState GetWeaponState() const { return WeaponState; }

	/** True if weapon is currently drawn and ready in hand */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|Weapon")
	bool IsDrawn() const { return WeaponState == EWeaponCombatState::Drawn; }

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

	/** Event triggered when weapon state changes */
	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|Weapon")
	FOnWeaponStateChanged OnWeaponStateChanged;

protected:
	virtual void BeginPlay() override;

	/** Root Scene Component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	/** The blade / main mesh of the weapon */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* WeaponMesh;

	/** The scabbard / sheath mesh (stays attached to hip/back) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* SheathMesh;

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

	/** Montage played when drawing weapon */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Animations")
	UAnimMontage* DrawMontage;

	/** Montage played when sheathing weapon */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Animations")
	UAnimMontage* SheathMontage;

	/** Single Montage containing the combo attack chain with sections */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Animations")
	UAnimMontage* ComboAttackMontage;

	/** Array of individual combo strike montages (e.g. Strike1, Strike2, Strike3, Strike4) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Animations")
	TArray<UAnimMontage*> ComboStrikeMontages;

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

	/** Handles to the granted abilities so we can remove them later */
	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;

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

	/** Helper to register weapon's IMC with local player subsystem */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Input")
	void AddWeaponMappingContext();

	/** Helper to unregister weapon's IMC from local player subsystem */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Input")
	void RemoveWeaponMappingContext();

	virtual void OnRep_Owner() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Cache of the owner's ASC */
	UPROPERTY()
	UAbilitySystemComponent* OwnerASC;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
