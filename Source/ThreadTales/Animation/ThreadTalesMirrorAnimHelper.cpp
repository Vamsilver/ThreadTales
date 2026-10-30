// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/ThreadTalesMirrorAnimHelper.h"
#include "Animation/MirrorDataTable.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "UObject/Package.h"

UMirrorDataTable* UThreadTalesMirrorAnimHelper::CreateMannequinMirrorTable(USkeleton* Skeleton, const FString& PackagePath)
{
	if (!Skeleton)
	{
		return nullptr;
	}

	UObject* Outer = GetTransientPackage();
	FName AssetName(TEXT("DT_SK_UEFN_Mannequin_Mirror"));

	if (!PackagePath.IsEmpty())
	{
		UPackage* Package = CreatePackage(*PackagePath);
		if (Package)
		{
			Package->FullyLoad();
			Outer = Package;
			AssetName = FName(*FPaths::GetCleanFilename(PackagePath));
		}
	}

	UMirrorDataTable* Table = NewObject<UMirrorDataTable>(Outer, AssetName, RF_Public | RF_Standalone);
	Table->RowStruct = FMirrorTableRow::StaticStruct();
	Table->Skeleton = Skeleton;
	Table->MirrorAxis = EAxis::X;
	Table->bMirrorRootMotion = true;

	Table->MirrorFindReplaceExpressions.Reset();
	Table->MirrorFindReplaceExpressions.Add(FMirrorFindReplaceExpression(TEXT("_l"), TEXT("_r"), EMirrorFindReplaceMethod::Suffix));
	Table->MirrorFindReplaceExpressions.Add(FMirrorFindReplaceExpression(TEXT("_L"), TEXT("_R"), EMirrorFindReplaceMethod::Suffix));
	Table->MirrorFindReplaceExpressions.Add(FMirrorFindReplaceExpression(TEXT("left"), TEXT("right"), EMirrorFindReplaceMethod::Prefix));
	Table->MirrorFindReplaceExpressions.Add(FMirrorFindReplaceExpression(TEXT("Left"), TEXT("Right"), EMirrorFindReplaceMethod::Prefix));

#if WITH_EDITOR
	Table->FindReplaceMirroredNames();
	Table->MarkPackageDirty();
#endif

	return Table;
}

bool UThreadTalesMirrorAnimHelper::ConfigureMontagePlayWindow(UAnimMontage* Montage, float StartTime, float EndTime, float BlendInTime, float BlendOutTime)
{
	if (!Montage || Montage->SlotAnimTracks.Num() == 0)
	{
		return false;
	}

	FAnimTrack& Track = Montage->SlotAnimTracks[0].AnimTrack;
	if (Track.AnimSegments.Num() == 0)
	{
		return false;
	}

	Montage->Modify();

	FAnimSegment& Segment = Track.AnimSegments[0];
	Segment.AnimStartTime = FMath::Max(0.0f, StartTime);
	if (EndTime > Segment.AnimStartTime)
	{
		Segment.AnimEndTime = EndTime;
	}

	Montage->BlendIn.SetBlendTime(BlendInTime);
	Montage->BlendOut.SetBlendTime(BlendOutTime);
	Montage->BlendOutTriggerTime = -1.0f;

	const float TotalCalculatedLength = Montage->CalculateSequenceLength();
	Montage->SetCompositeLength(TotalCalculatedLength);
	if (FProperty* SeqLenProp = UAnimSequenceBase::StaticClass()->FindPropertyByName(FName("SequenceLength")))
	{
		SeqLenProp->SetValue_InContainer(Montage, &TotalCalculatedLength);
	}

	Montage->MarkPackageDirty();
	return true;
}

