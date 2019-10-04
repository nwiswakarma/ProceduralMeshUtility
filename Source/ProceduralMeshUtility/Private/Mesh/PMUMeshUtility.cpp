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

#include "Mesh/PMUMeshUtility.h"

#include "RHICommandList.h"
#include "RenderResource.h"

#include "GenericWorkerThread.h"
#include "GWTTickUtilities.h"
#include "GWTTickManager.h"
#include "RHI/RULAlignedTypes.h"
#include "RHI/RULRHIBuffer.h"
#include "Shaders/RULShaderDefinitions.h"

#include "ProceduralMeshUtility.h"
#include "Mesh/PMUMeshSceneProxy.h"

template<uint32 bReverseWinding>
class TPMUMeshUtilityCreateGridMeshSectionCS : public FRULBaseComputeShader<16,16,1>
{
public:

    typedef FRULBaseComputeShader<16,16,1> FBaseType;

    DECLARE_SHADER_TYPE(TPMUMeshUtilityCreateGridMeshSectionCS, Global);

public:

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return RHISupportsComputeShaders(Parameters.Platform);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FBaseType::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("PMU_GRID_UTILITY_CREATE_GPU_MESH_SECTION_USE_REVERSE_WINDING"), bReverseWinding);
    }

    RUL_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER_WITH_TEXTURE(TPMUMeshUtilityCreateGridMeshSectionCS)

    RUL_DECLARE_SHADER_PARAMETERS_1(
        Texture,
        FShaderResourceParameter,
        FResourceId,
        "HeightTexture", HeightTexture
        )

    RUL_DECLARE_SHADER_PARAMETERS_1(
        Sampler,
        FShaderResourceParameter,
        FResourceId,
        "HeightTextureSampler", HeightTextureSampler
        )

    RUL_DECLARE_SHADER_PARAMETERS_0(SRV,,)

    RUL_DECLARE_SHADER_PARAMETERS_5(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutPositionData", OutPositionData,
        "OutTangentData",  OutTangentData,
        "OutTexCoordData", OutTexCoordData,
        "OutColorData",    OutColorData,
        "OutIndexData",    OutIndexData
        )

    RUL_DECLARE_SHADER_PARAMETERS_2(
        Value,
        FShaderParameter,
        FParameterId,
        "_Dimension",   Params_Dimension,
        "_HeightScale", Params_HeightScale
        )
};

IMPLEMENT_SHADER_TYPE(template<>, TPMUMeshUtilityCreateGridMeshSectionCS<0>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUGridUtilityCreateGPUMeshSectionCS.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, TPMUMeshUtilityCreateGridMeshSectionCS<1>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUGridUtilityCreateGPUMeshSectionCS.usf"), TEXT("MainCS"), SF_Compute);

template<uint32 CompileFlags>
class TPMUMeshUtilityApplyHeightMapToMeshSectionCS : public FRULBaseComputeShader<256,1,1>
{
public:

    typedef FRULBaseComputeShader<256,1,1> FBaseType;

    DECLARE_SHADER_TYPE(TPMUMeshUtilityApplyHeightMapToMeshSectionCS, Global);

public:

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return RHISupportsComputeShaders(Parameters.Platform);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FBaseType::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        const uint32 UseUVBuffer = ((CompileFlags&0x01) > 0) ? 1 : 0;
        OutEnvironment.SetDefine(TEXT("PMU_MESH_UTILITY_ASSIGN_HEIGHT_MAP_USE_UV_BUFFER"), UseUVBuffer);
    }

    RUL_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER_WITH_TEXTURE(TPMUMeshUtilityApplyHeightMapToMeshSectionCS)

    RUL_DECLARE_SHADER_PARAMETERS_1(
        Texture,
        FShaderResourceParameter,
        FResourceId,
        "HeightTexture", HeightTexture
        )

    RUL_DECLARE_SHADER_PARAMETERS_1(
        Sampler,
        FShaderResourceParameter,
        FResourceId,
        "HeightTextureSampler", HeightTextureSampler
        )

    RUL_DECLARE_SHADER_PARAMETERS_3(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "ColorData", ColorData,
        "UVData", UVData,
        "TangentData", TangentData
        )

    RUL_DECLARE_SHADER_PARAMETERS_1(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutPositionData", OutPositionData
        )

    RUL_DECLARE_SHADER_PARAMETERS_5(
        Value,
        FShaderParameter,
        FParameterId,
        "_VertexCount", Params_VertexCount,
        "_HeightScale", Params_HeightScale,
        "_Flags", Params_Flags,
        "_UVScale", Params_UVScale,
        "_ColorMask", Params_ColorMask
        )
};

IMPLEMENT_SHADER_TYPE(template<>, TPMUMeshUtilityApplyHeightMapToMeshSectionCS<0>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUMeshUtilityApplyHeightMapToMeshSectionCS.usf"), TEXT("ApplyHeightMap"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, TPMUMeshUtilityApplyHeightMapToMeshSectionCS<1>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUMeshUtilityApplyHeightMapToMeshSectionCS.usf"), TEXT("ApplyHeightMap"), SF_Compute);

