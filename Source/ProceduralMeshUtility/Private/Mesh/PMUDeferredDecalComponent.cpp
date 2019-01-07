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

#include "PMUDeferredDecalComponent.h"

#include "EngineGlobals.h"
#include "Containers/ResourceArray.h"
#include "Engine/Engine.h"
#include "Engine/CollisionProfile.h"
#include "Stats/Stats.h"

#include "RenderResource.h"
#include "RenderingThread.h"
#include "SceneManagement.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"

#include "VertexFactory.h"
#include "LocalVertexFactory.h"
#include "PositionVertexBuffer.h"
#include "StaticMeshVertexBuffer.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "DynamicMeshBuilder.h"

struct FPMUDeferredDecalVertex
{
    FVector   Position;
    FColor    Color;
    FVector2D LineStart;
    FVector2D LineEnd;
    FVector4  LineAttribute;
};

struct FPMUDeferredDecalInstancedVertex
{
    FVector Position;
};

struct FPMUDeferredDecalLineAttribute
{
    FColor    LineColor;
    FVector2D LineStart;
    FVector2D LineEnd;
    FVector4  LineAttribute;
};

class FPMUDeferredDecalVertexResourceArray : public FResourceArrayInterface
{
public:

    FPMUDeferredDecalVertexResourceArray(void* InData, uint32 InSize)
        : Data(InData)
        , Size(InSize)
    {
    }

    virtual const void* GetResourceData() const override { return Data; }
    virtual uint32 GetResourceDataSize() const override { return Size; }
    virtual void Discard() override {}
    virtual bool IsStatic() const override { return false; }
    virtual bool GetAllowCPUAccess() const override { return false; }
    virtual void SetAllowCPUAccess(bool bInNeedsCPUAccess) override {}

private:

    void* Data;
    uint32 Size;
};

class FPMUDeferredDecalVertexBuffer : public FVertexBuffer
{
public:

    TArray<FPMUDeferredDecalVertex> Vertices;

    virtual void InitRHI() override
    {
        const uint32 SizeInBytes = Vertices.Num() * Vertices.GetTypeSize();

        FPMUDeferredDecalVertexResourceArray ResourceArray(Vertices.GetData(), SizeInBytes);
        FRHIResourceCreateInfo CreateInfo(&ResourceArray);
        VertexBufferRHI = RHICreateVertexBuffer(SizeInBytes, BUF_Static, CreateInfo);
    }

};

class FPMUDeferredDecalInstancedVertexBuffer : public FVertexBuffer
{
public:

    TArray<FPMUDeferredDecalInstancedVertex> Vertices;

    virtual void InitRHI() override
    {
        const uint32 SizeInBytes = Vertices.Num() * Vertices.GetTypeSize();

        FPMUDeferredDecalVertexResourceArray ResourceArray(Vertices.GetData(), SizeInBytes);
        FRHIResourceCreateInfo CreateInfo(&ResourceArray);
        VertexBufferRHI = RHICreateVertexBuffer(SizeInBytes, BUF_Static, CreateInfo);
    }

};

class FPMUDeferredDecalLineAttributeBuffer : public FVertexBuffer
{
public:

    TArray<FPMUDeferredDecalLineAttribute> Instances;

    virtual void InitRHI() override
    {
        const uint32 SizeInBytes = Instances.Num() * Instances.GetTypeSize();

        FPMUDeferredDecalVertexResourceArray ResourceArray(Instances.GetData(), SizeInBytes);
        FRHIResourceCreateInfo CreateInfo(&ResourceArray);
        VertexBufferRHI = RHICreateVertexBuffer(SizeInBytes, BUF_Static, CreateInfo);
    }
};

class FPMUDeferredDecalIndexBuffer : public FIndexBuffer
{
public:

    TArray<int32> Indices;

    virtual void InitRHI() override
    {
        FRHIResourceCreateInfo CreateInfo;
        const int32 TypeSize = Indices.GetTypeSize();
        const int32 SizeInBytes  = Indices.Num() * TypeSize;
        void* Buffer = nullptr;
        IndexBufferRHI = RHICreateAndLockIndexBuffer(TypeSize, SizeInBytes, BUF_Static, CreateInfo, Buffer);

        // Write the indices to the index buffer.       
        FMemory::Memcpy(Buffer, Indices.GetData(), SizeInBytes);
        RHIUnlockIndexBuffer(IndexBufferRHI);
    }
};

