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
#include "LocalVertexFactory.h"
#include "PositionVertexBuffer.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "RenderResource.h"
#include "StaticMeshVertexBuffer.h"
#include "Materials/Material.h"
#include "PhysicsEngine/BodySetup.h"
#include "Rendering/ColorVertexBuffer.h"

class UPMUMeshComponent;

//////////////////////////////////////////////////////////////////////////

class FPMUPositionVertexBuffer : public FVertexBuffer
{
public:

    /** Default constructor. */
    FPMUPositionVertexBuffer();

    /** Destructor. */
    ~FPMUPositionVertexBuffer();

    /** Delete existing resources */
    void CleanUp();

    void Init(const TArray<FVector>& InPositions, bool bInNeedsCPUAccess = true);
    void Serialize(FArchive& Ar, bool bInNeedsCPUAccess);

    void operator=(const FPMUPositionVertexBuffer& Other);

    // Vertex data accessors.
    FORCEINLINE FVector& VertexPosition(uint32 VertexIndex)
    {
        checkSlow(VertexIndex < GetNumVertices());
        return ((FPositionVertex*)(Data + VertexIndex * Stride))->Position;
    }
    FORCEINLINE const FVector& VertexPosition(uint32 VertexIndex) const
    {
        checkSlow(VertexIndex < GetNumVertices());
        return ((FPositionVertex*)(Data + VertexIndex * Stride))->Position;
    }
    // Other accessors.
    FORCEINLINE uint32 GetStride() const
    {
        return Stride;
    }
    FORCEINLINE uint32 GetNumVertices() const
    {
        return NumVertices;
    }

    // FRenderResource interface.
    virtual void InitRHI() override;
    virtual void ReleaseRHI() override;
    virtual FString GetFriendlyName() const override { return TEXT("PositionOnly Static-mesh vertices"); }

    void BindPositionVertexBuffer(const class FVertexFactory* VertexFactory, struct FStaticMeshDataType& Data) const;

    void* GetVertexData()
    {
        return Data;
    }

    FORCEINLINE const FUnorderedAccessViewRHIParamRef GetUAV()
    {
        return PositionComponentUAV;
    }

private:

    FShaderResourceViewRHIRef PositionComponentSRV;
    FUnorderedAccessViewRHIRef PositionComponentUAV;

    /** The vertex data storage type */
    class FPMUPositionVertexData* VertexData;

    /** The cached vertex data pointer. */
    uint8* Data;

    /** The cached vertex stride. */
    uint32 Stride;

    /** The cached number of vertices. */
    uint32 NumVertices;

    bool bNeedsCPUAccess = true;

    /** Allocates the vertex data storage type. */
    void AllocateData(bool bInNeedsCPUAccess = true);
};

class FPMUStaticMeshVertexBuffer : public FStaticMeshVertexBuffer
{
public:

    FORCEINLINE bool IsValidTangentArrayInitSize(const uint32 DataSize)
    {
        return GetTangentSize() == DataSize;
    }

    FORCEINLINE bool IsValidTexCoordArrayInitSize(const uint32 DataSize)
    {
        return GetTexCoordSize() == DataSize;
    }

    void InitTangentsFromArray(const TArray<uint32>& InTangents)
    {
        if (InTangents.Num())
        {
            check(IsValidTangentArrayInitSize(InTangents.Num() * InTangents.GetTypeSize()));
            FMemory::Memcpy(GetTangentData(), InTangents.GetData(), GetTangentSize());
        }
    }

    void InitTexCoordsFromArray(const TArray<FVector2D>& InTexCoords)
    {
        if (InTexCoords.Num())
        {
            check(IsValidTexCoordArrayInitSize(InTexCoords.Num() * InTexCoords.GetTypeSize()));
            FMemory::Memcpy(GetTexCoordData(), InTexCoords.GetData(), GetTexCoordSize());
        }
    }
};

class FPMUColorVertexBuffer : public FColorVertexBuffer
{
public:

