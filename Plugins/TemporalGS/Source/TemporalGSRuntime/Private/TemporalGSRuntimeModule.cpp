// Copyright (c) 2026 TemporalGS contributors. MIT License.

#include "TemporalGSRuntimeModule.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogTemporalGSRuntime, Log, All);

void FTemporalGSRuntimeModule::StartupModule()
{
	UE_LOG(LogTemporalGSRuntime, Log, TEXT("TemporalGSRuntime started."));
}

void FTemporalGSRuntimeModule::ShutdownModule()
{
	UE_LOG(LogTemporalGSRuntime, Log, TEXT("TemporalGSRuntime shut down."));
}

IMPLEMENT_MODULE(FTemporalGSRuntimeModule, TemporalGSRuntime)