struct FPMUDeferredDecalVertexFactory : public FLocalVertexFactory
{
    DECLARE_VERTEX_FACTORY_TYPE(FPMUDeferredDecalVertexFactory);

public:

    struct FPMUDDDataType
    {
        //FVertexStreamComponent InstanceOriginComponent;
        //FVertexStreamComponent InstanceTransformComponent[3];
        //FVertexStreamComponent InstanceLightmapAndShadowMapUVBiasComponent;

        FVertexStreamComponent PositionComponent;
        FVertexStreamComponent ColorComponent;
        FVertexStreamComponent LineStartComponent;
        FVertexStreamComponent LineEndComponent;
        FVertexStreamComponent LineAttributeComponent;
    };

    FPMUDeferredDecalVertexFactory() = default;

    //static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
    //{
    //    return IsTranslucentBlendMode(Material->GetBlendMode())
    //        && FLocalVertexFactory::ShouldCache(Platform, Material, ShaderType);
    //}

    static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
    {
        OutEnvironment.SetDefine(TEXT("MIN_MATERIAL_TEXCOORDS"),TEXT("7"));
        FLocalVertexFactory::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
    }

    //static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

    // FRenderResource interface.
    virtual void InitRHI() override
    {
        FVertexDeclarationElementList Elements;

        // Register position component
        if (Data.PositionComponent.VertexBuffer)
        {
            Elements.Add(AccessStreamComponent(Data.PositionComponent,0));
        }

        // Register color component
        if (Data.ColorComponent.VertexBuffer)
        {
            Elements.Add(AccessStreamComponent(Data.ColorComponent,1));
        }
        else
        {
            //If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
            //This wastes 4 bytes of bandwidth per vertex, but prevents having to compile out twice the number of vertex factories.
            FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color);
            Elements.Add(AccessStreamComponent(NullColorComponent,1));
        }

        // Register position component
        if (Data.LineStartComponent.VertexBuffer)
        {
            Elements.Add(AccessStreamComponent(Data.LineStartComponent,2));
        }

        if (Data.LineEndComponent.VertexBuffer)
        {
            Elements.Add(AccessStreamComponent(Data.LineEndComponent,3));
        }

        if (Data.LineAttributeComponent.VertexBuffer)
        {
            Elements.Add(AccessStreamComponent(Data.LineAttributeComponent,4));
        }

        InitDeclaration(Elements);
    }

    void SetData(const FPMUDDDataType& InData)
    {
        Data = InData;
        UpdateRHI();
    }

    void Copy(const FPMUDeferredDecalVertexFactory& Other)
    {
        ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
            FPMUDeferredDecalVertexFactoryCopyData,
            FPMUDeferredDecalVertexFactory*, VertexFactory, this,
            const FPMUDDDataType*, DataCopy, &Other.Data,
        {
            VertexFactory->Data = *DataCopy;
        });
        BeginUpdateResourceRHI(this);
    }

    // Make sure we account for changes in the signature of GetStaticBatchElementVisibility()
    //static CONSTEXPR uint32 NumBitsForVisibilityMask()
    //{
    //    return 8 * sizeof(decltype(((FInstancedStaticMeshVertexFactory*)nullptr)->GetStaticBatchElementVisibility(FSceneView(FSceneViewInitOptions()), nullptr)));
    //}

    // Get a bitmask representing the visibility of each FMeshBatch element
    //virtual uint64 GetStaticBatchElementVisibility(const class FSceneView& View, const struct FMeshBatch* Batch) const override
    //{
    //    const uint32 NumBits = NumBitsForVisibilityMask();
    //    const uint32 NumElements = FMath::Min((uint32)Batch->Elements.Num(), NumBits);
    //    return NumElements == NumBits ? ~0ULL : (1ULL << (uint64)NumElements) - 1ULL;
    //}

    void InitInstanced(const FPMUDeferredDecalInstancedVertexBuffer* VertexBuffer, const FPMUDeferredDecalLineAttributeBuffer* AttributeBuffer)
    {
        if (IsInRenderingThread())
        {
            InitInstanced_RenderThread(VertexBuffer, AttributeBuffer);
        }
        else
        {
            ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
                InitPMUDeferredDecalVertexFactory,
                FPMUDeferredDecalVertexFactory*, VertexFactory, this,
                const FPMUDeferredDecalInstancedVertexBuffer*, VertexBuffer, VertexBuffer,
                const FPMUDeferredDecalLineAttributeBuffer*, AttributeBuffer, AttributeBuffer,
                {
                VertexFactory->InitInstanced_RenderThread(VertexBuffer, AttributeBuffer);
                } );
        }
    }

    void Init(const FPMUDeferredDecalVertexBuffer* VertexBuffer)
    {
        if (IsInRenderingThread())
        {
            Init_RenderThread(VertexBuffer);
        }
        else
        {
            ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
                InitPMUDeferredDecalVertexFactory,
                FPMUDeferredDecalVertexFactory*, VertexFactory, this,
                const FPMUDeferredDecalVertexBuffer*, VertexBuffer, VertexBuffer,
                {
                VertexFactory->Init_RenderThread(VertexBuffer);
                } );
        }
    }

