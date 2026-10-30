// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/ThreadTalesBowMontageHelper.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimNotifyState_EarlyLocomotionCancel.h"
#include "Animation/AnimNotifyState_ComboWindow.h"
#include "Animation/AnimNotify_AttachWeapon.h"
#include "UObject/Package.h"

bool UThreadTalesBowMontageHelper::ConfigureSingleSequenceMontage(
	UAnimMontage* Montage,
	UAnimSequenceBase* AnimSeq,
	FName SlotName,
	float BlendInTime,
	float BlendOutTime)
{
	if (!Montage || !AnimSeq)
	{
		return false;
	}

	Montage->Modify();

	if (Montage->SlotAnimTracks.Num() == 0)
	{
		FSlotAnimationTrack NewTrack;
		NewTrack.SlotName = SlotName.IsNone() ? FName("DefaultSlot") : SlotName;
		Montage->SlotAnimTracks.Add(NewTrack);
	}
	else
	{
		Montage->SlotAnimTracks[0].SlotName = SlotName.IsNone() ? FName("DefaultSlot") : SlotName;
	}

	FAnimTrack& Track = Montage->SlotAnimTracks[0].AnimTrack;
	Track.AnimSegments.Empty();

	FAnimSegment NewSegment;
	NewSegment.SetAnimReference(AnimSeq);
	NewSegment.AnimStartTime = 0.0f;
	NewSegment.AnimEndTime = AnimSeq->GetPlayLength();
	NewSegment.AnimPlayRate = 1.0f;
	NewSegment.StartPos = 0.0f;
	Track.AnimSegments.Add(NewSegment);

	Montage->BlendIn.SetBlendTime(BlendInTime);
	Montage->BlendOut.SetBlendTime(BlendOutTime);
	Montage->BlendOutTriggerTime = -1.0f;

	const float TotalCalculatedLength = Montage->CalculateSequenceLength();
	Montage->SetCompositeLength(TotalCalculatedLength);
	if (FProperty* SeqLenProp = UAnimSequenceBase::StaticClass()->FindPropertyByName(FName("SequenceLength")))
	{
		SeqLenProp->SetValue_InContainer(Montage, &TotalCalculatedLength);
	}

	Montage->CompositeSections.Empty();
	FCompositeSection DefaultSection;
	DefaultSection.SectionName = FName("Default");
	DefaultSection.Link(Montage, 0.0f);
	DefaultSection.NextSectionName = NAME_None;
	Montage->CompositeSections.Add(DefaultSection);

	Montage->MarkPackageDirty();
	return true;
}

