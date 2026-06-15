// Copyright (c) 2026 TemporalGS contributors. MIT License.

#pragma once

#include "CoreMinimal.h"

struct FGaussianSplatCPUData;

/**
 * Parser for INRIA-format 3D Gaussian Splatting .ply files (binary little-endian).
 *
 * Expected per-vertex float properties:
 *   x y z, f_dc_0..2 (SH DC), opacity, scale_0..2, rot_0..3 (quaternion w,x,y,z).
 * f_rest_* (higher SH bands) and normals are tolerated but ignored.
 * On load we decode: scale = exp(), opacity = sigmoid(), rotation normalized,
 * colorDC = 0.5 + C0 * f_dc.
 */
class TEMPORALGSRUNTIME_API FGaussianSplatPlyLoader
{
public:
	/** Returns false and fills OutError on failure. Positions are raw .ply coordinates. */
	static bool LoadInriaPly(const FString& FilePath, FGaussianSplatCPUData& Out, FBox& OutBounds, FString& OutError);
};
