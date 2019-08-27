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

class UPMUPrefabBuilder;

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUPrefabInputData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh")
    UStaticMesh* Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh")
    int32 LODIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh")
    int32 SectionIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh")
    FVector2D Offset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Line Distribution")
    float AngleThreshold = -1.f;
};

struct PROCEDURALMESHUTILITY_API FPMUPrefabGenerationData
{
    const UStaticMesh* Mesh;
    int32 LODIndex;
    int32 SectionIndex;
    FBox2D Bounds;
    float Length;
    float AngleThreshold;
    TArray<uint32> SortedIndexAlongX;
};

struct PROCEDURALMESHUTILITY_API FPMUPrefabGenerationBuffers
{
    TArray<FVector> Positions;
    TArray<FVector2D> UVs;
    TArray<FVector> TangentX;
    TArray<FVector4> TangentZ;
    TArray<FColor> Colors;
    TArray<uint32> Indices;

    FORCEINLINE uint32 Num() const
    {
        return Positions.Num();
    }

    FORCEINLINE bool HasUVs() const
    {
        return Positions.Num() == UVs.Num();
    }

    FORCEINLINE bool HasColors() const
    {
        return Positions.Num() == Colors.Num();
    }
};

UCLASS(BlueprintType, Blueprintable)
class PROCEDURALMESHUTILITY_API UPMUPrefabBuilderDecorator : public UObject
{
	GENERATED_BODY()

protected:

    enum { DEFAULT_INDEX = 0 };

    UPROPERTY()
    TWeakObjectPtr<UPMUPrefabBuilder> PrefabBuilderPtr;

    bool HasValidBuilder() const;
    UPMUPrefabBuilder* GetBuilder() const;

public:

    virtual void SetBuilder(UPMUPrefabBuilder* Builder);

    FORCEINLINE_DEBUGGABLE virtual void OnAllocateSections()
    {
        // Blank Implementation
    }

    FORCEINLINE_DEBUGGABLE virtual void OnPreDistributePrefab()
    {
        // Blank Implementation
    }

    FORCEINLINE_DEBUGGABLE virtual void OnPostDistributePrefab(uint32 SectionCount, uint32 PrefabCount)
    {
        // Blank Implementation
    }

    FORCEINLINE_DEBUGGABLE virtual int32 GetSectionIndexByOrigin(const FVector& PrefabOrigin)
    {
        return DEFAULT_INDEX;
    }

    FORCEINLINE_DEBUGGABLE virtual int32 GetPrefabIndex()
    {
        return DEFAULT_INDEX;
    }

    FORCEINLINE_DEBUGGABLE virtual void OnCopyPrefabToSection(FPMUMeshSection& Section, const FPMUPrefabGenerationData& Prefab, uint32 VertexOffsetIndex)
    {
        // Blank Implementation
    }

    FORCEINLINE_DEBUGGABLE virtual void OnPrePrefabTransform(int32 SectionIndex, int32 PrefabIndex, const FPMUPrefabGenerationData& Prefab, uint32 VertexOffsetIndex)
    {
        // Blank Implementation
    }

    FORCEINLINE_DEBUGGABLE virtual void OnTransformPosition(int32 SectionIndex, int32 PrefabIndex, uint32 VertexIndex, const FVector2D& Offset, const FVector2D& Tangent)
    {
        // Blank Implementation
    }

    FORCEINLINE_DEBUGGABLE virtual void OnPostPrefabTransform(int32 SectionIndex, int32 PrefabIndex, const FPMUPrefabGenerationData& Prefab, uint32 VertexOffsetIndex)
    {
        // Blank Implementation
    }
};

UCLASS(BlueprintType, Blueprintable)
class PROCEDURALMESHUTILITY_API UPMUPrefabBuilder : public UObject
{
	GENERATED_BODY()

public:

    typedef FPMUPrefabGenerationData    FPrefabData;
    typedef FPMUPrefabGenerationBuffers FPrefabBuffers;
    typedef TMap<const FPrefabData*, FPrefabBuffers> FPrefabBufferMap;

    TArray<FPrefabData> PreparedPrefabs;
    TArray<uint32> SortedPrefabs;
    bool bPrefabInitialized;

    TArray<FPMUMeshSection> GeneratedSections;

    UPROPERTY()
    UPMUPrefabBuilderDecorator* Decorator;

    void AllocateSection(FPMUMeshSection& OutSection, uint32 NumVertices, uint32 NumIndices);

