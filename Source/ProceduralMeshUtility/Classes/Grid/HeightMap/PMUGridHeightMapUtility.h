////////////////////////////////////////////////////////////////////////////////
//
// MIT License
// 
// Copyright (c) 2018-2019 Nuraga Wiswakarma
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////
// 

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Mesh/PMUMeshTypes.h"
#include "PMUGridData.h"
#include "PMUGridHeightMapUtility.generated.h"

class IPMUGridHeightMapGenerator;

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUGridHeightMapGeneratorRef
{
	GENERATED_BODY()

    TSharedPtr<IPMUGridHeightMapGenerator> Generator;

    FPMUGridHeightMapGeneratorRef() : Generator(nullptr)
    {
    }

    FPMUGridHeightMapGeneratorRef(TSharedPtr<IPMUGridHeightMapGenerator> InGenerator)
        : Generator(InGenerator)
    {
    }

    FORCEINLINE void Reset()
    {
        Generator.Reset();
    }

    FORCEINLINE bool IsValid() const
    {
        return Generator.IsValid();
    }
};

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUGridHeightMapGeneratorData
{
	GENERATED_BODY()

    IPMUGridHeightMapGenerator* Generator;

    FPMUGridHeightMapGeneratorData() : Generator(nullptr)
    {
    }

    FPMUGridHeightMapGeneratorData(IPMUGridHeightMapGenerator& InGenerator)
        : Generator(&InGenerator)
    {
    }

    FORCEINLINE void Reset()
    {
        Generator = nullptr;
    }

    FORCEINLINE bool IsValid() const
    {
        return Generator != nullptr;
    }
};

