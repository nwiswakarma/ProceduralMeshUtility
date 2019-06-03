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

#include "Mesh/PMUMeshSceneProxy.h"

#include "GPUSkinCache.h"
#include "MaterialShared.h"
#include "PositionVertexBuffer.h"
#include "RenderUtils.h"
#include "RenderingThread.h"
#include "SceneManagement.h"
#include "StaticMeshVertexBuffer.h"
#include "Rendering/ColorVertexBuffer.h"
#include "Stats/Stats.h"

#include "Mesh/PMUMeshComponent.h"

class FPMUPositionVertexData :
	public TStaticMeshVertexData<FPositionVertex>
{
public:
	FPMUPositionVertexData(bool InNeedsCPUAccess = false)
		: TStaticMeshVertexData<FPositionVertex>(InNeedsCPUAccess)
	{
	}
};

FPMUPositionVertexBuffer::FPMUPositionVertexBuffer():
	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0)
{
}

FPMUPositionVertexBuffer::~FPMUPositionVertexBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FPMUPositionVertexBuffer::CleanUp()
{
	if (VertexData)
	{
		delete VertexData;
		VertexData = NULL;
	}
}

void FPMUPositionVertexBuffer::Init(const TArray<FVector>& InPositions, bool bInNeedsCPUAccess)
{
	NumVertices = InPositions.Num();
	bNeedsCPUAccess = bInNeedsCPUAccess;
	if (NumVertices)
	{
		AllocateData(bInNeedsCPUAccess);
		check(Stride == InPositions.GetTypeSize());
		VertexData->ResizeBuffer(NumVertices);
		Data = VertexData->GetDataPointer();
		FMemory::Memcpy( Data, InPositions.GetData(), Stride * NumVertices );
	}
}

void FPMUPositionVertexBuffer::Serialize(FArchive& Ar, bool bInNeedsCPUAccess)
{
	bNeedsCPUAccess = bInNeedsCPUAccess;

	Ar << Stride << NumVertices;

	if (Ar.IsLoading())
	{
		// Allocate the vertex data storage type.
		AllocateData(bInNeedsCPUAccess);
	}

	if (VertexData != NULL)
	{
		// Serialize the vertex data.
		VertexData->Serialize(Ar);

		// Make a copy of the vertex data pointer.
		Data = NumVertices ? VertexData->GetDataPointer() : nullptr;
	}
}

/**
* Specialized assignment operator, only used when importing LOD's.  
*/
void FPMUPositionVertexBuffer::operator=(const FPMUPositionVertexBuffer &Other)
{
	// VertexData doesn't need to be allocated here
    // because Build will be called next,
	VertexData = NULL;
}

void FPMUPositionVertexBuffer::InitRHI()
{
	check(VertexData);
	FResourceArrayInterface* ResourceArray = VertexData->GetResourceArray();
	if (ResourceArray->GetResourceDataSize())
	{
		// Create the vertex buffer.
		FRHIResourceCreateInfo CreateInfo(ResourceArray);
		VertexBufferRHI = RHICreateVertexBuffer(ResourceArray->GetResourceDataSize(), BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess, CreateInfo);

		// we have decide to create the SRV based on GMaxRHIShaderPlatform because this is created once and shared between feature levels for editor preview.
		if (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform) || IsGPUSkinCacheAvailable())
		{
			PositionComponentSRV = RHICreateShaderResourceView(VertexBufferRHI, 4, PF_R32_FLOAT);
		}

        PositionComponentUAV = RHICreateUnorderedAccessView(VertexBufferRHI, PF_R32_FLOAT);
	}
}

void FPMUPositionVertexBuffer::ReleaseRHI()
{
	PositionComponentSRV.SafeRelease();
	PositionComponentUAV.SafeRelease();

	FVertexBuffer::ReleaseRHI();
}

void FPMUPositionVertexBuffer::AllocateData(bool bInNeedsCPUAccess)
{
	// Clear any old VertexData before allocating.
	CleanUp();

	VertexData = new FPMUPositionVertexData(bInNeedsCPUAccess);

	// Calculate the vertex stride.
	Stride = VertexData->GetStride();
}

void FPMUPositionVertexBuffer::BindPositionVertexBuffer(const FVertexFactory* VertexFactory, FStaticMeshDataType& StaticMeshData) const
{
	StaticMeshData.PositionComponent = FVertexStreamComponent(
		this,
		STRUCT_OFFSET(FPositionVertex, Position),
		GetStride(),
		VET_Float3
	);
	StaticMeshData.PositionComponentSRV = PositionComponentSRV;
}

//////////////////////////////////////////////////////////////////////////

FPMUMeshSceneProxy::FPMUMeshSceneProxy(UPMUMeshComponent* Component)
    : FPrimitiveSceneProxy(Component)
    , BodySetup(Component->GetBodySetup())
    , MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
{
    check(IsValid(Component));

    ConstructSections(*Component);
}

