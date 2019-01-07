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

#include "Grid/PMUGridUtility.h"
#include "Shaders/PMUShaderDefinitions.h"
#include "GWTAsyncTypes.h"

template<uint32 bReverseWinding>
class FPMUGridUtilityCreateGPUMeshSectionCS : public FPMUBaseComputeShader<16,16,1>
{
public:

    typedef FPMUBaseComputeShader<16,16,1> FBaseType;

    DECLARE_SHADER_TYPE(FPMUGridUtilityCreateGPUMeshSectionCS, Global);

public:

    static bool ShouldCache(EShaderPlatform Platform)
    {
        return RHISupportsComputeShaders(Platform);
    }

    static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
    {
        FBaseType::ModifyCompilationEnvironment(Platform, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("PMU_GRID_UTILITY_CREATE_GPU_MESH_SECTION_USE_REVERSE_WINDING"), bReverseWinding);
    }

    PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER_WITH_TEXTURE(FPMUGridUtilityCreateGPUMeshSectionCS)

    PMU_DECLARE_SHADER_PARAMETERS_1(
        Texture,
        FShaderResourceParameter,
        FResourceId,
        "HeightMap", HeightMap
        )

    PMU_DECLARE_SHADER_PARAMETERS_1(
        Sampler,
        FShaderResourceParameter,
        FResourceId,
        "HeightMapSampler", HeightMapSampler
        )

    PMU_DECLARE_SHADER_PARAMETERS_0(SRV,,)

    PMU_DECLARE_SHADER_PARAMETERS_2(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutVertexData", OutVertexData,
        "OutIndexData", OutIndexData
        )

    PMU_DECLARE_SHADER_PARAMETERS_3(
        Value,
        FShaderParameter,
        FParameterId,
        "_Dimension",       Params_Dimension,
        "_HeightScale",     Params_HeightScale,
        "_bReverseWinding", Params_bReverseWinding
        )
};

IMPLEMENT_SHADER_TYPE(template<>, FPMUGridUtilityCreateGPUMeshSectionCS<0>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUGridUtilityCreateGPUMeshSectionCS.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FPMUGridUtilityCreateGPUMeshSectionCS<1>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUGridUtilityCreateGPUMeshSectionCS.usf"), TEXT("MainCS"), SF_Compute);

void UPMUGridUtility::CreateGPUMeshSection(
    ERHIFeatureLevel::Type FeatureLevel,
    FPMUGridData& Grid,
    FPMUMeshSectionResource& Section,
    FPMUShaderTextureParameterInputResource TextureInput,
    float HeightScale,
    float BoundsHeightMin,
    float BoundsHeightMax,
    bool bReverseWinding,
    UGWTTickEvent* CallbackEvent
    )
{
    const FIntPoint Dimension(Grid.Dimension);
    const int32 DimX(Dimension.X);
    const int32 DimY(Dimension.Y);

    const FVector BoundsMin(0, 0, BoundsHeightMin);
    const FVector BoundsMax(DimX-1, DimY-1, BoundsHeightMax);
    const FBox Bounds(BoundsMin, BoundsMax);

    // Invalid parameters, abort
    if (DimX < 2 || DimY < 2)
    {
        return;
    }

    struct FRenderParameter
    {
        ERHIFeatureLevel::Type FeatureLevel;
        FPMUGridData* Grid;
        FPMUMeshSectionResource* Section;
        FPMUShaderTextureParameterInputResource TextureInput;
        float HeightScale;
        FBox Bounds;
        bool bReverseWinding;
        FGWTTickEventRef CallbackRef;
    };

    FRenderParameter RenderParameter = {
        FeatureLevel,
        &Grid,
        &Section,
        TextureInput,
        HeightScale,
        Bounds,
        bReverseWinding,
        { CallbackEvent }
        };

    ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
        FExecuteRenderThread,
        FRenderParameter, RenderParameter, RenderParameter,
        {
            UPMUGridUtility::CreateGPUMeshSection_RT(
                RHICmdList,
                RenderParameter.FeatureLevel,
                *RenderParameter.Grid,
                *RenderParameter.Section,
                RenderParameter.TextureInput,
                RenderParameter.HeightScale,
                RenderParameter.Bounds,
                RenderParameter.bReverseWinding
                );
            RenderParameter.CallbackRef.EnqueueCallback();
        } );
}

