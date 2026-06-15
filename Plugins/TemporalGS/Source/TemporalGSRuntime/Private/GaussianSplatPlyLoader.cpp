// Copyright (c) 2026 TemporalGS contributors. MIT License.

#include "GaussianSplatPlyLoader.h"
#include "GaussianSplatProxy.h"
#include "Misc/FileHelper.h"

DEFINE_LOG_CATEGORY_STATIC(LogTemporalGSLoader, Log, All);

namespace
{
	FORCEINLINE float Sigmoid(float X) { return 1.0f / (1.0f + FMath::Exp(-X)); }

	// SH degree-0 -> color constant.
	constexpr float SH_C0 = 0.28209479177387814f;
}

bool FGaussianSplatPlyLoader::LoadInriaPly(const FString& FilePath, FGaussianSplatCPUData& Out, FBox& OutBounds, FString& OutError)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *FilePath))
	{
		OutError = FString::Printf(TEXT("Cannot read file: %s"), *FilePath);
		return false;
	}

	// --- Locate "end_header\n" (header is ASCII) --------------------------------------------------
	const ANSICHAR* Marker = "end_header\n";
	const int32 MarkerLen = 11;
	int32 HeaderEnd = INDEX_NONE;
	for (int32 i = 0; i + MarkerLen <= Bytes.Num(); ++i)
	{
		if (FMemory::Memcmp(&Bytes[i], Marker, MarkerLen) == 0)
		{
			HeaderEnd = i + MarkerLen;
			break;
		}
	}
	if (HeaderEnd == INDEX_NONE)
	{
		OutError = TEXT("'end_header' not found - not a PLY file?");
		return false;
	}

	// --- Parse the header ------------------------------------------------------------------------
	TArray<ANSICHAR> HeaderAnsi;
	HeaderAnsi.Append(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), HeaderEnd);
	HeaderAnsi.Add('\0');
	const FString HeaderStr = FString(ANSI_TO_TCHAR(HeaderAnsi.GetData()));

	TArray<FString> Lines;
	HeaderStr.ParseIntoArrayLines(Lines);

	bool bLittleEndian = false;
	int64 VertexCount = 0;
	bool bInVertexElement = false;
	TArray<FString> Props;   // property names, in file order; all assumed 4-byte float

	for (const FString& Raw : Lines)
	{
		const FString Line = Raw.TrimStartAndEnd();
		if (Line.StartsWith(TEXT("format")))
		{
			bLittleEndian = Line.Contains(TEXT("binary_little_endian"));
		}
		else if (Line.StartsWith(TEXT("element vertex")))
		{
			VertexCount = FCString::Atoi64(*Line.RightChop(14).TrimStartAndEnd());
			bInVertexElement = true;
		}
		else if (Line.StartsWith(TEXT("element")))
		{
			bInVertexElement = false; // a different element ends the vertex property list
		}
		else if (bInVertexElement && Line.StartsWith(TEXT("property")))
		{
			TArray<FString> Tok;
			Line.ParseIntoArray(Tok, TEXT(" "), true);
			if (Tok.Num() >= 3)
			{
				Props.Add(Tok.Last()); // e.g. "x", "f_dc_0", "scale_2", "rot_3"
			}
		}
	}

	if (!bLittleEndian)
	{
		OutError = TEXT("Only 'binary_little_endian' PLY is supported.");
		return false;
	}
	if (VertexCount <= 0 || Props.Num() == 0)
	{
		OutError = TEXT("PLY has no vertices / properties.");
		return false;
	}

	auto Idx = [&Props](const TCHAR* Name) -> int32
	{
		return Props.IndexOfByPredicate([&](const FString& S) { return S == Name; });
	};
	const int32 iX = Idx(TEXT("x")),    iY = Idx(TEXT("y")),    iZ = Idx(TEXT("z"));
	const int32 iC0 = Idx(TEXT("f_dc_0")), iC1 = Idx(TEXT("f_dc_1")), iC2 = Idx(TEXT("f_dc_2"));
	const int32 iOp = Idx(TEXT("opacity"));
	const int32 iS0 = Idx(TEXT("scale_0")), iS1 = Idx(TEXT("scale_1")), iS2 = Idx(TEXT("scale_2"));
	const int32 iR0 = Idx(TEXT("rot_0")), iR1 = Idx(TEXT("rot_1")), iR2 = Idx(TEXT("rot_2")), iR3 = Idx(TEXT("rot_3"));

	const bool bHaveAll =
		iX >= 0 && iY >= 0 && iZ >= 0 &&
		iC0 >= 0 && iC1 >= 0 && iC2 >= 0 && iOp >= 0 &&
		iS0 >= 0 && iS1 >= 0 && iS2 >= 0 &&
		iR0 >= 0 && iR1 >= 0 && iR2 >= 0 && iR3 >= 0;
	if (!bHaveAll)
	{
		OutError = TEXT("PLY missing required 3DGS properties (x/y/z, f_dc_0..2, opacity, scale_0..2, rot_0..3).");
		return false;
	}

	const int32 Stride = Props.Num() * (int32)sizeof(float);
	const int64 NeedBytes = (int64)HeaderEnd + VertexCount * (int64)Stride;
	if ((int64)Bytes.Num() < NeedBytes)
	{
		OutError = FString::Printf(TEXT("Truncated PLY: need %lld bytes, have %d."), NeedBytes, Bytes.Num());
		return false;
	}

	// --- Read the binary body --------------------------------------------------------------------
	Out.Reset();
	Out.Positions.Reserve(VertexCount);
	Out.Scales.Reserve(VertexCount);
	Out.Rotations.Reserve(VertexCount);
	Out.Opacities.Reserve(VertexCount);
	Out.ColorsDC.Reserve(VertexCount);

	const uint8* Base = Bytes.GetData() + HeaderEnd;
	FBox Bounds(ForceInit);

	for (int64 v = 0; v < VertexCount; ++v)
	{
		// Little-endian float read (x86 is LE, so a direct cast is correct on supported platforms).
		const float* F = reinterpret_cast<const float*>(Base + v * Stride);

		const FVector3f P(F[iX], F[iY], F[iZ]);
		const FVector3f S(FMath::Exp(F[iS0]), FMath::Exp(F[iS1]), FMath::Exp(F[iS2]));
		FQuat4f Q(F[iR1], F[iR2], F[iR3], F[iR0]); // (x,y,z,w) <- (rot_1, rot_2, rot_3, rot_0=w)
		Q.Normalize();
		const float O = Sigmoid(F[iOp]);
		const FVector3f C(0.5f + SH_C0 * F[iC0], 0.5f + SH_C0 * F[iC1], 0.5f + SH_C0 * F[iC2]);

		Out.Positions.Add(P);
		Out.Scales.Add(S);
		Out.Rotations.Add(Q);
		Out.Opacities.Add(O);
		Out.ColorsDC.Add(C);
		Bounds += FVector(P);
	}

	OutBounds = Bounds;
	UE_LOG(LogTemporalGSLoader, Log, TEXT("Loaded %lld gaussians (%d props/stride=%dB) from %s"),
		VertexCount, Props.Num(), Stride, *FilePath);
	return true;
}