private:

    FPMUDDDataType Data;

    void Init_RenderThread(const FPMUDeferredDecalVertexBuffer* VertexBuffer)
    {
        check(IsInRenderingThread());
        // Initialize vertex stream components
        FPMUDDDataType NewData;
        NewData.PositionComponent      = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FPMUDeferredDecalVertex, Position,      VET_Float3);
        NewData.ColorComponent         = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FPMUDeferredDecalVertex, Color,         VET_Color);
        NewData.LineStartComponent     = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FPMUDeferredDecalVertex, LineStart,     VET_Float2);
        NewData.LineEndComponent       = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FPMUDeferredDecalVertex, LineEnd,       VET_Float2);
        NewData.LineAttributeComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FPMUDeferredDecalVertex, LineAttribute, VET_Float4);
        SetData(NewData);
    }

    void InitInstanced_RenderThread(const FPMUDeferredDecalInstancedVertexBuffer* VertexBuffer, const FPMUDeferredDecalLineAttributeBuffer* AttributeBuffer)
    {
        check(IsInRenderingThread());

        FPMUDDDataType NewData;

        // Initialize vertex stream components

        NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FPMUDeferredDecalInstancedVertex, Position, VET_Float3);

        // Initialize instance stream components

        NewData.ColorComponent = FVertexStreamComponent(
            AttributeBuffer,
            STRUCT_OFFSET(FPMUDeferredDecalLineAttribute, LineColor),
            sizeof(FPMUDeferredDecalLineAttribute),
            VET_Color,
            true
            );

        NewData.LineStartComponent = FVertexStreamComponent(
            AttributeBuffer,
            STRUCT_OFFSET(FPMUDeferredDecalLineAttribute, LineStart),
            sizeof(FPMUDeferredDecalLineAttribute),
            VET_Float2,
            true
            );

        NewData.LineEndComponent = FVertexStreamComponent(
            AttributeBuffer,
            STRUCT_OFFSET(FPMUDeferredDecalLineAttribute, LineEnd),
            sizeof(FPMUDeferredDecalLineAttribute),
            VET_Float2,
            true
            );

        NewData.LineAttributeComponent = FVertexStreamComponent(
            AttributeBuffer,
            STRUCT_OFFSET(FPMUDeferredDecalLineAttribute, LineAttribute),
            sizeof(FPMUDeferredDecalLineAttribute),
            VET_Float4,
            true
            );
            
        SetData(NewData);
    }
};

IMPLEMENT_VERTEX_FACTORY_TYPE(FPMUDeferredDecalVertexFactory,"/Plugin/ProceduralMeshUtility/Private/PMUDeferredDecalVertexFactory.ush",true,false,true,false,false);

struct FPMUDeferredDecalLineProxy
{
    enum { VERTEX_COUNT_PER_LINE = 8 };
    enum { INDEX_COUNT_PER_LINE  = 12 * 3 };

    const uint8 SRC_INDICES[INDEX_COUNT_PER_LINE] =
    {
        0, 2, 3,
        0, 3, 1,
        4, 5, 7,
        4, 7, 6,
        0, 1, 5,
        0, 5, 4,
        2, 6, 7,
        2, 7, 3,
        0, 4, 6,
        0, 6, 2,
        1, 3, 7,
        1, 7, 5
    };

    TArray<FPMUDeferredDecalLine> Lines;
    FColor Color;