UCLASS()
class PROCEDURALMESHUTILITY_API UPMUGridHeightMapUtility : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

    FORCEINLINE static float GetLerpX(const float X, const int32 IX, const int32 IY, const int32 Stride, const TArray<float>& HeightMap)
    {
        check(HeightMap.IsValidIndex(IY*Stride + IX+1));
        float Height0 = HeightMap[IY*Stride + IX];
        float Height1 = HeightMap[IY*Stride + IX+1];
        return FMath::Lerp(Height0, Height1, FMath::Clamp(X-IX, 0.f, 1.f));
    }

    FORCEINLINE static float GetLerpY(const float Y, const int32 IX, const int32 IY, const int32 Stride, const TArray<float>& HeightMap)
    {
        check(HeightMap.IsValidIndex((IY+1)*Stride + IX));
        float Height0 = HeightMap[ IY   *Stride + IX];
        float Height1 = HeightMap[(IY+1)*Stride + IX];
        return FMath::Lerp(Height0, Height1, FMath::Clamp(Y-IY, 0.f, 1.f));
    }

    FORCEINLINE static float GetBiLerp(const float X, const float Y, const int32 IX, const int32 IY, const int32 Stride, const TArray<float>& HeightMap)
    {
        check(HeightMap.IsValidIndex((IY+1)*Stride + IX+1));
        const float Height00 = HeightMap[IY*Stride + IX];
        const float Height10 = HeightMap[IY*Stride + IX+1];
        const float Height01 = HeightMap[(IY+1)*Stride + IX  ];
        const float Height11 = HeightMap[(IY+1)*Stride + IX+1];
        return FMath::BiLerp(
            Height00, Height10,
            Height01, Height11,
            FMath::Clamp(X-IX, 0.f, 1.f),
            FMath::Clamp(Y-IY, 0.f, 1.f)
            );
    }

    static void GetHeightNormal(const float X, const float Y, const int32 IX, const int32 IY, const FPMUGridData& GD, const int32 MapId, float& OutHeight, FVector& OutNormal);

    // Height map tools

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Create Height Map"))
    static void K2_CreateHeightMap(FPMUGridDataRef GridRef, int32 MapId);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Set Height Map Count"))
    static void K2_SetHeightMapNum(FPMUGridDataRef GridRef, int32 MapNum, bool bShrink = false);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Generate Height Map"))
	static void K2_GenerateHeightMap(FPMUGridDataRef GridRef, int32 MapId, FPMUGridHeightMapGeneratorRef GeneratorRef);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Generate Height Map Using Generator"))
	static void K2_GenerateHeightMapFromRef(FPMUGridDataRef GridRef, int32 MapId, FPMUGridHeightMapGeneratorData GeneratorRef);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Generate Height Map From Render Target"))
	static void K2_GenerateHeightMapFromRenderTarget(FPMUGridDataRef GridRef, int32 MapId, UTextureRenderTarget2D* RenderTarget);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Copy Height Map"))
	static void K2_CopyHeightMap(FPMUGridDataRef GridRef, int32 SrcId, int32 DstId);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Multiply By Value"))
    static void K2_MulHeightValue(FPMUGridDataRef GridRef, int32 MapId, float Value);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Multiply Height Map"))
    static void K2_MulHeightMap(FPMUGridDataRef GridRef, int32 SrcId, int32 DstId);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Add By Value"))
    static void K2_AddHeightValue(FPMUGridDataRef GridRef, int32 MapId, float Value);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Add Height Map"))
    static void K2_AddHeightMap(FPMUGridDataRef GridRef, int32 SrcId, int32 DstId);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Clamp Minimum Value"))
    static void K2_ClampMinValue(FPMUGridDataRef GridRef, int32 MapId, float MinValue);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Clamp Maximum Value"))
    static void K2_ClampMaxValue(FPMUGridDataRef GridRef, int32 MapId, float MaxValue);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Saturate Value"))
    static void K2_ClampHeightMap(FPMUGridDataRef GridRef, int32 MapId);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Clamp By Value Range"))
    static void K2_ClampHeightMapRange(FPMUGridDataRef GridRef, int32 MapId, float InMin = 0.f, float InMax = 1.f);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Scale To Value Range"))
	static void K2_ScaleHeightMapRange(FPMUGridDataRef GridRef, int32 MapId, float InMin = 0.f, float InMax = 1.f);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Apply Clip Map"))
	static void K2_ApplyClipMap(FPMUGridDataRef GridRef, int32 SrcId, int32 DstId, float Threshold = 0.01f);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Apply Value Curve"))
	static void K2_ApplyCurve(FPMUGridDataRef GridRef, int32 MapId, UCurveFloat* Curve, bool bNormalize = false);

    // Height map user tools

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Apply Radial Gradient"))
	static void K2_RadialGradient(FPMUGridDataRef GridRef, int32 MapId, FVector2D Center, float Radius, float InnerHeight, float OuterHeight, bool bUnbound = false);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Apply Smooth Radial Gradient"))
	static void K2_SmoothRadial(FPMUGridDataRef GridRef, int32 MapId, FVector2D Center, float Radius, float FalloffRadius, float Strength = 0.5f, float HeightOverride = 0.f, bool bUseHeightOverride = false);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Apply Smooth Box Gradient"))
	static void K2_SmoothBox(FPMUGridDataRef GridRef, int32 MapId, FVector2D Center, float Radius, float FalloffRadius, float Strength = 0.5f, float HeightOverride = 0.f, bool bUseHeightOverride = false);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Apply Smooth Multi-Box Gradient"))
	static void K2_SmoothMultiBox(FPMUGridDataRef GridRef, int32 MapId, const TArray<FBox2D>& MultiBox, float Height, float FalloffRadius, float Strength = 0.5f);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Smooth Height Map"))
	static void K2_Smooth(FPMUGridDataRef GridRef, int32 MapId, int32 Radius);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Smooth Height Map (X-Axis Only)"))
	static void K2_SmoothX(FPMUGridDataRef GridRef, int32 MapId, int32 Radius);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Smooth Height Map (Y-Axis Only)"))
	static void K2_SmoothY(FPMUGridDataRef GridRef, int32 MapId, int32 Radius);

    // Height map query tools

	UFUNCTION(BlueprintCallable)
	static FVector2D K2_GetCorners2DHeightExtrema(FPMUGridDataRef GridRef, int32 MapId, const FBox2D& Bounds);

	UFUNCTION(BlueprintCallable)
	static FVector2D K2_GetLine2DHeightExtrema(FPMUGridDataRef GridRef, int32 MapId, const FVector2D& LineStart, const FVector2D& LineEnd);

	UFUNCTION(BlueprintCallable)
	static FVector K2_GetPointHeight(FPMUGridDataRef GridRef, int32 MapId, const FVector2D& Point);

    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true", DisplayName="Create Height Map Render Target Texture"))
	static UTextureRenderTarget2D* K2_CreateHeightMapRTT(UObject* WorldContextObject, FPMUGridDataRef GridRef, int32 MapId);

    // Height map generator functions

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Create Fast-Noise Height Map Generator"))
    static FPMUGridHeightMapGeneratorRef CreateUFNHeightMapGenerator(class UUFNNoiseGenerator* NoiseGenerator);

    // Mesh height map functions

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Apply Height Map To Mesh Section"))
    static void AssignHeightMapToMeshSection(UPARAM(ref) FPMUMeshSection& Section, const UPMUGridInstance* Grid, const int32 MapId);

    // Substance map tools

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Create Height Map Using Substance Texture"))
    static void CreateSubstanceHeightMap(FPMUGridDataRef GridRef, int32 MapId, UObject* InSubstanceTexture);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Create Point Mask Substance Image Input"))
    static UObject* CreatePointMaskSubstanceImage(FPMUGridDataRef GridRef);

private:

    static void SmoothX(FPMUGridData& Grid, int32 MapId, int32 Radius);
    static void SmoothY(FPMUGridData& Grid, int32 MapId, int32 Radius);
    static void GetExtremas(FPMUGridData::FHeightMap& HeightMap, float& OutMin, float& OutMax);

    FORCEINLINE static float GetNormalizedSigned(float Value, float MinValue, float MaxValue)
    {
        return -1.f + ((Value-MinValue) * 2.f) / (MaxValue-MinValue);
    }
};
