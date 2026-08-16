// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ThreadTales : ModuleRules
{
	public ThreadTales(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"Niagara"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"ThreadTales",
			"ThreadTales/Character",
			"ThreadTales/Weapons",
			"ThreadTales/Abilities",
			"ThreadTales/Animation",
			"ThreadTales/Variant_Platforming",
			"ThreadTales/Variant_Platforming/Animation",
			"ThreadTales/Variant_Combat",
			"ThreadTales/Variant_Combat/AI",
			"ThreadTales/Variant_Combat/Animation",
			"ThreadTales/Variant_Combat/Gameplay",
			"ThreadTales/Variant_Combat/Interfaces",
			"ThreadTales/Variant_Combat/UI",
			"ThreadTales/Variant_SideScrolling",
			"ThreadTales/Variant_SideScrolling/AI",
			"ThreadTales/Variant_SideScrolling/Gameplay",
			"ThreadTales/Variant_SideScrolling/Interfaces",
			"ThreadTales/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