template<uint32 CompileFlags>
class TPMUMeshUtilityApplySeamlessTiledHeightMapToMeshSectionCS : public FRULBaseComputeShader<256,1,1>
{
public:

    typedef FRULBaseComputeShader<256,1,1> FBaseType;

    DECLARE_SHADER_TYPE(TPMUMeshUtilityApplySeamlessTiledHeightMapToMeshSectionCS, Global);

public:

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return RHISupportsComputeShaders(Parameters.Platform);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FBaseType::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        const uint32 UseUVBuffer = ((CompileFlags&0x01) > 0) ? 1 : 0;
        OutEnvironment.SetDefine(TEXT("PMU_MESH_UTILITY_ASSIGN_HEIGHT_MAP_USE_UV_BUFFER"), UseUVBuffer);
    }

    RUL_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER_WITH_TEXTURE(TPMUMeshUtilityApplySeamlessTiledHeightMapToMeshSectionCS)

    RUL_DECLARE_SHADER_PARAMETERS_3(
        Texture,
        FShaderResourceParameter,
        FResourceId,
        "HeightTextureA", HeightTextureA,
        "HeightTextureB", HeightTextureB,
        "NoiseTexture", NoiseTexture
        )

    RUL_DECLARE_SHADER_PARAMETERS_1(
        Sampler,
        FShaderResourceParameter,
        FResourceId,
        "HeightTextureSampler", HeightTextureSampler
        )

    RUL_DECLARE_SHADER_PARAMETERS_3(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "ColorData", ColorData,
        "UVData", UVData,
        "TangentData", TangentData
        )

    RUL_DECLARE_SHADER_PARAMETERS_1(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutPositionData", OutPositionData
        )

    RUL_DECLARE_SHADER_PARAMETERS_7(
        Value,
        FShaderParameter,
        FParameterId,
        "_VertexCount", Params_VertexCount,
        "_HeightScale", Params_HeightScale,
        "_Flags", Params_Flags,
        "_UVScale", Params_UVScale,
        "_ColorMask", Params_ColorMask,
        "_NoiseUVScale", Params_NoiseUVScale,
        "_HashConstants", Params_HashConstants
        )
};

IMPLEMENT_SHADER_TYPE(template<>, TPMUMeshUtilityApplySeamlessTiledHeightMapToMeshSectionCS<0>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUMeshUtilityApplySeamlessTiledHeightMapToMeshSectionCS.usf"), TEXT("ApplyHeightMap"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, TPMUMeshUtilityApplySeamlessTiledHeightMapToMeshSectionCS<1>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUMeshUtilityApplySeamlessTiledHeightMapToMeshSectionCS.usf"), TEXT("ApplyHeightMap"), SF_Compute);

void UPMUMeshUtility::ExpandSectionBoundsMulti(
    UPMUMeshComponent* MeshComponent,
    const TArray<int32>& SectionIndices,
    const FVector& NegativeExpand,
    const FVector& PositiveExpand
    )
{
    if (! IsValid(MeshComponent))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::ExpandSectionBoundsMulti() ABORTED, INVALID MESH COMPONENT"));
        return;
    }

    for (int32 i : SectionIndices)
    {
        if (MeshComponent->IsValidSection(i))
        {
            MeshComponent->ExpandSectionBounds(i, NegativeExpand, PositiveExpand);
        }
    }
}

