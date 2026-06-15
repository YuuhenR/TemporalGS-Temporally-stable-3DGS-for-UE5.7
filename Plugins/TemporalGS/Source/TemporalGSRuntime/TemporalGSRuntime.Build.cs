// Copyright (c) 2026 TemporalGS contributors. MIT License.

using UnrealBuildTool;

public class TemporalGSRuntime : ModuleRules
{
	public TemporalGSRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
			"RHI",
			"TemporalGSRenderer",   // register render-thread proxies with the SceneViewExtension
		});
	}
}