    uint32 CopyPrefabToSection(FPMUMeshSection& OutSection, FPrefabBufferMap& PrefabBufferMap, const FPrefabData& Prefab);
    uint32 CopyPrefabToSection(FPMUMeshSection& OutSection, const FPrefabBuffers& PrefabBuffers);

    const FStaticMeshVertexBuffers& GetVertexBuffers(const UStaticMesh& Mesh, int32 LODIndex) const;
    const FStaticMeshSection& GetSection(const UStaticMesh& Mesh, int32 LODIndex, int32 SectionIndex) const;
    FIndexArrayView GetIndexBuffer(const UStaticMesh& Mesh, int32 LODIndex) const;

    void GetPrefabGeometryCount(uint32& OutNumVertices, uint32& OutNumIndices, const FPrefabData& Prefab) const;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Prefab Settings")
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

    FORCEINLINE bool HasSection(int32 SectionIndex) const
    {
        return GeneratedSections.IsValidIndex(SectionIndex);
    }

    FORCEINLINE int32 GetSectionCount() const
    {
        return GeneratedSections.Num();
    }

    FORCEINLINE FPMUMeshSection& GetSection(int32 SectionIndex)
    {
        return GeneratedSections[SectionIndex];
    }

    FORCEINLINE const FPMUMeshSection& GetSection(int32 SectionIndex) const
    {
        return GeneratedSections[SectionIndex];
    }

    void ResetDecorator();
    void InitializeDecorator();

    FORCEINLINE bool HasDecorator() const
    {
        return Decorator != nullptr;
    }

    FORCEINLINE void SetDecorator(UPMUPrefabBuilderDecorator* InDecorator)
    {
        if (InDecorator)
        {
            Decorator = InDecorator;
            Decorator->SetBuilder(this);
        }
        else
        {
            ResetDecorator();
        }
    }

    void ResetPrefabs();
    void InitializePrefabs();

    void BuildPrefabsAlongPoly(const TArray<FVector2D>& Points, bool bClosedPoly);
    void BuildPrefabsAlongPoly(const TArray<FVector2D>& Positions, const TArray<FVector2D>& Tangents, const TArray<float>& Distances, bool bClosedPoly);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Set Decorator"))
    void K2_SetDecorator(UPMUPrefabBuilderDecorator* InDecorator);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Reset Prefabs"))
    void K2_ResetPrefabs();

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Build Prefabs Along Polyline"))
    void K2_BuildPrefabsAlongPoly(const TArray<FVector2D>& Points, bool bClosedPoly);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Get Section Count"))
    int32 K2_GetSectionCount() const;

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Get Section"))
    FPMUMeshSectionRef K2_GetSection(int32 SectionIndex);
};

FORCEINLINE_DEBUGGABLE bool UPMUPrefabBuilderDecorator::HasValidBuilder() const
{
    return PrefabBuilderPtr.IsValid();
}

FORCEINLINE_DEBUGGABLE UPMUPrefabBuilder* UPMUPrefabBuilderDecorator::GetBuilder() const
{
    return PrefabBuilderPtr.Get();
}

FORCEINLINE_DEBUGGABLE void UPMUPrefabBuilderDecorator::SetBuilder(UPMUPrefabBuilder* InBuilder)
{
    PrefabBuilderPtr = InBuilder;
}

FORCEINLINE_DEBUGGABLE void UPMUPrefabBuilder::K2_SetDecorator(UPMUPrefabBuilderDecorator* InDecorator)
{
    SetDecorator(InDecorator);
}

FORCEINLINE_DEBUGGABLE void UPMUPrefabBuilder::K2_ResetPrefabs()
{
    ResetPrefabs();
}

FORCEINLINE_DEBUGGABLE int32 UPMUPrefabBuilder::K2_GetSectionCount() const
{
    return GetSectionCount();
}

FORCEINLINE_DEBUGGABLE FPMUMeshSectionRef UPMUPrefabBuilder::K2_GetSection(int32 SectionIndex)
{
    return HasSection(SectionIndex)
        ? FPMUMeshSectionRef(GetSection(SectionIndex))
        : FPMUMeshSectionRef();
}

FORCEINLINE_DEBUGGABLE void UPMUPrefabBuilder::K2_BuildPrefabsAlongPoly(const TArray<FVector2D>& Points, bool bClosedPoly)
{
    BuildPrefabsAlongPoly(Points, bClosedPoly);
}