    void InitFromColorArrayMemcpy(const TArray<FColor>& InColors, bool bNeedsCPUAccess = true)
    {
        if (InColors.Num())
        {
            Init(InColors.Num(), bNeedsCPUAccess);
            check(GetStride() == InColors.GetTypeSize());
            check(GetAllocatedSize() == (InColors.Num() * InColors.GetTypeSize()));
            FMemory::Memcpy(GetVertexData(), InColors.GetData(), GetAllocatedSize());
        }
    }
};

class FPMUUniformTangentVertexFactory : public FLocalVertexFactory
{
    DECLARE_VERTEX_FACTORY_TYPE(FPMUUniformTangentVertexFactory);

public:

    static bool ShouldCompilePermutation(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

    FPMUUniformTangentVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
        : FLocalVertexFactory(InFeatureLevel, "FPMUUniformTangentVertexFactory")
    {
    }
};

class FPMUBaseMeshProxySection
{
public:
    virtual void InitSection(const FPMUMeshSection& Section, UMaterialInterface* InMaterial) = 0;
    virtual void InitResource() = 0;
    virtual void ReleaseResources() = 0;
    virtual UMaterialInterface* GetMaterial() = 0;
    virtual FPMUStaticMeshVertexBuffer* GetStaticMeshVertexBuffer() = 0;
    virtual FPMUPositionVertexBuffer* GetPositionVertexBuffer() = 0;
    virtual FPMUColorVertexBuffer* GetColorVertexBuffer() = 0;
    virtual FDynamicMeshIndexBuffer32* GetIndexBuffer() = 0;
    virtual FLocalVertexFactory* GetVertexFactory() = 0;
    virtual const UMaterialInterface* GetMaterial() const = 0;
    virtual const FPMUStaticMeshVertexBuffer* GetStaticMeshVertexBuffer() const = 0;
    virtual const FPMUPositionVertexBuffer* GetPositionVertexBuffer() const = 0;
    virtual const FPMUColorVertexBuffer* GetColorVertexBuffer() const = 0;
    virtual const FDynamicMeshIndexBuffer32* GetIndexBuffer() const = 0;
    virtual const FLocalVertexFactory* GetVertexFactory() const = 0;
    virtual bool IsSectionVisible() const = 0;
};

/** Class representing a single section of the proc mesh */
class FPMUMeshProxySection : public FPMUBaseMeshProxySection
{
public:

    /** Material applied to this section */
    UMaterialInterface* Material;
    /** The buffer containing tangents and uv data. */
    FPMUStaticMeshVertexBuffer StaticMeshVertexBuffer;
    /** The buffer containing the position vertex data. */
    FPMUPositionVertexBuffer PositionVertexBuffer;
    /** The buffer containing the vertex color data. */
    FPMUColorVertexBuffer ColorVertexBuffer;
    /** Index buffer for this section */
    FDynamicMeshIndexBuffer32 IndexBuffer;
    /** Vertex factory for this section */
    FLocalVertexFactory VertexFactory;
    /** Whether this section is currently visible */
    bool bSectionVisible;

    FPMUMeshProxySection(ERHIFeatureLevel::Type InFeatureLevel, bool bUseFullPrecisionUVs)
        : Material(nullptr)
        , VertexFactory(InFeatureLevel, "FPMUMeshProxySection")
        //, VertexFactory(InFeatureLevel)
        , bSectionVisible(true)
    {
        StaticMeshVertexBuffer.SetUseFullPrecisionUVs(bUseFullPrecisionUVs);
    }

    virtual ~FPMUMeshProxySection()
    {
        ReleaseResources();
    }

    virtual void InitSection(const FPMUMeshSection& Section, UMaterialInterface* InMaterial) override;

    virtual void InitResource() override;

    virtual void ReleaseResources() override
    {
        PositionVertexBuffer.ReleaseResource();
        StaticMeshVertexBuffer.ReleaseResource();
        ColorVertexBuffer.ReleaseResource();
        IndexBuffer.ReleaseResource();
        VertexFactory.ReleaseResource();
    }