void UPMUMeshUtility::CreateGridMeshSectionGPU(
    UPMUMeshComponent* MeshComponent,
    int32 SectionIndex,
    FBox Bounds,
    int32 GridSizeX,
    int32 GridSizeY,
    bool bReverseWinding,
    UTexture* HeightTexture,
    float HeightScale,
    bool bEnableCollision,
    bool bCalculateBounds,
    bool bUpdateRenderState,
    UGWTTickEvent* CallbackEvent
    )
{
	UWorld* World = GEngine->GetWorldFromContextObject(MeshComponent, EGetWorldErrorMode::LogAndReturnNull);

    if (! IsValid(World))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::CreateGridMeshSectionGPU() ABORTED, INVALID WORLD CONTEXT OBJECT"));
        return;
    }
    else
    if (! World->Scene)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::CreateGridMeshSectionGPU() ABORTED, INVALID WORLD SCENE"));
        return;
    }
    else
    if (GridSizeX < 2 || GridSizeY < 2)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::CreateGridMeshSectionGPU() ABORTED, INVALID DIMENSION (%d, %d)"), GridSizeX, GridSizeY);
        return;
    }

    struct FRenderParameter
    {
        ERHIFeatureLevel::Type FeatureLevel;
        // Dimension
        int32 GridSizeX;
        int32 GridSizeY;
        bool bReverseWinding;
        // Height Map
        const FTexture* HeightTexture;
        float HeightScale;
        // Mesh Construction
        UPMUMeshComponent* MeshComponent;
        int32 SectionIndex;
        bool bEnableCollision;
        bool bCalculateBounds;
        bool bUpdateRenderState;
        // Callback Event
        UGWTTickEvent* CallbackEvent;
    };

    FRenderParameter RenderParameter = {
        World->Scene->GetFeatureLevel(),
        // Dimension
        GridSizeX,
        GridSizeY,
        bReverseWinding,
        // Height Map
        HeightTexture ? HeightTexture->Resource : nullptr,
        HeightScale,
        // Mesh Construction
        MeshComponent,
        SectionIndex,
        bEnableCollision,
        bCalculateBounds,
        bUpdateRenderState,
        // Callback Event
        CallbackEvent
        };

    ENQUEUE_RENDER_COMMAND(PMUMeshUtility_CreateGridMeshSectionGPU)(
        [RenderParameter](FRHICommandListImmediate& RHICmdList)
        {
            UPMUMeshComponent* MeshComponent = RenderParameter.MeshComponent;
            int32 SectionIndex = RenderParameter.SectionIndex;
            bool bEnableCollision = RenderParameter.bEnableCollision;
            bool bCalculateBounds = RenderParameter.bCalculateBounds;

            FPMUMeshSectionShared SectionRef(new FPMUMeshSection);
            SectionRef->bSectionVisible = true;
            SectionRef->bEnableFastUVCopy = true;
            SectionRef->bEnableFastTangentsCopy = true;
            SectionRef->bEnableCollision = bEnableCollision;

            UPMUMeshUtility::CreateGridMeshSectionGPU_RT(
                RHICmdList,
                RenderParameter.FeatureLevel,
                *SectionRef,
                RenderParameter.GridSizeX,
                RenderParameter.GridSizeY,
                RenderParameter.bReverseWinding,
                RenderParameter.HeightTexture,
                RenderParameter.HeightScale
                );

            FGWTTickManager& TickManager(IGenericWorkerThread::Get().GetTickManager());
            FGWTTickManager::FTickCallback TickCallback(
                [MeshComponent,SectionIndex,SectionRef,bCalculateBounds]()
                {
                    if (IsValid(MeshComponent))
                    {
                        MeshComponent->CopyMeshSection(
                            SectionIndex,
                            *SectionRef,
                            bCalculateBounds
                            );
                        MeshComponent->UpdateRenderState();
                    }
                } );
            TickManager.EnqueueTickCallback(TickCallback);

            FGWTTickEventRef(RenderParameter.CallbackEvent).EnqueueCallback();
        }
    );
}

void UPMUMeshUtility::CreateGridMeshSectionGPU_RT(
    FRHICommandListImmediate& RHICmdList,
    ERHIFeatureLevel::Type FeatureLevel,
    FPMUMeshSection& Section,
    int32 GridSizeX,
    int32 GridSizeY,
    bool bReverseWinding,
    const FTexture* HeightTexture,
    float HeightScale
    )
{
    check(IsInRenderingThread());

    const FIntPoint Dimension(GridSizeX, GridSizeY);

    // Allocate buffers

    enum { POSITION_TYPE_SIZE = 3*sizeof(float) };  // Position
    enum { TANGENTS_TYPE_SIZE = 2*sizeof(uint32) }; // Tangent and Normal
    enum { TEXCOORD_TYPE_SIZE = sizeof(FRULAlignedVector2D) };
    enum { INDEX_TYPE_SIZE = sizeof(uint32) };
    enum { INDEX_PER_QUAD = 6 };

    const int32 QuadCountX = GridSizeX-1;
    const int32 QuadCountY = GridSizeY-1;
    const int32 QuadCount = QuadCountX * QuadCountY;

    const int32 VCount = GridSizeX * GridSizeY;
    const int32 ICount = QuadCount * INDEX_PER_QUAD;

    FRULRWByteAddressBuffer PositionData;
    FRULRWByteAddressBuffer TangentData;
    FRULRWBuffer TexCoordData;
    FRULRWBuffer IndexData;

    PositionData.Initialize(VCount*POSITION_TYPE_SIZE, BUF_Static);
    TangentData.Initialize(VCount*TANGENTS_TYPE_SIZE, BUF_Static);
    TexCoordData.Initialize(
        TEXCOORD_TYPE_SIZE,
        VCount,
        PF_G32R32F,
        BUF_Static,
        TEXT("UV Data")
        );
    IndexData.Initialize(
        INDEX_TYPE_SIZE,
        ICount,
        PF_R32_UINT,
        BUF_Static,
        TEXT("Index Data")
        );

    // Dispatch compute shader

    RHICmdList.BeginComputePass(TEXT("PMUMeshUtility_CreateGridMeshSection"));
    {
        FTextureRHIParamRef HeightTextureParam = HeightTexture ? HeightTexture->TextureRHI : nullptr;
        FSamplerStateRHIParamRef SamplerLinearClamp = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

        TPMUMeshUtilityCreateGridMeshSectionCS<0>::FBaseType* ComputeShader;

        if (bReverseWinding)
        {
            ComputeShader = *TShaderMapRef<TPMUMeshUtilityCreateGridMeshSectionCS<1>>(GetGlobalShaderMap(FeatureLevel));
        }
        else
        {
            ComputeShader = *TShaderMapRef<TPMUMeshUtilityCreateGridMeshSectionCS<0>>(GetGlobalShaderMap(FeatureLevel));
        }

        ComputeShader->SetShader(RHICmdList);
        ComputeShader->BindTexture(RHICmdList, TEXT("HeightTexture"), TEXT("HeightTextureSampler"), HeightTextureParam, SamplerLinearClamp);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutPositionData"), PositionData.UAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutTangentData"), TangentData.UAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutTexCoordData"), TexCoordData.UAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutIndexData"), IndexData.UAV);
        ComputeShader->SetParameter(RHICmdList, TEXT("_Dimension"), Dimension);
        ComputeShader->SetParameter(RHICmdList, TEXT("_HeightScale"), HeightScale);
        ComputeShader->DispatchAndClear(RHICmdList, GridSizeX, GridSizeY, 1);
    }
    RHICmdList.EndComputePass();

    // Construct section buffers

    void* PositionDataPtr = PositionData.LockReadOnly();
    void* TangentDataPtr  = TangentData.LockReadOnly();
    void* TexCoordDataPtr = TexCoordData.LockReadOnly();
    void* IndexDataPtr    = IndexData.LockReadOnly();

    {
        TArray<FVector>&   SectionPositionData(Section.Positions);
        TArray<uint32>&    SectionTangentData(Section.Tangents);
        TArray<FVector2D>& SectionTexCoordData(Section.UVs);
        TArray<uint32>&    SectionIndexData(Section.Indices);

        SectionPositionData.SetNumUninitialized(VCount);
        SectionTangentData.SetNumUninitialized(VCount*2);
        SectionTexCoordData.SetNumUninitialized(VCount);
        SectionIndexData.SetNumUninitialized(ICount);

        void* SectionPositionDataPtr = SectionPositionData.GetData();
        void* SectionTangentDataPtr = SectionTangentData.GetData();
        void* SectionTexCoordDataPtr = SectionTexCoordData.GetData();
        void* SectionIndexDataPtr = SectionIndexData.GetData();

        uint32 PositionDataSize = VCount * POSITION_TYPE_SIZE;
        uint32 TangentDataSize  = VCount * TANGENTS_TYPE_SIZE;
        uint32 TexCoordDataSize = VCount * TEXCOORD_TYPE_SIZE;
        uint32 IndexDataSize    = ICount * INDEX_TYPE_SIZE;

        FMemory::Memcpy(SectionPositionDataPtr, PositionDataPtr, PositionDataSize);
        FMemory::Memcpy(SectionTangentDataPtr, TangentDataPtr, TangentDataSize);
        FMemory::Memcpy(SectionTexCoordDataPtr, TexCoordDataPtr, TexCoordDataSize);
        FMemory::Memcpy(SectionIndexDataPtr, IndexDataPtr, IndexDataSize);
    }

    IndexData.Unlock();
    TexCoordData.Unlock();
    TangentData.Unlock();
    PositionData.Unlock();
}

