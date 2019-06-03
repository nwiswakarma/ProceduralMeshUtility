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
class TPMUMeshUtilityAssignHeightMapToMeshSectionCS : public FRULBaseComputeShader<256,1,1>
{
public:

    typedef FRULBaseComputeShader<256,1,1> FBaseType;

    DECLARE_SHADER_TYPE(TPMUMeshUtilityAssignHeightMapToMeshSectionCS, Global);

public:

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return RHISupportsComputeShaders(Parameters.Platform);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FBaseType::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        const uint32 UseUVBuffer  = ((CompileFlags&0x01) > 0) ? 1 : 0;
        const uint32 UseColorMask = ((CompileFlags&0x02) > 0) ? 1 : 0;
        OutEnvironment.SetDefine(TEXT("PMU_MESH_UTILITY_ASSIGN_HEIGHT_MAP_USE_UV_BUFFER"), UseUVBuffer);
        OutEnvironment.SetDefine(TEXT("PMU_MESH_UTILITY_ASSIGN_HEIGHT_MAP_USE_COLOR_BUFFER_MASK"), UseColorMask);
    }

    RUL_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER_WITH_TEXTURE(TPMUMeshUtilityAssignHeightMapToMeshSectionCS)

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

    RUL_DECLARE_SHADER_PARAMETERS_2(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "ColorData", ColorData,
        "UVData", UVData
        )

    RUL_DECLARE_SHADER_PARAMETERS_1(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutPositionData", OutPositionData
        )

    RUL_DECLARE_SHADER_PARAMETERS_4(
        Value,
        FShaderParameter,
        FParameterId,
        "_VertexCount", Params_VertexCount,
        "_HeightScale", Params_HeightScale,
        "_InverseMask", Params_InverseMask,
        "_PositionToUVScale", Params_PositionToUVScale
        )
};

#define IMPLEMENT_PMUMeshUtilityAssignHeightMapToMeshSectionCS(FLAGS)\
IMPLEMENT_SHADER_TYPE(template<>, TPMUMeshUtilityAssignHeightMapToMeshSectionCS<FLAGS>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUMeshUtilityAssignHeightMapToMeshSectionCS.usf"), TEXT("AssignSingleCS"), SF_Compute);

IMPLEMENT_PMUMeshUtilityAssignHeightMapToMeshSectionCS(0)
IMPLEMENT_PMUMeshUtilityAssignHeightMapToMeshSectionCS(1)
IMPLEMENT_PMUMeshUtilityAssignHeightMapToMeshSectionCS(2)
IMPLEMENT_PMUMeshUtilityAssignHeightMapToMeshSectionCS(3)

#undef IMPLEMENT_PMUMeshUtilityAssignHeightMapToMeshSectionCS

void UPMUMeshUtility::CreateGridMeshSectionGPU(
    UObject* WorldContextObject,
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
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

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

void UPMUMeshUtility::AssignHeightMapToMeshSection(
    UObject* WorldContextObject,
    UPMUMeshComponent* MeshComponent,
    int32 SectionIndex,
    UTexture* HeightTexture,
    float HeightScale,
    bool bUseUV,
    float PositionToUVScaleX,
    float PositionToUVScaleY,
    bool bMaskByColor,
    bool bInverseColorMask,
    UGWTTickEvent* CallbackEvent
    )
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

    if (! IsValid(World))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::AssignHeightMapToMeshSection() ABORTED, INVALID WORLD CONTEXT OBJECT"));
        return;
    }
    else
    if (! World->Scene)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::AssignHeightMapToMeshSection() ABORTED, INVALID WORLD SCENE"));
        return;
    }
    else
    if (! IsValid(MeshComponent))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::AssignHeightMapToMeshSection() ABORTED, INVALID MESH COMPONENT"));
        return;
    }
    else
    if (! MeshComponent->IsValidNonEmptySection(SectionIndex))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::AssignHeightMapToMeshSection() ABORTED, INVALID MESH SECTION"));
        return;
    }
    else
    if (! IsValid(HeightTexture))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::AssignHeightMapToMeshSection() ABORTED, INVALID HEIGHT TEXTURE"));
        return;
    }

    struct FRenderParameter
    {
        ERHIFeatureLevel::Type FeatureLevel;
        // Height Map
        const FTexture* HeightTexture;
        float HeightScale;
        // Mesh Construction
        FPMUMeshSceneProxy* SceneProxy;
        int32 SectionIndex;
        // Buffer Setup
        bool bUseUV;
        float PositionToUVScaleX;
        float PositionToUVScaleY;
        bool bMaskByColor;
        bool bInverseColorMask;
        // Callback Event
        UGWTTickEvent* CallbackEvent;
    };

    FRenderParameter RenderParameter = {
        World->Scene->GetFeatureLevel(),
        // Height Map
        HeightTexture->Resource,
        HeightScale,
        // Mesh Construction
        MeshComponent->GetSceneProxy(),
        SectionIndex,
        // Buffer Setup
        bUseUV,
        PositionToUVScaleX,
        PositionToUVScaleY,
        bMaskByColor,
        bInverseColorMask,
        // Callback Event
        CallbackEvent
        };

    ENQUEUE_RENDER_COMMAND(PMUMeshUtility_AssignHeightMapToMeshSection)(
        [RenderParameter](FRHICommandListImmediate& RHICmdList)
        {
            FPMUMeshSceneProxy* SceneProxy(RenderParameter.SceneProxy);
            int32 SectionIndex = RenderParameter.SectionIndex;

            if (! SceneProxy)
            {
                UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::AssignHeightMapToMeshSection() ABORTED, [RT] INVALID SCENE PROXY"));
                return;
            }

            FPMUMeshProxySection* Section = SceneProxy->GetSection(SectionIndex);

            if (! Section)
            {
                UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::AssignHeightMapToMeshSection() ABORTED, [RT] INVALID SECTION"));
                return;
            }

            if (! RenderParameter.HeightTexture)
            {
                return;
            }

            TArray<FPMUMeshProxySection*> Sections;
            Sections.Emplace(Section);

            AssignHeightMapToMeshSection_RT(
                RHICmdList,
                RenderParameter.FeatureLevel,
                Sections,
                *RenderParameter.HeightTexture,
                RenderParameter.HeightScale,
                RenderParameter.bUseUV,
                RenderParameter.PositionToUVScaleX,
                RenderParameter.PositionToUVScaleY,
                RenderParameter.bMaskByColor,
                RenderParameter.bInverseColorMask
                );

            FGWTTickEventRef(RenderParameter.CallbackEvent).EnqueueCallback();
        }
    );
}

