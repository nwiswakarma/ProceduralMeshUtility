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

/** Class representing a single section of the proc mesh */
class FPMUMeshProxySection
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
        , bSectionVisible(true)
	{
        StaticMeshVertexBuffer.SetUseFullPrecisionUVs(bUseFullPrecisionUVs);
    }

    void ReleaseResources()
    {
        PositionVertexBuffer.ReleaseResource();
        StaticMeshVertexBuffer.ReleaseResource();
        ColorVertexBuffer.ReleaseResource();
        IndexBuffer.ReleaseResource();
        VertexFactory.ReleaseResource();
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

    FORCEINLINE FPMUMeshProxySection* GetSection(int32 SectionIndex)
    {
        return Sections.IsValidIndex(SectionIndex) ? Sections[SectionIndex] : nullptr;
    }

    void ConstructSections(UPMUMeshComponent& Component);

#if 0
    // Called on render thread to assign new dynamic data
    void UpdateSection_RenderThread(FPMUMeshSectionUpdateData* SectionData)
    {
        //SCOPE_CYCLE_COUNTER(STAT_VoxelMesh_UpdateSectionRT);

        check(IsInRenderingThread());

        // Check we have data 
        if (SectionData != nullptr)
        {
            // Check it references a valid section
            if (SectionData->TargetSection < Sections.Num() &&
                Sections[SectionData->TargetSection] != nullptr)
            {
                FPMUMeshProxySection* Section = Sections[SectionData->TargetSection];

                // Lock vertex buffer
                const int32 NumVerts = SectionData->NewVertexBuffer.Num();
                FPMUPackedVertex* VertexBufferData = (FPMUPackedVertex*) RHILockVertexBuffer(Section->VertexBuffer.VertexBufferRHI, 0, NumVerts * sizeof(FPMUPackedVertex), RLM_WriteOnly);

                // Whether to use vertex uv or position as uv
                const bool bUsePositionAsUV = SectionData->bUsePositionAsUV;

                // Iterate through vertex data, copying in new info
                for (int32 VertIdx = 0; VertIdx < NumVerts; VertIdx++)
                {
                    const FPMUMeshVertex& PMUVertex(SectionData->NewVertexBuffer[VertIdx]);
                    FPMUPackedVertex& DynamicVertex(VertexBufferData[VertIdx]);
                    ConvertPMUMeshToDynMeshVertex(DynamicVertex, PMUVertex, bUsePositionAsUV);
                }

                // Unlock vertex buffer
                RHIUnlockVertexBuffer(Section->VertexBuffer.VertexBufferRHI);
            }

            // Free data sent from game thread
            delete SectionData;
        }
    }

    void SetSectionVisibility_RenderThread(int32 SectionIndex, bool bNewVisibility)
    {
        check(IsInRenderingThread());

        if (SectionIndex < Sections.Num() &&
            Sections[SectionIndex] != nullptr)
        {
            Sections[SectionIndex]->bSectionVisible = bNewVisibility;
        }
    }
#endif

    virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

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

private:

    TArray<FPMUMeshProxySection*> Sections;

    UBodySetup* BodySetup;
    FMaterialRelevance MaterialRelevance;
};
