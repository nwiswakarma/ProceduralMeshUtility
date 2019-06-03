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
#include "RHIResources.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Mesh/PMUMeshTypes.h"
#include "PMUMeshUtility.generated.h"

class UGWTTickEvent;
class UPMUMeshComponent;
class FPMUMeshProxySection;

UCLASS()
class PROCEDURALMESHUTILITY_API UPMUMeshUtility : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Create Grid Mesh Section (GPU)", AdvancedDisplay="Bounds"))
    static void CreateGridMeshSectionGPU(
        UObject* WorldContextObject,
        UPMUMeshComponent* MeshComponent,
        int32 SectionIndex,
        FBox Bounds,
        int32 GridSizeX = 256,
        int32 GridSizeY = 256,
        bool bReverseWinding = false,
        UTexture* HeightTexture = nullptr,
        float HeightScale = 1.f,
        bool bEnableCollision = false,
        bool bCalculateBounds = true,
        bool bUpdateRenderState = true,
        UGWTTickEvent* CallbackEvent = nullptr
        );

    static void CreateGridMeshSectionGPU_RT(
        FRHICommandListImmediate& RHICmdList,
        ERHIFeatureLevel::Type FeatureLevel,
        FPMUMeshSection& Section,
        int32 GridSizeX,
        int32 GridSizeY,
        bool bReverseWinding,
        const FTexture* HeightTexture,
        float HeightScale
        );

    UFUNCTION(BlueprintCallable)
    static void AssignHeightMapToMeshSection(
        UObject* WorldContextObject,
        UPMUMeshComponent* MeshComponent,
        int32 SectionIndex,
        UTexture* HeightTexture = nullptr,
        float HeightScale = 1.f,
        bool bUseUV = false,
        float PositionToUVScaleX = 1.f,
        float PositionToUVScaleY = 1.f,
        bool bMaskByColor = true,
        bool bInverseColorMask = false,
        UGWTTickEvent* CallbackEvent = nullptr
        );

    UFUNCTION(BlueprintCallable, meta=(AdvancedDisplay="bUseUV,PositionToUVScaleX,PositionToUVScaleY,bMaskByColor,bInverseColorMask,CallbackEvent"))
    static void AssignHeightMapToMeshSectionMulti(
        UObject* WorldContextObject,
        UPMUMeshComponent* MeshComponent,
        TArray<int32> SectionIndices,
        UTexture* HeightTexture = nullptr,
        float HeightScale = 1.f,
        bool bUseUV = false,
        float PositionToUVScaleX = 1.f,
        float PositionToUVScaleY = 1.f,
        bool bMaskByColor = true,
        bool bInverseColorMask = false,
        UGWTTickEvent* CallbackEvent = nullptr
        );

    static void AssignHeightMapToMeshSection_RT(
        FRHICommandListImmediate& RHICmdList,
        ERHIFeatureLevel::Type FeatureLevel,
        const TArray<FPMUMeshProxySection*>& Sections,
        const FTexture& HeightTexture,
        float HeightScale,
        bool bUseUV,
        float PositionToUVScaleX,
        float PositionToUVScaleY,
        bool bMaskByColor,
        bool bInverseColorMask
        );
};
