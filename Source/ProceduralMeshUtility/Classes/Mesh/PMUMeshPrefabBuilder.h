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
#include "Mesh/PMUMeshTypes.h"
#include "PMUMeshPrefabBuilder.generated.h"

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUPrefabInputData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UStaticMesh* Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 LODIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SectionIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector2D Offset;
};

UCLASS(BlueprintType, Blueprintable)
class PROCEDURALMESHUTILITY_API UPMUPrefabBuilder : public UObject
{
	GENERATED_BODY()

public:

    struct FPrefabData
    {
        const UStaticMesh* Mesh;
        int32 LODIndex;
        int32 SectionIndex;
        FBox2D Bounds;
        float Length;
        TArray<uint32> SortedIndexAlongX;
        TArray<uint32> SurfaceIndices;
        TArray<uint32> ExtrudeIndices;
    };

    struct FPrefabBuffers
    {
        TArray<FVector> Positions;
        TArray<FVector2D> UVs;
        TArray<uint32> Tangents;
        TArray<uint32> Indices;
    };

    typedef TMap<const FPrefabData*, FPrefabBuffers> FPrefabBufferMap;

    TArray<FPrefabData> PreparedPrefabs;
    TArray<uint32> SortedPrefabs;
    bool bPrefabInitialized;

    FPMUMeshSection GeneratedSection;

    void AllocateSection(FPMUMeshSection& OutSection, uint32 NumVertices, uint32 NumIndices);

    uint32 CopyPrefabToSection(FPMUMeshSection& OutSection, FPrefabBufferMap& PrefabBufferMap, const FPrefabData& Prefab);
    uint32 CopyPrefabToSection(FPMUMeshSection& OutSection, const FPrefabBuffers& PrefabBuffers);

    const FStaticMeshVertexBuffers& GetVertexBuffers(const UStaticMesh& Mesh, int32 LODIndex) const;
    const FStaticMeshSection& GetSection(const UStaticMesh& Mesh, int32 LODIndex, int32 SectionIndex) const;
    FIndexArrayView GetIndexBuffer(const UStaticMesh& Mesh, int32 LODIndex) const;

    void GetPrefabGeometryCount(const FPrefabData& Prefab, uint32& OutNumVertices, uint32& OutNumIndices) const;

    FORCEINLINE const FStaticMeshVertexBuffers& GetVertexBuffers(const FPrefabData& Prefab) const
    {
        check(Prefab.Mesh != nullptr);
        return GetVertexBuffers(*Prefab.Mesh, Prefab.LODIndex);
    }

    FORCEINLINE const FStaticMeshSection& GetSection(const FPrefabData& Prefab) const
    {
        check(Prefab.Mesh != nullptr);
        return GetSection(*Prefab.Mesh, Prefab.LODIndex, Prefab.SectionIndex);
    }

    FORCEINLINE FIndexArrayView GetIndexBuffer(const FPrefabData& Prefab) const
    {
        check(Prefab.Mesh != nullptr);
        return GetIndexBuffer(*Prefab.Mesh, Prefab.LODIndex);
    }

    FORCEINLINE const FPrefabData& GetSortedPrefab(uint32 i) const
    {
        return PreparedPrefabs[SortedPrefabs[i]];
    }

public:

    UPROPERTY(EditAnywhere, Category="Prefab Settings", BlueprintReadWrite)
    TArray<FPMUPrefabInputData> MeshPrefabs;

    UPMUPrefabBuilder();

    bool IsValidPrefab(const UStaticMesh* Mesh, int32 LODIndex, int32 SectionIndex) const;

    FORCEINLINE bool IsValidPrefab(const FPMUPrefabInputData& Input) const
    {
        return IsValidPrefab(Input.Mesh, Input.LODIndex, Input.SectionIndex);
    }

    FORCEINLINE bool IsValidPrefab(const FPrefabData& Prefab) const
    {
        return IsValidPrefab(Prefab.Mesh, Prefab.LODIndex, Prefab.SectionIndex);
    }

    FORCEINLINE int32 GetPreparedPrefabCount() const
    {
        return PreparedPrefabs.Num();
    }
    
    void ResetPrefabs();
    void InitializePrefabs();
    void BuildPrefabsAlongPoly(const TArray<FVector2D>& Points, bool bClosedPoly);
    void BuildPrefabsAlongPoly(const TArray<FVector2D>& Positions, const TArray<FVector2D>& Tangents, const TArray<float>& Distances, bool bClosedPoly);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Reset Prefabs"))
    void K2_ResetPrefabs();

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Build Prefabs Along Polyline"))
    void K2_BuildPrefabsAlongPoly(const TArray<FVector2D>& Points, bool bClosedPoly);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Get Generated Section"))
    FPMUMeshSectionRef K2_GetGeneratedSection();
};

FORCEINLINE_DEBUGGABLE void UPMUPrefabBuilder::K2_ResetPrefabs()
{
    ResetPrefabs();
}

FORCEINLINE_DEBUGGABLE FPMUMeshSectionRef UPMUPrefabBuilder::K2_GetGeneratedSection()
{
    return FPMUMeshSectionRef(GeneratedSection);
}

FORCEINLINE_DEBUGGABLE void UPMUPrefabBuilder::K2_BuildPrefabsAlongPoly(const TArray<FVector2D>& Points, bool bClosedPoly)
{
    BuildPrefabsAlongPoly(Points, bClosedPoly);
}