void UPMUMeshUtility::FApplyHeightMapInputData::Init(const TArray<FPMUMeshSection*>& Sections, const FPMUMeshApplyHeightParameters& Parameters)
{
    // Reset geometry counts

    VertexCount = 0;
    UVCount = 0;
    TangentCount = 0;
    ColorCount = 0;

    // Gather parameters

    bool bUseUV = Parameters.bUseUV;
    bool bMaskByColor = Parameters.bMaskByColor;
    bool bAlongTangents = Parameters.bAlongTangents;

    // Calculate vertex offsets and total vertex count

    const int32 SectionCount = Sections.Num();

    VertexOffsets.Reset();
    VertexOffsets.SetNumUninitialized(SectionCount);

    for (int32 i=0; i<SectionCount; ++i)
    {
        VertexOffsets[i] = VertexCount;
        VertexCount += Sections[i]->Positions.Num();
        UVCount += Sections[i]->UVs.Num();
        TangentCount += Sections[i]->Tangents.Num();
        ColorCount += Sections[i]->Colors.Num();
    }

    // Generate position input data

    PositionData.Reset(VertexCount);

    for (int32 i=0; i<SectionCount; ++i)
    {
        PositionData.Append(Sections[i]->Positions);
    }

    // Generate UV input data

    bUseUVInput = bUseUV && (UVCount == VertexCount);

    if (bUseUV && bUseUVInput)
    {
        UVData.Reset(UVCount);

        for (int32 i=0; i<SectionCount; ++i)
        {
            UVData.Append(Sections[i]->UVs);
        }
    }
    else
    {
        UVData.Empty();
    }

    // Generate tangent input data

    bUseTangentInput = bAlongTangents && (TangentCount == (2*VertexCount));

    if (bUseTangentInput)
    {
        TangentData.Reset(TangentCount);

        for (int32 i=0; i<SectionCount; ++i)
        {
            TangentData.Append(Sections[i]->Tangents);
        }
    }
    else
    {
        FVector4 TangentX(FVector::ForwardVector, 0);
        FVector4 TangentZ(FVector::UpVector, 1);

        TangentData.Reset(2);
        TangentData.Emplace(FPackedNormal(TangentX).Vector.Packed);
        TangentData.Emplace(FPackedNormal(TangentZ).Vector.Packed);
    }

    // Generate Color input data

    bUseColorInput = bMaskByColor && (ColorCount == VertexCount);

    if (bMaskByColor && bUseColorInput)
    {
        ColorData.Reset(ColorCount);

        for (int32 i=0; i<SectionCount; ++i)
        {
            ColorData.Append(Sections[i]->Colors);
        }
    }
    else
    {
        ColorData.Reset(1);
        ColorData.Emplace(FColor::White);
    }
}

