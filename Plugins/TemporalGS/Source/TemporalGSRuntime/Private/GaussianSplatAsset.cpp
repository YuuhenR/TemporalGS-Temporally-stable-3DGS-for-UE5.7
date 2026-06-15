// Copyright (c) 2026 TemporalGS contributors. MIT License.

#include "GaussianSplatAsset.h"

UGaussianSplatAsset::UGaussianSplatAsset()
	: Data(MakeShared<FGaussianSplatCPUData, ESPMode::ThreadSafe>())
{
}
