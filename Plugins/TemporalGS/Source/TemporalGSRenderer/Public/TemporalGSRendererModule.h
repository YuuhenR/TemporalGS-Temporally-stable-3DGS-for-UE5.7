// Copyright (c) 2026 TemporalGS contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "SceneViewExtension.h"

class FTemporalGSSceneViewExtension;

/**
 * TemporalGSRenderer module.
 *
 * Owns the SceneViewExtension that injects the splat rasterization + velocity passes on the
 * render thread, and maps the plugin's virtual shader directory ("/TemporalGS").
 *
 * Loaded at PostConfigInit so the shader mapping and the extension exist before the first frame.
 */
class TEMPORALGSRENDERER_API FTemporalGSRendererModule : public IModuleInterface
{
public:
	static FTemporalGSRendererModule& Get();
	static bool IsAvailable();

	//~ IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** The live SceneViewExtension (valid after engine init). */
	TSharedPtr<FTemporalGSSceneViewExtension, ESPMode::ThreadSafe> GetSceneViewExtension() const { return SceneViewExtension; }

private:
	/** NewExtension requires GEngine, which doesn't exist at PostConfigInit; register after engine init. */
	void RegisterSceneViewExtension();

	TSharedPtr<FTemporalGSSceneViewExtension, ESPMode::ThreadSafe> SceneViewExtension;
	FDelegateHandle PostEngineInitHandle;
};