    FPMUDeferredDecalVertexBuffer          VertexBuffer;
    FPMUDeferredDecalInstancedVertexBuffer InstancedVertexBuffer;
    FPMUDeferredDecalLineAttributeBuffer   AttributeBuffer;
    FPMUDeferredDecalVertexFactory         VertexFactory;
    FPMUDeferredDecalIndexBuffer           IndexBuffer;

    void InitResources()
    {
        if (Lines.Num() < 1)
        {
            return;
        }

        const int32 LineCount = Lines.Num();

        const int32 VertexCount = LineCount * VERTEX_COUNT_PER_LINE;
        const int32 IndexCount  = LineCount * INDEX_COUNT_PER_LINE;

        VertexBuffer.Vertices.SetNum(VertexCount);
        IndexBuffer.Indices.SetNumUninitialized(IndexCount);

        for (int32 li=0; li<LineCount; ++li)
        {
            const FPMUDeferredDecalLine& Line(Lines[li]);

            // Construct vertices
            {
                const int32 vi = li * VERTEX_COUNT_PER_LINE;

                VertexBuffer.Vertices[vi  ].Position.Set( 1.f,  1.f,  1.f);
                VertexBuffer.Vertices[vi+1].Position.Set(-1.f,  1.f,  1.f);
                VertexBuffer.Vertices[vi+2].Position.Set( 1.f, -1.f,  1.f);
                VertexBuffer.Vertices[vi+3].Position.Set(-1.f, -1.f,  1.f);
                VertexBuffer.Vertices[vi+4].Position.Set( 1.f,  1.f, -1.f);
                VertexBuffer.Vertices[vi+5].Position.Set(-1.f,  1.f, -1.f);
                VertexBuffer.Vertices[vi+6].Position.Set( 1.f, -1.f, -1.f);
                VertexBuffer.Vertices[vi+7].Position.Set(-1.f, -1.f, -1.f);

                VertexBuffer.Vertices[vi  ].Color = Color;
                VertexBuffer.Vertices[vi+1].Color = Color;
                VertexBuffer.Vertices[vi+2].Color = Color;
                VertexBuffer.Vertices[vi+3].Color = Color;
                VertexBuffer.Vertices[vi+4].Color = Color;
                VertexBuffer.Vertices[vi+5].Color = Color;
                VertexBuffer.Vertices[vi+6].Color = Color;
                VertexBuffer.Vertices[vi+7].Color = Color;

                VertexBuffer.Vertices[vi  ].LineStart = Line.LineStart;
                VertexBuffer.Vertices[vi+1].LineStart = Line.LineStart;
                VertexBuffer.Vertices[vi+2].LineStart = Line.LineStart;
                VertexBuffer.Vertices[vi+3].LineStart = Line.LineStart;
                VertexBuffer.Vertices[vi+4].LineStart = Line.LineStart;
                VertexBuffer.Vertices[vi+5].LineStart = Line.LineStart;
                VertexBuffer.Vertices[vi+6].LineStart = Line.LineStart;
                VertexBuffer.Vertices[vi+7].LineStart = Line.LineStart;

                VertexBuffer.Vertices[vi  ].LineEnd = Line.LineEnd;
                VertexBuffer.Vertices[vi+1].LineEnd = Line.LineEnd;
                VertexBuffer.Vertices[vi+2].LineEnd = Line.LineEnd;
                VertexBuffer.Vertices[vi+3].LineEnd = Line.LineEnd;
                VertexBuffer.Vertices[vi+4].LineEnd = Line.LineEnd;
                VertexBuffer.Vertices[vi+5].LineEnd = Line.LineEnd;
                VertexBuffer.Vertices[vi+6].LineEnd = Line.LineEnd;
                VertexBuffer.Vertices[vi+7].LineEnd = Line.LineEnd;

                const float ZeroTolerance = Line.LineStart.Equals(Line.LineEnd) ? KINDA_SMALL_NUMBER : 0.f;
                const FVector2D Line0(Line.LineStart - ZeroTolerance);
                const FVector2D Line1(Line.LineEnd   + ZeroTolerance);
                const float HeadingAngle = FVector((Line1-Line0).GetSafeNormal(), 0.f).HeadingAngle();

                VertexBuffer.Vertices[vi  ].LineAttribute.Set(Line.Radius, Line.Height, HeadingAngle, Line.HeightOffset);
                VertexBuffer.Vertices[vi+1].LineAttribute.Set(Line.Radius, Line.Height, HeadingAngle, Line.HeightOffset);
                VertexBuffer.Vertices[vi+2].LineAttribute.Set(Line.Radius, Line.Height, HeadingAngle, Line.HeightOffset);
                VertexBuffer.Vertices[vi+3].LineAttribute.Set(Line.Radius, Line.Height, HeadingAngle, Line.HeightOffset);
                VertexBuffer.Vertices[vi+4].LineAttribute.Set(Line.Radius, Line.Height, HeadingAngle, Line.HeightOffset);
                VertexBuffer.Vertices[vi+5].LineAttribute.Set(Line.Radius, Line.Height, HeadingAngle, Line.HeightOffset);
                VertexBuffer.Vertices[vi+6].LineAttribute.Set(Line.Radius, Line.Height, HeadingAngle, Line.HeightOffset);
                VertexBuffer.Vertices[vi+7].LineAttribute.Set(Line.Radius, Line.Height, HeadingAngle, Line.HeightOffset);
            }

            // Construct indices
            for (int32 i=0, io=li*INDEX_COUNT_PER_LINE, vo=li*VERTEX_COUNT_PER_LINE; i<INDEX_COUNT_PER_LINE; ++i)
            {
                IndexBuffer.Indices[io+i] = vo + SRC_INDICES[i];
            }
        }

        if ((VertexBuffer.Vertices.Num() > 0) || (IndexBuffer.Indices.Num() > 0))
        {
            // Initialize vertex factory
            VertexFactory.Init(&VertexBuffer);

            // Initialize resources
            BeginInitResource(&VertexBuffer);
            BeginInitResource(&VertexFactory);
            BeginInitResource(&IndexBuffer);
        }
    }