FPMUMeshSceneProxy::~FPMUMeshSceneProxy()
{
    for (FPMUMeshProxySection* Section : Sections)
    {
        if (Section != nullptr)
        {
            Section->ReleaseResources();
            delete Section;
        }
    }
}

void FPMUMeshSceneProxy::ConstructSections(UPMUMeshComponent& Component)
{
    ERHIFeatureLevel::Type FeatureLevel = GetScene().GetFeatureLevel();

    const int32 NumSections = Component.Sections.Num();
    Sections.SetNumZeroed(NumSections);

    for (int32 SectionIdx=0; SectionIdx<NumSections; ++SectionIdx)
    {
        FPMUMeshSection& SrcSection(Component.Sections[SectionIdx]);

        if (SrcSection.Indices.Num() >= 3 && SrcSection.Positions.Num() >= 3)
        {
            // Create new section

            Sections[SectionIdx] = new FPMUMeshProxySection(FeatureLevel, true);
            FPMUMeshProxySection& DstSection(*Sections[SectionIdx]);

            // Copy index buffer

            DstSection.IndexBuffer.Indices = SrcSection.Indices;

            // Copy position buffer

            const int32 NumVerts = SrcSection.Positions.Num();
            DstSection.PositionVertexBuffer.Init(SrcSection.Positions);

            // Copy color buffer if it has valid size, otherwise don't initialize
            // to use the default color buffer binding

            if (SrcSection.Colors.Num() == NumVerts)
            {
                DstSection.ColorVertexBuffer.InitFromColorArrayMemcpy(SrcSection.Colors);
            }

            // Copy UV and Tangent buffers

            FPMUStaticMeshVertexBuffer& UVTangentsBuffer(DstSection.StaticMeshVertexBuffer);

            UVTangentsBuffer.Init(NumVerts, 1);

            const uint32 TangentDataSize = SrcSection.Tangents.Num() * SrcSection.Tangents.GetTypeSize();
            const uint32 UVDataSize = SrcSection.UVs.Num() * SrcSection.UVs.GetTypeSize();

            const bool bHasValidTangentInitSize = UVTangentsBuffer.IsValidTangentArrayInitSize(TangentDataSize);
            const bool bHasValidTexCoordInitSize = UVTangentsBuffer.IsValidTexCoordArrayInitSize(UVDataSize);

            bool bRequireManualTangentInit = bHasValidTangentInitSize;
            bool bRequireManualTexCoordInit = bHasValidTexCoordInitSize; 

            if (SrcSection.bEnableFastTangentsCopy && bHasValidTangentInitSize)
            {
                UVTangentsBuffer.InitTangentsFromArray(SrcSection.Tangents);
                bRequireManualTangentInit = false;
            }

            if (SrcSection.bEnableFastUVCopy && bHasValidTexCoordInitSize)
            {
                UVTangentsBuffer.InitTexCoordsFromArray(SrcSection.UVs);
                bRequireManualTexCoordInit = false;
            }

            if (bRequireManualTangentInit || bRequireManualTexCoordInit)
            {
                for (int32 i=0; i<NumVerts; ++i)
                {
                    if (bRequireManualTangentInit)
                    {
                        const FVector& VX(SrcSection.TangentsX[i]);
                        const FVector& VZ(SrcSection.TangentsZ[i]);
                        FPackedNormal NX(VX);
                        FPackedNormal NZ(FVector4(VZ,1));

                        UVTangentsBuffer.SetVertexTangents(i, VX, GenerateYAxis(NX, NZ), VZ);
                    }

                    if (bRequireManualTexCoordInit)
                    {
                        UVTangentsBuffer.SetVertexUV(i, 0, SrcSection.UVs[i]);
                    }
                }
            }

            // Enqueue render resource initialization

            FPMUMeshProxySection* Section = &DstSection;
            FLocalVertexFactory* VertexFactory = &DstSection.VertexFactory;
            ENQUEUE_RENDER_COMMAND(ConstructSections_InitializeResources)(
                [VertexFactory, Section](FRHICommandListImmediate& RHICmdList)
                {
                    // Initialize buffers
                    Section->PositionVertexBuffer.InitResource();
                    Section->StaticMeshVertexBuffer.InitResource();
                    Section->ColorVertexBuffer.InitResource();
                    Section->IndexBuffer.InitResource();

                    // Bind vertex buffers
                    FLocalVertexFactory::FDataType Data;
                    Section->PositionVertexBuffer.BindPositionVertexBuffer(VertexFactory, Data);
                    Section->StaticMeshVertexBuffer.BindTangentVertexBuffer(VertexFactory, Data);
                    Section->StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(VertexFactory, Data);
                    Section->StaticMeshVertexBuffer.BindLightMapVertexBuffer(VertexFactory, Data, 0);
                    Section->ColorVertexBuffer.BindColorVertexBuffer(VertexFactory, Data);
                    VertexFactory->SetData(Data);

                    // Initialize vertex factory
                    VertexFactory->InitResource();
                } );

            // Assign material or revert to default if material
            // for the specified section does not exist

            DstSection.Material = Component.GetMaterial(SectionIdx);

            if (DstSection.Material == NULL)
            {
                DstSection.Material = UMaterial::GetDefaultMaterial(MD_Surface);
            }

            // Copy visibility info
            DstSection.bSectionVisible = SrcSection.bSectionVisible;
        }
    }
}

