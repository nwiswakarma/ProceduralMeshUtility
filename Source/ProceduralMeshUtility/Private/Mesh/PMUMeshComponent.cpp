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

#include "PMUMeshComponent.h"

#include "PrimitiveViewRelevance.h"
#include "RenderResource.h"
#include "RenderingThread.h"
#include "PrimitiveSceneProxy.h"
#include "Containers/ResourceArray.h"
#include "EngineGlobals.h"
#include "VertexFactory.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "LocalVertexFactory.h"
#include "Engine/Engine.h"
#include "SceneManagement.h"
#include "PhysicsEngine/BodySetup.h"
#include "DynamicMeshBuilder.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Stats/Stats.h"
#include "PositionVertexBuffer.h"
#include "StaticMeshVertexBuffer.h"

//DECLARE_STATS_GROUP(TEXT("ProceduralMesh"), STATGROUP_ProceduralMesh, STATCAT_Advanced);

//DECLARE_CYCLE_STAT(TEXT("PMUMesh ~ Create Mesh Proxy"), STAT_VoxelMesh_CreateSceneProxy, STATGROUP_Voxel);
//DECLARE_CYCLE_STAT(TEXT("PMUMesh ~ Create Mesh Section"), STAT_VoxelMesh_CreateMeshSection, STATGROUP_Voxel);
//DECLARE_CYCLE_STAT(TEXT("PMUMesh ~ UpdateSection GT"), STAT_VoxelMesh_UpdateSectionGT, STATGROUP_Voxel);
//DECLARE_CYCLE_STAT(TEXT("PMUMesh ~ UpdateSection RT"), STAT_VoxelMesh_UpdateSectionRT, STATGROUP_Voxel);
//DECLARE_CYCLE_STAT(TEXT("PMUMesh ~ Get Mesh Elements"), STAT_VoxelMesh_GetMeshElements, STATGROUP_Voxel);
//DECLARE_CYCLE_STAT(TEXT("PMUMesh ~ Update Collision"), STAT_VoxelMesh_UpdateCollision, STATGROUP_Voxel);

class FPMUMeshVertexFactory : public FLocalVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FPMUMeshVertexFactory);

public:

    FPMUMeshVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
        : FLocalVertexFactory(InFeatureLevel, "FPMUMeshVertexFactory")
	{
		bSupportsManualVertexFetch = false;
	}

	static bool ShouldCompilePermutation(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
	{
		return FLocalVertexFactory::ShouldCompilePermutation(Platform, Material, ShaderType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		const bool ContainsManualVertexFetch = OutEnvironment.GetDefinitions().Contains("MANUAL_VERTEX_FETCH");
		if (! ContainsManualVertexFetch)
		{
			OutEnvironment.SetDefine(TEXT("MANUAL_VERTEX_FETCH"), TEXT("0"));
		}

		// Override local vertex factory compilation environment to disable speed tree wind and manual vertex fetch
        //
		//FLocalVertexFactory::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	virtual void InitRHI() override
    {
        // If the vertex buffer containing position is not the same vertex buffer containing the rest of the data,
        // then initialize PositionStream and PositionDeclaration.
        if (Data.PositionComponent.VertexBuffer != Data.TangentBasisComponents[0].VertexBuffer)
        {
            FVertexDeclarationElementList PositionOnlyStreamElements;
            PositionOnlyStreamElements.Add(AccessPositionStreamComponent(Data.PositionComponent, 0));
            InitPositionDeclaration(PositionOnlyStreamElements);
        }

        FVertexDeclarationElementList Elements;

        if (Data.PositionComponent.VertexBuffer != NULL)
        {
            Elements.Add(AccessStreamComponent(Data.PositionComponent, 0));
        }

        // Only tangent and normal are used by the stream. Binormal is derived in the shader.
        uint8 TangentBasisAttributes[2] = { 1, 2 };
        for (int32 AxisIndex=0; AxisIndex<2; ++AxisIndex)
        {
            if (Data.TangentBasisComponents[AxisIndex].VertexBuffer != NULL)
            {
                Elements.Add(AccessStreamComponent(Data.TangentBasisComponents[AxisIndex], TangentBasisAttributes[AxisIndex]));
            }
        }

        ColorStreamIndex = -1;
        if (Data.ColorComponent.VertexBuffer)
        {
            Elements.Add(AccessStreamComponent(Data.ColorComponent, 3));
            ColorStreamIndex = Elements.Last().StreamIndex;
        }
        else
        {
            //If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
            //This wastes 4 bytes of bandwidth per vertex, but prevents having to compile out twice the number of vertex factories.
            FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color);
            Elements.Add(AccessStreamComponent(NullColorComponent, 3));
            ColorStreamIndex = Elements.Last().StreamIndex;
        }

        if (Data.TextureCoordinates.Num())
        {
            const int32 BaseTexCoordAttribute = 4;

            for (int32 CoordinateIndex=0; CoordinateIndex < Data.TextureCoordinates.Num(); ++CoordinateIndex)
            {
                Elements.Add(AccessStreamComponent(
                    Data.TextureCoordinates[CoordinateIndex],
                    BaseTexCoordAttribute + CoordinateIndex
                    ));
            }

            for (int32 CoordinateIndex=Data.TextureCoordinates.Num(); CoordinateIndex < MAX_STATIC_TEXCOORDS/2; ++CoordinateIndex)
            {
                Elements.Add(AccessStreamComponent(
                    Data.TextureCoordinates[Data.TextureCoordinates.Num()-1],
                    BaseTexCoordAttribute + CoordinateIndex
                    ));
            }
        }

        if (Data.LightMapCoordinateComponent.VertexBuffer)
        {
            Elements.Add(AccessStreamComponent(Data.LightMapCoordinateComponent, 15));
        }
        else
        if (Data.TextureCoordinates.Num())
        {
            Elements.Add(AccessStreamComponent(Data.TextureCoordinates[0], 15));
        }

        check(Streams.Num() > 0);

        InitDeclaration(Elements);

        check(IsValidRef(GetDeclaration()));

		// Manual vertex fetch is disabled, local vertex factory uniform buffer
        // does not need to be created
        //
        //if (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform))
        //{
        //    UniformBuffer = CreateLocalVFUniformBuffer(this, nullptr);
        //}
    }

    void Init(const FVertexBuffer* VertexBuffer)
    {
        ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
            InitPMUMeshVertexFactory,
            FPMUMeshVertexFactory*, VertexFactory, this,
            const FVertexBuffer*, VertexBuffer, VertexBuffer,
            {
                check(VertexBuffer != nullptr);

                const int32 VertexStride = sizeof(FPMUPackedVertex);

                FLocalVertexFactory::FDataType VertexData;

                VertexData.NumTexCoords = 1;
                VertexData.LightMapCoordinateIndex = 0;

                VertexData.PositionComponent = FVertexStreamComponent(VertexBuffer, STRUCT_OFFSET(FPMUPackedVertex, Position), VertexStride, VET_Float3);
                VertexData.TextureCoordinates.Emplace(
                    VertexBuffer, STRUCT_OFFSET(FPMUPackedVertex, TextureCoordinate), VertexStride, VET_Float2
                    );
                VertexData.TangentBasisComponents[0] = FVertexStreamComponent(VertexBuffer, STRUCT_OFFSET(FPMUPackedVertex, TangentX), VertexStride, VET_PackedNormal);
                VertexData.TangentBasisComponents[1] = FVertexStreamComponent(VertexBuffer, STRUCT_OFFSET(FPMUPackedVertex, TangentZ), VertexStride, VET_PackedNormal);
                VertexData.ColorComponent = FVertexStreamComponent(VertexBuffer, STRUCT_OFFSET(FPMUPackedVertex, Color), VertexStride, VET_Color);

                VertexFactory->SetData(VertexData);
            } );
    }
};

