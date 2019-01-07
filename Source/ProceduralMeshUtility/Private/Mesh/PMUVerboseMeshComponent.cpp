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

#include "PMUVerboseMeshComponent.h"

#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "Stats/Stats.h"
#include "SceneManagement.h"

#include "MaterialShared.h"
#include "Materials/Material.h"

#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsSettings.h"

#include "VertexFactory.h"
#include "Containers/ResourceArray.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "RenderResource.h"
#include "LocalVertexFactory.h"
#include "StaticMeshVertexBuffer.h"
#include "PositionVertexBuffer.h"
#include "DynamicMeshBuilder.h"

template<class FVertexType>
class FPMUVMeshVertexResourceArray : public TResourceArray<FVertexType, VERTEXBUFFER_ALIGNMENT>
{
public:

    typedef TResourceArray<FVertexType, VERTEXBUFFER_ALIGNMENT> FArrayType;

    FPMUVMeshVertexResourceArray(bool bInAllowCPUAccess = false)
        : FArrayType(bInAllowCPUAccess)
    {
    }

    virtual void Discard() override
    {
        // TResourceArray<>::Discard() only discard array content if not in editor or commandlet,
        // override to always discard if cpu access is not required
        if (! GetAllowCPUAccess())
        {
            Empty();
        }
    }
};

template<class FVertexType>
class FPMUVMeshVertexBuffer : public FVertexBuffer
{
public:

    FPMUVMeshVertexResourceArray<FVertexType> Vertices;
    int32 VertexCount = -1;

    FPMUVMeshVertexBuffer(bool bInAllowCPUAccess = false)
        : Vertices(bInAllowCPUAccess)
    {
    }

    virtual void InitRHI() override
    {
        VertexCount = Vertices.Num();

        FRHIResourceCreateInfo CreateInfo(&Vertices);
        VertexBufferRHI = RHICreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static, CreateInfo);
    }

    FORCEINLINE bool GetAllowCPUAccess(bool bEnabled) const
    {
        return Vertices.GetAllowCPUAccess();
    }

    FORCEINLINE void SetAllowCPUAccess(bool bEnabled)
    {
        Vertices.SetAllowCPUAccess(bEnabled);
    }

    FORCEINLINE int32 GetVertexCount() const
    {
        // Return assigned vertex count if vertex resource cpu access is not required
        // and vertex buffer RHI is valid since vertex resource content might have already been discarded
        return (!Vertices.GetAllowCPUAccess() && VertexBufferRHI.IsValid()) ? VertexCount : Vertices.Num();
    }

    FORCEINLINE int32 GetStride() const
    {
        return sizeof(FVertexType);
    }
};

class FPMUVMeshIndexBuffer : public FIndexBuffer
{
public:

    TArray<int32> Indices;
    int32 IndexCount;
    bool bAllowCPUAccess;

    FPMUVMeshIndexBuffer(bool bInAllowCPUAccess = false)
        : IndexCount(0)
        , bAllowCPUAccess(bInAllowCPUAccess)
    {
    }

    virtual void InitRHI() override
    {
        FRHIResourceCreateInfo CreateInfo;

        IndexCount = Indices.Num();

        // Create and write index buffer
        const int32 TypeSize = Indices.GetTypeSize();
        void* Buffer = nullptr;
        IndexBufferRHI = RHICreateAndLockIndexBuffer(TypeSize, IndexCount * TypeSize, BUF_Static, CreateInfo, Buffer);
        FMemory::Memcpy(Buffer, Indices.GetData(), IndexCount * TypeSize);
        RHIUnlockIndexBuffer(IndexBufferRHI);

        if (! bAllowCPUAccess)
        {
            Indices.Empty();
        }
    }

    FORCEINLINE bool GetAllowCPUAccess(bool bEnabled) const
    {
        return bAllowCPUAccess;
    }

    FORCEINLINE void SetAllowCPUAccess(bool bEnabled)
    {
        bAllowCPUAccess = bEnabled;
    }

    FORCEINLINE int32 GetIndexCount() const
    {
        // Return assigned index count if index resource cpu access is not required
        // and index buffer RHI is valid since vertex resource content might have already been discarded
        return (!bAllowCPUAccess && IndexBufferRHI.IsValid()) ? IndexCount : Indices.Num();
    }
};

struct FPMUVMeshSectionProxy
{
    UMaterialInterface* Material;

