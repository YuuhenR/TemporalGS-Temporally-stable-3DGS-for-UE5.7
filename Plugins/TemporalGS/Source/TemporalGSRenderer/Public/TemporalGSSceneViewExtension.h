// Copyright (c) 2026 TemporalGS contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "RenderGraphFwd.h"

/**
 * FTemporalGSSceneViewExtension
 *
 * Injects TemporalGS work into the engine's deferred renderer on the render thread.
 *
 * The interesting override is PostRenderBasePassDeferred_RenderThread: at that point the GBuffer
 * (including the velocity target) is bound and the scene depth exists, which is exactly where we
 * want to (a) rasterize the splats into scene color with a representative depth, and
 * (b) write per-pixel motion vectors into the velocity buffer so TSR/TAA can reproject correctly.
 *
 * v0.1: this class registers, logs liveness, and reserves the pass. The compute rasterizer and
 * velocity derivation land on top of the proxy registry (see ROADMAP.md, Phase 2/3).
 *
 * NOTE: SceneViewExtension callback signatures shift slightly between UE versions. These match
 * UE 5.7. If you build against a different minor version, reconcile against
 * Engine/Source/Runtime/Engine/Public/SceneViewExtension.h.
 */
class TEMPORALGSRENDERER_API FTemporalGSSceneViewExtension : public FSceneViewExtensionBase
{
public:
	explicit FTemporalGSSceneViewExtension(const FAutoRegister& AutoRegister);

	//~ Begin ISceneViewExtension (game thread)
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual int32 GetPriority() const override { return 0; }
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;
	//~ End ISceneViewExtension (game thread)

	//~ Begin ISceneViewExtension (render thread)
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;

	/**
	 * Phase 3: draw the splats just before post-processing (i.e. before TSR/TAA), into the HDR
	 * SceneColor that TSR consumes, while writing per-pixel motion vectors into the velocity buffer
	 * and depth-testing against scene depth for correct occlusion. This is where the temporal-
	 * stability contribution lives: splats now participate in TSR, and the velocity makes them
	 * reproject correctly instead of ghosting.
	 */
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView, const FPostProcessingInputs& Inputs) override;
	//~ End ISceneViewExtension (render thread)

private:
	/** Rasterize all registered splat proxies into SceneColor (+ velocity), depth-tested for occlusion. */
	void RenderSplats_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView,
		FRDGTextureRef ColorTarget, FRDGTextureRef VelocityTarget, FRDGTextureRef DepthTarget, const FIntRect& ViewRect);

	/** Throttle the liveness log so it does not spam every frame. */
	mutable int32 FrameLogCounter = 0;

	// Previous-frame (jittered) view-projection + TAA jitter, cached on the render thread for the
	// motion vector. FSceneView has no PrevViewMatrices (that lives in the private FViewInfo), so we
	// keep our own. Single-view assumption (fine for an editor viewport / game main view).
	FMatrix44f PrevFrameViewProj = FMatrix44f::Identity;
	FVector2f PrevFrameJitter = FVector2f::ZeroVector;
	bool bHasPrevFrame = false;
};