void UPMUMeshUtility::ApplyHeightMapToMeshSections(
    UObject* WorldContextObject,
    TArray<FPMUMeshSectionRef> SectionRefs,
    UTexture* HeightTexture,
    FPMUMeshApplyHeightParameters Parameters,
    UGWTTickEvent* CallbackEvent
    )
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

    if (! IsValid(World))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::ApplyHeightMapToMeshSections() ABORTED, INVALID WORLD CONTEXT OBJECT"));
        return;
    }
    else
    if (! World->Scene)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::ApplyHeightMapToMeshSections() ABORTED, INVALID WORLD SCENE"));
        return;
    }
    else
    if (! IsValid(WorldContextObject))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::ApplyHeightMapToMeshSections() ABORTED, INVALID MESH COMPONENT"));
        return;
    }
    else
    if (! IsValid(HeightTexture))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::ApplyHeightMapToMeshSections() ABORTED, INVALID HEIGHT TEXTURE"));
        return;
    }

    TArray<FPMUMeshSectionRef> FilteredSectionRefs;

    for (const FPMUMeshSectionRef& Section : SectionRefs)
    {
        if (Section.HasValidSection() && Section.SectionPtr->HasGeometry())
        {
            FilteredSectionRefs.Emplace(Section);
        }
    }

    if (FilteredSectionRefs.Num() < 1)
    {
        return;
    }

    struct FRenderParameter
    {
        ERHIFeatureLevel::Type FeatureLevel;
        // Height Map
        const FTexture* HeightTexture;
        // Mesh Construction
        TArray<FPMUMeshSectionRef> SectionRefs;
        // Shader Parameters
        FPMUMeshApplyHeightParameters Parameters;
        // Callback Event
        UGWTTickEvent* CallbackEvent;
    };

    FRenderParameter RenderParameter = {
        World->Scene->GetFeatureLevel(),
        // Height Map
        HeightTexture->Resource,
        // Mesh Construction
        FilteredSectionRefs,
        // Shader Parameters
        Parameters,
        // Callback Event
        CallbackEvent
        };

    ENQUEUE_RENDER_COMMAND(PMUMeshUtility_ApplyHeightMapToMeshSectionMulti)(
        [RenderParameter](FRHICommandListImmediate& RHICmdList)
        {
            TArray<FPMUMeshSection*> Sections;

            if (! RenderParameter.HeightTexture)
            {
                return;
            }

            for (const FPMUMeshSectionRef& SectionRef : RenderParameter.SectionRefs)
            {
                check(SectionRef.HasValidSection());
                check(SectionRef.SectionPtr->HasGeometry());
                Sections.Emplace(SectionRef.SectionPtr);
            }

            if (Sections.Num() > 0)
            {
                ApplyHeightMapToMeshSections_RT(
                    RHICmdList,
                    RenderParameter.FeatureLevel,
                    Sections,
                    *RenderParameter.HeightTexture,
                    RenderParameter.Parameters
                    );
            }

            FGWTTickEventRef(RenderParameter.CallbackEvent).EnqueueCallback();
        }
    );
}

