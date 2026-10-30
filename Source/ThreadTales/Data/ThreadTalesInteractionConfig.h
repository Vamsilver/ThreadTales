// Copyright ThreadTales. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "ThreadTalesInteractionConfig.generated.h"

class UAnimMontage;
class USoundBase;

/**
 * Type of interaction input triggering the event.
 */
UENUM(BlueprintType)
enum class EThreadTalesInteractionInputType : uint8
{
	Tap UMETA(DisplayName = "Tap (Press Key)"),
	Hold UMETA(DisplayName = "Hold Key (Progress Wheel)")
};

/**
 * AAA Data Asset defining interaction presentation and behavior for level designers.
 * Reusable across multiple world actors (e.g. all wooden chests, all heavy levers).
 */
UCLASS(BlueprintType, Const)
class THREADTALES_API UThreadTalesInteractionConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Contextual prompt text displayed on the HUD (e.g. "Open Chest", "Pull Lever") */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|UI", meta = (ToolTip = "HUD prompt text shown when in focus"))
	FText PromptText = FText::FromString(TEXT("Взаимодействовать"));

	/** Prompt text displayed when a toggleable object is currently active (e.g. "Reset Lever", "Close Door") */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|UI", meta = (ToolTip = "Optional HUD prompt text shown when toggleable object is active"))
	FText ToggledPromptText;

	/** Input mode: Instant Tap or Hold with progress wheel */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|Input", meta = (ToolTip = "Tap for instant trigger, Hold for timed key hold"))
	EThreadTalesInteractionInputType InputType = EThreadTalesInteractionInputType::Tap;

	/** Duration required to hold key if InputType is Hold (in seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|Input", meta = (EditCondition = "InputType == EThreadTalesInteractionInputType::Hold", ClampMin = "0.1", ClampMax = "10.0", ToolTip = "Hold duration in seconds"))
	float HoldDuration = 1.0f;

	/** If false, interaction triggers prop kinematics and finish immediately without locking, approaching, or animating the character */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|Animation", meta = (ToolTip = "If false, executes prop action instantly without walking or character animation"))
	bool bAnimateCharacter = true;

	/** Character montage to play during interaction (e.g. opening chest, pulling lever) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|Animation", meta = (EditCondition = "bAnimateCharacter", ToolTip = "Montage played on character when interaction executes"))
	TSoftObjectPtr<UAnimMontage> CharacterMontage = nullptr;

	/** Optional reverse/toggled character montage played when deactivating a toggleable object (e.g. AM_Lever_FrontToBack) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|Animation", meta = (EditCondition = "bAnimateCharacter", ToolTip = "Optional montage played when toggling an already active object back to its initial state"))
	TSoftObjectPtr<UAnimMontage> ToggledCharacterMontage = nullptr;

	/** Name of the Motion Warping target matching the anim montage warp window */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|Warping", meta = (ToolTip = "Motion Warping sync target name for character body (default: InteractionTarget)"))
	FName WarpTargetName = FName("InteractionTarget");

	/** Name of the secondary Motion Warping target for character hands (e.g. InteractionHandTarget) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|Warping", meta = (ToolTip = "Secondary Motion Warping target name for hand alignment"))
	FName HandWarpTargetName = FName("InteractionHandTarget");

	/** Maximum time allowed for character to align to interaction point before montage takes over */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|Warping", meta = (ClampMin = "0.05", ClampMax = "1.0", ToolTip = "Motion warping duration in seconds"))
	float WarpDuration = 0.3f;

	/** Whether character can trigger interaction instantly without stepping closer if already within reach distance */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|Reach", meta = (ToolTip = "If true, skips walk approach when character is within reach distance and facing object"))
	bool bEnableAdaptiveReach = true;

	/** Maximum distance (cm) from character to interaction target to consider within reach */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|Reach", meta = (ClampMin = "20.0", ClampMax = "200.0", ToolTip = "Maximum distance in cm for adaptive instant reach"))
	float AdaptiveReachDistance = 65.0f;

	/** Maximum yaw angle (degrees) between character facing direction and interaction point for adaptive reach */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|Reach", meta = (ClampMin = "10.0", ClampMax = "180.0", ToolTip = "Maximum angle in degrees facing the object for adaptive reach"))
	float AdaptiveReachAngle = 45.0f;

	/** Can this object be interacted with only once (e.g. chest) or repeatedly (e.g. reusable switch)? */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|Rules", meta = (ToolTip = "If true, becomes disabled after first successful interaction"))
	bool bOneShot = true;

	/** If true, interaction toggles state between active and inactive repeatedly */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|Rules", meta = (ToolTip = "If true, object toggles state repeatedly rather than single activation"))
	bool bToggleMode = false;

	/** Optional GameplayTag required in character inventory or state (e.g. Item.Key.DungeonKey) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|Requirements", meta = (ToolTip = "Optional tag required on character to unlock this interaction"))
	FGameplayTag RequiredTag = FGameplayTag::EmptyTag;

	/** If true, removes RequiredTag from character ASC once unlocked */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|Requirements", meta = (ToolTip = "If true, consumes RequiredTag from character when unlocked"))
	bool bConsumeRequiredTag = false;

	/** Optional text shown if player lacks the required key/tag (e.g. 'Требуется ключ') */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|Requirements", meta = (ToolTip = "Text displayed if RequiredTag is missing on character"))
	FText LockedPromptText = FText::FromString(TEXT("Заперто"));

	/** Sound played when player attempts to interact while locked (missing RequiredTag) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|Audio", meta = (ToolTip = "Sound played when interaction is denied due to missing RequiredTag"))
	TSoftObjectPtr<USoundBase> LockedSound = nullptr;

	/** Default sound played when interaction starts/opens (can be overridden per instance) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|Audio", meta = (ToolTip = "Default sound played when opening/activating"))
	TSoftObjectPtr<USoundBase> DefaultOpenSound = nullptr;

	/** Default sound played when resetting back to inactive state (can be overridden per instance) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThreadTales|Interaction|Audio", meta = (ToolTip = "Default sound played on reset/closing"))
	TSoftObjectPtr<USoundBase> DefaultResetSound = nullptr;
};
