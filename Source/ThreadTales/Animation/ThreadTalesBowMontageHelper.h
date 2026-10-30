// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ThreadTalesBowMontageHelper.generated.h"

class UAnimMontage;
class UAnimSequenceBase;

/**
 * Dedicated helper function library for programmatic configuration and slot binding of bow combat montages.
 */
UCLASS()
class THREADTALES_API UThreadTalesBowMontageHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Configures a single AnimSequence into an AnimMontage track with proper AnimSegment, slot, and blend times */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Animation|Bow")
	static bool ConfigureSingleSequenceMontage(UAnimMontage* Montage, UAnimSequenceBase* AnimSeq, FName SlotName = FName("Spine"), float BlendInTime = 0.20f, float BlendOutTime = 0.20f);

	/** Configures a 3-phase hold montage with a Start section and an indefinitely looping Loop section */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Animation|Bow")
	static bool ConfigureStartLoopMontage(UAnimMontage* Montage, UAnimSequenceBase* StartSeq, UAnimSequenceBase* LoopSeq, FName SlotName = FName("Spine"), float StartPlayRate = 1.0f);

	/** Rebuilds AM_Bow_Draw_Hold with Start (AS_Aim_the_Target_Start_Seq) -> Loop (AS_Aim_the_Target_Loop_Seq) on Spine slot */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Animation|Bow")
	static bool RebuildBowDrawHoldMontage();

	/** Enforces that all bow release montages (Release_LV1..3) use DefaultSlot with Root Motion enabled and Skill_04_Charge sequences */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Animation|Bow")
	static bool ConfigureHeavyChargeAndReleaseMontages();

	/** Enforces that all bow montages (Aim_Ready, Draw_Hold, Release_LV1..3) use the Spine slot and appropriate blend times */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Animation|Bow")
	static bool EnsureBowCombatMontagesConfigured();

	/** Assigns AM_Bow_Combo_01..03 to Spine slot (UpperBody) allowing full locomotion, and AM_Bow_Combo_04 to DefaultSlot (FullBody finisher) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Animation|Bow")
	static bool ConfigureBowComboMontagesSlots();

	/** Adds UAnimNotifyState_EarlyLocomotionCancel near the recovery end of AM_Bow_DodgeAttack_* with smooth 0.35s blend */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Animation|Bow")
	static bool AddEarlyLocomotionCancelToDodgeMontages();

	/** Strips any locomotion cancel from AM_Bow_Combo_01..04, enforces DefaultSlot, and configures responsive combo input windows */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Animation|Bow")
	static bool ConfigureBowComboWindows();

	/** Configures AM_Bow_Equip and AM_Bow_Unequip with AnimNotify_AttachWeapon (Hand=true @ 0.567s for equip, Hand=false @ 0.517s for unequip) */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Animation|Bow")
	static bool ConfigureBowEquipMontages();
};