void UPMUMeshUtility::ApplyHeightMapToMeshSections_RT(
    FRHICommandListImmediate& RHICmdList,
    ERHIFeatureLevel::Type FeatureLevel,
    const TArray<FPMUMeshSection*>& Sections,
    const FTexture& HeightTexture,
    const FPMUMeshApplyHeightParameters& Parameters
    )
{
    const int32 SectionCount = Sections.Num();
    FTextureRHIRef HeightTextureRef = HeightTexture.TextureRHI;

    check(Sections.Num() > 0);

    if (! HeightTextureRef)
    {
        return;
    }

    FApplyHeightMapInputData InputData;
    InputData.Init(Sections, Parameters);

    // Get shader parameters

    float HeightScale = Parameters.HeightScale;

    FVector2D UVScale;
    UVScale.X = Parameters.UVScaleX;
    UVScale.Y = Parameters.UVScaleY;

    FVector4 ColorMask;
    ColorMask.X = Parameters.ColorMask.R;
    ColorMask.Y = Parameters.ColorMask.G;
    ColorMask.Z = Parameters.ColorMask.B;
    ColorMask.W = Parameters.ColorMask.A;

    bool bSampleWrap = Parameters.bSampleWrap;

    // Create vertex buffers

    FVertexBufferRHIRef PositionData;
    FUnorderedAccessViewRHIRef PositionDataUAV;

    {
        const uint32 PositionDataSize = InputData.PositionData.GetResourceDataSize();
        FRHIResourceCreateInfo CreateInfo(&InputData.PositionData);
        PositionData = RHICreateVertexBuffer(PositionDataSize, BUF_Static | BUF_UnorderedAccess, CreateInfo);
        PositionDataUAV = RHICreateUnorderedAccessView(PositionData, PF_R32_FLOAT);
    }

    FRULReadBuffer TangentData;
    FRULReadBuffer UVData;
    FRULReadBuffer ColorData;

    // Dispatch shader

    RHICmdList.BeginComputePass(TEXT("ApplyHeightMapToMeshSection"));
    {
        FSamplerStateRHIRef TextureSampler = bSampleWrap
            ? TStaticSamplerState<SF_Bilinear,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI()
            : TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

        TPMUMeshUtilityApplyHeightMapToMeshSectionCS<0>::FBaseType* ComputeShader = nullptr;

        uint32 ShaderFlags = 0;
        ShaderFlags |= InputData.bUseUVInput ? 0x01 : 0;

        switch (ShaderFlags)
        {
            case 0: ComputeShader = *TShaderMapRef<TPMUMeshUtilityApplyHeightMapToMeshSectionCS<0>>(GetGlobalShaderMap(FeatureLevel)); break;
            case 1: ComputeShader = *TShaderMapRef<TPMUMeshUtilityApplyHeightMapToMeshSectionCS<1>>(GetGlobalShaderMap(FeatureLevel)); break;
        };

        check(ComputeShader != nullptr);

        FIntVector4 Flags;
        Flags.X = InputData.bUseTangentInput;
        Flags.Y = InputData.bUseColorInput;

        ComputeShader->SetShader(RHICmdList);

        // Bind height texture
        ComputeShader->BindTexture(RHICmdList, TEXT("HeightTexture"), TEXT("HeightTextureSampler"), HeightTextureRef, TextureSampler);

        // Bind constants
        ComputeShader->SetParameter(RHICmdList, TEXT("_VertexCount"), InputData.VertexCount);
        ComputeShader->SetParameter(RHICmdList, TEXT("_HeightScale"), HeightScale);
        ComputeShader->SetParameter(RHICmdList, TEXT("_Flags"), Flags);
        ComputeShader->SetParameter(RHICmdList, TEXT("_UVScale"), UVScale);
        ComputeShader->SetParameter(RHICmdList, TEXT("_ColorMask"), ColorMask);

        // Bind position output data
        ComputeShader->BindUAV(RHICmdList, TEXT("OutPositionData"), PositionDataUAV);

        check(InputData.HasValidUVCount());
        check(InputData.HasValidTangentCount());
        check(InputData.HasValidColorCount());

        // Bind UV input data
        if (InputData.bUseUVInput)
        {
            UVData.Initialize(InputData.UVData.GetTypeSize(), InputData.UVData.Num(), PF_G32R32F, &InputData.UVData, BUF_Static);
            ComputeShader->BindSRV(RHICmdList, TEXT("UVData"), UVData.SRV);
        }

        // Bind tangent input data
        {
            TangentData.Initialize(InputData.TangentData.GetTypeSize(), InputData.TangentData.Num(), PF_R8G8B8A8_SNORM, &InputData.TangentData, BUF_Static);
            ComputeShader->BindSRV(RHICmdList, TEXT("TangentData"), TangentData.SRV);
        }

        // Bind color input data
        {
            ColorData.Initialize(InputData.ColorData.GetTypeSize(), InputData.ColorData.Num(), PF_B8G8R8A8, &InputData.ColorData, BUF_Static);
            ComputeShader->BindSRV(RHICmdList, TEXT("ColorData"), ColorData.SRV);
        }

        ComputeShader->DispatchAndClear(RHICmdList, InputData.VertexCount, 1, 1);
    }
    RHICmdList.EndComputePass();

    // Copy shader result

    const FVector* PositionDataPtr = reinterpret_cast<FVector*>(RHILockVertexBuffer(PositionData, 0, PositionData->GetSize(), RLM_ReadOnly));

    for (int32 i=0; i<SectionCount; ++i)
    {
        FPMUMeshSection& Section(*Sections[i]);
        int32 Offset = InputData.VertexOffsets[i];

        FVector* DstPosData = Section.Positions.GetData();
        FMemory::Memcpy(DstPosData, PositionDataPtr+Offset, Section.Positions.Num() * Section.Positions.GetTypeSize());
    }

    RHIUnlockVertexBuffer(PositionData);
}