static bool ConfigureSlicedStartLoopMontage(
	UAnimMontage* Montage,
	UAnimSequenceBase* StartSeq, float StartBegin, float StartEnd, float StartRate,
	UAnimSequenceBase* LoopSeq, float LoopBegin, float LoopEnd, float LoopRate,
	FName SlotName, float BlendInTime, float BlendOutTime)
{
	if (!Montage || !StartSeq || !LoopSeq) { return false; }
	Montage->Modify();

	if (Montage->SlotAnimTracks.Num() == 0) { Montage->SlotAnimTracks.AddDefaulted(); }
	FSlotAnimationTrack& Track = Montage->SlotAnimTracks[0];
	Track.SlotName = SlotName;
	Track.AnimTrack.AnimSegments.Empty();

	const float EffStartRate = FMath::Max(0.01f, StartRate);
	const float ScaledStartDur = (StartEnd - StartBegin) / EffStartRate;
	const float EffLoopRate = FMath::Max(0.01f, LoopRate);

	FAnimSegment StartSeg;
	StartSeg.SetAnimReference(StartSeq);
	StartSeg.StartPos = 0.0f;
	StartSeg.AnimStartTime = StartBegin;
	StartSeg.AnimEndTime = StartEnd;
	StartSeg.AnimPlayRate = EffStartRate;
	StartSeg.LoopingCount = 1;
	Track.AnimTrack.AnimSegments.Add(StartSeg);

	FAnimSegment LoopSeg;
	LoopSeg.SetAnimReference(LoopSeq);
	LoopSeg.StartPos = ScaledStartDur;
	LoopSeg.AnimStartTime = LoopBegin;
	LoopSeg.AnimEndTime = LoopEnd;
	LoopSeg.AnimPlayRate = EffLoopRate;
	LoopSeg.LoopingCount = 1;
	Track.AnimTrack.AnimSegments.Add(LoopSeg);

	const float TotalLen = Montage->CalculateSequenceLength();
	Montage->SetCompositeLength(TotalLen);
	if (FProperty* SeqLenProp = UAnimSequenceBase::StaticClass()->FindPropertyByName(FName("SequenceLength")))
	{
		SeqLenProp->SetValue_InContainer(Montage, &TotalLen);
	}

	Montage->CompositeSections.Empty();
	FCompositeSection StartSec;
	StartSec.SectionName = FName("Start");
	StartSec.Link(Montage, 0.0f);
	StartSec.NextSectionName = FName("Loop");
	Montage->CompositeSections.Add(StartSec);

	FCompositeSection LoopSec;
	LoopSec.SectionName = FName("Loop");
	LoopSec.Link(Montage, ScaledStartDur);
	LoopSec.NextSectionName = FName("Loop");
	Montage->CompositeSections.Add(LoopSec);

	Montage->BlendIn.SetBlendTime(BlendInTime);
	Montage->BlendOut.SetBlendTime(BlendOutTime);
	Montage->BlendOutTriggerTime = -1.0f;
#if WITH_EDITOR
	Montage->PostEditChange();
#endif
	Montage->MarkPackageDirty();
	return true;
}

bool UThreadTalesBowMontageHelper::ConfigureStartLoopMontage(
	UAnimMontage* Montage,
	UAnimSequenceBase* StartSeq,
	UAnimSequenceBase* LoopSeq,
	FName SlotName,
	float StartPlayRate)
{
	if (!Montage || !StartSeq || !LoopSeq) { return false; }
	return ConfigureSlicedStartLoopMontage(
		Montage,
		StartSeq, 0.0f, StartSeq->GetPlayLength(), StartPlayRate,
		LoopSeq, 0.0f, LoopSeq->GetPlayLength(), 1.0f,
		SlotName, 0.20f, 0.25f);
}

bool UThreadTalesBowMontageHelper::RebuildBowDrawHoldMontage()
{
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Draw_Hold.AM_Bow_Draw_Hold"));
	UAnimSequence* StartSeq = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Animations/Archer_Retarget/02_Attack/06_Attack/AS_Aim_the_Target_Start_Seq.AS_Aim_the_Target_Start_Seq"));
	UAnimSequence* LoopSeq = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Animations/Archer_Retarget/02_Attack/06_Attack/AS_Aim_the_Target_Loop_Seq.AS_Aim_the_Target_Loop_Seq"));
	if (!Montage || !StartSeq || !LoopSeq) { return false; }

	StartSeq->bEnableRootMotion = false;
	LoopSeq->bEnableRootMotion = false;

	// Play draw pull and hold on 'Spine' slot so spine, shoulders, and arms animate author posture
	return ConfigureSlicedStartLoopMontage(
		Montage,
		StartSeq, 0.25f, StartSeq->GetPlayLength(), 0.37f,
		LoopSeq, 0.0f, LoopSeq->GetPlayLength(), 1.0f,
		FName("Spine"), 0.12f, 0.18f);
}