void UPMUMeshUtility::AssignHeightMapToMeshSectionMulti(
    UObject* WorldContextObject,
    UPMUMeshComponent* MeshComponent,
    TArray<int32> SectionIndices,
    UTexture* HeightTexture,
    float HeightScale,
    bool bUseUV,
    float PositionToUVScaleX,
    float PositionToUVScaleY,
    bool bMaskByColor,
    bool bInverseColorMask,
    UGWTTickEvent* CallbackEvent
    )
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

    if (! IsValid(World))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::AssignHeightMapToMeshSectionMulti() ABORTED, INVALID WORLD CONTEXT OBJECT"));
        return;
    }
    else
    if (! World->Scene)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::AssignHeightMapToMeshSectionMulti() ABORTED, INVALID WORLD SCENE"));
        return;
    }
    else
    if (! IsValid(MeshComponent))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::AssignHeightMapToMeshSectionMulti() ABORTED, INVALID MESH COMPONENT"));
        return;
    }
    else
    if (! IsValid(HeightTexture))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::AssignHeightMapToMeshSectionMulti() ABORTED, INVALID HEIGHT TEXTURE"));
        return;
    }

    TArray<int32> FilteredSectionIndices;

    for (int32 i : SectionIndices)
    {
        if (MeshComponent->IsValidNonEmptySection(i))
        {
            FilteredSectionIndices.Emplace(i);
        }
    }

    if (FilteredSectionIndices.Num() < 1)
    {
        return;
    }

    struct FRenderParameter
    {
        ERHIFeatureLevel::Type FeatureLevel;
        // Height Map
        const FTexture* HeightTexture;
        float HeightScale;
        // Mesh Construction
        FPMUMeshSceneProxy* SceneProxy;
        TArray<int32> SectionIndices;
        // Buffer Setup
        bool bUseUV;
        float PositionToUVScaleX;
        float PositionToUVScaleY;
        bool bMaskByColor;
        bool bInverseColorMask;
        // Callback Event
        UGWTTickEvent* CallbackEvent;
    };

    FRenderParameter RenderParameter = {
        World->Scene->GetFeatureLevel(),
        // Height Map
        HeightTexture->Resource,
        HeightScale,
        // Mesh Construction
        MeshComponent->GetSceneProxy(),
        FilteredSectionIndices,
        // Buffer Setup
        bUseUV,
        PositionToUVScaleX,
        PositionToUVScaleY,
        bMaskByColor,
        bInverseColorMask,
        // Callback Event
        CallbackEvent
        };

    ENQUEUE_RENDER_COMMAND(PMUMeshUtility_AssignHeightMapToMeshSectionMulti)(
        [RenderParameter](FRHICommandListImmediate& RHICmdList)
        {
            FPMUMeshSceneProxy* SceneProxy(RenderParameter.SceneProxy);
            TArray<FPMUMeshProxySection*> Sections;

            if (! SceneProxy)
            {
                UE_LOG(LogPMU,Warning, TEXT("UPMUMeshUtility::AssignHeightMapToMeshSectionMulti() ABORTED, [RT] INVALID SCENE PROXY"));
                return;
            }
            else
            if (! RenderParameter.HeightTexture)
            {
                return;
            }

            for (int32 i : RenderParameter.SectionIndices)
            {
                FPMUMeshProxySection* Section = SceneProxy->GetSection(i);

                if (Section)
                {
                    Sections.Emplace(Section);
                }
            }

            if (Sections.Num() > 0)
            {
                AssignHeightMapToMeshSection_RT(
                    RHICmdList,
                    RenderParameter.FeatureLevel,
                    Sections,
                    *RenderParameter.HeightTexture,
                    RenderParameter.HeightScale,
                    RenderParameter.bUseUV,
                    RenderParameter.PositionToUVScaleX,
                    RenderParameter.PositionToUVScaleY,
                    RenderParameter.bMaskByColor,
                    RenderParameter.bInverseColorMask
                    );
            }

            FGWTTickEventRef(RenderParameter.CallbackEvent).EnqueueCallback();
        }
    );
}

