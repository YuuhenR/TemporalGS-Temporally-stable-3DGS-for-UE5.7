// Copyright (c) 2026 TemporalGS contributors. MIT License.

#include "GaussianSplatActor.h"
#include "GaussianSplatComponent.h"
#include "GaussianSplatAsset.h"
#include "GaussianSplatPlyLoader.h"
#include "GaussianSplatProxy.h"

DEFINE_LOG_CATEGORY_STATIC(LogTemporalGSActor, Log, All);

AGaussianSplatActor::AGaussianSplatActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SplatComponent = CreateDefaultSubobject<UGaussianSplatComponent>(TEXT("SplatComponent"));
	RootComponent = SplatComponent;
}

void AGaussianSplatActor::BeginPlay()
{
	Super::BeginPlay();
	if (!PlyFilePath.IsEmpty() && (!SplatComponent->SplatAsset || !SplatComponent->SplatAsset->IsLoaded()))
	{
		LoadPly();
	}
}

void AGaussianSplatActor::LoadPly()
{
	if (PlyFilePath.IsEmpty())
	{
		UE_LOG(LogTemporalGSActor, Warning, TEXT("PlyFilePath is empty."));
		return;
	}

	UGaussianSplatAsset* Asset = NewObject<UGaussianSplatAsset>(this);
	FString Error;
	if (FGaussianSplatPlyLoader::LoadInriaPly(PlyFilePath, *Asset->Data, Asset->LocalBounds, Error))
	{
		Asset->NumPoints = Asset->Data->Num();
		SplatComponent->SetAsset(Asset);
		UE_LOG(LogTemporalGSActor, Log, TEXT("Loaded %d gaussians from '%s'. Bounds=%s"),
			Asset->NumPoints, *PlyFilePath, *Asset->LocalBounds.ToString());
	}
	else
	{
		UE_LOG(LogTemporalGSActor, Error, TEXT("Failed to load '%s': %s"), *PlyFilePath, *Error);
	}
}
