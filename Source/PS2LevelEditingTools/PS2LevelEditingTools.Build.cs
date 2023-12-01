// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PS2LevelEditingTools : ModuleRules
{
	public PS2LevelEditingTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "Core",
				"CoreUObject",
				"Engine",
				"DeveloperSettings",
				"InterchangeCore",
				"InterchangeImport",
				"InterchangeEngine",
				"InterchangeNodes",
				"MeshDescription",
                "StaticMeshDescription",
				"Slate",
				"SlateCore",

				"MeshOptimizer",
                "PS2LevelEditingToolsLibrary"
			}
		);
	}
}