void UPMUMeshUtility::AssignHeightMapToMeshSection_RT(
    FRHICommandListImmediate& RHICmdList,
    ERHIFeatureLevel::Type FeatureLevel,
    const TArray<FPMUMeshProxySection*>& Sections,
    const FTexture& HeightTexture,
    float HeightScale,
    bool bUseUV,
    float PositionToUVScaleX,
    float PositionToUVScaleY,
    bool bMaskByColor,
    bool bInverseColorMask
    )
{
    FTextureRHIParamRef HeightTextureParam = HeightTexture.TextureRHI;

    if (! HeightTextureParam || Sections.Num() < 1)
    {
        return;
    }

    const int32 SectionCount = Sections.Num();

    for (int32 i=0; i<SectionCount; ++i)
    {
        FPMUMeshProxySection& Section(*Sections[i]);
        FPMUPositionVertexBuffer& PositionBuffer(Section.PositionVertexBuffer);
        FPMUColorVertexBuffer& ColorBuffer(Section.ColorVertexBuffer);
        auto& UVBuffer(Section.StaticMeshVertexBuffer.TexCoordVertexBuffer);

        check(PositionBuffer.IsInitialized());

        const FUnorderedAccessViewRHIParamRef PositionUAV = PositionBuffer.GetUAV();
        FShaderResourceViewRHIRef UVSRV;

        if (! PositionUAV)
        {
            return;
        }

        const uint32 NumVertices = PositionBuffer.GetNumVertices();

        RHICmdList.BeginComputePass(TEXT("AssignHeightMapToMeshSection"));
        {
            FSamplerStateRHIParamRef TextureSampler = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

            FVector2D PositionToUVScale = FVector2D::UnitVector;
            PositionToUVScale.X = PositionToUVScaleX;
            PositionToUVScale.Y = PositionToUVScaleY;

            TPMUMeshUtilityAssignHeightMapToMeshSectionCS<0>::FBaseType* ComputeShader;

            if (bUseUV && bMaskByColor)
            {
                ComputeShader = *TShaderMapRef<TPMUMeshUtilityAssignHeightMapToMeshSectionCS<3>>(GetGlobalShaderMap(FeatureLevel));
            }
            else
            if (bUseUV)
            {
                ComputeShader = *TShaderMapRef<TPMUMeshUtilityAssignHeightMapToMeshSectionCS<1>>(GetGlobalShaderMap(FeatureLevel));
            }
            else
            if (bMaskByColor)
            {
                ComputeShader = *TShaderMapRef<TPMUMeshUtilityAssignHeightMapToMeshSectionCS<2>>(GetGlobalShaderMap(FeatureLevel));
            }
            else
            {
                ComputeShader = *TShaderMapRef<TPMUMeshUtilityAssignHeightMapToMeshSectionCS<0>>(GetGlobalShaderMap(FeatureLevel));
            }

            ComputeShader->SetShader(RHICmdList);
            ComputeShader->BindTexture(RHICmdList, TEXT("HeightTexture"), TEXT("HeightTextureSampler"), HeightTextureParam, TextureSampler);
            ComputeShader->BindUAV(RHICmdList, TEXT("OutPositionData"), PositionUAV);
            ComputeShader->SetParameter(RHICmdList, TEXT("_VertexCount"), NumVertices);
            ComputeShader->SetParameter(RHICmdList, TEXT("_HeightScale"), HeightScale);
            ComputeShader->SetParameter(RHICmdList, TEXT("_PositionToUVScale"), PositionToUVScale);

            if (bUseUV)
            {
                UVSRV = RHICreateShaderResourceView(UVBuffer.VertexBufferRHI, 8, PF_G32R32F);
                ComputeShader->BindSRV(RHICmdList, TEXT("UVData"), UVSRV);
            }

            if (bMaskByColor)
            {
                const FShaderResourceViewRHIParamRef ColorSRV = ColorBuffer.GetColorComponentsSRV();
                ComputeShader->BindSRV(RHICmdList, TEXT("ColorData"), ColorSRV);
                ComputeShader->SetParameter(RHICmdList, TEXT("_InverseMask"), bInverseColorMask ? 1.f : 0.f);
            }

            ComputeShader->DispatchAndClear(RHICmdList, NumVertices, 1, 1);
        }
        RHICmdList.EndComputePass();
    }
}
