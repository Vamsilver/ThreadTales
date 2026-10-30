// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/ThreadTalesMotionMatchingLibrary.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Chooser.h"
#include "ObjectChooser_Asset.h"

bool UThreadTalesMotionMatchingLibrary::AddAnimationToDatabase(
	UPoseSearchDatabase* Database,
	UAnimationAsset* AnimationAsset,
	EPoseSearchMirrorOption MirrorOption,
	bool bDisableReselection,
	bool bEnabled,
	FVector2D SamplingRange
)
{
	if (!Database || !AnimationAsset)
	{
		return false;
	}

	FPoseSearchDatabaseAnimationAsset Entry;
	Entry.AnimAsset = AnimationAsset;
#if WITH_EDITORONLY_DATA
	Entry.MirrorOption = MirrorOption;
	Entry.bDisableReselection = bDisableReselection;
	Entry.bEnabled = bEnabled;
	Entry.SamplingRange = FFloatInterval(SamplingRange.X, SamplingRange.Y);
#endif

	Database->Modify();
	Database->AddAnimationAsset(Entry);

#if WITH_EDITOR
	Database->SynchronizeWithExternalDependencies();
	Database->NotifyDerivedDataRebuild();
#endif

	return true;
}

void UThreadTalesMotionMatchingLibrary::ClearDatabase(UPoseSearchDatabase* Database)
{
	if (!Database)
	{
		return;
	}

	Database->Modify();
	while (Database->GetNumAnimationAssets() > 0)
	{
		Database->RemoveAnimationAssetAt(Database->GetNumAnimationAssets() - 1);
	}

#if WITH_EDITOR
	Database->SynchronizeWithExternalDependencies();
	Database->NotifyDerivedDataRebuild();
#endif
}

void UThreadTalesMotionMatchingLibrary::SetDatabaseSchema(UPoseSearchDatabase* Database, const UPoseSearchSchema* Schema)
{
	if (!Database)
	{
		return;
	}

	Database->Modify();
	Database->Schema = Schema;

#if WITH_EDITOR
	Database->SynchronizeWithExternalDependencies();
	Database->NotifyDerivedDataRebuild();
#endif
}

void UThreadTalesMotionMatchingLibrary::SynchronizeAndRebuildDatabase(UPoseSearchDatabase* Database)
{
	if (!Database)
	{
		return;
	}

#if WITH_EDITOR
	Database->Modify();
	Database->SynchronizeWithExternalDependencies();
	Database->NotifyDerivedDataRebuild();
#endif
}

TArray<UObject*> UThreadTalesMotionMatchingLibrary::GetDatabaseAnimationAssets(const UPoseSearchDatabase* Database)
{
	TArray<UObject*> Result;
	if (!Database)
	{
		return Result;
	}

	const int32 NumAssets = Database->GetNumAnimationAssets();
	Result.Reserve(NumAssets);
	for (int32 i = 0; i < NumAssets; ++i)
	{
		if (UObject* Asset = Database->GetAnimationAsset(i))
		{
			Result.Add(Asset);
		}
	}
	return Result;
}