bool UThreadTalesBowMontageHelper::ConfigureHeavyChargeAndReleaseMontages()
{
	const FName DefaultSlot = FName("DefaultSlot");

	struct FReleaseConfig
	{
		const TCHAR* MontagePath;
		const TCHAR* SequencePath;
	};

	const FReleaseConfig Releases[] = {
		{
			TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Release_LV1.AM_Bow_Release_LV1"),
			TEXT("/Game/Animations/Archer_Retarget/02_Attack/07_Skill_Attack/AS_Skill_04_Charge_LV1_End_Seq.AS_Skill_04_Charge_LV1_End_Seq")
		},
		{
			TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Release_LV2.AM_Bow_Release_LV2"),
			TEXT("/Game/Animations/Archer_Retarget/02_Attack/07_Skill_Attack/AS_Skill_04_Charge_LV2_End_Seq.AS_Skill_04_Charge_LV2_End_Seq")
		},
		{
			TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Release_LV3.AM_Bow_Release_LV3"),
			TEXT("/Game/Animations/Archer_Retarget/02_Attack/07_Skill_Attack/AS_Skill_04_Charge_LV3_End_Seq.AS_Skill_04_Charge_LV3_End_Seq")
		}
	};

	for (const FReleaseConfig& Cfg : Releases)
	{
		UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, Cfg.MontagePath);
		UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, Cfg.SequencePath);
		if (Montage && Seq)
		{
			Seq->bEnableRootMotion = true;
			Seq->MarkPackageDirty();

			ConfigureSingleSequenceMontage(Montage, Seq, DefaultSlot, 0.05f, 0.20f);
			Montage->MarkPackageDirty();
		}
	}

	struct FHoldConfig
	{
		const TCHAR* MontagePath;
		const TCHAR* StartPath;
		const TCHAR* LoopPath;
	};

	const FHoldConfig Holds[] = {
		{
			TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Draw_Hold_LV1.AM_Bow_Draw_Hold_LV1"),
			TEXT("/Game/Animations/Archer_Retarget/02_Attack/07_Skill_Attack/AS_Skill_04_Charge_LV1_Start_Seq.AS_Skill_04_Charge_LV1_Start_Seq"),
			TEXT("/Game/Animations/Archer_Retarget/02_Attack/07_Skill_Attack/AS_Skill_04_Charge_LV1_Loop_Seq.AS_Skill_04_Charge_LV1_Loop_Seq")
		},
		{
			TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Draw_Hold_LV2.AM_Bow_Draw_Hold_LV2"),
			TEXT("/Game/Animations/Archer_Retarget/02_Attack/07_Skill_Attack/AS_Skill_04_Charge_LV2_Start_Seq.AS_Skill_04_Charge_LV2_Start_Seq"),
			TEXT("/Game/Animations/Archer_Retarget/02_Attack/07_Skill_Attack/AS_Skill_04_Charge_LV2_Loop_Seq.AS_Skill_04_Charge_LV2_Loop_Seq")
		},
		{
			TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Draw_Hold_LV3.AM_Bow_Draw_Hold_LV3"),
			TEXT("/Game/Animations/Archer_Retarget/02_Attack/07_Skill_Attack/AS_Skill_04_Charge_LV3_Start_Seq.AS_Skill_04_Charge_LV3_Start_Seq"),
			TEXT("/Game/Animations/Archer_Retarget/02_Attack/07_Skill_Attack/AS_Skill_04_Charge_LV3_Loop_Seq.AS_Skill_04_Charge_LV3_Loop_Seq")
		}
	};

	const FName HoldSlot = FName("Spine");
	for (const FHoldConfig& Cfg : Holds)
	{
		UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, Cfg.MontagePath);
		UAnimSequence* StartSeq = LoadObject<UAnimSequence>(nullptr, Cfg.StartPath);
		UAnimSequence* LoopSeq = LoadObject<UAnimSequence>(nullptr, Cfg.LoopPath);
		if (Montage && StartSeq && LoopSeq)
		{
			StartSeq->bEnableRootMotion = false;
			StartSeq->MarkPackageDirty();
			LoopSeq->bEnableRootMotion = false;
			LoopSeq->MarkPackageDirty();
			ConfigureStartLoopMontage(Montage, StartSeq, LoopSeq, HoldSlot, 1.0f);
			Montage->MarkPackageDirty();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[MontageHelper] ConfigureHeavyChargeAndReleaseMontages: Missing hold asset for montage '%s' (Montage=%s, Start=%s, Loop=%s)"),
				Cfg.MontagePath, Montage ? TEXT("OK") : TEXT("MISSING"), StartSeq ? TEXT("OK") : TEXT("MISSING"), LoopSeq ? TEXT("OK") : TEXT("MISSING"));
		}
	}

	UE_LOG(LogTemp, Display, TEXT("[MontageHelper] ConfigureHeavyChargeAndReleaseMontages: Release_LV1..3 configured with Root Motion on DefaultSlot, and Draw_Hold_LV1..3 configured on Spine slot."));
	return true;
}

