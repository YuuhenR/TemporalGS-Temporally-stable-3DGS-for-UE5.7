// Copyright (c) 2026 TemporalGS contributors. MIT License.

#include "TemporalGSRendererModule.h"
#include "TemporalGSSceneViewExtension.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Misc/CoreDelegates.h"
#include "Engine/Engine.h"          // GEngine
#include "ShaderCore.h"
#include "SceneViewExtension.h"

DEFINE_LOG_CATEGORY_STATIC(LogTemporalGS, Log, All);

#define LOCTEXT_NAMESPACE "FTemporalGSRendererModule"

FTemporalGSRendererModule& FTemporalGSRendererModule::Get()
{
	return FModuleManager::LoadModuleChecked<FTemporalGSRendererModule>("TemporalGSRenderer");
}

bool FTemporalGSRendererModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("TemporalGSRenderer");
}

void FTemporalGSRendererModule::StartupModule()
{
	// 1) Map the plugin's "Shaders/" folder to the virtual path "/TemporalGS" (safe this early).
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("TemporalGS")))
	{
		const FString ShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/TemporalGS"), ShaderDir);
		UE_LOG(LogTemporalGS, Log, TEXT("TemporalGS: mapped shader dir /TemporalGS -> %s"), *ShaderDir);
	}

	// 2) Register the SceneViewExtension only once GEngine exists. FSceneViewExtensions::NewExtension
	//    ensures on GEngine, and this module loads at PostConfigInit (before the engine is created),
	//    so registering here directly fails and the extension never runs.
	if (GEngine)
	{
		RegisterSceneViewExtension();
	}
	else
	{
		PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FTemporalGSRendererModule::RegisterSceneViewExtension);
	}
}

void FTemporalGSRendererModule::RegisterSceneViewExtension()
{
	if (!SceneViewExtension.IsValid())
	{
		SceneViewExtension = FSceneViewExtensions::NewExtension<FTemporalGSSceneViewExtension>();
		UE_LOG(LogTemporalGS, Log, TEXT("TemporalGS: SceneViewExtension registered."));
	}
}

void FTemporalGSRendererModule::ShutdownModule()
{
	if (PostEngineInitHandle.IsValid())
	{
		FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
		PostEngineInitHandle.Reset();
	}
	SceneViewExtension.Reset();
	UE_LOG(LogTemporalGS, Log, TEXT("TemporalGS: shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTemporalGSRendererModule, TemporalGSRenderer)
