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
#include "RHI/PMURWBuffer.h"
#include "PMUMarchingSquaresStencil.h"
#include "PMUMarchingSquaresStencilPoly.generated.h"

class FPMUMarchingSquaresMap;
class UPMUMarchingSquaresMapRef;

class FPMUMarchingSquaresStencilPoly
{
public:

    struct FGenerateVoxelFeatureParameter
    {
        FPMUMarchingSquaresMap* Map;
        uint32                  FillType;

        TArray<FVector2D>       StencilPoints;
        float                   StencilEdgeRadius;
    };

private:

    MS_ALIGN(64) struct FAlignedLineGeom
    {
        FVector2D P0;
        FVector2D P1;
        FVector2D P2;
        FVector2D P3;

        FVector2D E0;
        FVector2D E1;
        FVector2D E2;
        FVector2D E3;
    } GCC_ALIGN(64);

    typedef TResourceArray<FAlignedLineGeom, VERTEXBUFFER_ALIGNMENT> FLineGeomData;

    // Render Resource Data

    FIntPoint Dimension  = FIntPoint::ZeroValue;
    int32     VoxelCount = 0;

    FTexture2DRHIRef          StencilTextureRTV;
    FTexture2DRHIRef          StencilTextureRSV;
    FShaderResourceViewRHIRef StencilTextureSRV;

    // Transient Render Data

    uint32            FillType;
    float             StencilEdgeRadius;
    TArray<FVector2D> StencilPoints;

    FPMURWBufferStructured LineGeomData;

    FRHICommandListImmediate*      RHICmdListPtr = nullptr;
    TShaderMap<FGlobalShaderType>* RHIShaderMap  = nullptr;

    void DrawStencilMask_RT();
    void DrawStencilEdge_RT();
    void ResolveStencilTexture_RT();
    void ReleaseResources_RT();

    void GenerateVoxelFeatures_RT(FRHICommandListImmediate& RHICmdList, FPMUMarchingSquaresMap& Map, uint32 FillType);
    void GenerateVoxelFeatures_RT(FRHICommandListImmediate& RHICmdList, const FGenerateVoxelFeatureParameter& Parameter);
    void ClearStencil_RT(FRHICommandListImmediate& RHICmdList);

public:

    void GenerateVoxelFeatures(const FGenerateVoxelFeatureParameter& Parameter);
    void ClearStencil();
};

UCLASS()
class UPMUMarchingSquaresStencilPolyRef : public UPMUMarchingSquaresStencilRef
{
    GENERATED_BODY()

    FPMUMarchingSquaresStencilPoly Stencil;

public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stencil Settings")
    TArray<FVector2D> StencilPoints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stencil Settings")
    float StencilEdgeRadius;

    void ClearStencil() override;
    void ApplyStencilToMap(UPMUMarchingSquaresMapRef* MapRef, int32 FillType) override;
};