    FPMUVMeshVertexBuffer<FVector> PositionBuffer;
    FPMUVMeshVertexBuffer<FPackedNormal> NormalBuffer;
    FPMUVMeshVertexBuffer<FPackedNormal> TangentBuffer;
    FPMUVMeshVertexBuffer<FVector2D> UVBuffer;
    FPMUVMeshVertexBuffer<FColor> ColorBuffer;

    FPMUVMeshIndexBuffer IndexBuffer;
    FLocalVertexFactory VertexFactory;

    bool bSectionVisible;
    bool bAllowCPUAccess;

    FPMUVMeshSectionProxy(ERHIFeatureLevel::Type InFeatureLevel, bool bAllowCPUAccess)
        : Material(nullptr)
        , PositionBuffer(bAllowCPUAccess)
        , NormalBuffer(bAllowCPUAccess)
        , TangentBuffer(bAllowCPUAccess)
        , UVBuffer(bAllowCPUAccess)
        , ColorBuffer(bAllowCPUAccess)
        , IndexBuffer(bAllowCPUAccess)
    {
        VertexFactory.SetFeatureLevel(InFeatureLevel);
    }

    void InitVertexFactory()
    {
		if (IsInRenderingThread())
		{
            FLocalVertexFactory::FDataType Data;
            Data.PositionComponent = FVertexStreamComponent(&PositionBuffer, 0, PositionBuffer.GetStride(), VET_Float3);
            Data.TextureCoordinates.Add(
                FVertexStreamComponent(&UVBuffer, 0, UVBuffer.GetStride(), VET_Float2)
                );
            Data.TangentBasisComponents[0] = FVertexStreamComponent(&TangentBuffer, 0, TangentBuffer.GetStride(), VET_PackedNormal);
            Data.TangentBasisComponents[1] = FVertexStreamComponent(&NormalBuffer, 0, NormalBuffer.GetStride(), VET_PackedNormal);
            Data.ColorComponent = FVertexStreamComponent(&ColorBuffer, 0, ColorBuffer.GetStride(), VET_Color);
            VertexFactory.SetData(Data);
		}
		else
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				FPMUVMeshSectionProxy_InitVertexFactory,
				FPMUVMeshSectionProxy*, Section, this,
				{
                    Section->InitVertexFactory();
				} );
		}
    }

    void InitResource()
    {
        InitVertexFactory();

        BeginInitResource(&PositionBuffer);
        BeginInitResource(&NormalBuffer);
        BeginInitResource(&TangentBuffer);
        BeginInitResource(&UVBuffer);
        BeginInitResource(&ColorBuffer);
        BeginInitResource(&IndexBuffer);
        BeginInitResource(&VertexFactory);
    }

    void ReleaseResource()
    {
        PositionBuffer.ReleaseResource();
        NormalBuffer.ReleaseResource();
        TangentBuffer.ReleaseResource();
        UVBuffer.ReleaseResource();
        ColorBuffer.ReleaseResource();
        IndexBuffer.ReleaseResource();
        VertexFactory.ReleaseResource();
    }
};

//static void ConvertProcMeshToDynMeshVertex(FDynamicMeshVertex& Vert, const FProcMeshVertex& ProcVert)
//{
//    Vert.Position = ProcVert.Position;
//    Vert.Color = ProcVert.Color;
//    Vert.TextureCoordinate[0] = ProcVert.UV0;
//    Vert.TextureCoordinate[1] = ProcVert.UV1;
//    Vert.TextureCoordinate[2] = ProcVert.UV2;
//    Vert.TextureCoordinate[3] = ProcVert.UV3;
//    Vert.TangentX = ProcVert.Tangent.TangentX;
//    Vert.TangentZ = ProcVert.Normal;
//    Vert.TangentZ.Vector.W = ProcVert.Tangent.bFlipTangentY ? -127 : 127;
//}

