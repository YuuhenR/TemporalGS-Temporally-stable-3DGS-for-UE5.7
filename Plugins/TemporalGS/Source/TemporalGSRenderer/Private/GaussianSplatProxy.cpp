// Copyright (c) 2026 TemporalGS contributors. MIT License.

#include "GaussianSplatProxy.h"

void FGaussianSplatProxy::BuildGpuArrays()
{
	if (!CpuData.IsValid())
	{
		return;
	}
	const int32 N = CpuData->Num();
	GpuPositions.SetNumUninitialized(N);
	GpuColors.SetNumUninitialized(N);
	GpuScales.SetNumUninitialized(N);
	GpuRotations.SetNumUninitialized(N);
	for (int32 i = 0; i < N; ++i)
	{
		const FVector3f& P = CpuData->Positions[i];
		const FVector3f& C = CpuData->ColorsDC[i];
		const FVector3f& S = CpuData->Scales[i];
		const FQuat4f&   Q = CpuData->Rotations[i];
		GpuPositions[i] = FVector4f(P.X, P.Y, P.Z, 1.0f);
		GpuColors[i]    = FVector4f(C.X, C.Y, C.Z, CpuData->Opacities[i]);
		GpuScales[i]    = FVector4f(S.X, S.Y, S.Z, 0.0f);
		GpuRotations[i] = FVector4f(Q.X, Q.Y, Q.Z, Q.W);
	}
}

FTemporalGSProxyRegistry& FTemporalGSProxyRegistry::Get()
{
	static FTemporalGSProxyRegistry Singleton;
	return Singleton;
}

void FTemporalGSProxyRegistry::Register(const FGaussianSplatProxyPtr& Proxy)
{
	if (!Proxy.IsValid())
	{
		return;
	}
	FScopeLock Lock(&Mutex);
	Proxies.AddUnique(Proxy);
}

void FTemporalGSProxyRegistry::Unregister(const FGaussianSplatProxyPtr& Proxy)
{
	FScopeLock Lock(&Mutex);
	Proxies.RemoveSingleSwap(Proxy);
}

void FTemporalGSProxyRegistry::GetProxies(TArray<FGaussianSplatProxyPtr>& Out) const
{
	FScopeLock Lock(&Mutex);
	Out = Proxies;
}

int32 FTemporalGSProxyRegistry::Num() const
{
	FScopeLock Lock(&Mutex);
	return Proxies.Num();
}