    void InitResourcesInstanced()
    {
        if (Lines.Num() < 1)
        {
            return;
        }

        check(GRHISupportsInstancing);

        const int32 LineCount = Lines.Num();

        const int32 VertexCount = VERTEX_COUNT_PER_LINE;
        const int32 IndexCount  = INDEX_COUNT_PER_LINE;

        InstancedVertexBuffer.Vertices.SetNum(VertexCount);
        AttributeBuffer.Instances.SetNum(LineCount);
        IndexBuffer.Indices.SetNumUninitialized(IndexCount);

        // Construct instanced vertex buffer

        InstancedVertexBuffer.Vertices[0].Position.Set( 1.f,  1.f,  1.f);
        InstancedVertexBuffer.Vertices[1].Position.Set(-1.f,  1.f,  1.f);
        InstancedVertexBuffer.Vertices[2].Position.Set( 1.f, -1.f,  1.f);
        InstancedVertexBuffer.Vertices[3].Position.Set(-1.f, -1.f,  1.f);
        InstancedVertexBuffer.Vertices[4].Position.Set( 1.f,  1.f, -1.f);
        InstancedVertexBuffer.Vertices[5].Position.Set(-1.f,  1.f, -1.f);
        InstancedVertexBuffer.Vertices[6].Position.Set( 1.f, -1.f, -1.f);
        InstancedVertexBuffer.Vertices[7].Position.Set(-1.f, -1.f, -1.f);

        // Construct index buffer

        for (int32 i=0; i<IndexCount; ++i)
        {
            IndexBuffer.Indices[i] = SRC_INDICES[i];
        }

        // Construct instance attribute buffer

        for (int32 i=0; i<LineCount; ++i)
        {
            const FPMUDeferredDecalLine& Line(Lines[i]);

            const float ZeroTolerance = Line.LineStart.Equals(Line.LineEnd) ? KINDA_SMALL_NUMBER : 0.f;
            const FVector2D Line0(Line.LineStart - ZeroTolerance);
            const FVector2D Line1(Line.LineEnd   + ZeroTolerance);
            const float HeadingAngle = FVector((Line1-Line0).GetSafeNormal(), 0.f).HeadingAngle();

            FPMUDeferredDecalLineAttribute& Attribute(AttributeBuffer.Instances[i]);
            Attribute.LineColor = Color;
            Attribute.LineStart = Line.LineStart;
            Attribute.LineEnd   = Line.LineEnd;
            Attribute.LineAttribute.Set(Line.Radius, Line.Height, HeadingAngle, Line.HeightOffset);
        }

        if ((InstancedVertexBuffer.Vertices.Num() > 0) || (IndexBuffer.Indices.Num() > 0) || (AttributeBuffer.Instances.Num() > 0))
        {
            // Initialize vertex factory
            VertexFactory.InitInstanced(&InstancedVertexBuffer, &AttributeBuffer);

            // Initialize resources
            BeginInitResource(&InstancedVertexBuffer);
            BeginInitResource(&AttributeBuffer);
            BeginInitResource(&VertexFactory);
            BeginInitResource(&IndexBuffer);
        }
    }

