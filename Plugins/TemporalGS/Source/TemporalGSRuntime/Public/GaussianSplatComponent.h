// Copyright (c) 2026 TemporalGS contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GaussianSplatProxy.h"   // FGaussianSplatProxyPtr
#include "GaussianSplatComponent.generated.h"

class UGaussianSplatAsset;

/**
 * Places a Gaussian splat cloud in the world and owns its render-thread proxy.
 * On register it builds an FGaussianSplatProxy from the asset and adds it to the global registry
 * that FTemporalGSSceneViewExtension iterates each frame.
 */
UCLASS(ClassGroup = (TemporalGS), meta = (BlueprintSpawnableComponent))
class TEMPORALGSRUNTIME_API UGaussianSplatComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UGaussianSplatComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TemporalGS")
	TObjectPtr<UGaussianSplatAsset> SplatAsset;

	/** Assign a (possibly newly loaded) asset and refresh the proxy. */
	void SetAsset(UGaussianSplatAsset* InAsset);

protected:
	//~ USceneComponent
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

private:
	void CreateAndRegisterProxy();
	void UnregisterProxy();
	void PushTransformToProxy();

	FGaussianSplatProxyPtr Proxy;
};