TArray<UObject*> UThreadTalesMotionMatchingLibrary::GetChooserTableResultAssets(const UChooserTable* ChooserTable)
{
	TArray<UObject*> Result;
	if (!ChooserTable)
	{
		return Result;
	}

#if WITH_EDITORONLY_DATA
	UE_LOG(LogTemp, Warning, TEXT("ChooserTable %s: ResultsStructs=%d, ColumnsStructs=%d"),
		*ChooserTable->GetName(), ChooserTable->ResultsStructs.Num(), ChooserTable->ColumnsStructs.Num());

	for (int32 j = 0; j < ChooserTable->ColumnsStructs.Num(); ++j)
	{
		const FInstancedStruct& ColStruct = ChooserTable->ColumnsStructs[j];
		const UScriptStruct* ColScriptStruct = ColStruct.GetScriptStruct();
		UE_LOG(LogTemp, Warning, TEXT("  Column[%d]: %s"), j, ColScriptStruct ? *ColScriptStruct->GetName() : TEXT("nullptr"));
	}

	for (int32 i = 0; i < ChooserTable->ResultsStructs.Num(); ++i)
	{
		const FInstancedStruct& ResultStruct = ChooserTable->ResultsStructs[i];
		const UScriptStruct* ScriptStruct = ResultStruct.GetScriptStruct();
		UE_LOG(LogTemp, Warning, TEXT("  Result[%d] struct: %s"), i, ScriptStruct ? *ScriptStruct->GetName() : TEXT("nullptr"));
		if (ScriptStruct == FAssetChooser::StaticStruct())
		{
			const FAssetChooser& AssetChooser = ResultStruct.Get<FAssetChooser>();
			UE_LOG(LogTemp, Warning, TEXT("    Asset: %s"), AssetChooser.Asset ? *AssetChooser.Asset->GetPathName() : TEXT("nullptr"));
			if (AssetChooser.Asset)
			{
				Result.Add(AssetChooser.Asset);
			}
		}
		else if (ScriptStruct == FNestedChooser::StaticStruct())
		{
			const FNestedChooser& Nested = ResultStruct.Get<FNestedChooser>();
			UE_LOG(LogTemp, Warning, TEXT("    NestedChooser: %s (Results=%d, Columns=%d)"),
				Nested.Chooser ? *Nested.Chooser->GetName() : TEXT("nullptr"),
				Nested.Chooser ? Nested.Chooser->ResultsStructs.Num() : 0,
				Nested.Chooser ? Nested.Chooser->ColumnsStructs.Num() : 0);
			if (Nested.Chooser)
			{
				Result.Append(GetChooserTableResultAssets(Nested.Chooser));
			}
		}
		else if (ScriptStruct == FEvaluateChooser::StaticStruct())
		{
			const FEvaluateChooser& Eval = ResultStruct.Get<FEvaluateChooser>();
			UE_LOG(LogTemp, Warning, TEXT("    EvaluateChooser: %s (Results=%d, Columns=%d)"),
				Eval.Chooser ? *Eval.Chooser->GetName() : TEXT("nullptr"),
				Eval.Chooser ? Eval.Chooser->ResultsStructs.Num() : 0,
				Eval.Chooser ? Eval.Chooser->ColumnsStructs.Num() : 0);
			if (Eval.Chooser)
			{
				Result.Append(GetChooserTableResultAssets(Eval.Chooser));
			}
		}
	}
#endif

	return Result;
}

int32 UThreadTalesMotionMatchingLibrary::ReplaceChooserTableResultAssets(UChooserTable* ChooserTable, const TMap<UObject*, UObject*>& AssetReplacements)
{
	if (!ChooserTable || AssetReplacements.IsEmpty())
	{
		return 0;
	}

	int32 ReplacedCount = 0;

#if WITH_EDITORONLY_DATA
	ChooserTable->Modify();
	for (FInstancedStruct& ResultStruct : ChooserTable->ResultsStructs)
	{
		if (ResultStruct.GetScriptStruct() == FAssetChooser::StaticStruct())
		{
			FAssetChooser& AssetChooser = ResultStruct.GetMutable<FAssetChooser>();
			if (UObject* CurrentAsset = AssetChooser.Asset.Get())
			{
				if (UObject* const* Replacement = AssetReplacements.Find(CurrentAsset))
				{
					if (*Replacement)
					{
						AssetChooser.Asset = *Replacement;
						ReplacedCount++;
					}
				}
			}
		}
		else if (ResultStruct.GetScriptStruct() == FNestedChooser::StaticStruct())
		{
			const FNestedChooser& Nested = ResultStruct.Get<FNestedChooser>();
			if (Nested.Chooser)
			{
				ReplacedCount += ReplaceChooserTableResultAssets(Nested.Chooser, AssetReplacements);
			}
		}
		else if (ResultStruct.GetScriptStruct() == FEvaluateChooser::StaticStruct())
		{
			const FEvaluateChooser& Eval = ResultStruct.Get<FEvaluateChooser>();
			if (Eval.Chooser)
			{
				ReplacedCount += ReplaceChooserTableResultAssets(Eval.Chooser, AssetReplacements);
			}
		}
	}

	if (ReplacedCount > 0)
	{
		ChooserTable->Compile(true);
	}
#endif

	return ReplacedCount;
}