bool UThreadTalesBowMontageHelper::EnsureBowCombatMontagesConfigured()
{
	const FName SpineSlot = FName("Spine");
	RebuildBowDrawHoldMontage();

	UAnimSequence* StartSeq = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Animations/Archer_Retarget/02_Attack/06_Attack/AS_Aim_the_Target_Start_Seq.AS_Aim_the_Target_Start_Seq"));
	if (StartSeq) { StartSeq->bEnableRootMotion = false; }

	if (UAnimMontage* AimReady = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Aim_Ready.AM_Bow_Aim_Ready")))
	{
		if (StartSeq)
		{
			ConfigureSingleSequenceMontage(AimReady, StartSeq, SpineSlot, 0.12f, 0.18f);
			const float NockPhaseLen = 0.26f;
			AimReady->SetCompositeLength(NockPhaseLen);
			if (AimReady->SlotAnimTracks.Num() > 0 && AimReady->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() > 0)
			{
				AimReady->SlotAnimTracks[0].AnimTrack.AnimSegments[0].AnimEndTime = NockPhaseLen;
			}
		}
	}

	ConfigureHeavyChargeAndReleaseMontages();
	return true;
}

bool UThreadTalesBowMontageHelper::ConfigureBowComboMontagesSlots()
{
	const FName DefaultSlot = FName("DefaultSlot");

	const TCHAR* AllComboPaths[] = {
		TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Combo_01.AM_Bow_Combo_01"),
		TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Combo_02.AM_Bow_Combo_02"),
		TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Combo_03.AM_Bow_Combo_03"),
		TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Combo_04.AM_Bow_Combo_04")
	};

	for (const TCHAR* ComboPath : AllComboPaths)
	{
		if (UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, ComboPath))
		{
			Montage->Modify();
			if (Montage->SlotAnimTracks.Num() > 0)
			{
				Montage->SlotAnimTracks[0].SlotName = DefaultSlot;
			}
			Montage->BlendIn.SetBlendTime(0.25f);
			Montage->BlendOut.SetBlendTime(0.35f);
			Montage->BlendOutTriggerTime = -1.0f;
#if WITH_EDITOR
			Montage->PostEditChange();
#endif
			Montage->MarkPackageDirty();
			UE_LOG(LogTemp, Display, TEXT("[MontageHelper] Assigned %s to DefaultSlot (BlendIn=0.25s, BlendOut=0.35s)"), *Montage->GetName());
		}
	}

	return true;
}

bool UThreadTalesBowMontageHelper::AddEarlyLocomotionCancelToDodgeMontages()
{
	const TCHAR* DodgeMontages[] = {
		TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_DodgeAttack_F.AM_Bow_DodgeAttack_F"),
		TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_DodgeAttack_B.AM_Bow_DodgeAttack_B"),
		TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_DodgeAttack_L.AM_Bow_DodgeAttack_L"),
		TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_DodgeAttack_R.AM_Bow_DodgeAttack_R")
	};
	for (const TCHAR* DodgePath : DodgeMontages)
	{
		if (UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, DodgePath))
		{
			Montage->Modify();
			for (int32 i = Montage->Notifies.Num() - 1; i >= 0; --i)
			{
				if (Montage->Notifies[i].NotifyStateClass && Montage->Notifies[i].NotifyStateClass->IsA<UAnimNotifyState_EarlyLocomotionCancel>())
				{
					Montage->Notifies.RemoveAt(i);
				}
			}
			FAnimNotifyEvent NewEvent;
			UAnimNotifyState_EarlyLocomotionCancel* CancelState = NewObject<UAnimNotifyState_EarlyLocomotionCancel>(Montage);
			CancelState->BlendOutTime = 0.35f;
			NewEvent.NotifyStateClass = CancelState;
			NewEvent.NotifyName = FName("Early Locomotion Cancel");
			NewEvent.Link(Montage, 0.75f);
			NewEvent.SetDuration(1.08f);
			NewEvent.TrackIndex = 0;
			Montage->Notifies.Add(NewEvent);
			Montage->BlendIn.SetBlendTime(0.25f);
			Montage->BlendOut.SetBlendTime(0.35f);
			Montage->BlendOutTriggerTime = -1.0f;
#if WITH_EDITOR
			Montage->PostEditChange();
#endif
			Montage->MarkPackageDirty();
			UE_LOG(LogTemp, Display, TEXT("[MontageHelper] Added Early Locomotion Cancel notify to %s (0.75s - 1.83s, BlendOut=0.35s)"), *Montage->GetName());
		}
	}
	return true;
}

