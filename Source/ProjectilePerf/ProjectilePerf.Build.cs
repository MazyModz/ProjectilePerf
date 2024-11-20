// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProjectilePerf : ModuleRules
{
	public ProjectilePerf(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Niagara", "NiagaraCore" });
	}
}