void UPMUGridUtility::CreateGPUMeshSection_RT(
    FRHICommandListImmediate& RHICmdList,
    ERHIFeatureLevel::Type FeatureLevel,
    FPMUGridData& Grid,
    FPMUMeshSectionResource& Section,
    FPMUShaderTextureParameterInputResource& TextureInput,
    float HeightScale,
    FBox Bounds,
    bool bReverseWinding
    )
{
    check(IsInRenderingThread());

    FTexture2DRHIParamRef HeightTexture = TextureInput.GetTextureParamRef_RT();

    const FIntPoint Dimension(Grid.Dimension);
    const int32 DimX(Dimension.X);
    const int32 DimY(Dimension.Y);

    // Allocate vertex buffer

    const int32 VertexCount = DimX * DimY;
    const int32 VertexTypeSize = sizeof(FDynamicMeshVertex);
    const int32 VertexBufferSize = VertexCount * VertexTypeSize;

    FRHIResourceCreateInfo VertexBufferCreateInfo;

    FStructuredBufferRHIRef VertexBufferRHI = RHICreateStructuredBuffer(
        VertexTypeSize,
        VertexBufferSize,
        BUF_Static | BUF_UnorderedAccess,
        VertexBufferCreateInfo
        );

    FUnorderedAccessViewRHIRef VertexBufferUAV = RHICreateUnorderedAccessView(VertexBufferRHI, false, false);

    // Allocate index buffer

    enum { INDEX_COUNT_PER_QUAD = 6 };

    const int32 QuadCountX = DimX-1;
    const int32 QuadCountY = DimY-1;
    const int32 QuadCount = QuadCountX * QuadCountY;
    const int32 IndexCount = QuadCount * INDEX_COUNT_PER_QUAD;

    const int32 IndexTypeSize = sizeof(uint32);
    const int32 IndexBufferSize = IndexCount * IndexTypeSize;
    FRHIResourceCreateInfo IndexBufferCreateInfo;

    FVertexBufferRHIRef IndexBufferRHI = RHICreateVertexBuffer(
        IndexBufferSize,
        BUF_Static | BUF_UnorderedAccess,
        IndexBufferCreateInfo
        );

    FUnorderedAccessViewRHIRef IndexBufferUAV = RHICreateUnorderedAccessView(
        IndexBufferRHI,
        PF_R32_UINT
        );

    // Clear render target in case height texture has been set as render target
    SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());

    // Dispatch compute shader

    {
        FSamplerStateRHIParamRef SamplerLinearClamp = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

        FPMUGridUtilityCreateGPUMeshSectionCS<0>::FBaseType* ComputeShader;

        if (bReverseWinding)
        {
            ComputeShader = *TShaderMapRef<FPMUGridUtilityCreateGPUMeshSectionCS<0>>(GetGlobalShaderMap(FeatureLevel));
        }
        else
        {
            ComputeShader = *TShaderMapRef<FPMUGridUtilityCreateGPUMeshSectionCS<1>>(GetGlobalShaderMap(FeatureLevel));
        }

        ComputeShader->SetShader(RHICmdList);
        ComputeShader->BindTexture(RHICmdList, TEXT("HeightMap"), TEXT("HeightMapSampler"), HeightTexture, SamplerLinearClamp);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutVertexData"), VertexBufferUAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutIndexData"), IndexBufferUAV);
        ComputeShader->SetParameter(RHICmdList, TEXT("_Dimension"), Dimension);
        ComputeShader->SetParameter(RHICmdList, TEXT("_HeightScale"), HeightScale);
        ComputeShader->SetParameter(RHICmdList, TEXT("_bReverseWinding"), bReverseWinding);
        ComputeShader->DispatchAndClear(RHICmdList, DimX, DimY, 1);
    }

    // Construct gpu section buffers

    // Copy Vertex Buffer
    {
        FRHIResourceCreateInfo CreateInfo;

        auto& Vertices(Section.Buffer.GetVBArray());
        Vertices.SetNumUninitialized(VertexCount);

        void* SectionVertexBufferData = Vertices.GetData();
        void* VertexBufferData = RHILockStructuredBuffer(VertexBufferRHI, 0, VertexBufferSize, RLM_ReadOnly);

        FMemory::Memcpy(SectionVertexBufferData, VertexBufferData, VertexBufferSize);

        RHIUnlockStructuredBuffer(VertexBufferRHI);
    }

    // Copy Index Buffer
    {
        FRHIResourceCreateInfo CreateInfo;

        auto& Indices(Section.Buffer.GetIBArray());
        Indices.SetNumUninitialized(IndexCount);

        void* SectionIndexBufferData = Indices.GetData();
        void* IndexBufferData = RHILockVertexBuffer(IndexBufferRHI, 0, IndexBufferSize, RLM_ReadOnly);

        FMemory::Memcpy(SectionIndexBufferData, IndexBufferData, IndexBufferSize);

        RHIUnlockVertexBuffer(IndexBufferRHI);
    }

    Section.VertexCount = VertexCount;
    Section.IndexCount  = IndexCount;
    Section.LocalBounds = Bounds;
}