    void ReleaseResource()
    {
        VertexBuffer.ReleaseResource();
        InstancedVertexBuffer.ReleaseResource();
        AttributeBuffer.ReleaseResource();
        VertexFactory.ReleaseResource();
        IndexBuffer.ReleaseResource();
    }
};

class FPMUDeferredDecalSceneProxy : public FPrimitiveSceneProxy
{

public:

    FPMUDeferredDecalSceneProxy(UPMUDeferredDecalComponent* Component)
        : FPrimitiveSceneProxy(Component)
        , MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
        , bInstanced(GRHISupportsInstancing)
    {
        check(Component != nullptr);

        // Construct & Initialize line proxies

        const TArray<FPMUDeferredDecalLineGroup>& SrcLineGroups(Component->LineGroups);
        const int32 LineGroupCount = SrcLineGroups.Num();

        LineGroups.SetNum(LineGroupCount);

        for (int32 i=0; i<LineGroupCount; ++i)
        {
            const FPMUDeferredDecalLineGroup& LineGroup(SrcLineGroups[i]);
            FPMUDeferredDecalLineProxy& LineProxy(LineGroups[i]);
            LineProxy.Lines = LineGroup.Lines;
            LineProxy.Color = LineGroup.Color;

            if (bInstanced)
            {
                LineProxy.InitResourcesInstanced();
            }
            else
            {
                LineProxy.InitResources();
            }
        }

        // Use default material if no material assigned

        if ((ProxyMaterial = Component->GetMaterial(0)) == nullptr)
        {
            ProxyMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
        }
    }

    virtual ~FPMUDeferredDecalSceneProxy()
    {
        for (FPMUDeferredDecalLineProxy& LineProxy : LineGroups)
        {
            LineProxy.ReleaseResource();
        }
    }

    virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
    {
        //SCOPE_CYCLE_COUNTER(STAT_VoxelMesh_GetMeshElements);

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

        check(ProxyMaterial != nullptr);

        FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : ProxyMaterial->GetRenderProxy(IsSelected());

        for (int32 i=0; i<LineGroups.Num(); ++i)
        {
            const FPMUDeferredDecalLineProxy& LineProxy(LineGroups[i]);
            const int32 LineCount = LineProxy.Lines.Num();

            // No line in the proxy, continue
            if (LineCount < 1)
            {
                continue;
            }

            for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
            {
                if (VisibilityMap & (1 << ViewIndex))
                {
                    const FSceneView* View = Views[ViewIndex];

                    // Constrct mesh element batch for mesh batch draw

                    FMeshBatch& Mesh = Collector.AllocateMesh();
                    FMeshBatchElement& BatchElement = Mesh.Elements[0];

                    Mesh.bWireframe = bWireframe;
                    Mesh.VertexFactory = &LineProxy.VertexFactory;
                    Mesh.MaterialRenderProxy = MaterialProxy;

                    Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
                    Mesh.Type = PT_TriangleList;
                    Mesh.DepthPriorityGroup = SDPG_World;
                    Mesh.bCanApplyViewModeOverrides = false;

                    if (bWireframe)
                    {
                        Mesh.bWireframe = true;
                        Mesh.bDisableBackfaceCulling = true;
                    }

                    BatchElement.IndexBuffer = &LineProxy.IndexBuffer;
                    BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(
                        GetLocalToWorld(),
                        GetBounds(),
                        GetLocalBounds(),
                        false,
                        UseEditorDepthTest()
                        );

                    BatchElement.FirstIndex = 0;
                    BatchElement.NumPrimitives = LineProxy.IndexBuffer.Indices.Num() / 3;

                    if (bInstanced)
                    {
                        BatchElement.bIsInstancedMesh = true;
                        BatchElement.NumInstances = LineCount;

                        BatchElement.MinVertexIndex = 0;
                        BatchElement.MaxVertexIndex = LineProxy.InstancedVertexBuffer.Vertices.Num() - 1;
                    }
                    else
                    {
                        BatchElement.bIsInstancedMesh = false;

                        BatchElement.MinVertexIndex = 0;
                        BatchElement.MaxVertexIndex = LineProxy.VertexBuffer.Vertices.Num() - 1;
                    }

                    Collector.AddMesh(ViewIndex, Mesh);
                }
            }
        }

        // Draw bounds
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
        for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
        {
            if (VisibilityMap & (1 << ViewIndex))
            {
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
        Result.bDynamicRelevance = true;
        Result.bShadowRelevance = IsShadowCast(View);
        Result.bRenderInMainPass = ShouldRenderInMainPass();
        Result.bRenderCustomDepth = ShouldRenderCustomDepth();
        Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
        MaterialRelevance.SetPrimitiveViewRelevance(Result);
        return Result;
    }

    virtual bool CanBeOccluded() const override
    {
        return !MaterialRelevance.bDisableDepthTest;
    }

    virtual uint32 GetMemoryFootprint(void) const override
    {
        return(sizeof(*this) + GetAllocatedSize());
    }

    uint32 GetAllocatedSize(void) const
    {
        return FPrimitiveSceneProxy::GetAllocatedSize();
    }

private:

    static const int32 VERTEX_COUNT_PER_LINE = 8;
    static const int32 INDEX_COUNT_PER_LINE  = 12 * 3;

    bool bInstanced;
    TArray<FPMUDeferredDecalLineProxy> LineGroups;

    UMaterialInterface* ProxyMaterial = nullptr;
    FMaterialRelevance MaterialRelevance;
};

UPMUDeferredDecalComponent::UPMUDeferredDecalComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bUseAsOccluder = false;
    bReceivesDecals = false;

    CastShadow = false;
    bCastStaticShadow = false;
    bCastDynamicShadow = false;

    bSelectable = true;
    bGenerateOverlapEvents = false;
    bCanEverAffectNavigation = false;

    CanCharacterStepUpOn = ECB_No;

    SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

FBoxSphereBounds UPMUDeferredDecalComponent::CalcBounds(const FTransform& LocalToWorld) const
{
    FBoxSphereBounds Ret(LocalBounds.TransformBy(LocalToWorld));

    Ret.BoxExtent *= BoundsScale;
    Ret.SphereRadius *= BoundsScale;

    return Ret;
}

FPrimitiveSceneProxy* UPMUDeferredDecalComponent::CreateSceneProxy()
{
    return new FPMUDeferredDecalSceneProxy(this);
}

void UPMUDeferredDecalComponent::UpdateLocalBounds()
{
    FBox LocalBox(ForceInitToZero);

    for (const FPMUDeferredDecalLineGroup& LineGroup : LineGroups)
    {
        LocalBox += LineGroup.Bounds;
    }

    // Swizzle vector components to decal coordinate if box is valid
    if (LocalBox.IsValid)
    {
        const FVector& Min(LocalBox.Min);
        const FVector& Max(LocalBox.Max);
        LocalBox.Min = FVector(Min.Z, Min.Y, Min.X);
        LocalBox.Max = FVector(Max.Z, Max.Y, Max.X);
    }

    LocalBounds = LocalBox.IsValid ? FBoxSphereBounds(LocalBox) : FBoxSphereBounds(FVector(0, 0, 0), FVector(0, 0, 0), 0);

    UpdateBounds();
    MarkRenderTransformDirty();
}

void UPMUDeferredDecalComponent::UpdateRenderState()
{
    UpdateLocalBounds();
    MarkRenderStateDirty();
}

void UPMUDeferredDecalComponent::SetLineGroup(int32 GroupId, const FPMUDeferredDecalLineGroup& LineGroup, bool bUpdateRenderState)
{
    // Invalid group id, abort
    if (GroupId < 0)
    {
        return;
    }

    // Allocate new line group if required
    if (! LineGroups.IsValidIndex(GroupId))
    {
        LineGroups.SetNum(GroupId+1, false);
    }

    LineGroups[GroupId] = LineGroup;

    UpdateLineGroup(GroupId);

    if (bUpdateRenderState)
    {
        UpdateRenderState();
    }
}

void UPMUDeferredDecalComponent::UpdateLineGroup(int32 GroupId)
{
    // Invalid group id, abort
    if (! LineGroups.IsValidIndex(GroupId))
    {
        return;
    }

    FPMUDeferredDecalLineGroup& LineGroup(LineGroups[GroupId]);
    const TArray<FPMUDeferredDecalLine> Lines(LineGroup.Lines);

    FBox LocalBox(ForceInitToZero);

    for (const FPMUDeferredDecalLine& Line : Lines)
    {
        FBox LineBounds(ForceInitToZero);
        LineBounds += FVector(Line.LineStart, (-Line.Height)-Line.HeightOffset);
        LineBounds += FVector(Line.LineEnd,   ( Line.Height)-Line.HeightOffset);
        LineBounds = LineBounds.ExpandBy(Line.Radius);
        LocalBox += LineBounds;
    }

    LineGroup.Bounds = LocalBox;
}

TArray<FPMUDeferredDecalLine> UPMUDeferredDecalComponent::ConvertPointsToLines(const TArray<FVector2D>& InPoints, float Radius, float Height, FColor Color, bool bClosePoints)
{
    const int32 PointCount = InPoints.Num();

    if (PointCount < 1)
    {
        return {};
    }

    TArray<FPMUDeferredDecalLine> OutLines;
    OutLines.Reserve(PointCount-1);

    for (int32 i=1; i<PointCount; ++i)
    {
        FPMUDeferredDecalLine Line;
        Line.LineStart = InPoints[i-1];
        Line.LineEnd   = InPoints[i];
        Line.Radius    = Radius;
        Line.Height    = Height;
        OutLines.Emplace(Line);
    }

    if (bClosePoints && PointCount > 2)
    {
        const FVector2D& P0(OutLines[0].LineStart);
        const FVector2D& PN(OutLines.Last().LineEnd);

        if (! P0.Equals(PN, KINDA_SMALL_NUMBER))
        {
            FPMUDeferredDecalLine Line;
            Line.LineStart = P0;
            Line.LineEnd   = PN;
            Line.Radius    = Radius;
            Line.Height    = Height;
            OutLines.Emplace(Line);
        }
    }

    return OutLines;
}

TArray<FPMUDeferredDecalLine> UPMUDeferredDecalComponent::ConvertPointsToLines3D(const TArray<FVector>& InPoints, float Radius, float Height, FColor Color, bool bClosePoints)
{
    const int32 PointCount = InPoints.Num();

    if (PointCount < 1)
    {
        return {};
    }

    TArray<FPMUDeferredDecalLine> OutLines;
    OutLines.Reserve(PointCount-1);

    for (int32 i=1; i<PointCount; ++i)
    {
        const FVector& P0(InPoints[i-1]);
        const FVector& P1(InPoints[i]);

        FPMUDeferredDecalLine Line;
        ConvertPointPairToLine3D(Line, P0, P1, Radius, Height, Color);
        OutLines.Emplace(Line);
    }

    if (bClosePoints && PointCount > 2)
    {
        const FVector& P0(InPoints[0]);
        const FVector& PN(InPoints[PointCount-1]);

        if (! P0.Equals(PN, KINDA_SMALL_NUMBER))
        {
            FPMUDeferredDecalLine Line;
            ConvertPointPairToLine3D(Line, P0, PN, Radius, Height, Color);
            OutLines.Emplace(Line);
        }
    }

    return OutLines;
}

void UPMUDeferredDecalComponent::ConvertPointPairToLine3D(FPMUDeferredDecalLine& Line, const FVector& P0, const FVector& P1, float Radius, float Height, FColor Color)
{
    const bool bFlipHeightOffset = (P1.Z < P0.Z);
    const float HeightRadius = (bFlipHeightOffset ? (P0.Z-P1.Z) : (P1.Z-P0.Z)) * .5f;
    const float HeightOffset = (bFlipHeightOffset ? P1.Z : P0.Z) + HeightRadius;
    Line.LineStart    = FVector2D(P0);
    Line.LineEnd      = FVector2D(P1);
    Line.Radius       = Radius;
    Line.Height       = HeightRadius + Height;
    Line.HeightOffset = HeightOffset;
}
