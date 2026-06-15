// Copyright (c) 2026 TemporalGS contributors. MIT License.

using UnrealBuildTool;
using System.IO;

public class TemporalGSRenderer : ModuleRules
{
	public TemporalGSRenderer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"RenderCore",
			"RHI",
			"Renderer",
			"Projects",        // IPluginManager (locate the Shaders dir)
		});

		// FPostProcessingInputs (the PrePostProcessPass payload that exposes the view-family color
		// target) lives in the engine's Renderer "Internal" headers. Those headers ship with the
		// binary (launcher) engine too, and we only READ struct fields (no private symbol linking),
		// so this builds against a launcher install. Adjust if your engine layout differs.
		PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source/Runtime/Renderer/Internal"));
	}
}