void FPMUMeshSceneProxy::GetDynamicMeshElements(
    const TArray<const FSceneView*>& Views,
    const FSceneViewFamily& ViewFamily,
    uint32 VisibilityMap,
    FMeshElementCollector& Collector
    ) const
{
    //SCOPE_CYCLE_COUNTER(STAT_VoxelMesh_GetMeshElements);

    // Set up wireframe material (if needed)
    const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

    FColoredMaterialRenderProxy* WireframeMaterialInstance = NULL;
    if (bWireframe)
    {
        WireframeMaterialInstance = new FColoredMaterialRenderProxy(
            GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : NULL,
            FLinearColor(0, 0.5f, 1.f)
            );

        Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
    }

    // Iterate over direct sections
    for (const FPMUMeshProxySection* Section : Sections)
    {
        if (Section != nullptr && Section->bSectionVisible)
        {
            FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : Section->Material->GetRenderProxy();

            for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
            {
                if (VisibilityMap & (1 << ViewIndex))
                {
                    const FSceneView* View = Views[ViewIndex];

                    // Setup mesh data

                    FMeshBatch& Mesh = Collector.AllocateMesh();
                    Mesh.bWireframe = bWireframe;
                    Mesh.VertexFactory = &Section->VertexFactory;
                    Mesh.MaterialRenderProxy = MaterialProxy;
                    Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
                    Mesh.Type = PT_TriangleList;
                    Mesh.DepthPriorityGroup = SDPG_World;
                    Mesh.bCanApplyViewModeOverrides = false;

                    // Setup mesh element

                    FMeshBatchElement& BatchElement = Mesh.Elements[0];
                    BatchElement.IndexBuffer = &Section->IndexBuffer;
                    BatchElement.FirstIndex = 0;
                    BatchElement.NumPrimitives = Section->IndexBuffer.Indices.Num() / 3;
                    BatchElement.MinVertexIndex = 0;
                    BatchElement.MaxVertexIndex = Section->PositionVertexBuffer.GetNumVertices() - 1;

                    // Create single frame primitive uniform buffer
                    {
                        bool bHasPrecomputedVolumetricLightmap;
                        FMatrix PreviousLocalToWorld;
                        int32 SingleCaptureIndex;
                        GetScene().GetPrimitiveUniformShaderParameters_RenderThread(
                            GetPrimitiveSceneInfo(),
                            bHasPrecomputedVolumetricLightmap,
                            PreviousLocalToWorld,
                            SingleCaptureIndex
                            );

                        FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
                        DynamicPrimitiveUniformBuffer.Set(
                            GetLocalToWorld(),
                            PreviousLocalToWorld,
                            GetBounds(),
                            GetLocalBounds(),
                            true,
                            bHasPrecomputedVolumetricLightmap,
                            UseEditorDepthTest()
                            );

                        BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;
                    }

                    Collector.AddMesh(ViewIndex, Mesh);
                }
            }
        }
    }

    // Draw bounds
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
    {
        if (VisibilityMap & (1 << ViewIndex))
        {
            // Draw simple collision as wireframe if 'show collision', and collision is enabled, and we are not using the complex as the simple
            if (ViewFamily.EngineShowFlags.Collision && IsCollisionEnabled() && BodySetup->GetCollisionTraceFlag() != ECollisionTraceFlag::CTF_UseComplexAsSimple)
            {
                FTransform GeomTransform(GetLocalToWorld());
                BodySetup->AggGeom.GetAggGeom(GeomTransform, GetSelectionColor(FColor(157, 149, 223, 255), IsSelected(), IsHovered()).ToFColor(true), NULL, false, false, UseEditorDepthTest(), ViewIndex, Collector);
            }

            // Render bounds
            RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
        }
    }
#endif
}

FPrimitiveViewRelevance FPMUMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
    FPrimitiveViewRelevance Result;
    Result.bDrawRelevance = IsShown(View);
    Result.bShadowRelevance = IsShadowCast(View);
    Result.bDynamicRelevance = true;
    Result.bRenderInMainPass = ShouldRenderInMainPass();
    Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
    Result.bRenderCustomDepth = ShouldRenderCustomDepth();
    Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
    MaterialRelevance.SetPrimitiveViewRelevance(Result);
    Result.bVelocityRelevance = IsMovable() && Result.bOpaqueRelevance && Result.bRenderInMainPass;
    return Result;
}
