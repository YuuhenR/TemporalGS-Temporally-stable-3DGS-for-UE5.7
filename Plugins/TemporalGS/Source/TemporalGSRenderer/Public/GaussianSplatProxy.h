// Copyright (c) 2026 TemporalGS contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Math/Matrix.h"
#include "RenderGraphResources.h"   // FRDGPooledBuffer (persistent GPU buffers)

/**
 * CPU-side Gaussian data in struct-of-arrays form (GPU-upload friendly).
 * All values are already decoded from the .ply storage convention:
 *   - Scales:   exp() applied
 *   - Opacities: sigmoid() applied
 *   - Rotations: normalized quaternion (w, x, y, z)
 *   - ColorsDC:  0.5 + C0 * f_dc  (SH degree-0 to linear-ish color)
 */
struct FGaussianSplatCPUData
{
	TArray<FVector3f> Positions;   // local space (raw .ply coords)
	TArray<FVector3f> Scales;      // exp(scale)
	TArray<FQuat4f>   Rotations;   // normalized
	TArray<float>     Opacities;   // sigmoid(opacity)
	TArray<FVector3f> ColorsDC;    // base color from SH DC

	int32 Num() const { return Positions.Num(); }

	bool IsValid() const
	{
		const int32 N = Positions.Num();
		return N > 0
			&& Scales.Num() == N
			&& Rotations.Num() == N
			&& Opacities.Num() == N
			&& ColorsDC.Num() == N;
	}

	void Reset()
	{
		Positions.Reset();
		Scales.Reset();
		Rotations.Reset();
		Opacities.Reset();
		ColorsDC.Reset();
	}

	SIZE_T GetAllocatedSize() const
	{
		return Positions.GetAllocatedSize() + Scales.GetAllocatedSize()
			+ Rotations.GetAllocatedSize() + Opacities.GetAllocatedSize()
			+ ColorsDC.GetAllocatedSize();
	}
};

/**
 * Render-thread representation of one placed splat cloud.
 *
 * Phase 1: holds the CPU data + the component's current/previous transforms (the previous
 * transform is what makes per-pixel velocity possible later).
 * Phase 2: lazily creates GPU StructuredBuffers from CpuData on first use.
 * Phase 3: GSProjectCS uses LocalToWorld vs PrevLocalToWorld for the velocity.
 */
class TEMPORALGSRENDERER_API FGaussianSplatProxy
{
public:
	/** Shared CPU data (owned by the component, referenced here so we don't copy ~hundreds of MB). */
	TSharedPtr<FGaussianSplatCPUData, ESPMode::ThreadSafe> CpuData;

	/** Current and previous frame transforms (for velocity). PrevLocalToWorld starts == LocalToWorld. */
	FMatrix44f LocalToWorld = FMatrix44f::Identity;
	FMatrix44f PrevLocalToWorld = FMatrix44f::Identity;

	/** Set when CpuData changed and GPU buffers must be (re)uploaded. */
	bool bGpuDirty = true;

	int32 Num() const { return CpuData.IsValid() ? CpuData->Num() : 0; }

	// --- Phase 2: packed arrays the rasterizer uploads to GPU StructuredBuffers --------------------
	// Built once from CpuData (BuildGpuArrays). Kept resident so RDG can copy them each frame.
	TArray<FVector4f> GpuPositions;   // xyz = local position, w = 1
	TArray<FVector4f> GpuColors;      // rgb = DC color, a = opacity
	TArray<FVector4f> GpuScales;      // xyz = scale (exp'd), w unused
	TArray<FVector4f> GpuRotations;   // xyzw = normalized quaternion

	/** Populate the GPU arrays from CpuData. Call on the game thread after CpuData is set. */
	void BuildGpuArrays();

	// --- Phase 6: render-thread caches (persistent buffers + sort cache) --------------------------
	// Static per-Gaussian data uploaded to the GPU once (not re-uploaded every frame).
	TRefCountPtr<FRDGPooledBuffer> PooledPositions;
	TRefCountPtr<FRDGPooledBuffer> PooledColors;
	TRefCountPtr<FRDGPooledBuffer> PooledScales;
	TRefCountPtr<FRDGPooledBuffer> PooledRotations;

	// World-space positions cached for depth sorting; rebuilt only when LocalToWorld changes.
	TArray<FVector3f> WorldPositions;
	FMatrix44f WorldPosL2W = FMatrix44f::Identity;
	bool bWorldDirty = true;

	// Back-to-front index order cached; re-sorted only when the camera moves enough.
	TArray<uint32> CachedSortedIndices;
	FVector3f LastSortCamPos = FVector3f(3.4e38f);
	bool bSortValid = false;
};

using FGaussianSplatProxyPtr = TSharedPtr<FGaussianSplatProxy, ESPMode::ThreadSafe>;

/**
 * Thread-safe registry of live splat proxies.
 *
 * Components register/unregister on the game thread; the SceneViewExtension iterates on the
 * render thread. A critical section guards the array (the per-frame proxy count is tiny).
 */
class TEMPORALGSRENDERER_API FTemporalGSProxyRegistry
{
public:
	static FTemporalGSProxyRegistry& Get();

	void Register(const FGaussianSplatProxyPtr& Proxy);
	void Unregister(const FGaussianSplatProxyPtr& Proxy);

	/** Snapshot the current proxies (copy under lock) for safe render-thread iteration. */
	void GetProxies(TArray<FGaussianSplatProxyPtr>& Out) const;

	int32 Num() const;

private:
	mutable FCriticalSection Mutex;
	TArray<FGaussianSplatProxyPtr> Proxies;
};
