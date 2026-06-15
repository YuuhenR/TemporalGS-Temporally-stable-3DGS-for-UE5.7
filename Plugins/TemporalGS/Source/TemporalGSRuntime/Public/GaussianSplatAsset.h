// Copyright (c) 2026 TemporalGS contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GaussianSplatProxy.h"   // FGaussianSplatCPUData (from TemporalGSRenderer)
#include "GaussianSplatAsset.generated.h"

/**
 * Holds the decoded Gaussian cloud for one splat scene.
 * The heavy per-Gaussian arrays live in a thread-safe shared FGaussianSplatCPUData so the render
 * proxy can reference them without copying hundreds of MB.
 */
UCLASS(BlueprintType)
class TEMPORALGSRUNTIME_API UGaussianSplatAsset : public UObject
{
	GENERATED_BODY()

public:
	UGaussianSplatAsset();

	/** Struct-of-arrays gaussian data (shared with the render proxy). Not a UPROPERTY (raw floats). */
	TSharedRef<FGaussianSplatCPUData, ESPMode::ThreadSafe> Data;

	UPROPERTY(VisibleAnywhere, Category = "TemporalGS")
	int32 NumPoints = 0;

	/** Local-space AABB of the gaussian means (raw .ply coordinates). */
	UPROPERTY(VisibleAnywhere, Category = "TemporalGS")
	FBox LocalBounds = FBox(ForceInit);

	/** True once Data has been populated from a .ply. */
	bool IsLoaded() const { return NumPoints > 0 && Data->IsValid(); }
};