IMPLEMENT_VERTEX_FACTORY_TYPE(FPMUMeshVertexFactory, "/Engine/Private/LocalVertexFactory.ush", true, true, true, true, true);

struct FPMUMeshProxySection
{
    UMaterialInterface* Material;
    FPMUMeshVertexFactory VertexFactory;
	FPMUMeshSectionResourceBuffer Buffer;

    int32 VertexCount;
    int32 IndexCount;
    bool bSectionVisible;

    FPMUMeshProxySection(ERHIFeatureLevel::Type InFeatureLevel)
        : Material(NULL)
        , VertexFactory(InFeatureLevel)
        , bSectionVisible(true)
    {
    }

    FPMUMeshProxySection(ERHIFeatureLevel::Type InFeatureLevel, const FPMUMeshSectionResource& InSection)
        : Material(NULL)
        , VertexFactory(InFeatureLevel)
        , bSectionVisible(InSection.bSectionVisible)
    {
        Buffer.VertexBuffer = InSection.Buffer.VertexBuffer;
        Buffer.IndexBuffer  = InSection.Buffer.IndexBuffer;

        VertexCount = InSection.VertexCount;
        IndexCount  = InSection.IndexCount;
    }

    void InitResources()
    {
        VertexFactory.Init(&Buffer.VertexBuffer);
        BeginInitResource(&Buffer.VertexBuffer);
        BeginInitResource(&Buffer.IndexBuffer);
        BeginInitResource(&VertexFactory);
    }

    void ReleaseResources()
    {
        Buffer.VertexBuffer.ReleaseResource();
        Buffer.IndexBuffer.ReleaseResource();
        VertexFactory.ReleaseResource();
    }

    FORCEINLINE int32 GetVertexCount() const
    {
        return VertexCount;
    }

    FORCEINLINE int32 GetIndexCount() const
    {
        return IndexCount;
    }
};

// Struct used to send update to mesh data
// Arrays may be empty, in which case no update is performed.
struct FPMUMeshSectionUpdateData
{
    TArray<FPMUMeshVertex> NewVertexBuffer;
    int32 TargetSection;
    bool bUsePositionAsUV;
};

static void ConvertPMUMeshToDynMeshVertex(FPMUPackedVertex& DynamicVertex, const FPMUMeshVertex& PMUVertex, const bool bUsePositionAsUV)
{
    DynamicVertex.Position = FVector4(PMUVertex.Position, 0);
    DynamicVertex.Color = PMUVertex.Color;
    DynamicVertex.TextureCoordinate = bUsePositionAsUV ? FVector2D(DynamicVertex.Position) : PMUVertex.UV0;

    // Set packed normal and normal perpendicular vector as tangent
    const FVector& Normal(PMUVertex.Normal);
    DynamicVertex.TangentZ = FVector4(Normal, 1.f);
    DynamicVertex.TangentX = FVector(Normal.Z, Normal.Y, -Normal.X);
}

// Procedural mesh scene proxy
class FPMUMeshSceneProxy : public FPrimitiveSceneProxy
{
public:

    FPMUMeshSceneProxy(UPMUMeshComponent* Component)
        : FPrimitiveSceneProxy(Component)
        , BodySetup(Component->GetBodySetup())
        , MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
    {
        check(IsValid(Component));

        ConstructSectionsFromGeometry(*Component);
        ConstructSectionsFromResource(*Component);
    }

