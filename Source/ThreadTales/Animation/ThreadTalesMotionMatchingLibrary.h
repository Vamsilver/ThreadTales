// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "ThreadTalesMotionMatchingLibrary.generated.h"

class UPoseSearchSchema;
class UAnimationAsset;

/**
 * Utility library for creating, populating, and managing Pose Search Databases and Motion Matching assets.
 */
UCLASS()
class THREADTALES_API UThreadTalesMotionMatchingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Adds an animation sequence or blendspace to a Pose Search Database with configurable mirror and reselection settings */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|MotionMatching")
	static bool AddAnimationToDatabase(
		UPoseSearchDatabase* Database,
		UAnimationAsset* AnimationAsset,
		EPoseSearchMirrorOption MirrorOption = EPoseSearchMirrorOption::UnmirroredOnly,
		bool bDisableReselection = false,
		bool bEnabled = true,
		FVector2D SamplingRange = FVector2D::ZeroVector
	);

	/** Removes all animation entries from a Pose Search Database */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|MotionMatching")
	static void ClearDatabase(UPoseSearchDatabase* Database);

	/** Sets the Pose Search Schema for a Database */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|MotionMatching")
	static void SetDatabaseSchema(UPoseSearchDatabase* Database, const UPoseSearchSchema* Schema);

	/** Triggers database synchronization and rebuilds derived search data */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|MotionMatching")
	static void SynchronizeAndRebuildDatabase(UPoseSearchDatabase* Database);

	/** Returns all animation assets currently indexed in the database */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|MotionMatching")
	static TArray<UObject*> GetDatabaseAnimationAssets(const UPoseSearchDatabase* Database);

	/** Gets all referenced result assets from a Chooser Table */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|MotionMatching")
	static TArray<UObject*> GetChooserTableResultAssets(const class UChooserTable* ChooserTable);

	/** Replaces result assets in a Chooser Table by mapping old asset objects to new replacement objects */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|MotionMatching")
	static int32 ReplaceChooserTableResultAssets(class UChooserTable* ChooserTable, const TMap<UObject*, UObject*>& AssetReplacements);

	/** Sets the evaluated ChooserTable for an FEvaluateChooser or FNestedChooser row */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|MotionMatching")
	static bool SetChooserEvaluatedTableAt(class UChooserTable* ChooserTable, int32 RowIndex, class UChooserTable* NewTargetChooser);

	/** Sets the asset result for an FAssetChooser row */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|MotionMatching")
	static bool SetChooserAssetResultAt(class UChooserTable* ChooserTable, int32 RowIndex, UObject* NewAsset);

	/** Dumps the complete structure, columns, and results of a ChooserTable */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|MotionMatching")
	static void DumpChooserTable(const class UChooserTable* ChooserTable);

	/** Inspects all graphs, state nodes, and pose search bindings in an AnimBlueprint */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|MotionMatching")
	static void InspectAnimBlueprintNodes(class UAnimBlueprint* AnimBP);

	/** Copies all asset results from SourceChooser into DestinationChooser recursively */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|MotionMatching")
	static int32 CopyChooserTableResults(const class UChooserTable* SourceChooser, class UChooserTable* DestinationChooser);

	/** Toggles the active Chooser locomotion tables between default Unarmed and Katana Combat Stance */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|MotionMatching")
	static void SetCombatStanceActive(bool bInCombat);
};
