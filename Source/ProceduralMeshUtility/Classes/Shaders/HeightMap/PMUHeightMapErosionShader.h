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
#include "PMUHeightMapErosionShader.generated.h"

UCLASS()
class PROCEDURALMESHUTILITY_API UPMUHeightMapErosionShader : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
    static void ApplyErosionShader(
        UObject* WorldContextObject,
        FPMUShaderTextureParameterInput SourceTexture,
        UTextureRenderTarget2D* HeightMapRTT,
        UTextureRenderTarget2D* FlowMapRTT,
        // Erosion Configuration
        int32 MaxIteration = 15,
        float MaxDuration = 1.f,
        float WaterAmount = 1.f,
        float ThermalWeatheringAmount = 0.f,
        // Flow Constants
        float PipeCrossSectionArea = 20.f,
        float PipeLength = 1.f,
        float GravitationalAcceleration = 9.7f,
        float WaterErosionFactor = .985f,
        // Erosion Constants
        float MinimumComputedSurfaceTilt = .2f,
        float SedimentCapacityConstant = 1.f,
        float DissolveConstant = .5f,
        float DepositConstant = 1.f,
        // Game Thread Callback Event
        UGWTTickEvent* CallbackEvent = nullptr
        );

    static void ApplyErosionShader_RT(
        FRHICommandListImmediate& RHICmdList,
        ERHIFeatureLevel::Type FeatureLevel,
        FPMUShaderTextureParameterInputResource& SourceTextureInput,
        FTextureRenderTarget2DResource* HeightMapRTT,
        FTextureRenderTarget2DResource* FlowMapRTT,
        FIntPoint Dimension,
        int32 MaxIteration,
        float MaxDuration,
        float WaterAmount,
        float ThermalWeatheringAmount,
        // Flow Constants:
        //  .x Pipe Cross-Section Area
        //  .y Pipe Length
        //  .z Gravitational Acceleration
        //  .w Water Erosion Factor
        const FVector4 FlowConstants,
        // Erosion Constants:
        //  .x Minimum Computed Surface Tilt
        //  .y Sediment Capacity Constant
        //  .z Dissolve Constant
        //  .w Deposit Constant
        const FVector4 ErosionConstants
        );
};
