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
			"Niagara",
			"PoseSearch",
			"Chooser",
			"StructUtils"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"ThreadTales",
			"ThreadTales/Core",
			"ThreadTales/Characters",
			"ThreadTales/Combat",
			"ThreadTales/Abilities",
			"ThreadTales/Animation",
			"ThreadTales/Input"
		});
	}
}