/** Procedural mesh scene proxy */
class FPMUVMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:

    FPMUVMeshSceneProxy(UPMUVerboseMeshComponent* Component)
        : FPrimitiveSceneProxy(Component)
        , BodySetup(Component->GetBodySetup())
        , MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
    {
        const ERHIFeatureLevel::Type FeatureLevel = GetScene().GetFeatureLevel();

        const int32 NumSections = Component->MeshSections.Num();
        Sections.AddZeroed(NumSections);

        for (int SectionIdx=0; SectionIdx < NumSections; SectionIdx++)
        {
            const FPMUMeshSectionBufferData& SrcSection(Component->MeshSections[SectionIdx]);

            // Skip incomplete buffer sections
            if (! SrcSection.HasCompleteBuffers())
            {
                continue;
            }

            FPMUVMeshSectionProxy* DstSection = new FPMUVMeshSectionProxy(FeatureLevel, false);
            const int32 VertexCount = SrcSection.GetVertexCount();
            const int32 IndexCount = SrcSection.GetIndexCount();

            // Copy vertices

            auto& PositionBuffer(DstSection->PositionBuffer.Vertices);
            auto& UVBuffer(DstSection->UVBuffer.Vertices);
            auto& ColorBuffer(DstSection->ColorBuffer.Vertices);
            auto& NormalBuffer(DstSection->NormalBuffer.Vertices);
            auto& TangentBuffer(DstSection->TangentBuffer.Vertices);
            auto& IndexBuffer(DstSection->IndexBuffer.Indices);

            PositionBuffer.SetNumUninitialized(VertexCount);
            UVBuffer.SetNumUninitialized(VertexCount);
            ColorBuffer.SetNumUninitialized(VertexCount);
            NormalBuffer.SetNumUninitialized(VertexCount);
            TangentBuffer.SetNumUninitialized(VertexCount);
            IndexBuffer.AddZeroed(IndexCount);

            for (int32 i=0; i<VertexCount; ++i)
            {
                PositionBuffer[i] = SrcSection.PositionBuffer[i];
                UVBuffer[i] = SrcSection.UVBuffer[i];
                ColorBuffer[i] = SrcSection.ColorBuffer[i];
                NormalBuffer[i] = SrcSection.NormalBuffer[i];
                TangentBuffer[i] = SrcSection.TangentBuffer[i];
            }

            for (int32 i=0; i<IndexCount; ++i)
            {
                IndexBuffer[i] = SrcSection.IndexBuffer[i];
            }

            // Enqueue initialization of render resource

            DstSection->InitResource();

            // Assign material

            DstSection->Material = Component->GetMaterial(SectionIdx);

            if (! DstSection->Material)
            {
                DstSection->Material = UMaterial::GetDefaultMaterial(MD_Surface);
            }

            // Copy visibility info
            DstSection->bSectionVisible = SrcSection.bSectionVisible;

            // Assign new section
            Sections[SectionIdx] = DstSection;
        }
    }

    virtual ~FPMUVMeshSceneProxy()
    {
        for (FPMUVMeshSectionProxy* Section : Sections)
        {
            if (Section != nullptr)
            {
                Section->ReleaseResource();
                delete Section;
            }
        }
    }

    virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
    {
        //SCOPE_CYCLE_COUNTER(STAT_ProcMesh_GetMeshElements);

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
        for (const FPMUVMeshSectionProxy* Section : Sections)
        {
            if (Section && Section->bSectionVisible)
            {
                FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : Section->Material->GetRenderProxy(IsSelected());

                // For each view..
                for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
                {
                    if (VisibilityMap & (1 << ViewIndex))
                    {
                        const FSceneView* View = Views[ViewIndex];
                        // Draw the mesh.
                        FMeshBatch& Mesh = Collector.AllocateMesh();
                        FMeshBatchElement& BatchElement = Mesh.Elements[0];
                        BatchElement.IndexBuffer = &Section->IndexBuffer;
                        Mesh.bWireframe = bWireframe;
                        Mesh.VertexFactory = &Section->VertexFactory;
                        Mesh.MaterialRenderProxy = MaterialProxy;
                        BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, UseEditorDepthTest());
                        BatchElement.FirstIndex = 0;
                        BatchElement.NumPrimitives = Section->IndexBuffer.GetIndexCount() / 3;
                        BatchElement.MinVertexIndex = 0;
                        BatchElement.MaxVertexIndex = Section->PositionBuffer.GetVertexCount() - 1;
                        Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
                        Mesh.Type = PT_TriangleList;
                        Mesh.DepthPriorityGroup = SDPG_World;
                        Mesh.bCanApplyViewModeOverrides = false;
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

    virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const
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

    virtual uint32 GetMemoryFootprint() const override
    {
        return (sizeof(*this) + GetAllocatedSize());
    }

    uint32 GetAllocatedSize(void) const
    {
        return(FPrimitiveSceneProxy::GetAllocatedSize());
    }

private:

    TArray<FPMUVMeshSectionProxy*> Sections;
    UBodySetup* BodySetup;
    FMaterialRelevance MaterialRelevance;
};

//////////////////////////////////////////////////////////////////////////

UPMUVerboseMeshComponent::UPMUVerboseMeshComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bUseComplexAsSimpleCollision = true;
    bUseAsyncCooking = false;
}

void UPMUVerboseMeshComponent::PostLoad()
{
    Super::PostLoad();

    if (MeshBodySetup && IsTemplate())
    {
        MeshBodySetup->SetFlags(RF_Public);
    }
}

// SECTION FUNCTIONS

int32 UPMUVerboseMeshComponent::GetNumSections() const
{
    return MeshSections.Num();
}

void UPMUVerboseMeshComponent::ClearMeshSectionGeometry(int32 SectionIndex)
{
    if (SectionIndex < MeshSections.Num())
    {
        MeshSections[SectionIndex].Empty();
        UpdateLocalBounds();
        UpdateCollision();
        MarkRenderStateDirty();
    }
}

void UPMUVerboseMeshComponent::ClearAllMeshSections()
{
    MeshSections.Empty();
    UpdateLocalBounds();
    UpdateCollision();
    MarkRenderStateDirty();
}

bool UPMUVerboseMeshComponent::IsMeshSectionVisible(int32 SectionIndex) const
{
    return MeshSections.IsValidIndex(SectionIndex)
        ? MeshSections[SectionIndex].bSectionVisible
        : false;
}

FPMUMeshSectionBufferData* UPMUVerboseMeshComponent::GetMeshSection(int32 SectionIndex)
{
    return MeshSections.IsValidIndex(SectionIndex)
        ? &MeshSections[SectionIndex]
        : nullptr;
}

void UPMUVerboseMeshComponent::SetNumMeshSections(int32 SectionCount)
{
    // Ensure sections array is long enough
    if (SectionCount > MeshSections.Num())
    {
        MeshSections.SetNum(SectionCount, false);
    }
}

void UPMUVerboseMeshComponent::SetMeshSection(int32 SectionIndex, const FPMUMeshSectionBufferData& Section, bool bUpdateRenderState)
{
    // Invalid section index, abort
    if (SectionIndex < 0)
    {
        return;
    }

    // Ensure sections array is long enough
    if (SectionIndex >= MeshSections.Num())
    {
        MeshSections.SetNum(SectionIndex + 1, false);
    }

    MeshSections[SectionIndex] = Section;

    if (bUpdateRenderState)
    {
        UpdateLocalBounds();    // Update overall bounds
        UpdateCollision();      // Mark collision as dirty
        MarkRenderStateDirty(); // New section requires recreating scene proxy
    }
}

void UPMUVerboseMeshComponent::UpdateRenderState()
{
    UpdateLocalBounds();    // Update overall bounds
    UpdateCollision();      // Mark collision as dirty
    MarkRenderStateDirty(); // New section requires recreating scene proxy
}

void UPMUVerboseMeshComponent::SimplifySection(int32 SectionIndex, const FPMUMeshSimplifierOptions& Options)
{
    if (MeshSections.IsValidIndex(SectionIndex))
    {
        FPMUMeshSimplifier Simplifier;
        //Simplifier.Simplify(MeshSections[SectionIndex], FVector::ZeroVector, Options);
    }
}

void UPMUVerboseMeshComponent::UpdateLocalBounds()
{
    FBox LocalBox(ForceInit);

    for (const FPMUMeshSectionBufferData& Section : MeshSections)
    {
        LocalBox += Section.Bounds;
    }

    LocalBounds = LocalBox.IsValid ? FBoxSphereBounds(LocalBox) : FBoxSphereBounds(FVector(0, 0, 0), FVector(0, 0, 0), 0);

    UpdateBounds();
    MarkRenderTransformDirty();
}

FPrimitiveSceneProxy* UPMUVerboseMeshComponent::CreateSceneProxy()
{
    //SCOPE_CYCLE_COUNTER(STAT_VoxelMesh_CreateSceneProxy);

    return new FPMUVMeshSceneProxy(this);
}

int32 UPMUVerboseMeshComponent::GetNumMaterials() const
{
    return bUseSingleMaterial ? 1 : MeshSections.Num();
}

FBoxSphereBounds UPMUVerboseMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
    FBoxSphereBounds Ret(LocalBounds.TransformBy(LocalToWorld));

    Ret.BoxExtent *= BoundsScale;
    Ret.SphereRadius *= BoundsScale;

    return Ret;
}

UMaterialInterface* UPMUVerboseMeshComponent::GetMaterial(int32 ElementIndex) const
{
    return UMeshComponent::GetMaterial(bUseSingleMaterial ? 0 : ElementIndex);
}

UMaterialInterface* UPMUVerboseMeshComponent::GetMaterialFromCollisionFaceIndex(int32 FaceIndex, int32& SectionIndex) const
{
    UMaterialInterface* Result = nullptr;
    SectionIndex = 0;

    // Look for element that corresponds to the supplied face
    int32 TotalFaceCount = 0;
    for (int32 SectionIdx = 0; SectionIdx < MeshSections.Num(); SectionIdx++)
    {
        const FPMUMeshSectionBufferData& Section = MeshSections[SectionIdx];
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

// COLLISION FUNCTIONS

void UPMUVerboseMeshComponent::AddCollisionConvexMesh(TArray<FVector> ConvexVerts)
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

void UPMUVerboseMeshComponent::ClearCollisionConvexMeshes()
{
    // Empty simple collision info
    CollisionConvexElems.Empty();

    // Refresh collision
    UpdateCollision();
}

void UPMUVerboseMeshComponent::SetCollisionConvexMeshes(const TArray< TArray<FVector> >& ConvexMeshes)
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

bool UPMUVerboseMeshComponent::GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
{
    int32 VertexBase = 0; // Base vertex index for current section

    // See if we should copy UVs
    bool bCopyUVs = UPhysicsSettings::Get()->bSupportUVFromHitResults;
    if (bCopyUVs)
    {
        // Only one UV channel
        CollisionData->UVs.AddZeroed(1);
    }

    for (int32 SectionIdx = 0; SectionIdx < MeshSections.Num(); SectionIdx++)
    {
        FPMUMeshSectionBufferData& Section = MeshSections[SectionIdx];

        // Construct collision data if required
        if (Section.bEnableCollision)
        {
            // Copy vert data
            for (int32 VertIdx=0; VertIdx < Section.GetVertexCount(); VertIdx++)
            {
                const FVector& Position(Section.PositionBuffer[VertIdx]);

                CollisionData->Vertices.Emplace(Position);

                // Copy UV if desired
                if (bCopyUVs)
                {
                    // Copy UV buffer if available
                    if (Section.HasUVBuffer())
                    {
                        CollisionData->UVs[0].Emplace(Section.UVBuffer[VertIdx]);
                    }
                    // Otherwise, use position buffer
                    else
                    {
                        CollisionData->UVs[0].Emplace(Position);
                    }
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

bool UPMUVerboseMeshComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
    for (const FPMUMeshSectionBufferData& Section : MeshSections)
    {
        if (Section.IndexBuffer.Num() >= 3 && Section.bEnableCollision)
        {
            return true;
        }
    }

    return false;
}

UBodySetup* UPMUVerboseMeshComponent::CreateBodySetupHelper()
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

void UPMUVerboseMeshComponent::CreateMeshBodySetup()
{
    if (MeshBodySetup == nullptr)
    {
        MeshBodySetup = CreateBodySetupHelper();
    }
}

void UPMUVerboseMeshComponent::UpdateCollision()
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
        UseBodySetup->CreatePhysicsMeshesAsync(FOnAsyncPhysicsCookFinished::CreateUObject(this, &UPMUVerboseMeshComponent::FinishPhysicsAsyncCook, UseBodySetup));
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

void UPMUVerboseMeshComponent::FinishPhysicsAsyncCook(UBodySetup* FinishedBodySetup)
{
    TArray<UBodySetup*> NewQueue;
    NewQueue.Reserve(AsyncBodySetupQueue.Num());

    int32 FoundIdx;
    if (AsyncBodySetupQueue.Find(FinishedBodySetup, FoundIdx))
    {
        // The new body was found in the array meaning it's newer so use it
        MeshBodySetup = FinishedBodySetup;
        RecreatePhysicsState();

        // Remove any async body setups that were requested before this one
        for (int32 AsyncIdx = FoundIdx + 1; AsyncIdx < AsyncBodySetupQueue.Num(); ++AsyncIdx)
        {
            NewQueue.Emplace(AsyncBodySetupQueue[AsyncIdx]);
        }

        AsyncBodySetupQueue = NewQueue;
    }
}

UBodySetup* UPMUVerboseMeshComponent::GetBodySetup()
{
    CreateMeshBodySetup();
    return MeshBodySetup;
}