bool UThreadTalesBowMontageHelper::ConfigureBowComboWindows()
{
	struct FComboConfig
	{
		const TCHAR* Path;
		bool bHasComboWindow;
		float WindowStart;
		float WindowDuration;
		float CancelStart;
		float CancelDuration;
	};

	// Combo 01 (Len=1.33s): shot at 0.25s -> ComboWindow [0.42s - 1.15s], EarlyLocomotionCancel [0.50s - 1.33s]
	// Combo 02 (Len=1.37s): shot at 0.28s -> ComboWindow [0.45s - 1.20s], EarlyLocomotionCancel [0.55s - 1.37s]
	// Combo 03 (Len=1.58s): shot at 0.28s -> ComboWindow [0.48s - 1.35s], EarlyLocomotionCancel [0.58s - 1.58s]
	// Combo 04 (Len=1.58s): shot at 0.35s, 2m backflip lands at 0.55s -> Finisher (no ComboWindow), EarlyLocomotionCancel [0.75s - 1.58s]
	const FComboConfig Combos[] = {
		{ TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Combo_01.AM_Bow_Combo_01"), true,  0.42f, 0.73f, 0.50f, 0.83f },
		{ TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Combo_02.AM_Bow_Combo_02"), true,  0.45f, 0.75f, 0.55f, 0.82f },
		{ TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Combo_03.AM_Bow_Combo_03"), true,  0.48f, 0.87f, 0.58f, 1.00f },
		{ TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Combo_04.AM_Bow_Combo_04"), false, 0.00f, 0.00f, 0.75f, 0.83f }
	};

	const FName DefaultSlot = FName("DefaultSlot");

	for (const FComboConfig& Cfg : Combos)
	{
		if (UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, Cfg.Path))
		{
			Montage->Modify();

			// 1. Enforce DefaultSlot for full body archer kinematics
			if (Montage->SlotAnimTracks.Num() > 0)
			{
				Montage->SlotAnimTracks[0].SlotName = DefaultSlot;
			}

			// 2. Clear old notifies
			for (int32 i = Montage->Notifies.Num() - 1; i >= 0; --i)
			{
				if (Montage->Notifies[i].NotifyStateClass && Montage->Notifies[i].NotifyStateClass->IsA<UAnimNotifyState_EarlyLocomotionCancel>())
				{
					Montage->Notifies.RemoveAt(i);
				}
				else if (Montage->Notifies[i].NotifyStateClass && Montage->Notifies[i].NotifyStateClass->IsA<UAnimNotifyState_ComboWindow>())
				{
					Montage->Notifies.RemoveAt(i);
				}
			}

			// 3. Add responsive ComboWindow on Track 0 (Steps 1..3 only; Step 4 is the finisher)
			if (Cfg.bHasComboWindow)
			{
				FAnimNotifyEvent NewEvent;
				UAnimNotifyState_ComboWindow* ComboState = NewObject<UAnimNotifyState_ComboWindow>(Montage);
				NewEvent.NotifyStateClass = ComboState;
				NewEvent.NotifyName = FName("Combo Window");
				NewEvent.Link(Montage, Cfg.WindowStart);
				NewEvent.SetDuration(Cfg.WindowDuration);
				NewEvent.TrackIndex = 0;
				Montage->Notifies.Add(NewEvent);
			}

			// 4. Add EarlyLocomotionCancel on Track 1 after arrow release and landing with smooth 0.35s blend
			FAnimNotifyEvent CancelEvent;
			UAnimNotifyState_EarlyLocomotionCancel* CancelState = NewObject<UAnimNotifyState_EarlyLocomotionCancel>(Montage);
			CancelState->BlendOutTime = 0.35f;
			CancelEvent.NotifyStateClass = CancelState;
			CancelEvent.NotifyName = FName("Early Locomotion Cancel");
			CancelEvent.Link(Montage, Cfg.CancelStart);
			CancelEvent.SetDuration(Cfg.CancelDuration);
			CancelEvent.TrackIndex = 1;
			Montage->Notifies.Add(CancelEvent);

			Montage->BlendIn.SetBlendTime(0.25f);
			Montage->BlendOut.SetBlendTime(0.35f);
			Montage->BlendOutTriggerTime = -1.0f;

#if WITH_EDITOR
			Montage->PostEditChange();
#endif
			Montage->MarkPackageDirty();
			UE_LOG(LogTemp, Display, TEXT("[MontageHelper] Configured %s: HasComboWindow=%d [%.2fs - %.2fs], LocomotionCancel [%.2fs - %.2fs], BlendIn=0.25s, BlendOut=0.35s"),
				*Montage->GetName(), Cfg.bHasComboWindow ? 1 : 0, Cfg.WindowStart, Cfg.WindowStart + Cfg.WindowDuration, Cfg.CancelStart, Cfg.CancelStart + Cfg.CancelDuration);
		}
	}
	return true;
}

bool UThreadTalesBowMontageHelper::ConfigureBowEquipMontages()
{
	const FName ArmLSlot = FName("Arm L");
	struct FEquipConfig { const TCHAR* Path; bool bHand; float Time; const TCHAR* Name; };
	const FEquipConfig Configs[] = {
		{ TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Equip.AM_Bow_Equip"), true, 0.567f, TEXT("Equip") },
		{ TEXT("/Game/Animations/Archer_Retarget/Montages/AM_Bow_Unequip.AM_Bow_Unequip"), false, 0.517f, TEXT("Unequip") }
	};
	for (const FEquipConfig& C : Configs)
	{
		if (UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, C.Path))
		{
			Montage->Modify();
			if (Montage->SlotAnimTracks.Num() > 0) { Montage->SlotAnimTracks[0].SlotName = ArmLSlot; }
			for (int32 i = Montage->Notifies.Num() - 1; i >= 0; --i)
			{
				if (Montage->Notifies[i].Notify && Montage->Notifies[i].Notify->IsA<UAnimNotify_AttachWeapon>())
				{
					Montage->Notifies.RemoveAt(i);
				}
			}
			FAnimNotifyEvent AttachEvent;
			UAnimNotify_AttachWeapon* AttachNotify = NewObject<UAnimNotify_AttachWeapon>(Montage);
			AttachNotify->bAttachToHand = C.bHand;
			AttachEvent.Notify = AttachNotify;
			AttachEvent.NotifyName = C.bHand ? FName("Attach Weapon To Hand") : FName("Attach Weapon To Sheath");
			AttachEvent.Link(Montage, C.Time);
			AttachEvent.TrackIndex = 0;
			Montage->Notifies.Add(AttachEvent);
			Montage->BlendIn.SetBlendTime(0.20f);
			Montage->BlendOut.SetBlendTime(0.25f);
			Montage->BlendOutTriggerTime = -1.0f;
#if WITH_EDITOR
			Montage->PostEditChange();
#endif
			Montage->MarkPackageDirty();
			UE_LOG(LogTemp, Display, TEXT("[MontageHelper] Configured %s: Slot=Spine, Hand=%d @ %.3fs"), C.Name, C.bHand ? 1 : 0, C.Time);
		}
	}
	return true;
}

