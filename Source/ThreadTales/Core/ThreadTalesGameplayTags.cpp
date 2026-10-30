// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesGameplayTags.h"

namespace ThreadTalesGameplayTags
{
	// Abilities
	UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Melee, "Ability.Melee");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Ranged, "Ability.Ranged");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Ranged_Fire, "Ability.Ranged.Fire");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Ranged_Slingshot, "Ability.Ranged.Slingshot");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Evade, "Ability.Evade");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Block, "Ability.Block");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Movement_DoubleJump, "Ability.Movement.DoubleJump");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Movement_AirEvade, "Ability.Movement.AirEvade");

	// Events
	UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Evade_CheckInput, "Event.Ability.Evade.CheckInput");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Combat_Attack_Dodge, "Event.Combat.Attack.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Combat_Attack_ParryCounter, "Event.Combat.Attack.ParryCounter");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Combat_Parry, "Event.Combat.Parry");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Combat_PostureBreak, "Event.Combat.PostureBreak");

	// Combat & Movement States
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Combat_InCombat, "State.Combat.InCombat");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Combat_Attacking, "State.Combat.Attacking");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Combat_Aiming, "State.Combat.Aiming");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Combat_Evading, "State.Combat.Evading");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Combat_Invulnerable, "State.Combat.Invulnerable");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Combat_Staggered, "State.Combat.Staggered");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Combat_PostureBroken, "State.Combat.PostureBroken");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Combat_HitReact, "State.Combat.HitReact");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Combat_StaminaRegenBlocked, "State.Combat.StaminaRegenBlocked");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Combat_Blocking, "State.Combat.Blocking");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Combat_Parrying, "State.Combat.Parrying");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Combat_ParryCounterWindow, "State.Combat.ParryCounterWindow");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Combat_TargetLocked, "State.Combat.TargetLocked");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Combat_Dummy_Windup, "State.Combat.Dummy.Windup");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Combat_Dummy_ParryWindow, "State.Combat.Dummy.ParryWindow");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Movement_Grounded, "State.Movement.Grounded");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Movement_Airborne, "State.Movement.Airborne");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Movement_DoubleJumping, "State.Movement.DoubleJumping");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Movement_HasAirDashed, "State.Movement.HasAirDashed");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Movement_Climbing, "State.Movement.Climbing");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Movement_Climbing_Hang_Braced, "State.Movement.Climbing.Hang.Braced");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Movement_Climbing_Hang_Freehang, "State.Movement.Climbing.Hang.Freehang");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Movement_Climbing_Shimmy, "State.Movement.Climbing.Shimmy");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Movement_Climbing_Shimmy_Left, "State.Movement.Climbing.Shimmy.Left");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Movement_Climbing_Shimmy_Right, "State.Movement.Climbing.Shimmy.Right");
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Interacting, "State.Interacting");

	// Input Tags
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Move, "InputTag.Move");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Look, "InputTag.Look");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Crouch, "InputTag.Crouch");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Sprint, "InputTag.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Ability_Melee, "InputTag.Ability.Combat.Melee");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Ability_Ranged, "InputTag.Ability.Combat.Ranged");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Ability_Ranged_Fire, "InputTag.Ability.Combat.Ranged.Fire");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Ability_Evade, "InputTag.Ability.Combat.Evade");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Ability_Block, "InputTag.Ability.Combat.Block");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Ability_Jump, "InputTag.Ability.Movement.Jump");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Ability_ToggleStance, "InputTag.Ability.Combat.ToggleStance");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Ability_TargetLock, "InputTag.Ability.Combat.TargetLock");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Ability_TargetSwitch, "InputTag.Ability.Combat.TargetSwitch");

	// SetByCaller Tags for GameplayEffects
	UE_DEFINE_GAMEPLAY_TAG(TAG_SetByCaller_Damage, "SetByCaller.Damage");
	UE_DEFINE_GAMEPLAY_TAG(TAG_SetByCaller_PoiseDamage, "SetByCaller.PoiseDamage");
}
