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
class FPMUBaseMeshProxySection;

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUMeshApplyHeightParameters
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HeightScale = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseUV = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float UVScaleX = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float UVScaleY = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bMaskByColor = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bInverseColorMask = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAlongTangents = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAssignTangents = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSampleWrap = false;
};

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUMeshApplySeamlessTileHeightParameters
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture* HeightTextureA;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture* HeightTextureB;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture* NoiseTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NoiseUVScaleX = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NoiseUVScaleY = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HashConstantX = .135f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HashConstantY = .350f;

    FORCEINLINE bool HasValidInputTextures() const
    {
        return IsValid(HeightTextureA) && IsValid(HeightTextureB) && IsValid(NoiseTexture);
    }
};

struct FPMUMeshApplySeamlessTileHeightParametersRT
{
    FTexture* HeightTextureA;
    FTexture* HeightTextureB;
    FTexture* NoiseTexture;
    float NoiseUVScaleX;
    float NoiseUVScaleY;
    float HashConstantX;
    float HashConstantY;

    FORCEINLINE bool HasValidInputTextures() const
    {
        return HeightTextureA != nullptr
            && HeightTextureB != nullptr
            && NoiseTexture != nullptr;
    }
};

UCLASS()
class PROCEDURALMESHUTILITY_API UPMUMeshUtility : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

    struct FApplyHeightMapInputData
    {
        TResourceArray<FVector, VERTEXBUFFER_ALIGNMENT> PositionData;
        TResourceArray<FVector2D, VERTEXBUFFER_ALIGNMENT> UVData;
        TResourceArray<uint32, VERTEXBUFFER_ALIGNMENT> TangentData;
        TResourceArray<FColor, VERTEXBUFFER_ALIGNMENT> ColorData;
        TArray<int32> VertexOffsets;

        int32 VertexCount;
        int32 UVCount;
        int32 TangentCount;
        int32 ColorCount;

        bool bUseUVInput;
        bool bUseTangentInput;
        bool bUseColorInput;

        void Init(const TArray<FPMUMeshSection*>& Sections, const FPMUMeshApplyHeightParameters& Parameters);

        FORCEINLINE bool HasValidUVCount() const
        {
            return bUseUVInput
                ? UVData.Num() == VertexCount
                : TangentData.Num() == 0;
        }

        FORCEINLINE bool HasValidTangentCount() const
        {
            return bUseTangentInput
                ? TangentData.Num() == 2*VertexCount
                : TangentData.Num() == 2;
        }

        FORCEINLINE bool HasValidColorCount() const
        {
            return bUseColorInput
                ? ColorData.Num() == VertexCount
                : ColorData.Num() == 1;
        }
    };

public:

    UFUNCTION(BlueprintCallable, meta=(AutoCreateRefTerm="NegativeExpand,PositiveExpand"))
    static void ExpandSectionBoundsMulti(
        UPMUMeshComponent* MeshComponent,
        const TArray<int32>& SectionIndices,
        const FVector& NegativeExpand,
        const FVector& PositiveExpand
        );

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Create Grid Mesh Section (GPU)", AdvancedDisplay="Bounds"))
    static void CreateGridMeshSectionGPU(
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

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Apply Height Map To Mesh Sections", AdvancedDisplay="CallbackEvent"))
    static void ApplyHeightMapToMeshSections(
        UObject* WorldContextObject,
        TArray<FPMUMeshSectionRef> SectionRefs,
        UTexture* HeightTexture,
        FPMUMeshApplyHeightParameters Parameters,
        UGWTTickEvent* CallbackEvent = nullptr
        );

    static void ApplyHeightMapToMeshSections_RT(
        FRHICommandListImmediate& RHICmdList,
        ERHIFeatureLevel::Type FeatureLevel,
        const TArray<FPMUMeshSection*>& Sections,
        const FTexture& HeightTexture,
        const FPMUMeshApplyHeightParameters& Parameters
        );

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Apply Seamless Tiled Height Map To Mesh Sections", AdvancedDisplay="CallbackEvent"))
    static void ApplySeamlessTiledHeightMapToMeshSections(
        UObject* WorldContextObject,
        TArray<FPMUMeshSectionRef> SectionRefs,
        FPMUMeshApplySeamlessTileHeightParameters TilingParameters,
        FPMUMeshApplyHeightParameters ShaderParameters,
        UGWTTickEvent* CallbackEvent = nullptr
        );

    static void ApplySeamlessTiledHeightMapToMeshSections_RT(
        FRHICommandListImmediate& RHICmdList,
        ERHIFeatureLevel::Type FeatureLevel,
        const TArray<FPMUMeshSection*>& Sections,
        const FPMUMeshApplySeamlessTileHeightParametersRT& TilingParameters,
        const FPMUMeshApplyHeightParameters& ShaderParameters
        );
};