bool UThreadTalesMotionMatchingLibrary::SetChooserEvaluatedTableAt(UChooserTable* ChooserTable, int32 RowIndex, UChooserTable* NewTargetChooser)
{
	if (!ChooserTable || !NewTargetChooser)
	{
		return false;
	}

#if WITH_EDITORONLY_DATA
	if (!ChooserTable->ResultsStructs.IsValidIndex(RowIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("SetChooserEvaluatedTableAt: Invalid RowIndex %d (Total: %d)"), RowIndex, ChooserTable->ResultsStructs.Num());
		return false;
	}

	ChooserTable->Modify();
	FInstancedStruct& ResultStruct = ChooserTable->ResultsStructs[RowIndex];
	const UScriptStruct* ScriptStruct = ResultStruct.GetScriptStruct();

	if (ScriptStruct == FEvaluateChooser::StaticStruct())
	{
		FEvaluateChooser& Eval = ResultStruct.GetMutable<FEvaluateChooser>();
		Eval.Chooser = NewTargetChooser;
		ChooserTable->Compile(true);
		UE_LOG(LogTemp, Display, TEXT("SetChooserEvaluatedTableAt: Successfully set Row[%d] EvaluateChooser -> %s"), RowIndex, *NewTargetChooser->GetName());
		return true;
	}
	else if (ScriptStruct == FNestedChooser::StaticStruct())
	{
		FNestedChooser& Nested = ResultStruct.GetMutable<FNestedChooser>();
		Nested.Chooser = NewTargetChooser;
		ChooserTable->Compile(true);
		UE_LOG(LogTemp, Display, TEXT("SetChooserEvaluatedTableAt: Successfully set Row[%d] NestedChooser -> %s"), RowIndex, *NewTargetChooser->GetName());
		return true;
	}
	else
	{
		// Replace struct with FEvaluateChooser
		ResultStruct.InitializeAs(FEvaluateChooser::StaticStruct());
		FEvaluateChooser& Eval = ResultStruct.GetMutable<FEvaluateChooser>();
		Eval.Chooser = NewTargetChooser;
		ChooserTable->Compile(true);
		UE_LOG(LogTemp, Display, TEXT("SetChooserEvaluatedTableAt: Initialized Row[%d] as EvaluateChooser -> %s"), RowIndex, *NewTargetChooser->GetName());
		return true;
	}
#else
	return false;
#endif
}

bool UThreadTalesMotionMatchingLibrary::SetChooserAssetResultAt(UChooserTable* ChooserTable, int32 RowIndex, UObject* NewAsset)
{
	if (!ChooserTable || !NewAsset)
	{
		return false;
	}

#if WITH_EDITORONLY_DATA
	if (!ChooserTable->ResultsStructs.IsValidIndex(RowIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("SetChooserAssetResultAt: Invalid RowIndex %d (Total: %d)"), RowIndex, ChooserTable->ResultsStructs.Num());
		return false;
	}

	ChooserTable->Modify();
	FInstancedStruct& ResultStruct = ChooserTable->ResultsStructs[RowIndex];
	const UScriptStruct* ScriptStruct = ResultStruct.GetScriptStruct();

	if (ScriptStruct == FAssetChooser::StaticStruct())
	{
		FAssetChooser& AssetChooser = ResultStruct.GetMutable<FAssetChooser>();
		AssetChooser.Asset = NewAsset;
		ChooserTable->Compile(true);
		UE_LOG(LogTemp, Display, TEXT("SetChooserAssetResultAt: Successfully set Row[%d] Asset -> %s"), RowIndex, *NewAsset->GetName());
		return true;
	}
	else
	{
		ResultStruct.InitializeAs(FAssetChooser::StaticStruct());
		FAssetChooser& AssetChooser = ResultStruct.GetMutable<FAssetChooser>();
		AssetChooser.Asset = NewAsset;
		ChooserTable->Compile(true);
		UE_LOG(LogTemp, Display, TEXT("SetChooserAssetResultAt: Initialized Row[%d] as AssetChooser -> %s"), RowIndex, *NewAsset->GetName());
		return true;
	}
#else
	return false;
#endif
}

void UThreadTalesMotionMatchingLibrary::DumpChooserTable(const UChooserTable* ChooserTable)
{
	if (!ChooserTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("DumpChooserTable: ChooserTable is null"));
		return;
	}

