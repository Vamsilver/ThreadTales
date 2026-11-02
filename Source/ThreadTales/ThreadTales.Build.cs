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
			"SlateCore",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"Niagara",
			"PoseSearch",
			"Chooser",
			"StructUtils",
			"MotionWarping",
			"NavigationSystem",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"SteamSockets",
			"EngineSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
		AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] {
				"StateTreeEditorModule",
				"UnrealEd"
			});
		}

		PublicIncludePaths.AddRange(new string[] {
			"ThreadTales",
			"ThreadTales/Core",
			"ThreadTales/Characters",
			"ThreadTales/Combat",
			"ThreadTales/Abilities",
			"ThreadTales/Animation",
			"ThreadTales/Input",
			"ThreadTales/UI"
		});
	}
}

