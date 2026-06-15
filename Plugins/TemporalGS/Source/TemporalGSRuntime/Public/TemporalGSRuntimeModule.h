// Copyright (c) 2026 TemporalGS contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * TemporalGSRuntime module: gameplay-facing layer.
 * Holds the Gaussian splat asset, the .ply loader, and the component/actor that place splats in
 * the world and register a render-thread proxy with TemporalGSRenderer.
 */
class FTemporalGSRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
