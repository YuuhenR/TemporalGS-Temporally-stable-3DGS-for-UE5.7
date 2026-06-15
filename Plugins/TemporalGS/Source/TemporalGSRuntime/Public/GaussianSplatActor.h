// Copyright (c) 2026 TemporalGS contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GaussianSplatActor.generated.h"

class UGaussianSplatComponent;

/**
 * Drop-in actor for a Gaussian splat scene.
 * Set PlyFilePath to an INRIA .ply and press "Load Ply" in the details panel (or it loads on
 * BeginPlay). The component registers a render proxy that TemporalGS renders.
 */
UCLASS()
class TEMPORALGSRUNTIME_API AGaussianSplatActor : public AActor
{
	GENERATED_BODY()

public:
	AGaussianSplatActor();

	UPROPERTY(VisibleAnywhere, Category = "TemporalGS")
	TObjectPtr<UGaussianSplatComponent> SplatComponent;

	/** Absolute path to an INRIA .ply, e.g. E:/Code/UE/Optimize/Content/Splats/train_7000.ply */
	UPROPERTY(EditAnywhere, Category = "TemporalGS", meta = (FilePathFilter = "ply"))
	FString PlyFilePath;

	/** Load (or reload) PlyFilePath now. */
	UFUNCTION(CallInEditor, Category = "TemporalGS")
	void LoadPly();

protected:
	virtual void BeginPlay() override;
};
