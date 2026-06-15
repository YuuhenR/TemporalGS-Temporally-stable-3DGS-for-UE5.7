// Copyright (c) 2026 TemporalGS contributors. MIT License.

#include "GaussianSplatComponent.h"
#include "GaussianSplatAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogTemporalGSComp, Log, All);

UGaussianSplatComponent::UGaussianSplatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsOnUpdateTransform = true;
}

void UGaussianSplatComponent::SetAsset(UGaussianSplatAsset* InAsset)
{
	SplatAsset = InAsset;
	if (IsRegistered())
	{
		UnregisterProxy();
		CreateAndRegisterProxy();
	}
	UpdateBounds();
	MarkRenderStateDirty();
}

void UGaussianSplatComponent::OnRegister()
{
	Super::OnRegister();
	CreateAndRegisterProxy();
}

void UGaussianSplatComponent::OnUnregister()
{
	UnregisterProxy();
	Super::OnUnregister();
}

void UGaussianSplatComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
	PushTransformToProxy();
}

FBoxSphereBounds UGaussianSplatComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (SplatAsset && SplatAsset->IsLoaded() && SplatAsset->LocalBounds.IsValid)
	{
		return FBoxSphereBounds(SplatAsset->LocalBounds).TransformBy(LocalToWorld);
	}
	return FBoxSphereBounds(FVector::ZeroVector, FVector(50.f), 50.f).TransformBy(LocalToWorld);
}

void UGaussianSplatComponent::CreateAndRegisterProxy()
{
	if (!SplatAsset || !SplatAsset->IsLoaded())
	{
		return; // nothing to draw yet
	}

	Proxy = MakeShared<FGaussianSplatProxy, ESPMode::ThreadSafe>();
	Proxy->CpuData = SplatAsset->Data;                 // share, don't copy
	Proxy->BuildGpuArrays();                            // pack positions/colors for the rasterizer
	Proxy->LocalToWorld = FMatrix44f(GetComponentTransform().ToMatrixWithScale());
	Proxy->PrevLocalToWorld = Proxy->LocalToWorld;
	Proxy->bGpuDirty = true;

	FTemporalGSProxyRegistry::Get().Register(Proxy);
	UE_LOG(LogTemporalGSComp, Log, TEXT("Registered splat proxy: %d gaussians."), Proxy->Num());
}

void UGaussianSplatComponent::UnregisterProxy()
{
	if (Proxy.IsValid())
	{
		FTemporalGSProxyRegistry::Get().Unregister(Proxy);
		Proxy.Reset();
	}
}

void UGaussianSplatComponent::PushTransformToProxy()
{
	if (!Proxy.IsValid())
	{
		return;
	}
	// Phase 1: update on the game thread (transform is not yet consumed on the render thread).
	// Phase 3 will route this through ENQUEUE_RENDER_COMMAND and keep PrevLocalToWorld for velocity.
	Proxy->PrevLocalToWorld = Proxy->LocalToWorld;
	Proxy->LocalToWorld = FMatrix44f(GetComponentTransform().ToMatrixWithScale());
}
