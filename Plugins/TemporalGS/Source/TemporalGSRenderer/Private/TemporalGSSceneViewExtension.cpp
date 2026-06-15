// Copyright (c) 2026 TemporalGS contributors. MIT License.

#include "TemporalGSSceneViewExtension.h"
#include "GaussianSplatProxy.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneView.h"
#include "SceneRenderTargetParameters.h"            // FSceneTextureUniformParameters
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "CommonRenderResources.h"                  // GEmptyVertexDeclaration
#include "PostProcess/PostProcessInputs.h"          // FPostProcessingInputs (Renderer/Internal)

DEFINE_LOG_CATEGORY_STATIC(LogTemporalGSSVE, Log, All);

// ---- Console variables -------------------------------------------------------------------------
static TAutoConsoleVariable<int32> CVarTGSEnable(
	TEXT("r.TemporalGS.Enable"), 1, TEXT("Enable the TemporalGS SceneViewExtension."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarTGSSplatPixelRadius(
	TEXT("r.TemporalGS.SplatPixelRadius"), 3.0f, TEXT("Disc mode radius in pixels."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarTGSMode(
	TEXT("r.TemporalGS.Mode"), 1, TEXT("0 = disc, 1 = anisotropic conic."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarTGSSigma(
	TEXT("r.TemporalGS.Sigma"), 3.0f, TEXT("Conic mode: sigmas the splat quad spans."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarTGSOpacityScale(
	TEXT("r.TemporalGS.OpacityScale"), 1.0f, TEXT("Density multiplier on per-splat opacity."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarTGSDebug(
	TEXT("r.TemporalGS.Debug"), 0, TEXT("1 = solid magenta (visibility test)."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarTGSOcclude(
	TEXT("r.TemporalGS.Occlude"), 1, TEXT("Depth-test splats against the scene depth (1) so meshes occlude them."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarTGSWriteVelocity(
	TEXT("r.TemporalGS.WriteVelocity"), 1,
	TEXT("Write per-pixel motion vectors for splats into the velocity buffer (1=fixed) or not (0=baseline TSR ghosting)."),
	ECVF_RenderThreadSafe);

// ---- Rasterizer shaders ------------------------------------------------------------------------
BEGIN_SHADER_PARAMETER_STRUCT(FGSRasterParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GSPositions)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GSColors)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GSScales)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GSRotations)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, GSSortedIndices)
	SHADER_PARAMETER(FMatrix44f, LocalToWorld)
	SHADER_PARAMETER(FMatrix44f, ViewProj)
	SHADER_PARAMETER(FMatrix44f, PrevViewProj)
	SHADER_PARAMETER(FVector2f, JitterCurr)
	SHADER_PARAMETER(FVector2f, JitterPrev)
	SHADER_PARAMETER(FVector2f, OutputExtent)
	SHADER_PARAMETER(float, QuadPixelRadius)
	SHADER_PARAMETER(float, SigmaScale)
	SHADER_PARAMETER(float, OpacityScale)
	SHADER_PARAMETER(uint32, NumGaussians)
	SHADER_PARAMETER(uint32, DebugMode)
	SHADER_PARAMETER(uint32, RenderMode)
	SHADER_PARAMETER(uint32, WriteVelocity)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FGSRasterVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGSRasterVS);
	SHADER_USE_PARAMETER_STRUCT(FGSRasterVS, FGlobalShader);
	using FParameters = FGSRasterParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters&) { return true; }
};

class FGSRasterPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGSRasterPS);
	SHADER_USE_PARAMETER_STRUCT(FGSRasterPS, FGlobalShader);
	using FParameters = FGSRasterParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters&) { return true; }
};

IMPLEMENT_GLOBAL_SHADER(FGSRasterVS, "/TemporalGS/Private/GSRaster.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FGSRasterPS, "/TemporalGS/Private/GSRaster.usf", "MainPS", SF_Pixel);

namespace
{
	// LSD radix sort of Gaussian indices into far->near order. ~10x faster than a comparison sort
	// over ~750k elements, which keeps camera navigation smooth.
	void RadixSortFarToNear(const TArray<FVector3f>& WorldPos, const FVector3f& CamPos, TArray<uint32>& Out)
	{
		const int32 N = WorldPos.Num();
		TArray<uint32> Keys;
		Keys.SetNumUninitialized(N);
		for (int32 i = 0; i < N; ++i)
		{
			const float D = FVector3f::DistSquared(WorldPos[i], CamPos);   // >= 0
			uint32 K;
			FMemory::Memcpy(&K, &D, sizeof(uint32));
			Keys[i] = ~K;   // non-negative float bits are monotonic; ~ makes an ascending sort far-first
		}

		TArray<uint32> A, B;
		A.SetNumUninitialized(N);
		B.SetNumUninitialized(N);
		for (int32 i = 0; i < N; ++i) { A[i] = (uint32)i; }

		uint32* Src = A.GetData();
		uint32* Dst = B.GetData();
		for (int32 Shift = 0; Shift < 32; Shift += 8)
		{
			uint32 Count[256] = {};
			for (int32 i = 0; i < N; ++i) { Count[(Keys[Src[i]] >> Shift) & 0xFF]++; }
			uint32 Sum = 0;
			for (int32 b = 0; b < 256; ++b) { const uint32 C = Count[b]; Count[b] = Sum; Sum += C; }
			for (int32 i = 0; i < N; ++i) { const uint32 Idx = Src[i]; Dst[Count[(Keys[Idx] >> Shift) & 0xFF]++] = Idx; }
			Swap(Src, Dst);
		}
		// 4 (even) passes -> sorted result is back in A (== Src now).
		Out.SetNumUninitialized(N);
		FMemory::Memcpy(Out.GetData(), Src, (SIZE_T)N * sizeof(uint32));
	}
}

// ------------------------------------------------------------------------------------------------

FTemporalGSSceneViewExtension::FTemporalGSSceneViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
}

bool FTemporalGSSceneViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	return CVarTGSEnable.GetValueOnAnyThread() != 0 && FTemporalGSProxyRegistry::Get().Num() > 0;
}

void FTemporalGSSceneViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
}

void FTemporalGSSceneViewExtension::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView, const FPostProcessingInputs& Inputs)
{
	if (CVarTGSEnable.GetValueOnRenderThread() == 0 || FTemporalGSProxyRegistry::Get().Num() == 0)
	{
		return;
	}
	if (!Inputs.SceneTextures)
	{
		return;
	}

	const FSceneTextureUniformParameters* ST = Inputs.SceneTextures->GetContents();
	if (ST == nullptr || ST->SceneColorTexture == nullptr)
	{
		return;
	}

	FRDGTextureRef ColorTarget = ST->SceneColorTexture;     // HDR scene color, consumed by TSR/TAA
	FRDGTextureRef DepthTarget = ST->SceneDepthTexture;     // for occlusion
	FRDGTextureRef VelocityTarget = ST->GBufferVelocityTexture; // Phase 3b

	// Pre-upscale scene color is at render resolution; draw over the whole target.
	const FIntRect ViewRect(0, 0, ColorTarget->Desc.Extent.X, ColorTarget->Desc.Extent.Y);

	static int32 PpLog = 0;
	if ((PpLog++ % 180) == 0)
	{
		UE_LOG(LogTemporalGSSVE, Display, TEXT("TemporalGS: PrePostProcess draw. proxies=%d color=%dx%d depth=%d velocity=%d"),
			FTemporalGSProxyRegistry::Get().Num(), ViewRect.Width(), ViewRect.Height(),
			DepthTarget ? 1 : 0, VelocityTarget ? 1 : 0);
	}

	RenderSplats_RenderThread(GraphBuilder, InView, ColorTarget, VelocityTarget, DepthTarget, ViewRect);
}

void FTemporalGSSceneViewExtension::RenderSplats_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView,
	FRDGTextureRef ColorTarget, FRDGTextureRef VelocityTarget, FRDGTextureRef DepthTarget, const FIntRect& ViewRect)
{
	TArray<FGaussianSplatProxyPtr> Proxies;
	FTemporalGSProxyRegistry::Get().GetProxies(Proxies);
	if (Proxies.Num() == 0 || ColorTarget == nullptr)
	{
		return;
	}

	const FMatrix44f ViewProj = FMatrix44f(InView.ViewMatrices.GetViewProjectionMatrix());
	const FVector2f OutExtent((float)ViewRect.Width(), (float)ViewRect.Height());
	const float QuadRadius = CVarTGSSplatPixelRadius.GetValueOnRenderThread();
	const float Sigma = CVarTGSSigma.GetValueOnRenderThread();
	const float OpacityScale = CVarTGSOpacityScale.GetValueOnRenderThread();
	const uint32 DebugMode = (uint32)CVarTGSDebug.GetValueOnRenderThread();
	const uint32 RenderMode = (uint32)CVarTGSMode.GetValueOnRenderThread();
	const bool bOcclude = CVarTGSOcclude.GetValueOnRenderThread() != 0 && DepthTarget != nullptr;
	const FVector3f CamPos = (FVector3f)InView.ViewMatrices.GetViewOrigin();

	// Phase 3b: motion vector uses this frame's (jittered) VP + jitter and last frame's cached ones.
	const FVector2f JitterCurr = (FVector2f)InView.ViewMatrices.GetTemporalAAJitter();
	const FMatrix44f PrevViewProj = bHasPrevFrame ? PrevFrameViewProj : ViewProj;
	const FVector2f JitterPrev = bHasPrevFrame ? PrevFrameJitter : JitterCurr;
	const bool bWriteVel = CVarTGSWriteVelocity.GetValueOnRenderThread() != 0 && VelocityTarget != nullptr;

	RDG_EVENT_SCOPE(GraphBuilder, "TemporalGS_Raster");

	for (const FGaussianSplatProxyPtr& Proxy : Proxies)
	{
		const int32 N = Proxy.IsValid() ? Proxy->Num() : 0;
		if (N == 0 || Proxy->GpuPositions.Num() != N || Proxy->GpuColors.Num() != N
			|| Proxy->GpuScales.Num() != N || Proxy->GpuRotations.Num() != N)
		{
			continue;
		}

		// --- Phase 6: cache world positions; rebuild only when the transform changes. ----------------
		if (Proxy->bWorldDirty || Proxy->WorldPositions.Num() != N || Proxy->WorldPosL2W != Proxy->LocalToWorld)
		{
			Proxy->WorldPositions.SetNumUninitialized(N);
			const FMatrix44f& L2W = Proxy->LocalToWorld;
			for (int32 i = 0; i < N; ++i)
			{
				const FVector4f& LP = Proxy->GpuPositions[i];
				Proxy->WorldPositions[i] = L2W.TransformPosition(FVector3f(LP.X, LP.Y, LP.Z));
			}
			Proxy->WorldPosL2W = Proxy->LocalToWorld;
			Proxy->bWorldDirty = false;
			Proxy->bSortValid = false;
		}

		// --- Phase 6: re-sort (radix, far->near) only when the camera moved enough -------------------
		const float ReSortDistSq = 1.0f; // world units^2; skip the sort while essentially static
		if (!Proxy->bSortValid || Proxy->CachedSortedIndices.Num() != N
			|| FVector3f::DistSquared(CamPos, Proxy->LastSortCamPos) > ReSortDistSq)
		{
			RadixSortFarToNear(Proxy->WorldPositions, CamPos, Proxy->CachedSortedIndices);
			Proxy->LastSortCamPos = CamPos;
			Proxy->bSortValid = true;
		}

		// --- Phase 6: static per-Gaussian data uploaded once; only the index buffer is per-frame. ----
		auto GetOrCreatePooled = [&GraphBuilder](TRefCountPtr<FRDGPooledBuffer>& Pooled, const TCHAR* Name, const void* Data, int32 Count) -> FRDGBufferRef
		{
			if (!Pooled.IsValid())
			{
				FRDGBufferRef Tmp = CreateStructuredBuffer(GraphBuilder, Name, sizeof(FVector4f), Count, Data, (uint64)Count * sizeof(FVector4f));
				Pooled = GraphBuilder.ConvertToExternalBuffer(Tmp);
				return Tmp;
			}
			return GraphBuilder.RegisterExternalBuffer(Pooled, Name);
		};

		FRDGBufferRef PosBuf = GetOrCreatePooled(Proxy->PooledPositions, TEXT("TGS.Positions"), Proxy->GpuPositions.GetData(), N);
		FRDGBufferRef ColBuf = GetOrCreatePooled(Proxy->PooledColors, TEXT("TGS.Colors"), Proxy->GpuColors.GetData(), N);
		FRDGBufferRef ScaleBuf = GetOrCreatePooled(Proxy->PooledScales, TEXT("TGS.Scales"), Proxy->GpuScales.GetData(), N);
		FRDGBufferRef RotBuf = GetOrCreatePooled(Proxy->PooledRotations, TEXT("TGS.Rotations"), Proxy->GpuRotations.GetData(), N);
		FRDGBufferRef IdxBuf = CreateStructuredBuffer(GraphBuilder, TEXT("TGS.SortedIndices"), sizeof(uint32), N, Proxy->CachedSortedIndices.GetData(), (uint64)N * sizeof(uint32));

		FGSRasterParameters* P = GraphBuilder.AllocParameters<FGSRasterParameters>();
		P->GSPositions = GraphBuilder.CreateSRV(PosBuf);
		P->GSColors = GraphBuilder.CreateSRV(ColBuf);
		P->GSScales = GraphBuilder.CreateSRV(ScaleBuf);
		P->GSRotations = GraphBuilder.CreateSRV(RotBuf);
		P->GSSortedIndices = GraphBuilder.CreateSRV(IdxBuf);
		P->LocalToWorld = Proxy->LocalToWorld;
		P->ViewProj = ViewProj;
		P->PrevViewProj = PrevViewProj;
		P->JitterCurr = JitterCurr;
		P->JitterPrev = JitterPrev;
		P->OutputExtent = OutExtent;
		P->QuadPixelRadius = QuadRadius;
		P->SigmaScale = Sigma;
		P->OpacityScale = OpacityScale;
		P->NumGaussians = (uint32)N;
		P->DebugMode = DebugMode;
		P->RenderMode = RenderMode;
		P->WriteVelocity = bWriteVel ? 1u : 0u;
		P->RenderTargets[0] = FRenderTargetBinding(ColorTarget, ERenderTargetLoadAction::ELoad);
		if (bWriteVel)
		{
			P->RenderTargets[1] = FRenderTargetBinding(VelocityTarget, ERenderTargetLoadAction::ELoad);
		}
		if (bOcclude)
		{
			P->RenderTargets.DepthStencil = FDepthStencilBinding(
				DepthTarget, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthRead_StencilNop);
		}

		TShaderMapRef<FGSRasterVS> VS(GetGlobalShaderMap(InView.GetFeatureLevel()));
		TShaderMapRef<FGSRasterPS> PS(GetGlobalShaderMap(InView.GetFeatureLevel()));

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("TGS.SplatDraw(%d)", N), P, ERDGPassFlags::Raster,
			[P, VS, PS, N, ViewRect, bOcclude, bWriteVel](FRHICommandList& RHICmdList)
			{
				FGraphicsPipelineStateInitializer PSOInit;
				RHICmdList.ApplyCachedRenderTargets(PSOInit);
				PSOInit.PrimitiveType = PT_TriangleStrip;
				// RT0 = premultiplied "over" color; RT1 (velocity) = opaque RG overwrite (nearest splat wins).
				PSOInit.BlendState = bWriteVel
					? TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha,
					                    CW_RG,   BO_Add, BF_One, BF_Zero,               BO_Add, BF_One, BF_Zero>::GetRHI()
					: TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
				PSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
				// Read-only depth test (reversed-Z: splat passes if in front of the scene); no depth write.
				PSOInit.DepthStencilState = bOcclude
					? TStaticDepthStencilState<false, CF_Greater>::GetRHI()
					: TStaticDepthStencilState<false, CF_Always>::GetRHI();
				PSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
				PSOInit.BoundShaderState.VertexShaderRHI = VS.GetVertexShader();
				PSOInit.BoundShaderState.PixelShaderRHI = PS.GetPixelShader();
				SetGraphicsPipelineState(RHICmdList, PSOInit, 0);

				SetShaderParameters(RHICmdList, VS, VS.GetVertexShader(), *P);
				SetShaderParameters(RHICmdList, PS, PS.GetPixelShader(), *P);

				RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f);
				RHICmdList.DrawPrimitive(0, 2, N);
			});
	}

	// Cache this frame's (jittered) view-projection + jitter for next frame's motion vector.
	PrevFrameViewProj = ViewProj;
	PrevFrameJitter = JitterCurr;
	bHasPrevFrame = true;
}