void UPMUMeshUtility::ApplySeamlessTiledHeightMapToMeshSections(
    UObject* WorldContextObject,
    TArray<FPMUMeshSectionRef> SectionRefs,
    FPMUMeshApplySeamlessTileHeightParameters TilingParameters,
    FPMUMeshApplyHeightParameters ShaderParameters,
    UGWTTickEvent* CallbackEvent
    )
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

    if (! IsValid(World))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::ApplySeamlessTiledHeightMapToMeshSections() ABORTED, INVALID WORLD CONTEXT OBJECT"));
        return;
    }
    else
    if (! World->Scene)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::ApplySeamlessTiledHeightMapToMeshSections() ABORTED, INVALID WORLD SCENE"));
        return;
    }
    else
    if (! IsValid(WorldContextObject))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::ApplySeamlessTiledHeightMapToMeshSections() ABORTED, INVALID MESH COMPONENT"));
        return;
    }
    else
    if (! TilingParameters.HasValidInputTextures())
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::ApplySeamlessTiledHeightMapToMeshSections() ABORTED, INVALID INPUT TEXTURES"));
        return;
    }

    TArray<FPMUMeshSectionRef> FilteredSectionRefs;

    for (const FPMUMeshSectionRef& Section : SectionRefs)
    {
        if (Section.HasValidSection() && Section.SectionPtr->HasGeometry())
        {
            FilteredSectionRefs.Emplace(Section);
        }
    }

    if (FilteredSectionRefs.Num() < 1)
    {
        return;
    }

    FPMUMeshApplySeamlessTileHeightParametersRT TilingParametersRT;
    TilingParametersRT = {
        TilingParameters.HeightTextureA->Resource,
        TilingParameters.HeightTextureB->Resource,
        TilingParameters.NoiseTexture->Resource,
        TilingParameters.NoiseUVScaleX,
        TilingParameters.NoiseUVScaleY,
        TilingParameters.HashConstantX,
        TilingParameters.HashConstantY
        };

    struct FRenderParameter
    {
        ERHIFeatureLevel::Type FeatureLevel;
        // Mesh Construction
        TArray<FPMUMeshSectionRef> SectionRefs;
        // Tiling Parameters
        FPMUMeshApplySeamlessTileHeightParametersRT TilingParameters;
        // Shader Parameters
        FPMUMeshApplyHeightParameters ShaderParameters;
        // Callback Event
        UGWTTickEvent* CallbackEvent;
    };

    FRenderParameter RenderParameter = {
        World->Scene->GetFeatureLevel(),
        // Mesh Construction
        FilteredSectionRefs,
        // Tiling Parameters
        TilingParametersRT,
        // Shader Parameters
        ShaderParameters,
        // Callback Event
        CallbackEvent
        };

    ENQUEUE_RENDER_COMMAND(PMUMeshUtility_ApplySeamlessTiledHeightMapToMeshSectionMulti)(
        [RenderParameter](FRHICommandListImmediate& RHICmdList)
        {
            TArray<FPMUMeshSection*> Sections;

            if (! RenderParameter.TilingParameters.HasValidInputTextures())
            {
                return;
            }

            for (const FPMUMeshSectionRef& SectionRef : RenderParameter.SectionRefs)
            {
                check(SectionRef.HasValidSection());
                check(SectionRef.SectionPtr->HasGeometry());
                Sections.Emplace(SectionRef.SectionPtr);
            }

            if (Sections.Num() > 0)
            {
                ApplySeamlessTiledHeightMapToMeshSections_RT(
                    RHICmdList,
                    RenderParameter.FeatureLevel,
                    Sections,
                    RenderParameter.TilingParameters,
                    RenderParameter.ShaderParameters
                    );
            }

            FGWTTickEventRef(RenderParameter.CallbackEvent).EnqueueCallback();
        }
    );
}

