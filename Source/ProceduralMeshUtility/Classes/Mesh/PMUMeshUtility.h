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
    FLinearColor ColorMask;

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
                : UVData.Num() == 0;
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

    static bool IsValidSectionData(const UStaticMesh* Mesh, int32 LODIndex, int32 SectionIndex);
    FORCEINLINE static bool IsValidSectionData(const FPMUStaticMeshSectionData& Input);

    static const FStaticMeshVertexBuffers& GetVertexBuffers(const UStaticMesh& Mesh, int32 LODIndex);
    static const FStaticMeshSection& GetSection(const UStaticMesh& Mesh, int32 LODIndex, int32 SectionIndex);
    static FIndexArrayView GetIndexBuffer(const UStaticMesh& Mesh, int32 LODIndex);

    FORCEINLINE static const FStaticMeshVertexBuffers& GetVertexBuffers(const FPMUStaticMeshSectionData& Input);
    FORCEINLINE static const FStaticMeshSection& GetSection(const FPMUStaticMeshSectionData& Input);
    FORCEINLINE static FIndexArrayView GetIndexBuffer(const FPMUStaticMeshSectionData& Input);

    template<typename IndexArrayType>
    static void GenerateEdges(TArray<FPMUEdge>& OutEdges, const IndexArrayType& Triangles)
    {
        OutEdges.Reset(Triangles.Num()*3);

        for (int32 i=0; i<Triangles.Num(); i+=3)
        {
            const uint32 vi0 = Triangles[i  ];
            const uint32 vi1 = Triangles[i+1];
            const uint32 vi2 = Triangles[i+2];

            OutEdges.Emplace(FMath::Min(vi0, vi1), FMath::Max(vi0, vi1));
            OutEdges.Emplace(FMath::Min(vi0, vi2), FMath::Max(vi0, vi2));
            OutEdges.Emplace(FMath::Min(vi1, vi2), FMath::Max(vi1, vi2));
        }
    }

    template<typename IndexArrayType>
    static void GenerateBoundaryEdges(TArray<FPMUEdge>& OutEdges, const IndexArrayType& Triangles)
    {
        TMap<uint64, int32> EdgeMap;

        EdgeMap.Reserve(Triangles.Num()*3);

        for (int32 i=0; i<Triangles.Num(); i+=3)
        {
            const uint32 vi0 = Triangles[i  ];
            const uint32 vi1 = Triangles[i+1];
            const uint32 vi2 = Triangles[i+2];

            FPMUEdge E0(FMath::Min(vi0, vi1), FMath::Max(vi0, vi1));
            FPMUEdge E1(FMath::Min(vi0, vi2), FMath::Max(vi0, vi2));
            FPMUEdge E2(FMath::Min(vi1, vi2), FMath::Max(vi1, vi2));

            if (int32* Count0 = EdgeMap.Find(E0.IndexPacked))
            {
                ++(*Count0);
            }
            else
            {
                EdgeMap.Emplace(E0.IndexPacked, 1);
            }

            if (int32* Count1 = EdgeMap.Find(E1.IndexPacked))
            {
                ++(*Count1);
            }
            else
            {
                EdgeMap.Emplace(E1.IndexPacked, 1);
            }

            if (int32* Count2 = EdgeMap.Find(E2.IndexPacked))
            {
                ++(*Count2);
            }
            else
            {
                EdgeMap.Emplace(E2.IndexPacked, 1);
            }
        }

        OutEdges.Reset(EdgeMap.Num());

        for (const auto& EdgeData : EdgeMap)
        {
            if (EdgeData.Get<1>() == 1)
            {
                OutEdges.Emplace(EdgeData.Get<0>());
            }
        }
    }

    static void GatherBoundaryEdges(TArray<FPMUEdge>& OutEdges, const TArray<FPMUEdge>& InEdges, const int32 VertexCount);

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

FORCEINLINE bool UPMUMeshUtility::IsValidSectionData(const FPMUStaticMeshSectionData& Input)
{
    return IsValidSectionData(Input.Mesh, Input.LODIndex, Input.SectionIndex);
}

FORCEINLINE const FStaticMeshVertexBuffers& UPMUMeshUtility::GetVertexBuffers(const FPMUStaticMeshSectionData& Input)
{
    return GetVertexBuffers(*Input.Mesh, Input.LODIndex);
}

FORCEINLINE const FStaticMeshSection& UPMUMeshUtility::GetSection(const FPMUStaticMeshSectionData& Input)
{
    return GetSection(*Input.Mesh, Input.LODIndex, Input.SectionIndex);
}

FORCEINLINE FIndexArrayView UPMUMeshUtility::GetIndexBuffer(const FPMUStaticMeshSectionData& Input)
{
    return GetIndexBuffer(*Input.Mesh, Input.LODIndex);
}
