// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MyCppProject : ModuleRules
{
	public MyCppProject(ReadOnlyTargetRules Target) : base(Target)
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
			"GameplayTasks"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"MyCppProject",
			"MyCppProject/Variant_Platforming",
			"MyCppProject/Variant_Platforming/Animation",
			"MyCppProject/Variant_Combat",
			"MyCppProject/Variant_Combat/AI",
			"MyCppProject/Variant_Combat/Animation",
			"MyCppProject/Variant_Combat/Gameplay",
			"MyCppProject/Variant_Combat/Interfaces",
			"MyCppProject/Variant_Combat/UI",
			"MyCppProject/Variant_SideScrolling",
			"MyCppProject/Variant_SideScrolling/AI",
			"MyCppProject/Variant_SideScrolling/Gameplay",
			"MyCppProject/Variant_SideScrolling/Interfaces",
			"MyCppProject/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