void UPMUMeshUtility::ApplySeamlessTiledHeightMapToMeshSections_RT(
    FRHICommandListImmediate& RHICmdList,
    ERHIFeatureLevel::Type FeatureLevel,
    const TArray<FPMUMeshSection*>& Sections,
    const FPMUMeshApplySeamlessTileHeightParametersRT& TilingParameters,
    const FPMUMeshApplyHeightParameters& ShaderParameters
    )
{
    const int32 SectionCount = Sections.Num();

    FTextureRHIRef HeightTextureARef = TilingParameters.HeightTextureA->TextureRHI;
    FTextureRHIRef HeightTextureBRef = TilingParameters.HeightTextureB->TextureRHI;
    FTextureRHIRef NoiseTextureRef = TilingParameters.NoiseTexture->TextureRHI;

    check(Sections.Num() > 0);

    if (! HeightTextureARef || ! HeightTextureBRef || ! NoiseTextureRef)
    {
        return;
    }

    FApplyHeightMapInputData InputData;
    InputData.Init(Sections, ShaderParameters);

    // Get shader parameters

    float HeightScale = ShaderParameters.HeightScale;

    FVector2D UVScale;
    UVScale.X = ShaderParameters.UVScaleX;
    UVScale.Y = ShaderParameters.UVScaleY;

    FVector4 ColorMask;
    ColorMask.X = ShaderParameters.ColorMask.R;
    ColorMask.Y = ShaderParameters.ColorMask.G;
    ColorMask.Z = ShaderParameters.ColorMask.B;
    ColorMask.W = ShaderParameters.ColorMask.A;

    bool bSampleWrap = ShaderParameters.bSampleWrap;

    // Get tiling parameters

    FVector2D NoiseUVScale;
    NoiseUVScale.X = TilingParameters.NoiseUVScaleX;
    NoiseUVScale.Y = TilingParameters.NoiseUVScaleY;

    FVector2D HashConstants;
    HashConstants.X = TilingParameters.HashConstantX;
    HashConstants.Y = TilingParameters.HashConstantY;

    // Create vertex buffers

    FVertexBufferRHIRef PositionData;
    FUnorderedAccessViewRHIRef PositionDataUAV;

    {
        const uint32 PositionDataSize = InputData.PositionData.GetResourceDataSize();
        FRHIResourceCreateInfo CreateInfo(&InputData.PositionData);
        PositionData = RHICreateVertexBuffer(PositionDataSize, BUF_Static | BUF_UnorderedAccess, CreateInfo);
        PositionDataUAV = RHICreateUnorderedAccessView(PositionData, PF_R32_FLOAT);
    }

    FRULReadBuffer TangentData;
    FRULReadBuffer UVData;
    FRULReadBuffer ColorData;

    // Dispatch shader

    RHICmdList.BeginComputePass(TEXT("ApplySeamlessTiledHeightMapToMeshSection"));
    {
        FSamplerStateRHIRef TextureSampler = bSampleWrap
            ? TStaticSamplerState<SF_Bilinear,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI()
            : TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

        TPMUMeshUtilityApplySeamlessTiledHeightMapToMeshSectionCS<0>::FBaseType* ComputeShader = nullptr;

        uint32 ShaderFlags = 0;
        ShaderFlags |= InputData.bUseUVInput ? 0x01 : 0;

        switch (ShaderFlags)
        {
            case 0: ComputeShader = *TShaderMapRef<TPMUMeshUtilityApplySeamlessTiledHeightMapToMeshSectionCS<0>>(GetGlobalShaderMap(FeatureLevel)); break;
            case 1: ComputeShader = *TShaderMapRef<TPMUMeshUtilityApplySeamlessTiledHeightMapToMeshSectionCS<1>>(GetGlobalShaderMap(FeatureLevel)); break;
        };

        check(ComputeShader != nullptr);

        FIntVector4 Flags;
        Flags.X = InputData.bUseTangentInput;
        Flags.Y = InputData.bUseColorInput;

        ComputeShader->SetShader(RHICmdList);

        // Bind height texture
        ComputeShader->BindTexture(RHICmdList, TEXT("HeightTextureA"), TEXT("HeightTextureSampler"), HeightTextureARef, TextureSampler);
        ComputeShader->BindTexture(RHICmdList, TEXT("HeightTextureB"), TEXT("HeightTextureSampler"), HeightTextureBRef, TextureSampler);
        ComputeShader->BindTexture(RHICmdList, TEXT("NoiseTexture"), TEXT("HeightTextureSampler"), NoiseTextureRef, TextureSampler);

        // Bind constants
        ComputeShader->SetParameter(RHICmdList, TEXT("_VertexCount"), InputData.VertexCount);
        ComputeShader->SetParameter(RHICmdList, TEXT("_HeightScale"), HeightScale);
        ComputeShader->SetParameter(RHICmdList, TEXT("_Flags"), Flags);
        ComputeShader->SetParameter(RHICmdList, TEXT("_UVScale"), UVScale);
        ComputeShader->SetParameter(RHICmdList, TEXT("_ColorMask"), ColorMask);
        ComputeShader->SetParameter(RHICmdList, TEXT("_NoiseUVScale"), NoiseUVScale);
        ComputeShader->SetParameter(RHICmdList, TEXT("_HashConstants"), HashConstants);

        // Bind position output data
        ComputeShader->BindUAV(RHICmdList, TEXT("OutPositionData"), PositionDataUAV);

        check(InputData.HasValidUVCount());
        check(InputData.HasValidTangentCount());
        check(InputData.HasValidColorCount());

        // Bind UV input data
        if (InputData.bUseUVInput)
        {
            UVData.Initialize(InputData.UVData.GetTypeSize(), InputData.UVData.Num(), PF_G32R32F, &InputData.UVData, BUF_Static);
            ComputeShader->BindSRV(RHICmdList, TEXT("UVData"), UVData.SRV);
        }

        // Bind tangent input data
        {
            TangentData.Initialize(InputData.TangentData.GetTypeSize(), InputData.TangentData.Num(), PF_R8G8B8A8_SNORM, &InputData.TangentData, BUF_Static);
            ComputeShader->BindSRV(RHICmdList, TEXT("TangentData"), TangentData.SRV);
        }

        // Bind color input data
        {
            ColorData.Initialize(InputData.ColorData.GetTypeSize(), InputData.ColorData.Num(), PF_B8G8R8A8, &InputData.ColorData, BUF_Static);
            ComputeShader->BindSRV(RHICmdList, TEXT("ColorData"), ColorData.SRV);
        }

        ComputeShader->DispatchAndClear(RHICmdList, InputData.VertexCount, 1, 1);
    }
    RHICmdList.EndComputePass();

    // Copy shader result

    const FVector* PositionDataPtr = reinterpret_cast<FVector*>(RHILockVertexBuffer(PositionData, 0, PositionData->GetSize(), RLM_ReadOnly));

    for (int32 i=0; i<SectionCount; ++i)
    {
        FPMUMeshSection& Section(*Sections[i]);
        int32 Offset = InputData.VertexOffsets[i];

        FVector* DstPosData = Section.Positions.GetData();
        FMemory::Memcpy(DstPosData, PositionDataPtr+Offset, Section.Positions.Num() * Section.Positions.GetTypeSize());
    }

    RHIUnlockVertexBuffer(PositionData);
}
