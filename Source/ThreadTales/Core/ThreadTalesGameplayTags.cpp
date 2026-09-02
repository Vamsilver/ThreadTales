// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesGameplayTags.h"

namespace ThreadTalesGameplayTags
{
	// Abilities
	UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Melee, "Ability.Melee");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Ranged, "Ability.Ranged");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Ranged_Slingshot, "Ability.Ranged.Slingshot");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Evade, "Ability.Evade");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Movement_DoubleJump, "Ability.Movement.DoubleJump");

	// Events
	UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Evade_CheckInput, "Event.Ability.Evade.CheckInput");

	// Combat & Movement States
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Combat_InCombat, "State.Combat.InCombat");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Combat_Attacking, "State.Combat.Attacking");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Combat_Aiming, "State.Combat.Aiming");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Combat_Evading, "State.Combat.Evading");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Combat_Invulnerable, "State.Combat.Invulnerable");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Movement_DoubleJumping, "State.Movement.DoubleJumping");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Movement_HasAirDashed, "State.Movement.HasAirDashed");

	// Input Tags
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Move, "InputTag.Move");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Look, "InputTag.Look");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Ability_Melee, "InputTag.Ability.Combat.Melee");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Ability_Ranged, "InputTag.Ability.Combat.Ranged");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Ability_Evade, "InputTag.Ability.Combat.Evade");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Ability_Jump, "InputTag.Ability.Movement.Jump");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Ability_ToggleStance, "InputTag.Ability.Combat.ToggleStance");
}
