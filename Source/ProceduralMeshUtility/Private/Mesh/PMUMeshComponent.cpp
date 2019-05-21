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

#include "Mesh/PMUMeshComponent.h"

#include "DynamicMeshBuilder.h"
#include "EngineGlobals.h"
#include "LocalVertexFactory.h"
#include "MaterialShared.h"
#include "PositionVertexBuffer.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "RenderResource.h"
#include "RenderUtils.h"
#include "RenderingThread.h"
#include "SceneManagement.h"
#include "StaticMeshVertexBuffer.h"
#include "VertexFactory.h"
#include "Containers/ResourceArray.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Rendering/ColorVertexBuffer.h"
#include "Stats/Stats.h"

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

    static void ModifyCompilationEnvironment(
        const FVertexFactoryType* Type,
        EShaderPlatform Platform,
        const FMaterial* Material,
        FShaderCompilerEnvironment& OutEnvironment
        )
	{
        // Disable manual vertex fetch

		const bool ContainsManualVertexFetch = OutEnvironment.GetDefinitions().Contains("MANUAL_VERTEX_FETCH");

		if (! ContainsManualVertexFetch)
		{
			OutEnvironment.SetDefine(TEXT("MANUAL_VERTEX_FETCH"), TEXT("0"));
		}

        FLocalVertexFactory::ModifyCompilationEnvironment(Type, Platform, Material, OutEnvironment);
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
        ENQUEUE_RENDER_COMMAND(FPMUMeshVertexFactory_Init)(
            [this, VertexBuffer](FRHICommandListImmediate& RHICmdList)
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

                SetData(VertexData);
            }
        );
    }
};

IMPLEMENT_VERTEX_FACTORY_TYPE(FPMUMeshVertexFactory, "/Engine/Private/LocalVertexFactory.ush", true, true, true, true, true);

#if 0
//////////////////////////////////////////////////////////////////////////

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

        auto& SrcVB(Buffer.GetVBArray());
        auto& SrcIB(Buffer.GetIBArray());
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

class FPMUMeshSceneProxy : public FPrimitiveSceneProxy
{
public:

    FPMUMeshSceneProxy(UPMUMeshComponent* Component)
        : FPrimitiveSceneProxy(Component)
        , BodySetup(Component->GetBodySetup())
        , MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
    {
        check(IsValid(Component));

        ConstructSectionsFromResource(*Component);
    }

    virtual ~FPMUMeshSceneProxy()
    {
        for (FPMUMeshProxySection* Section : DirectSections)
        {
            if (Section != nullptr)
            {
                Section->ReleaseResources();
                delete Section;
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

                DstSection.Material = Component.GetMaterial(SectionIdx);

                if (DstSection.Material == NULL)
                {
                    DstSection.Material = UMaterial::GetDefaultMaterial(MD_Surface);
                }
            }
        }
    }

    // Called on render thread to assign new dynamic data
#if 0
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
#endif

    void SetSectionVisibility_RenderThread(int32 SectionIndex, bool bNewVisibility)
    {
        check(IsInRenderingThread());

#if 0
        if (SectionIndex < Sections.Num() &&
            Sections[SectionIndex] != nullptr)
        {
            Sections[SectionIndex]->bSectionVisible = bNewVisibility;
        }
#endif
    }

    virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
    {
        return;

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
        for (const FPMUMeshProxySection* Section : DirectSections)
        {
            if (Section != nullptr && Section->bSectionVisible)
            {
                FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : Section->Material->GetRenderProxy();

                for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
                {
                    if (VisibilityMap & (1 << ViewIndex))
                    {
                        /*
                        const FSceneView* View = Views[ViewIndex];

                        FMeshBatch& Mesh = Collector.AllocateMesh();
                        Mesh.bWireframe = bWireframe;
                        Mesh.VertexFactory = &Section->VertexFactory;
                        Mesh.MaterialRenderProxy = MaterialProxy;

                        Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
                        Mesh.Type = PT_TriangleList;
                        Mesh.DepthPriorityGroup = SDPG_World;
                        Mesh.bCanApplyViewModeOverrides = false;

                        if (bWireframe)
                        {
                            Mesh.bDisableBackfaceCulling = true;
                        }

                        FMeshBatchElement& BatchElement = Mesh.Elements[0];
                        BatchElement.FirstIndex = 0;
                        BatchElement.NumPrimitives = Section->GetIndexCount() / 3;
                        BatchElement.MinVertexIndex = 0;
                        BatchElement.MaxVertexIndex = Section->GetVertexCount() - 1;
                        BatchElement.IndexBuffer = &Section->Buffer.IndexBuffer;

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

                        Collector.AddMesh(ViewIndex, Mesh);
                        */

						const FSceneView* View = Views[ViewIndex];
						// Draw the mesh.
						FMeshBatch& Mesh = Collector.AllocateMesh();
						FMeshBatchElement& BatchElement = Mesh.Elements[0];
                        BatchElement.IndexBuffer = &Section->Buffer.IndexBuffer;
						Mesh.bWireframe = bWireframe;
                        Mesh.VertexFactory = &Section->VertexFactory;
						Mesh.MaterialRenderProxy = MaterialProxy;

						bool bHasPrecomputedVolumetricLightmap;
						FMatrix PreviousLocalToWorld;
						int32 SingleCaptureIndex;
						GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex);

						FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
						DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, UseEditorDepthTest());
						BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

						BatchElement.FirstIndex = 0;
                        BatchElement.NumPrimitives = Section->GetIndexCount() / 3;
						BatchElement.MinVertexIndex = 0;
                        BatchElement.MaxVertexIndex = Section->GetVertexCount() - 1;
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

    virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
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

    TArray<FPMUMeshProxySection*> DirectSections;

    UBodySetup* BodySetup;
    FMaterialRelevance MaterialRelevance;
};
#endif

//////////////////////////////////////////////////////////////////////////

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
	FPositionVertexBuffer PositionVertexBuffer;
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

    FPMUMeshSceneProxy(UPMUMeshComponent* Component)
        : FPrimitiveSceneProxy(Component)
        , BodySetup(Component->GetBodySetup())
        , MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
    {
        check(IsValid(Component));

        ConstructSections(*Component);
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
    }

    void ConstructSections(UPMUMeshComponent& Component)
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

    // Called on render thread to assign new dynamic data
#if 0
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
#endif

#if 0
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

    virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
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

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const
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

    UBodySetup* BodySetup;
    FMaterialRelevance MaterialRelevance;
};

//////////////////////////////////////////////////////////////////////////

UPMUMeshComponent::UPMUMeshComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bUseComplexAsSimpleCollision = true;
}

void UPMUMeshComponent::PostLoad()
{
    Super::PostLoad();

    if (MeshBodySetup && IsTemplate())
    {
        MeshBodySetup->SetFlags(RF_Public);
    }
}

FPrimitiveSceneProxy* UPMUMeshComponent::CreateSceneProxy()
{
    return new FPMUMeshSceneProxy(this);
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

    // Single material is used for all sections, return the material
    if (bUseSingleMaterial)
    {
        Result = GetMaterial(SectionIndex);
    }
    // Otherwise, look for element that corresponds to the supplied face
    else
    {
        int32 TotalFaceCount = 0;

        for (int32 i=0; i<Sections.Num(); ++i)
        {
            const FPMUMeshSection& Section = Sections[i];

            int32 NumFaces = Section.Indices.Num() / 3;
            TotalFaceCount += NumFaces;

            if (FaceIndex < TotalFaceCount)
            {
                // Grab the material
                Result = GetMaterial(i);
                SectionIndex = i;
                break;
            }
        }
    }

    return Result;
}

int32 UPMUMeshComponent::GetNumMaterials() const
{
    return bUseSingleMaterial ? 1 : Sections.Num();
}

bool UPMUMeshComponent::GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
{
    // Base vertex index for current section
    int32 VertexBase = 0;

    // Copy UVs if required
    bool bCopyUVs = UPhysicsSettings::Get()->bSupportUVFromHitResults;
    if (bCopyUVs)
    {
        // Only one UV channel
        CollisionData->UVs.AddZeroed(1);
    }

    // Construct collision data
    for (int32 SectionIdx=0; SectionIdx < Sections.Num(); SectionIdx++)
    {
        FPMUMeshSection& Section = Sections[SectionIdx];

        // Construct collision data if required
        if (Section.bEnableCollision)
        {
            // Copy vert data
            for (int32 VertIdx = 0; VertIdx < Section.Positions.Num(); VertIdx++)
            {
                CollisionData->Vertices.Emplace(Section.Positions[VertIdx]);

                // Copy UV if desired
                if (bCopyUVs)
                {
                    CollisionData->UVs[0].Emplace(Section.UVs[VertIdx]);
                }
            }

            // Copy triangle data
            const int32 NumTriangles = Section.Indices.Num() / 3;
            for (int32 TriIdx = 0; TriIdx < NumTriangles; TriIdx++)
            {
                // Need to add base offset for indices
                FTriIndices Triangle;
                Triangle.v0 = Section.Indices[(TriIdx * 3) + 0] + VertexBase;
                Triangle.v1 = Section.Indices[(TriIdx * 3) + 1] + VertexBase;
                Triangle.v2 = Section.Indices[(TriIdx * 3) + 2] + VertexBase;
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
    // True if any of there is any non-empty section with collision enabled
    for (const FPMUMeshSection& Section : Sections)
    {
        if (Section.Indices.Num() >= 3 && Section.bEnableCollision)
        {
            return true;
        }
    }

    return false;
}

void UPMUMeshComponent::UpdateRenderState()
{
    UpdateLocalBounds();
    UpdateCollision();
    MarkRenderStateDirty();
}

void UPMUMeshComponent::SetNumSectionResources(int32 SectionCount, bool bAllowShrinking)
{
    if (SectionCount > SectionResources.Num())
    {
        SectionResources.SetNum(SectionCount, bAllowShrinking);
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

void UPMUMeshComponent::ClearSection(int32 SectionIndex)
{
	if (SectionIndex < Sections.Num())
	{
		Sections[SectionIndex].Reset();
	}
}

void UPMUMeshComponent::ClearAllSections()
{
	Sections.Empty();
}

bool UPMUMeshComponent::IsSectionVisible(int32 SectionIndex) const
{
	return Sections.IsValidIndex(SectionIndex) ? Sections[SectionIndex].bSectionVisible : false;
}

int32 UPMUMeshComponent::GetNumSections() const
{
	return Sections.Num();
}

void UPMUMeshComponent::CreateSectionFromGeometryBuffers(int32 SectionIndex, const TArray<FVector>& Positions, const TArray<FVector>& Normals, const TArray<int32>& Indices)
{
    // Invalid section index, abort
    if (SectionIndex < 0)
    {
        return;
    }

    if (! SectionResources.IsValidIndex(SectionIndex))
    {
        SectionResources.SetNum(SectionIndex+1, false);
    }

    FPMUMeshSectionResource& SectionResource(SectionResources[SectionIndex]);

    FBox& SectionBounds(SectionResource.LocalBounds);
    auto& SrcVB(SectionResource.Buffer.GetVBArray());
    auto& SrcIB(SectionResource.Buffer.GetIBArray());

    // Construct vertex buffer

    const int32 VertexCount = Positions.Num();
    bool bHasNormalBuffer = Normals.Num() == VertexCount;

    SrcVB.Reset(VertexCount);

    for (int32 i=0; i<VertexCount; ++i)
    {
        FPMUPackedVertex Vertex;

        Vertex.Position = Positions[i];
        Vertex.TextureCoordinate = FVector2D(Vertex.Position);
        Vertex.TangentX = FVector(1,0,0);

        if (bHasNormalBuffer)
        {
            Vertex.TangentZ = Normals[i];
        }

        SrcVB.Emplace(Vertex);
        SectionBounds += Vertex.Position;
    }

    // Copy index buffer

    SrcIB.SetNumUninitialized(Indices.Num(), true);
    check(SrcIB.GetResourceDataSize() == Indices.Num() * Indices.GetTypeSize());
    FMemory::Memcpy(SrcIB.GetData(), Indices.GetData(), SrcIB.GetResourceDataSize());

    SectionResource.VertexCount = SrcVB.Num();
    SectionResource.IndexCount = SrcIB.Num();
}

void UPMUMeshComponent::CreateSectionFromSectionResource(int32 SectionIndex, const FPMUMeshSectionResourceRef& Section, bool bCreateCollision, bool bCalculateBounds)
{
    // Invalid section resource, abort
    if (! Section.IsValid())
    {
        return;
    }

    TArray<FVector> Positions;
    TArray<FVector> Normals;
    TArray<FVector> Tangents;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> LinearColors;

    auto& SrcIB(Section.SectionPtr->Buffer.GetIBArray());
    auto& SrcVB(Section.SectionPtr->Buffer.GetVBArray());
    FBox SectionBounds(Section.SectionPtr->LocalBounds);

    const int32 NumVerts = SrcVB.Num();

    Positions.SetNumUninitialized(NumVerts);
    Normals.SetNumUninitialized(NumVerts);
    Tangents.SetNumUninitialized(NumVerts);
    UVs.SetNumUninitialized(NumVerts);
    LinearColors.SetNumUninitialized(NumVerts);

    for (int32 i=0; i<NumVerts; ++i)
    {
        const FPMUPackedVertex& Vertex(SrcVB[i]);
        Positions[i] = Vertex.Position;
        Normals[i] = Vertex.TangentZ.ToFVector();
        Tangents[i] = Vertex.TangentX.ToFVector();
        UVs[i] = Vertex.TextureCoordinate;
        LinearColors[i] = Vertex.Color.ReinterpretAsLinear();
    }

    TArray<int32> Indices;
    Indices.SetNumZeroed(SrcIB.Num());
    FMemory::Memcpy(Indices.GetData(), SrcIB.GetData(), SrcIB.GetResourceDataSize());

    CreateMeshSection_LinearColor(
        SectionIndex,
        Positions,
        Indices,
        Normals,
        Tangents,
        UVs,
        LinearColors,
        SectionBounds,
        bCreateCollision,
        bCalculateBounds
        );
}

void UPMUMeshComponent::CreateSectionFromSectionRef(int32 SectionIndex, const FPMUMeshSectionRef& Section)
{
    // Invalid section resource, abort
    if (! Section.HasValidSection())
    {
        return;
    }

	if (SectionIndex >= Sections.Num())
	{
		Sections.SetNum(SectionIndex + 1, false);
	}

    const FPMUMeshSection& SrcSection(*Section.SectionPtr);
	FPMUMeshSection& DstSection(Sections[SectionIndex]);

	DstSection.Reset();
    DstSection.Positions = SrcSection.Positions;
    DstSection.Indices = SrcSection.Indices;

    const int32 NumVerts = DstSection.Positions.Num();

    // Copy tangent data

    if (SrcSection.Tangents.Num() == (NumVerts*2))
    {
        DstSection.Tangents = SrcSection.Tangents;
    }
    else
    if (SrcSection.bInitializeInvalidVertexData)
    {
        DstSection.Tangents.SetNumZeroed(NumVerts*2);
    }

    // Copy uv data

    if (SrcSection.UVs.Num() == NumVerts)
    {
        DstSection.UVs = SrcSection.UVs;
    }
    else
    if (SrcSection.bInitializeInvalidVertexData)
    {
        DstSection.UVs.SetNumZeroed(NumVerts);
    }

    // Copy color data

    if (SrcSection.Colors.Num() == NumVerts)
    {
        DstSection.Colors = SrcSection.Colors;
    }
    else
    if (SrcSection.bInitializeInvalidVertexData)
    {
        DstSection.Colors.SetNumZeroed(NumVerts);
    }

    DstSection.SectionLocalBox = SrcSection.SectionLocalBox;
    DstSection.bSectionVisible = SrcSection.bSectionVisible;
    DstSection.bEnableCollision = SrcSection.bEnableCollision;
    DstSection.bEnableFastUVCopy = SrcSection.bEnableFastUVCopy;
    DstSection.bEnableFastTangentsCopy = SrcSection.bEnableFastTangentsCopy;
    DstSection.bInitializeInvalidVertexData = SrcSection.bInitializeInvalidVertexData;
}

void UPMUMeshComponent::CreateMeshSection_LinearColor(
    int32 SectionIndex,
    const TArray<FVector>& Positions,
    const TArray<int32>& Indices,
    const TArray<FVector>& Normals,
    const TArray<FVector>& Tangents,
    const TArray<FVector2D>& UVs,
    const TArray<FLinearColor>& LinearColors,
    FBox SectionBounds,
    bool bCreateCollision,
    bool bCalculateBounds
    )
{
    // Invalid section index, abort
    if (SectionIndex < 0)
    {
        return;
    }

    // Empty position buffer, abort

	const int32 NumVerts = Positions.Num();

    if (NumVerts < 1)
    {
        return;
    }

	// Convert FLinearColors to FColors

	TArray<FColor> Colors;

	if (LinearColors.Num() == NumVerts)
	{
		Colors.SetNumUninitialized(LinearColors.Num());

		for (int32 ColorIdx = 0; ColorIdx < LinearColors.Num(); ColorIdx++)
		{
			Colors[ColorIdx] = LinearColors[ColorIdx].ToFColor(false);
		}
	}

	// Ensure sections array is long enough

	if (SectionIndex >= Sections.Num())
	{
		Sections.SetNum(SectionIndex + 1, false);
	}

	// Reset section (in case it already existed)
	FPMUMeshSection& NewSection = Sections[SectionIndex];
	NewSection.Reset();

	// Copy data to vertex buffer

    NewSection.Positions = Positions;

    // Copy tangents data

    const bool bHasValidTangentsX = Tangents.Num() == NumVerts;
    const bool bHasValidTangentsZ = Normals.Num() == NumVerts;

    if (NewSection.bEnableFastTangentsCopy)
    {
        NewSection.Tangents.SetNumZeroed(NumVerts*2);

        if (bHasValidTangentsX && bHasValidTangentsZ)
        {
            for (int32 i=0; i<NumVerts; ++i)
            {
                NewSection.Tangents[2*i  ] = FPackedNormal(Tangents[i]).Vector.Packed;
                NewSection.Tangents[2*i+1] = FPackedNormal(FVector4(Normals[i],1)).Vector.Packed;
            }
        }
        else
        if (bHasValidTangentsX)
        {
            for (int32 i=0; i<NumVerts; ++i)
            {
                NewSection.Tangents[2*i  ] = FPackedNormal(Tangents[i]).Vector.Packed;
                NewSection.Tangents[2*i+1] = FPackedNormal(FVector4(0,0,1,1)).Vector.Packed;
            }
        }
        else
        if (bHasValidTangentsZ)
        {
            for (int32 i=0; i<NumVerts; ++i)
            {
                NewSection.Tangents[2*i  ] = FPackedNormal(FVector(1,0,0)).Vector.Packed;
                NewSection.Tangents[2*i+1] = FPackedNormal(FVector4(Normals[i],1)).Vector.Packed;
            }
        }
        else
        {
            for (int32 i=0; i<NumVerts; ++i)
            {
                NewSection.Tangents[2*i  ] = FPackedNormal(FVector(1,0,0)).Vector.Packed;
                NewSection.Tangents[2*i+1] = FPackedNormal(FVector4(0,0,1,1)).Vector.Packed;
            }
        }
    }
    else
    {
        if (bHasValidTangentsZ)
        {
            NewSection.TangentsZ = Normals;
        }
        else
        {
            NewSection.TangentsZ.Init(FVector(0,0,1), NumVerts);
        }

        if (bHasValidTangentsX)
        {
            NewSection.TangentsX = Tangents;
        }
        else
        {
            NewSection.TangentsX.Init(FVector(1,0,0), NumVerts);
        }
    }

    // Copy color data

    if (Colors.Num() == NumVerts)
    {
        NewSection.Colors = Colors;
    }
    else
    {
        NewSection.Colors.Init(FColor(255,255,255), NumVerts);
    }

    // Copy uv data

    if (UVs.Num() == NumVerts)
    {
        NewSection.UVs = UVs;
    }
    else
    {
        NewSection.UVs.SetNumZeroed(NumVerts);
    }

    // Calculate section local bounds if required

    if (bCalculateBounds)
    {
        for (int32 i=0; i<NumVerts; i++)
        {
            NewSection.SectionLocalBox += Positions[i];
        }
    }
    else
    {
		NewSection.SectionLocalBox = SectionBounds;
    }

	// Copy index buffer

	int32 NumIndices = Indices.Num();
	// Ensure we have exact number of triangles (array is multiple of 3 long)
    NumIndices = (NumIndices/3) * 3;

    NewSection.Indices.SetNumUninitialized(NumIndices, true);

    for (int32 i=0; i<NumIndices; ++i)
    {
        NewSection.Indices[i] = Indices[i];
    }

    // Set collision mode

	NewSection.bEnableCollision = bCreateCollision;
}

void UPMUMeshComponent::CopyMeshSection(
    int32 SectionIndex,
    const FPMUMeshSection& SourceSection,
    bool bCalculateBounds
    )
{
    // Invalid section index, abort
    if (SectionIndex < 0)
    {
        return;
    }

	// Ensure sections array is long enough
	if (SectionIndex >= Sections.Num())
	{
		Sections.SetNum(SectionIndex + 1, false);
	}

	// Reset section (in case it already existed)
	FPMUMeshSection& NewSection = Sections[SectionIndex];
	NewSection.Reset();

	// Copy data
    NewSection = SourceSection;

    // Calculate section local bounds if required
    if (bCalculateBounds)
    {
        for (const FVector& Position : NewSection.Positions)
        {
            NewSection.SectionLocalBox += Position;
        }
    }
}

void UPMUMeshComponent::UpdateLocalBounds()
{
    FBox LocalBox(ForceInit);

    for (const FPMUMeshSection& Section : Sections)
    {
        LocalBox += Section.SectionLocalBox;
    }

    LocalBounds = LocalBox.IsValid ? FBoxSphereBounds(LocalBox) : FBoxSphereBounds(FVector(0, 0, 0), FVector(0, 0, 0), 0);

    UpdateBounds();
    MarkRenderTransformDirty();
}

FBoxSphereBounds UPMUMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
    FBoxSphereBounds Ret(LocalBounds.TransformBy(LocalToWorld));

    Ret.BoxExtent *= BoundsScale;
    Ret.SphereRadius *= BoundsScale;

    return Ret;
}

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
		CollisionConvexElems.Add(NewConvexElem);
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
