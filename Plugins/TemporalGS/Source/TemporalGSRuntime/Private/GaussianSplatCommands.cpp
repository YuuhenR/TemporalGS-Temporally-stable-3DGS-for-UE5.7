// Copyright (c) 2026 TemporalGS contributors. MIT License.
//
// Console commands for quickly loading / clearing splats without navigating the editor UI:
//   TemporalGS.Load [path-to.ply]    spawn a Gaussian Splat actor, load the .ply, auto-fit to view
//   TemporalGS.Clear                 remove all Gaussian Splat actors

#include "GaussianSplatActor.h"
#include "GaussianSplatComponent.h"
#include "GaussianSplatAsset.h"
#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogTemporalGSCmd, Log, All);

static const TCHAR* GDefaultPlyPath = TEXT("E:/Code/UE/Optimize/Content/Splats/train_7000.ply");

static void TGSLoadCommand(const TArray<FString>& Args, UWorld* World)
{
	if (World == nullptr)
	{
		UE_LOG(LogTemporalGSCmd, Error, TEXT("TemporalGS.Load: no world."));
		return;
	}

	FString Path = (Args.Num() >= 1) ? Args[0].TrimQuotes() : FString(GDefaultPlyPath);

	AGaussianSplatActor* Actor = World->SpawnActor<AGaussianSplatActor>(AGaussianSplatActor::StaticClass(), FTransform::Identity);
	if (Actor == nullptr)
	{
		UE_LOG(LogTemporalGSCmd, Error, TEXT("TemporalGS.Load: failed to spawn actor."));
		return;
	}

	Actor->PlyFilePath = Path;
	Actor->LoadPly();

	UGaussianSplatAsset* Asset = Actor->SplatComponent ? Actor->SplatComponent->SplatAsset : nullptr;
	if (Asset == nullptr || !Asset->IsLoaded())
	{
		UE_LOG(LogTemporalGSCmd, Error, TEXT("TemporalGS.Load: load failed (see log). Path='%s'"), *Path);
		return;
	}

	// Auto-fit: scale the cloud to ~1000 units and center it just above the world origin so it's
	// visible regardless of the .ply's native units/orientation.
	const FBox B = Asset->LocalBounds;
	const FVector Size = B.GetSize();
	const double MaxDim = FMath::Max3(Size.X, Size.Y, Size.Z);
	const double Scale = (MaxDim > 1.0) ? (1000.0 / MaxDim) : 1.0;
	const FVector Center = B.GetCenter();

	Actor->SetActorScale3D(FVector(Scale));
	Actor->SetActorLocation(FVector(0, 0, 300) - Center * Scale);

	UE_LOG(LogTemporalGSCmd, Display,
		TEXT("TemporalGS.Load: %d gaussians, auto-scaled x%.4f, centered near origin. ")
		TEXT("Select 'GaussianSplatActor' in the Outliner and press F to focus."),
		Asset->NumPoints, Scale);
}

static void TGSClearCommand(UWorld* World)
{
	if (World == nullptr) { return; }
	int32 Removed = 0;
	for (TActorIterator<AGaussianSplatActor> It(World); It; ++It)
	{
		It->Destroy();
		++Removed;
	}
	UE_LOG(LogTemporalGSCmd, Display, TEXT("TemporalGS.Clear: removed %d splat actor(s)."), Removed);
}

static FAutoConsoleCommandWithWorldAndArgs GTGSLoadCmd(
	TEXT("TemporalGS.Load"),
	TEXT("Spawn a Gaussian Splat actor and load an INRIA .ply (auto-fit to view). Usage: TemporalGS.Load [path]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TGSLoadCommand));

static FAutoConsoleCommandWithWorld GTGSClearCmd(
	TEXT("TemporalGS.Clear"),
	TEXT("Remove all Gaussian Splat actors from the current world."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&TGSClearCommand));