    // Accessor

    FORCEINLINE virtual UMaterialInterface* GetMaterial() override
    {
        return Material;
    }

    FORCEINLINE virtual FPMUStaticMeshVertexBuffer* GetStaticMeshVertexBuffer() override
    {
        return &StaticMeshVertexBuffer;
    }

    FORCEINLINE virtual FPMUPositionVertexBuffer* GetPositionVertexBuffer() override
    {
        return &PositionVertexBuffer;
    }

    FORCEINLINE virtual FPMUColorVertexBuffer* GetColorVertexBuffer() override
    {
        return &ColorVertexBuffer;
    }

    FORCEINLINE virtual FDynamicMeshIndexBuffer32* GetIndexBuffer() override
    {
        return &IndexBuffer;
    }

    FORCEINLINE virtual FLocalVertexFactory* GetVertexFactory() override
    {
        return &VertexFactory;
    }

    // Const Accessor

    FORCEINLINE virtual const UMaterialInterface* GetMaterial() const override
    {
        return Material;
    }

    FORCEINLINE virtual const FPMUStaticMeshVertexBuffer* GetStaticMeshVertexBuffer() const override
    {
        return &StaticMeshVertexBuffer;
    }

    FORCEINLINE virtual const FPMUPositionVertexBuffer* GetPositionVertexBuffer() const override
    {
        return &PositionVertexBuffer;
    }

    FORCEINLINE virtual const FPMUColorVertexBuffer* GetColorVertexBuffer() const override
    {
        return &ColorVertexBuffer;
    }

    FORCEINLINE virtual const FDynamicMeshIndexBuffer32* GetIndexBuffer() const override
    {
        return &IndexBuffer;
    }

    FORCEINLINE virtual const FLocalVertexFactory* GetVertexFactory() const override
    {
        return &VertexFactory;
    }

    FORCEINLINE virtual bool IsSectionVisible() const override
    {
        return bSectionVisible;
    }
};

class FPMUMeshSceneProxy : public FPrimitiveSceneProxy
{
public:

    FPMUMeshSceneProxy(UPMUMeshComponent* Component);
    virtual ~FPMUMeshSceneProxy();

    FORCEINLINE bool IsValidSection(int32 SectionIndex) const
    {
        return Sections.IsValidIndex(SectionIndex) && Sections[SectionIndex] != nullptr;
    }

    FORCEINLINE int32 GetSectionCount() const
    {
        return Sections.Num();
    }

    FORCEINLINE FPMUBaseMeshProxySection* GetSection(int32 SectionIndex)
    {
        return Sections.IsValidIndex(SectionIndex) ? Sections[SectionIndex] : nullptr;
    }

    void ConstructSections(UPMUMeshComponent& Component);

    virtual void GetDynamicMeshElements(
        const TArray<const FSceneView*>& Views,
        const FSceneViewFamily& ViewFamily,
        uint32 VisibilityMap,
        FMeshElementCollector& Collector
        ) const override;

    virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const;

    virtual bool CanBeOccluded() const override
    {
        return !MaterialRelevance.bDisableDepthTest;
    }

    virtual SIZE_T GetTypeHash() const override
    {
        static size_t UniquePointer;
        return reinterpret_cast<size_t>(&UniquePointer);
    }

    virtual uint32 GetMemoryFootprint(void) const override
    {
        return sizeof(*this) + GetAllocatedSize();
    }

    uint32 GetAllocatedSize(void) const
    {
        return FPrimitiveSceneProxy::GetAllocatedSize();
    }

protected:

    virtual FPMUBaseMeshProxySection* CreateSectionProxy(ERHIFeatureLevel::Type FeatureLevel);

private:

    TArray<FPMUBaseMeshProxySection*> Sections;

    UBodySetup* BodySetup;
    FMaterialRelevance MaterialRelevance;
};