#if WITH_EDITORONLY_DATA
	UE_LOG(LogTemp, Display, TEXT("================== CHOOSER DUMP: %s =================="), *ChooserTable->GetName());
	UE_LOG(LogTemp, Display, TEXT("Results: %d, Columns: %d"), ChooserTable->ResultsStructs.Num(), ChooserTable->ColumnsStructs.Num());

	for (int32 j = 0; j < ChooserTable->ColumnsStructs.Num(); ++j)
	{
		const FInstancedStruct& ColStruct = ChooserTable->ColumnsStructs[j];
		const UScriptStruct* ColScriptStruct = ColStruct.GetScriptStruct();
		UE_LOG(LogTemp, Display, TEXT("  Column[%d]: %s"), j, ColScriptStruct ? *ColScriptStruct->GetName() : TEXT("nullptr"));
	}

	for (int32 i = 0; i < ChooserTable->ResultsStructs.Num(); ++i)
	{
		const FInstancedStruct& ResultStruct = ChooserTable->ResultsStructs[i];
		const UScriptStruct* ScriptStruct = ResultStruct.GetScriptStruct();
		if (ScriptStruct == FAssetChooser::StaticStruct())
		{
			const FAssetChooser& AssetChooser = ResultStruct.Get<FAssetChooser>();
			UE_LOG(LogTemp, Display, TEXT("  Row[%d] Asset: %s"), i, AssetChooser.Asset ? *AssetChooser.Asset->GetPathName() : TEXT("None"));
		}
		else if (ScriptStruct == FNestedChooser::StaticStruct())
		{
			const FNestedChooser& Nested = ResultStruct.Get<FNestedChooser>();
			UE_LOG(LogTemp, Display, TEXT("  Row[%d] NestedChooser: %s"), i, Nested.Chooser ? *Nested.Chooser->GetName() : TEXT("None"));
		}
		else if (ScriptStruct == FEvaluateChooser::StaticStruct())
		{
			const FEvaluateChooser& Eval = ResultStruct.Get<FEvaluateChooser>();
			UE_LOG(LogTemp, Display, TEXT("  Row[%d] EvaluateChooser: %s"), i, Eval.Chooser ? *Eval.Chooser->GetName() : TEXT("None"));
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("  Row[%d] Struct: %s"), i, ScriptStruct ? *ScriptStruct->GetName() : TEXT("Unknown"));
		}
	}
	UE_LOG(LogTemp, Display, TEXT("======================================================"));
#endif
}

void UThreadTalesMotionMatchingLibrary::InspectAnimBlueprintNodes(UAnimBlueprint* AnimBP)
{
	if (!AnimBP)
	{
		return;
	}

	TArray<UObject*> SubObjects;
	GetObjectsWithPackage(AnimBP->GetOutermost(), SubObjects);

	UE_LOG(LogTemp, Warning, TEXT("=== AnimBP: %s (Total subobjects=%d) ==="), *AnimBP->GetName(), SubObjects.Num());
	for (UObject* Obj : SubObjects)
	{
		if (!Obj)
		{
			continue;
		}

		FString ClassName = Obj->GetClass()->GetName();
		if (ClassName.Contains(TEXT("State")) || ClassName.Contains(TEXT("PoseSearch")) || ClassName.Contains(TEXT("Chooser")) || ClassName.Contains(TEXT("MotionMatching")) || ClassName.Contains(TEXT("AnimGraphNode")))
		{
			UE_LOG(LogTemp, Warning, TEXT("  SubObj: %s [%s]"), *Obj->GetName(), *ClassName);
			for (TFieldIterator<FProperty> PropIt(Obj->GetClass()); PropIt; ++PropIt)
			{
				FProperty* Prop = *PropIt;
				FString PropName = Prop->GetName();
				if (PropName.Contains(TEXT("Database")) || PropName.Contains(TEXT("Chooser")) || PropName.Contains(TEXT("Asset")) || PropName.Contains(TEXT("Extension")) || PropName.Contains(TEXT("StateName")))
				{
					FString ValueStr;
					Prop->ExportTextItem_Direct(ValueStr, Prop->ContainerPtrToValuePtr<void>(Obj), nullptr, Obj, PPF_None);
					if (!ValueStr.IsEmpty() && ValueStr != TEXT("None") && ValueStr != TEXT("()") && ValueStr != TEXT("nullptr"))
					{
						UE_LOG(LogTemp, Warning, TEXT("    %s = %s"), *PropName, *ValueStr);
					}
				}
			}
		}
	}
}