    virtual ~FPMUMeshSceneProxy()
    {
        for (FPMUMeshProxySection* Section : Sections)
        {
            if (Section != nullptr)
            {
                Section->ReleaseResources();
                delete Section;
            }
        }

        for (FPMUMeshProxySection* Section : DirectSections)
        {
            if (Section != nullptr)
            {
                Section->ReleaseResources();
                delete Section;
            }
        }
    }

    void ConstructSectionsFromGeometry(UPMUMeshComponent& Component)
    {
        ERHIFeatureLevel::Type FeatureLevel = GetScene().GetFeatureLevel();

        const int32 NumSections = Component.MeshSections.Num();
        Sections.AddZeroed(NumSections);

        for (int32 SectionIdx = 0; SectionIdx < NumSections; SectionIdx++)
        {
            FPMUMeshSection& SrcSection(Component.MeshSections[SectionIdx]);

            if (SrcSection.IndexBuffer.Num() > 0 && SrcSection.VertexBuffer.Num() > 0)
            {
                // Create new section
                Sections[SectionIdx] = new FPMUMeshProxySection(FeatureLevel);
                FPMUMeshProxySection& DstSection(*Sections[SectionIdx]);

                // Copy data from vertex buffer
                const int32 NumVerts = SrcSection.VertexBuffer.Num();

                // Whether to use vertex uv or position as uv
                const bool bUsePositionAsUV = SrcSection.bUsePositionAsUV;

                // Allocate verts
                DstSection.VertexCount = NumVerts;
                DstSection.Buffer.GetVBArray().SetNumUninitialized(NumVerts);
                // Copy verts
                for (int32 i=0; i<NumVerts; ++i)
                {
                    const FPMUMeshVertex& PMUVertex(SrcSection.VertexBuffer[i]);
                    FPMUPackedVertex& DynamicVertex(DstSection.Buffer.GetVBArray()[i]);
                    ConvertPMUMeshToDynMeshVertex(DynamicVertex, PMUVertex, bUsePositionAsUV);
                }

                // Copy index buffer
                DstSection.IndexCount = SrcSection.IndexBuffer.Num();
                DstSection.Buffer.GetIBArray().SetNumUninitialized(DstSection.IndexCount);
                FMemory::Memcpy(
                    DstSection.Buffer.GetIBArray().GetData(),
                    SrcSection.IndexBuffer.GetData(),
                    SrcSection.IndexBuffer.GetTypeSize() * DstSection.IndexCount
                    );

                // Enqueue initialization of render resource
                DstSection.InitResources();

                // Grab material
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

    void ConstructSectionsFromResource(UPMUMeshComponent& Component)
    {
        ERHIFeatureLevel::Type FeatureLevel = GetScene().GetFeatureLevel();

        const int32 NumSections = Component.SectionResources.Num();
        DirectSections.AddZeroed(NumSections);

        for (int32 SectionIdx = 0; SectionIdx < NumSections; SectionIdx++)
        {
            FPMUMeshSectionResource& SrcSection(Component.SectionResources[SectionIdx]);

            if (SrcSection.GetIndexCount() >= 3 && SrcSection.GetVertexCount() >= 3)
            {
                // Create new section

                DirectSections[SectionIdx] = new FPMUMeshProxySection(FeatureLevel, SrcSection);
                FPMUMeshProxySection& DstSection(*DirectSections[SectionIdx]);

                DstSection.InitResources();

                // Assign material or revert to default if component material for the specified
                // section does not exist

                DstSection.Material = Component.GetMaterial(Sections.Num() + SectionIdx);

                if (DstSection.Material == NULL)
                {
                    DstSection.Material = UMaterial::GetDefaultMaterial(MD_Surface);
                }
            }
        }
    }

    // Called on render thread to assign new dynamic data
    void UpdateSection_RenderThread(FPMUMeshSectionUpdateData* SectionData)
    {
        //SCOPE_CYCLE_COUNTER(STAT_VoxelMesh_UpdateSectionRT);

        check(IsInRenderingThread());

#if 0
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
#endif
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

    virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
    {
        //SCOPE_CYCLE_COUNTER(STAT_VoxelMesh_GetMeshElements);

        // Set up wireframe material (if needed)
        const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

        FColoredMaterialRenderProxy* WireframeMaterialInstance = NULL;
        if (bWireframe)
        {
            WireframeMaterialInstance = new FColoredMaterialRenderProxy(
                GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy(IsSelected()) : NULL,
                FLinearColor(0, 0.5f, 1.f)
            );

            Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
        }

        // Iterate over sections
        for (FPMUMeshProxySection* Section : Sections)
        {
            if (Section != nullptr && Section->bSectionVisible)
            {
                FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : Section->Material->GetRenderProxy(IsSelected());

                for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
                {
                    if (VisibilityMap & (1 << ViewIndex))
                    {
                        const FSceneView* View = Views[ViewIndex];

                        FMeshBatch& Mesh = Collector.AllocateMesh();
                        FMeshBatchElement& BatchElement = Mesh.Elements[0];
                        BatchElement.IndexBuffer = &Section->Buffer.IndexBuffer;
                        Mesh.bWireframe = bWireframe;
                        Mesh.VertexFactory = &Section->VertexFactory;
                        Mesh.MaterialRenderProxy = MaterialProxy;

                        BatchElement.FirstIndex = 0;
                        BatchElement.NumPrimitives = Section->GetIndexCount() / 3;
                        BatchElement.MinVertexIndex = 0;
                        BatchElement.MaxVertexIndex = Section->GetVertexCount() - 1;
                        BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(
                            GetLocalToWorld(),
                            GetBounds(),
                            GetLocalBounds(),
                            true,
                            UseEditorDepthTest()
                            );

                        Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
                        Mesh.Type = PT_TriangleList;
                        Mesh.DepthPriorityGroup = SDPG_World;
                        Mesh.bCanApplyViewModeOverrides = false;

                        if (bWireframe)
                        {
                            Mesh.bDisableBackfaceCulling = true;
                        }

                        Collector.AddMesh(ViewIndex, Mesh);
                    }
                }
            }
        }

        // Iterate over direct sections
        for (FPMUMeshProxySection* Section : DirectSections)
        {
            if (Section != nullptr && Section->bSectionVisible)
            {
                FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : Section->Material->GetRenderProxy(IsSelected());

                for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
                {
                    if (VisibilityMap & (1 << ViewIndex))
                    {
                        const FSceneView* View = Views[ViewIndex];

                        FMeshBatch& Mesh = Collector.AllocateMesh();
                        FMeshBatchElement& BatchElement = Mesh.Elements[0];
                        BatchElement.IndexBuffer = &Section->Buffer.IndexBuffer;
                        Mesh.bWireframe = bWireframe;
                        Mesh.VertexFactory = &Section->VertexFactory;
                        Mesh.MaterialRenderProxy = MaterialProxy;

                        BatchElement.FirstIndex = 0;
                        BatchElement.NumPrimitives = Section->GetIndexCount() / 3;
                        BatchElement.MinVertexIndex = 0;
                        BatchElement.MaxVertexIndex = Section->GetVertexCount() - 1;
                        BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(
                            GetLocalToWorld(),
                            GetBounds(),
                            GetLocalBounds(),
                            true,
                            UseEditorDepthTest()
                            );

                        Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
                        Mesh.Type = PT_TriangleList;
                        Mesh.DepthPriorityGroup = SDPG_World;
                        Mesh.bCanApplyViewModeOverrides = false;

                        if (bWireframe)
                        {
                            Mesh.bDisableBackfaceCulling = true;
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

    virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
    {
        FPrimitiveViewRelevance Result;
        Result.bDrawRelevance = IsShown(View);
        Result.bShadowRelevance = IsShadowCast(View);
        Result.bDynamicRelevance = true;
        Result.bRenderInMainPass = ShouldRenderInMainPass();
        Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
        Result.bRenderCustomDepth = ShouldRenderCustomDepth();
        MaterialRelevance.SetPrimitiveViewRelevance(Result);
        return Result;
    }

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
        return(sizeof(*this) + GetAllocatedSize());
    }

    uint32 GetAllocatedSize(void) const
    {
        return(FPrimitiveSceneProxy::GetAllocatedSize());
    }

private:

    TArray<FPMUMeshProxySection*> Sections;
    TArray<FPMUMeshProxySection*> DirectSections;

    UBodySetup* BodySetup;
    FMaterialRelevance MaterialRelevance;
};

//////////////////////////////////////////////////////////////////////////

UPMUMeshComponent::UPMUMeshComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bUseComplexAsSimpleCollision = true;
    bUseAsyncCooking = false;
}

void UPMUMeshComponent::PostLoad()
{
    Super::PostLoad();

    if (MeshBodySetup && IsTemplate())
    {
        MeshBodySetup->SetFlags(RF_Public);
    }
}

void UPMUMeshComponent::CreateMeshSection(
    int32 SectionIndex,
    const TArray<FVector>& Vertices,
    const TArray<int32>& Triangles,
    const TArray<FVector>& Normals,
    const TArray<FVector2D>& UV0,
    const TArray<FColor>& VertexColors,
    const TArray<FPMUMeshTangent>& Tangents,
    bool bCreateCollision
    )
{
    //SCOPE_CYCLE_COUNTER(STAT_VoxelMesh_CreateMeshSection);

    // Ensure sections array is long enough
    if (SectionIndex >= MeshSections.Num())
    {
        MeshSections.SetNum(SectionIndex + 1, false);
    }

    // Reset this section (in case it already existed)
    FPMUMeshSection& NewSection = MeshSections[SectionIndex];
    NewSection.Reset();

    // Copy data to vertex buffer
    const int32 NumVerts = Vertices.Num();
    NewSection.VertexBuffer.Reset();
    NewSection.VertexBuffer.AddUninitialized(NumVerts);
    for (int32 VertIdx = 0; VertIdx < NumVerts; VertIdx++)
    {
        FPMUMeshVertex& Vertex = NewSection.VertexBuffer[VertIdx];

        Vertex.Position = Vertices[VertIdx];
        Vertex.Normal = (Normals.Num() == NumVerts) ? Normals[VertIdx] : FVector(0.f, 0.f, 1.f);
        //Vertex.UV0 = (UV0.Num() == NumVerts) ? UV0[VertIdx] : FVector2D(0.f, 0.f);
        Vertex.Color = (VertexColors.Num() == NumVerts) ? VertexColors[VertIdx] : FColor(255, 255, 255);
        //Vertex.Tangent = (Tangents.Num() == NumVerts) ? Tangents[VertIdx] : FPMUMeshTangent();

        // Update bounding box
        NewSection.LocalBox += Vertex.Position;
    }

    // Copy index buffer (clamping to vertex range)
    int32 NumTriIndices = Triangles.Num();
    NumTriIndices = (NumTriIndices / 3) * 3; // Ensure we have exact number of triangles (array is multiple of 3 long)

    NewSection.IndexBuffer.Reset();
    NewSection.IndexBuffer.AddUninitialized(NumTriIndices);
    for (int32 IndexIdx = 0; IndexIdx < NumTriIndices; IndexIdx++)
    {
        NewSection.IndexBuffer[IndexIdx] = FMath::Min(Triangles[IndexIdx], NumVerts - 1);
    }

    NewSection.bEnableCollision = bCreateCollision;

    UpdateLocalBounds(); // Update overall bounds
    UpdateCollision(); // Mark collision as dirty
    MarkRenderStateDirty(); // New section requires recreating scene proxy
}

void UPMUMeshComponent::CreateMeshSection_LinearColor(
    int32 SectionIndex,
    const TArray<FVector>& Vertices,
    const TArray<int32>& Triangles,
    const TArray<FVector>& Normals,
    const TArray<FVector2D>& UV0,
    const TArray<FLinearColor>& VertexColors,
    const TArray<FPMUMeshTangent>& Tangents,
    bool bCreateCollision
    )
{
    TArray<FColor> Colors;

    if (VertexColors.Num() > 0)
    {
        Colors.SetNum(VertexColors.Num());

        for (int32 ColorIdx = 0; ColorIdx < VertexColors.Num(); ColorIdx++)
        {
            Colors[ColorIdx] = VertexColors[ColorIdx].ToFColor(false);
        }
    }

    CreateMeshSection(SectionIndex, Vertices, Triangles, Normals, UV0, Colors, Tangents, bCreateCollision);
}

void UPMUMeshComponent::UpdateMeshSection(
    int32 SectionIndex,
    const TArray<FVector>& Vertices,
    const TArray<FVector>& Normals,
    const TArray<FVector2D>& UV0,
    const TArray<FColor>& VertexColors,
    const TArray<FPMUMeshTangent>& Tangents
    )
{
    //SCOPE_CYCLE_COUNTER(STAT_VoxelMesh_UpdateSectionGT);

    if (SectionIndex < MeshSections.Num())
    {
        FPMUMeshSection& Section = MeshSections[SectionIndex];
        const int32 NumVerts = Section.VertexBuffer.Num();

        // See if positions are changing
        const bool bPositionsChanging = (Vertices.Num() == NumVerts);

        // Update bounds, if we are getting new position data
        if (bPositionsChanging)
        {
            Section.LocalBox.Init();
        }

        // Iterate through vertex data, copying in new info
        for (int32 VertIdx = 0; VertIdx < NumVerts; VertIdx++)
        {
            FPMUMeshVertex& ModifyVert = Section.VertexBuffer[VertIdx];

            // Position data
            if (Vertices.Num() == NumVerts)
            {
                ModifyVert.Position = Vertices[VertIdx];
                Section.LocalBox += ModifyVert.Position;
            }

            // Normal data
            if (Normals.Num() == NumVerts)
            {
                ModifyVert.Normal = Normals[VertIdx];
            }

            // Tangent data 
            //if (Tangents.Num() == NumVerts)
            //{
            //    ModifyVert.Tangent = Tangents[VertIdx];
            //}

            // UV data
            //if (UV0.Num() == NumVerts)
            //{
            //    ModifyVert.UV0 = UV0[VertIdx];
            //}

            // Color data
            if (VertexColors.Num() == NumVerts)
            {
                ModifyVert.Color = VertexColors[VertIdx];
            }
        }

        if (SceneProxy)
        {
            // Create data to update section
            FPMUMeshSectionUpdateData* SectionData = new FPMUMeshSectionUpdateData;
            SectionData->TargetSection = SectionIndex;
            SectionData->NewVertexBuffer = Section.VertexBuffer;
            SectionData->bUsePositionAsUV = Section.bUsePositionAsUV;

            // Enqueue command to send to render thread
            ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
                FPMUMeshSectionUpdate,
                FPMUMeshSceneProxy*, MeshSceneProxy, (FPMUMeshSceneProxy*)SceneProxy,
                FPMUMeshSectionUpdateData*, SectionData, SectionData,
                {
                    MeshSceneProxy->UpdateSection_RenderThread(SectionData);
                }
            );
        }

        // If we have collision enabled on this section, update that too
        if (bPositionsChanging && Section.bEnableCollision)
        {
            TArray<FVector> CollisionPositions;

            // We have one collision mesh for all sections, so need to build array of _all_ positions
            for (const FPMUMeshSection& CollisionSection : MeshSections)
            {
                // If section has collision, copy it
                if (CollisionSection.bEnableCollision)
                {
                    for (int32 VertIdx = 0; VertIdx < CollisionSection.VertexBuffer.Num(); VertIdx++)
                    {
                        CollisionPositions.Emplace(CollisionSection.VertexBuffer[VertIdx].Position);
                    }
                }
            }

            // Pass new positions to trimesh
            BodyInstance.UpdateTriMeshVertices(CollisionPositions);
        }

        if (bPositionsChanging)
        {
            UpdateLocalBounds();        // Update overall bounds
            MarkRenderTransformDirty(); // Need to send new bounds to render thread
        }
    }
}

void UPMUMeshComponent::UpdateMeshSection_LinearColor(
    int32 SectionIndex,
    const TArray<FVector>& Vertices,
    const TArray<FVector>& Normals,
    const TArray<FVector2D>& UV0,
    const TArray<FLinearColor>& VertexColors,
    const TArray<FPMUMeshTangent>& Tangents
    )
{
    // Convert FLinearColors to FColors
    TArray<FColor> Colors;
    if (VertexColors.Num() > 0)
    {
        Colors.SetNum(VertexColors.Num());

        for (int32 ColorIdx = 0; ColorIdx < VertexColors.Num(); ColorIdx++)
        {
            Colors[ColorIdx] = VertexColors[ColorIdx].ToFColor(true);
        }
    }

    UpdateMeshSection(SectionIndex, Vertices, Normals, UV0, Colors, Tangents);
}

void UPMUMeshComponent::ClearMeshSectionGeometry(int32 SectionIndex)
{
    if (MeshSections.IsValidIndex(SectionIndex))
    {
        MeshSections[SectionIndex].Reset();
        UpdateLocalBounds();
        UpdateCollision();
        MarkRenderStateDirty();
    }
}

void UPMUMeshComponent::ClearAllMeshSections()
{
    MeshSections.Empty();
    UpdateLocalBounds();
    UpdateCollision();
    MarkRenderStateDirty();
}

void UPMUMeshComponent::SetMeshSectionVisible(int32 SectionIndex, bool bNewVisibility)
{
    if (MeshSections.IsValidIndex(SectionIndex))
    {
        // Set game thread state
        MeshSections[SectionIndex].bSectionVisible = bNewVisibility;

        if (SceneProxy)
        {
            // Enqueue command to modify render thread info
            ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
                FPMUMeshSectionVisibilityUpdate,
                FPMUMeshSceneProxy*, MeshSceneProxy, (FPMUMeshSceneProxy*)SceneProxy,
                int32, SectionIndex, SectionIndex,
                bool, bNewVisibility, bNewVisibility,
                {
                    MeshSceneProxy->SetSectionVisibility_RenderThread(SectionIndex, bNewVisibility);
                }
            );
        }
    }
}

bool UPMUMeshComponent::IsMeshSectionVisible(int32 SectionIndex) const
{
    return MeshSections.IsValidIndex(SectionIndex) ? MeshSections[SectionIndex].bSectionVisible : false;
}

int32 UPMUMeshComponent::GetNumSections() const
{
    return MeshSections.Num();
}

FPMUMeshSection* UPMUMeshComponent::GetMeshSection(int32 SectionIndex)
{
    return MeshSections.IsValidIndex(SectionIndex)
        ? &MeshSections[SectionIndex]
        : nullptr;
}

FPMUMeshSectionResourceRef UPMUMeshComponent::GetSectionResourceRef(int32 SectionIndex)
{
    return SectionResources.IsValidIndex(SectionIndex)
        ? FPMUMeshSectionResourceRef(SectionResources[SectionIndex])
        : FPMUMeshSectionResourceRef();
}

FPMUMeshSection UPMUMeshComponent::CreateSectionFromSectionResource(int32 SectionIndex)
{
    FPMUMeshSection Section;

    if (SectionResources.IsValidIndex(SectionIndex))
    {
        FPMUMeshSectionResource& SectionResource(SectionResources[SectionIndex]);

        const auto& SrcVB(SectionResource.Buffer.GetVBArray());
        const auto& SrcIB(SectionResource.Buffer.GetIBArray());

        auto& DstVB(Section.VertexBuffer);
        auto& DstIB(Section.IndexBuffer);

        DstVB.SetNumUninitialized(SrcVB.Num());

        for (int32 i=0; i<SrcVB.Num(); ++i)
        {
            const FPMUPackedVertex& SrcVert(SrcVB[i]);
            FPMUMeshVertex DstVert;

            DstVert.Position = SrcVert.Position;
            DstVert.Normal   = SrcVert.TangentZ.ToFVector();
            DstVert.UV0      = SrcVert.TextureCoordinate;
            DstVert.Color    = SrcVert.Color;

            DstVB[i] = DstVert;

            Section.LocalBox += DstVert.Position;
        }

        DstIB.SetNumUninitialized(SrcIB.Num());
        FMemory::Memcpy(DstIB.GetData(), SrcIB.GetData(), SrcIB.Num() * SrcIB.GetTypeSize());
    }

    return Section;
}

void UPMUMeshComponent::CreateSectionBuffersFromSectionResource(int32 SectionIndex, TArray<FVector>& Positions, TArray<FVector>& Normals, TArray<int32>& Indices, FBox& LocalBounds)
{
    if (SectionResources.IsValidIndex(SectionIndex))
    {
        FPMUMeshSectionResource& SectionResource(SectionResources[SectionIndex]);

        const auto& SrcVB(SectionResource.Buffer.GetVBArray());
        const auto& SrcIB(SectionResource.Buffer.GetIBArray());

        Positions.SetNumUninitialized(SrcVB.Num());
        Normals.SetNumUninitialized(SrcVB.Num());

        for (int32 i=0; i<SrcVB.Num(); ++i)
        {
            const FPMUPackedVertex& SrcVert(SrcVB[i]);

            Positions[i] = SrcVert.Position;
            Normals[i] = SrcVert.TangentZ.ToFVector();

            LocalBounds += SrcVert.Position;
        }

        Indices.SetNumUninitialized(SrcIB.Num());
        FMemory::Memcpy(Indices.GetData(), SrcIB.GetData(), SrcIB.Num() * SrcIB.GetTypeSize());
    }
}

void UPMUMeshComponent::SetNumMeshSections(int32 SectionCount, bool bAllowShrinking)
{
    if (SectionCount > MeshSections.Num())
    {
        MeshSections.SetNum(SectionCount, bAllowShrinking);
    }
}

void UPMUMeshComponent::SetNumSectionResources(int32 SectionCount, bool bAllowShrinking)
{
    if (SectionCount > SectionResources.Num())
    {
        SectionResources.SetNum(SectionCount, bAllowShrinking);
    }
}

void UPMUMeshComponent::SetMeshSection(int32 SectionIndex, const FPMUMeshSection& Section, bool bUpdateRenderState)
{
    // Invalid section index, abort
    if (SectionIndex < 0)
    {
        return;
    }

    if (SectionIndex >= MeshSections.Num())
    {
        MeshSections.SetNum(SectionIndex + 1, false);
    }

    MeshSections[SectionIndex] = Section;

    if (bUpdateRenderState)
    {
        UpdateLocalBounds();
        UpdateCollision();
        MarkRenderStateDirty();
    }
}

void UPMUMeshComponent::SetSectionResource(int32 SectionIndex, const FPMUMeshSectionResource& Section, bool bUpdateRenderState)
{
    // Invalid section index, abort
    if (SectionIndex < 0)
    {
        return;
    }

    if (SectionIndex >= SectionResources.Num())
    {
        SectionResources.SetNum(SectionIndex + 1, false);
    }

    SectionResources[SectionIndex] = Section;

    if (bUpdateRenderState)
    {
        UpdateLocalBounds();
        UpdateCollision();
        MarkRenderStateDirty();
    }
}

void UPMUMeshComponent::UpdateRenderState()
{
    UpdateLocalBounds();
    UpdateCollision();
    MarkRenderStateDirty();
}

void UPMUMeshComponent::UpdateLocalBounds()
{
    FBox LocalBox(ForceInit);

    for (const FPMUMeshSection& Section : MeshSections)
    {
        LocalBox += Section.LocalBox;
    }

    for (const FPMUMeshSectionResource& Section : SectionResources)
    {
        LocalBox += Section.LocalBounds;
    }

    LocalBounds = LocalBox.IsValid ? FBoxSphereBounds(LocalBox) : FBoxSphereBounds(FVector(0, 0, 0), FVector(0, 0, 0), 0);

    UpdateBounds();
    MarkRenderTransformDirty();
}

void UPMUMeshComponent::SimplifySection(int32 SectionIndex, const FPMUMeshSimplifierOptions& Options)
{
    if (MeshSections.IsValidIndex(SectionIndex))
    {
        FPMUMeshSimplifier Simplifier;
        Simplifier.Simplify(MeshSections[SectionIndex], FVector::ZeroVector, Options);
    }
}

FPrimitiveSceneProxy* UPMUMeshComponent::CreateSceneProxy()
{
    //SCOPE_CYCLE_COUNTER(STAT_VoxelMesh_CreateSceneProxy);

    return new FPMUMeshSceneProxy(this);
}

int32 UPMUMeshComponent::GetNumMaterials() const
{
    return bUseSingleMaterial ? 1 : (MeshSections.Num() + SectionResources.Num());
}

// COLLISION FUNCTIONS

void UPMUMeshComponent::AddCollisionConvexMesh(TArray<FVector> ConvexVerts)
{
    if (ConvexVerts.Num() >= 4)
    {
        // New element
        FKConvexElem NewConvexElem;
        // Copy in vertex info
        NewConvexElem.VertexData = ConvexVerts;
        // Update bounding box
        NewConvexElem.ElemBox = FBox(NewConvexElem.VertexData);
        // Add to array of convex elements
        CollisionConvexElems.Emplace(NewConvexElem);
        // Refresh collision
        UpdateCollision();
    }
}

void UPMUMeshComponent::ClearCollisionConvexMeshes()
{
    // Empty simple collision info
    CollisionConvexElems.Empty();
    // Refresh collision
    UpdateCollision();
}

void UPMUMeshComponent::SetCollisionConvexMeshes(const TArray< TArray<FVector> >& ConvexMeshes)
{
    CollisionConvexElems.Reset();

    // Create element for each convex mesh
    for (int32 ConvexIndex = 0; ConvexIndex < ConvexMeshes.Num(); ConvexIndex++)
    {
        FKConvexElem NewConvexElem;
        NewConvexElem.VertexData = ConvexMeshes[ConvexIndex];
        NewConvexElem.ElemBox = FBox(NewConvexElem.VertexData);

        CollisionConvexElems.Emplace(NewConvexElem);
    }

    UpdateCollision();
}

FBoxSphereBounds UPMUMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
    FBoxSphereBounds Ret(LocalBounds.TransformBy(LocalToWorld));

    Ret.BoxExtent *= BoundsScale;
    Ret.SphereRadius *= BoundsScale;

    return Ret;
}

bool UPMUMeshComponent::GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
{
    int32 VertexBase = 0; // Base vertex index for current section

    // See if we should copy UVs
    bool bCopyUVs = UPhysicsSettings::Get()->bSupportUVFromHitResults;
    if (bCopyUVs)
    {
        // Only one UV channel
        CollisionData->UVs.AddZeroed(1);
    }

    for (int32 SectionIdx=0; SectionIdx < MeshSections.Num(); SectionIdx++)
    {
        FPMUMeshSection& Section = MeshSections[SectionIdx];

        // Construct collision data if required
        if (Section.bEnableCollision)
        {
            // Copy vert data
            for (int32 VertIdx = 0; VertIdx < Section.VertexBuffer.Num(); VertIdx++)
            {
                CollisionData->Vertices.Emplace(Section.VertexBuffer[VertIdx].Position);

                // Copy UV if desired
                if (bCopyUVs)
                {
                    // CollisionData->UVs[0].Emplace(Section.VertexBuffer[VertIdx].UV0);
                    CollisionData->UVs[0].Emplace(Section.VertexBuffer[VertIdx].Position);
                }
            }

            // Copy triangle data
            const int32 NumTriangles = Section.IndexBuffer.Num() / 3;
            for (int32 TriIdx = 0; TriIdx < NumTriangles; TriIdx++)
            {
                // Need to add base offset for indices
                FTriIndices Triangle;
                Triangle.v0 = Section.IndexBuffer[(TriIdx * 3) + 0] + VertexBase;
                Triangle.v1 = Section.IndexBuffer[(TriIdx * 3) + 1] + VertexBase;
                Triangle.v2 = Section.IndexBuffer[(TriIdx * 3) + 2] + VertexBase;
                CollisionData->Indices.Emplace(Triangle);

                // Also store material info
                CollisionData->MaterialIndices.Emplace(SectionIdx);
            }

            // Remember the base index that new verts will be added from in next section
            VertexBase = CollisionData->Vertices.Num();
        }
    }

    CollisionData->bFlipNormals = true;
    CollisionData->bDeformableMesh = true;
    CollisionData->bFastCook = true;

    return true;
}

bool UPMUMeshComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
    for (const FPMUMeshSection& Section : MeshSections)
    {
        if (Section.IndexBuffer.Num() >= 3 && Section.bEnableCollision)
        {
            return true;
        }
    }

    return false;
}

UBodySetup* UPMUMeshComponent::CreateBodySetupHelper()
{
    // The body setup in a template needs to be public since the property is instanced
    // and thus is the archetype of the instance meaning there is a direct reference
    UBodySetup* NewBodySetup = NewObject<UBodySetup>(this, NAME_None, (IsTemplate() ? RF_Public : RF_NoFlags));
    NewBodySetup->BodySetupGuid = FGuid::NewGuid();

    NewBodySetup->bGenerateMirroredCollision = false;
    NewBodySetup->bDoubleSidedGeometry = true;
    NewBodySetup->CollisionTraceFlag = bUseComplexAsSimpleCollision ? CTF_UseComplexAsSimple : CTF_UseDefault;

    return NewBodySetup;
}

void UPMUMeshComponent::CreateMeshBodySetup()
{
    if (MeshBodySetup == nullptr)
    {
        MeshBodySetup = CreateBodySetupHelper();
    }
}

void UPMUMeshComponent::UpdateCollision()
{
    //SCOPE_CYCLE_COUNTER(STAT_VoxelMesh_UpdateCollision);

    UWorld* World = GetWorld();
    const bool bUseAsyncCook = World && World->IsGameWorld() && bUseAsyncCooking;

    if (bUseAsyncCook)
    {
        AsyncBodySetupQueue.Emplace(CreateBodySetupHelper());
    }
    else
    {
        // If for some reason we modified the async at runtime,
        // just clear any pending async body setups
        AsyncBodySetupQueue.Empty();
        CreateMeshBodySetup();
    }

    UBodySetup* UseBodySetup = bUseAsyncCook ? AsyncBodySetupQueue.Last() : MeshBodySetup;

    // Fill in simple collision convex elements
    UseBodySetup->AggGeom.ConvexElems = CollisionConvexElems;

    // Set trace flag
    UseBodySetup->CollisionTraceFlag = bUseComplexAsSimpleCollision ? CTF_UseComplexAsSimple : CTF_UseDefault;

    if (bUseAsyncCook)
    {
        UseBodySetup->CreatePhysicsMeshesAsync(FOnAsyncPhysicsCookFinished::CreateUObject(this, &UPMUMeshComponent::FinishPhysicsAsyncCook, UseBodySetup));
    }
    else
    {
        // New GUID as collision has changed
        UseBodySetup->BodySetupGuid = FGuid::NewGuid();
        // Also we want cooked data for this
        UseBodySetup->bHasCookedCollisionData = true;
        UseBodySetup->InvalidatePhysicsData();
        UseBodySetup->CreatePhysicsMeshes();
        RecreatePhysicsState();
    }
}

void UPMUMeshComponent::FinishPhysicsAsyncCook(bool bSuccess, UBodySetup* FinishedBodySetup)
{
	TArray<UBodySetup*> NewQueue;
	NewQueue.Reserve(AsyncBodySetupQueue.Num());

	int32 FoundIdx;
	if (AsyncBodySetupQueue.Find(FinishedBodySetup, FoundIdx))
	{
		if (bSuccess)
		{
			//The new body was found in the array meaning it's newer so use it
			MeshBodySetup = FinishedBodySetup;
			RecreatePhysicsState();

			//remove any async body setups that were requested before this one
			for (int32 AsyncIdx = FoundIdx + 1; AsyncIdx < AsyncBodySetupQueue.Num(); ++AsyncIdx)
			{
				NewQueue.Add(AsyncBodySetupQueue[AsyncIdx]);
			}

			AsyncBodySetupQueue = NewQueue;
		}
		else
		{
			AsyncBodySetupQueue.RemoveAt(FoundIdx);
		}
	}
}

UBodySetup* UPMUMeshComponent::GetBodySetup()
{
    CreateMeshBodySetup();
    return MeshBodySetup;
}

UMaterialInterface* UPMUMeshComponent::GetMaterial(int32 ElementIndex) const
{
    return UMeshComponent::GetMaterial(bUseSingleMaterial ? 0 : ElementIndex);
}

UMaterialInterface* UPMUMeshComponent::GetMaterialFromCollisionFaceIndex(int32 FaceIndex, int32& SectionIndex) const
{
    UMaterialInterface* Result = nullptr;
    SectionIndex = 0;

    // Look for element that corresponds to the supplied face
    int32 TotalFaceCount = 0;
    for (int32 SectionIdx = 0; SectionIdx < MeshSections.Num(); SectionIdx++)
    {
        const FPMUMeshSection& Section = MeshSections[SectionIdx];
        int32 NumFaces = Section.IndexBuffer.Num() / 3;
        TotalFaceCount += NumFaces;

        if (FaceIndex < TotalFaceCount)
        {
            // Grab the material
            Result = GetMaterial(SectionIdx);
            SectionIndex = SectionIdx;
            break;
        }
    }

    return Result;
}
