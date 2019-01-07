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
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Shaders/PMUShaderParameters.h"
#include "GWTTickUtilities.h"
#include "PMUUtilityShaderLibrary.generated.h"

class UPMUFilterShaderBase;
class FPMUFilterRenderThreadWorkerBase;
class UMaterialInterface;
class FMaterialRenderProxy;
class FMaterial;
//class FCanvas;

UCLASS()
class PROCEDURALMESHUTILITY_API UPMUUtilityShaderLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
    static void DrawPolyPoints(
        UObject* WorldContextObject,
        UTextureRenderTarget2D* RenderTarget,
        FIntPoint DrawSize,
        const TArray<FVector2D>& Points,
        float ExpandRadius = 0.f
        );

    // Draw indexed primitive to a render target with vertex color stored in vertex z component
    static void DrawGeometry_RT(
        FRHICommandListImmediate& RHICmdList,
        ERHIFeatureLevel::Type FeatureLevel,
        FTextureRenderTarget2DResource* TextureResource,
        const TArray<FVector>& Vertices,
        const TArray<int32>& Indices,
        bool bResolveRenderTarget = true
        );

    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
    static void GetTexturePointValues(
        UObject* WorldContextObject,
        FPMUShaderTextureParameterInput TextureInput,
        const FVector2D Dimension,
        const TArray<FVector2D>& Points,
        UPARAM(ref) TArray<FLinearColor>& Values
        );

    static void GetTexturePointValues_RT(
        FRHICommandListImmediate& RHICmdList,
        ERHIFeatureLevel::Type FeatureLevel,
        FPMUShaderTextureParameterInputResource& TextureInput,
        const FVector2D Dimension,
        const TArray<FVector2D>& Points,
        TArray<FLinearColor>& Values
        );

    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
    static void ApplyFilter(
        UObject* WorldContextObject,
        UPMUFilterShaderBase* FilterShader,
        UTextureRenderTarget2D* RenderTarget,
        UTextureRenderTarget2D* SwapTarget = nullptr,
        UGWTTickEvent* CallbackEvent = nullptr
        );

    static void ApplyFilter_RT(
        FRHICommandListImmediate& RHICmdList,
        ERHIFeatureLevel::Type FeatureLevel,
        FTextureRenderTarget2DResource* RenderTargetResource,
        FTextureRenderTarget2DResource* SwapTargetResource,
        FPMUFilterRenderThreadWorkerBase* FilterShaderWorker
        );

    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
    static void ApplyMaterialFilter(
        UObject* WorldContextObject,
        UMaterialInterface* Material,
        int32 RepeatCount,
        FPMUShaderTextureParameterInput SourceTexture,
        UTextureRenderTarget2D* RenderTarget,
        UTextureRenderTarget2D* SwapTarget = nullptr,
        UGWTTickEvent* CallbackEvent = nullptr
        );

    static void ApplyMaterialFilter_RT(
        FRHICommandListImmediate& RHICmdList,
        ERHIFeatureLevel::Type FeatureLevel,
        int32 RepeatCount,
        FPMUShaderTextureParameterInputResource& SourceTextureResource,
        FTextureRenderTarget2DResource* RenderTargetResource,
        FTextureRenderTarget2DResource* SwapTargetResource,
        const FMaterialRenderProxy* MaterialRenderProxy
        );

    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
    static void TestGPUCompute(
        UObject* WorldContextObject,
        int32 TestCount,
        UGWTTickEvent* CallbackEvent = nullptr
        );
};
