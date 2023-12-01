// Fill out your copyright notice in the Description page of Project Settings.

using System.IO;
using UnrealBuildTool;

public class MeshOptimizer : ModuleRules
{
	public MeshOptimizer(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		Type = ModuleType.External;
		PublicSystemIncludePaths.Add("$(ModuleDir)/meshoptimizer/src");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Add the import library
			PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "meshoptimizer", "build", "Debug", "meshoptimizer.lib"));
		}
	}
}