int32 UThreadTalesMotionMatchingLibrary::CopyChooserTableResults(const UChooserTable* SourceChooser, UChooserTable* DestinationChooser)
{
	if (!SourceChooser || !DestinationChooser)
	{
		return 0;
	}

	int32 CopiedCount = 0;

#if WITH_EDITORONLY_DATA
	DestinationChooser->Modify();
	const int32 NumResults = FMath::Min(SourceChooser->ResultsStructs.Num(), DestinationChooser->ResultsStructs.Num());
	for (int32 i = 0; i < NumResults; ++i)
	{
		const FInstancedStruct& Src = SourceChooser->ResultsStructs[i];
		FInstancedStruct& Dst = DestinationChooser->ResultsStructs[i];

		if (Src.GetScriptStruct() == FAssetChooser::StaticStruct() && Dst.GetScriptStruct() == FAssetChooser::StaticStruct())
		{
			Dst.GetMutable<FAssetChooser>().Asset = Src.Get<FAssetChooser>().Asset;
			CopiedCount++;
		}
		else if (Src.GetScriptStruct() == FNestedChooser::StaticStruct() && Dst.GetScriptStruct() == FNestedChooser::StaticStruct())
		{
			CopiedCount += CopyChooserTableResults(Src.Get<FNestedChooser>().Chooser, Dst.GetMutable<FNestedChooser>().Chooser);
		}
		else if (Src.GetScriptStruct() == FEvaluateChooser::StaticStruct() && Dst.GetScriptStruct() == FEvaluateChooser::StaticStruct())
		{
			CopiedCount += CopyChooserTableResults(Src.Get<FEvaluateChooser>().Chooser, Dst.GetMutable<FEvaluateChooser>().Chooser);
		}
	}

	if (CopiedCount > 0)
	{
		DestinationChooser->Compile(true);
	}
#endif

	return CopiedCount;
}

void UThreadTalesMotionMatchingLibrary::SetCombatStanceActive(bool bInCombat)
{
#if WITH_EDITORONLY_DATA
	// 1. Switch Root Motion Matching Chooser
	UChooserTable* RootChooser = LoadObject<UChooserTable>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/CHT_PoseSearchDatabases"));
	UChooserTable* KatanaChooser = LoadObject<UChooserTable>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/CHT_PoseSearchDatabases_Katana"));
	UChooserTable* DenseChooser = LoadObject<UChooserTable>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/CHT_PoseSearchDatabases_Dense"));

	if (RootChooser && KatanaChooser && DenseChooser)
	{
		SetChooserEvaluatedTableAt(RootChooser, 0, bInCombat ? KatanaChooser : DenseChooser);
	}

	// 2. Switch State Machine / BlendStack Chooser with full 481-entry precision
	UChooserTable* CMCAnimChooser = LoadObject<UChooserTable>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Animations/ExperimentalStateMachineData/CHT_CMCCharacterAnimations"));
	UChooserTable* CMCKatanaChooser = LoadObject<UChooserTable>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Animations/ExperimentalStateMachineData/CHT_CMCCharacterAnimations_Katana"));
	UChooserTable* CMCCleanChooser = LoadObject<UChooserTable>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Animations/ExperimentalStateMachineData/CHT_CMCCharacterAnimations_Clean"));

	int32 Copied = 0;
	if (CMCAnimChooser && CMCKatanaChooser && CMCCleanChooser)
	{
		Copied = CopyChooserTableResults(bInCombat ? CMCKatanaChooser : CMCCleanChooser, CMCAnimChooser);
	}

	UE_LOG(LogTemp, Display, TEXT("SetCombatStanceActive: Successfully switched locomotion to %s (%d entries synced)"), 
		bInCombat ? TEXT("Katana (Combat)") : TEXT("Unarmed (Neutral)"), Copied);
#endif
}

bool UThreadTalesMotionMatchingLibrary::SetMontageSlotName(UAnimMontage* Montage, FName NewSlotName)
{
	if (!Montage || Montage->SlotAnimTracks.Num() == 0)
	{
		return false;
	}

	Montage->Modify();
	Montage->SlotAnimTracks[0].SlotName = NewSlotName;
	Montage->MarkPackageDirty();
	return true;
}

bool UThreadTalesMotionMatchingLibrary::SetMontageAnimSequence(UAnimMontage* Montage, UAnimSequenceBase* NewAnimSeq)
{
	if (!Montage || !NewAnimSeq || Montage->SlotAnimTracks.Num() == 0)
	{
		return false;
	}

	if (Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() == 0)
	{
		return false;
	}

	Montage->Modify();
	Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].SetAnimReference(NewAnimSeq);
	Montage->CalculateSequenceLength();
	Montage->MarkPackageDirty();
	return true;
}



