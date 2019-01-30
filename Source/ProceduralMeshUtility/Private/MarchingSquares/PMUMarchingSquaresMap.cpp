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

#include "PMUMarchingSquaresMap.h"

#include "DynamicMeshBuilder.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "ShaderParameters.h"
#include "ShaderCore.h"
#include "ShaderParameterUtils.h"
#include "UniformBuffer.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/TextureRenderTarget2D.h"

#include "ProceduralMeshUtility.h"
#include "Shaders/PMUShaderDefinitions.h"
#include "Shaders/PMUPrefixSumScan.h"

#include "EarcutTypes.h"

DEFINE_LOG_CATEGORY(LogMSq);
DEFINE_LOG_CATEGORY(UntMSq);

// COMPUTE SHADER DEFINITIONS

template<uint32 bGenerateWalls>
class TPMUMarchingSquaresMapWriteCellCaseCS : public FPMUBaseComputeShader<16,16,1>
{
public:

    typedef FPMUBaseComputeShader<16,16,1> FBaseType;

    DECLARE_SHADER_TYPE(TPMUMarchingSquaresMapWriteCellCaseCS, Global);

public:

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return RHISupportsComputeShaders(Parameters.Platform);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FBaseType::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("PMU_MARCHING_SQUARES_GENERATE_WALLS"), bGenerateWalls);
    }

    PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER(TPMUMarchingSquaresMapWriteCellCaseCS)

    PMU_DECLARE_SHADER_PARAMETERS_1(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "VoxelStateData", VoxelStateData
        )

    PMU_DECLARE_SHADER_PARAMETERS_3(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutCellCaseData",  OutCellCaseData,
        "OutGeomCountData", OutGeomCountData,
        "OutDebugTexture",  OutDebugTexture
        )

    PMU_DECLARE_SHADER_PARAMETERS_3(
        Value,
        FShaderParameter,
        FParameterId,
        "_GDim",     Params_GDim,
        "_LDim",     Params_LDim,
        "_FillType", Params_FillType
        )
};

class FPMUMarchingSquaresMapWriteCellCompactIdCS : public FPMUBaseComputeShader<16,16,1>
{
    typedef FPMUBaseComputeShader<16,16,1> FBaseType;

    PMU_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS(
        FPMUMarchingSquaresMapWriteCellCompactIdCS,
        Global,
        RHISupportsComputeShaders(Parameters.Platform)
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "GeomCountData", GeomCountData,
        "OffsetData",    OffsetData
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutFillCellIdData", OutFillCellIdData,
        "OutEdgeCellIdData", OutEdgeCellIdData
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        Value,
        FShaderParameter,
        FParameterId,
        "_GDim", Params_GDim,
        "_LDim", Params_LDim
        )
};

template<uint32 bGenerateWalls>
class TPMUMarchingSquaresMapTriangulateFillCellCS : public FPMUBaseComputeShader<256,1,1>
{
public:

    typedef FPMUBaseComputeShader<256,1,1> FBaseType;

    DECLARE_SHADER_TYPE(TPMUMarchingSquaresMapTriangulateFillCellCS, Global);

public:

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return RHISupportsComputeShaders(Parameters.Platform);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FBaseType::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("PMU_MARCHING_SQUARES_GENERATE_WALLS"), bGenerateWalls);
    }

    PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER_WITH_TEXTURE(TPMUMarchingSquaresMapTriangulateFillCellCS)

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
        "samplerHeightMap", SurfaceHeightMapSampler
        )

    PMU_DECLARE_SHADER_PARAMETERS_3(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "OffsetData",     OffsetData,
        "SumData",        SumData,
        "FillCellIdData", FillCellIdData
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutVertexData", OutVertexData,
        "OutIndexData",  OutIndexData
        )

    PMU_DECLARE_SHADER_PARAMETERS_9(
        Value,
        FShaderParameter,
        FParameterId,
        "_GDim",          Params_GDim,
        "_LDim",          Params_LDim,
        "_GeomCount",     Params_GeomCount,
        "_SampleLevel",   Params_SampleLevel,
        "_FillCellCount", Params_FillCellCount,
        "_BlockOffset",   Params_BlockOffset,
        "_HeightScale",   Params_HeightScale,
        "_HeightOffset",  Params_HeightOffset,
        "_Color",         Params_Color
        )
};

template<uint32 bGenerateWalls>
class TPMUMarchingSquaresMapTriangulateEdgeCellCS : public FPMUBaseComputeShader<256,1,1>
{
public:

    typedef FPMUBaseComputeShader<256,1,1> FBaseType;

    DECLARE_SHADER_TYPE(TPMUMarchingSquaresMapTriangulateEdgeCellCS, Global);

public:

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return RHISupportsComputeShaders(Parameters.Platform);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FBaseType::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("PMU_MARCHING_SQUARES_GENERATE_WALLS"), bGenerateWalls);
    }

    PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER_WITH_TEXTURE(TPMUMarchingSquaresMapTriangulateEdgeCellCS)

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
        "samplerHeightMap", SurfaceHeightMapSampler
        )

    PMU_DECLARE_SHADER_PARAMETERS_5(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "VoxelFeatureData", VoxelFeatureData,
        "OffsetData",       OffsetData,
        "SumData",          SumData,
        "EdgeCellIdData",   EdgeCellIdData,
        "CellCaseData",     CellCaseData
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutVertexData", OutVertexData,
        "OutIndexData",  OutIndexData
        )

    PMU_DECLARE_SHADER_PARAMETERS_9(
        Value,
        FShaderParameter,
        FParameterId,
        "_GDim",          Params_GDim,
        "_LDim",          Params_LDim,
        "_GeomCount",     Params_GeomCount,
        "_SampleLevel",   Params_SampleLevel,
        "_EdgeCellCount", Params_FillCellCount,
        "_BlockOffset",   Params_BlockOffset,
        "_HeightScale",   Params_HeightScale,
        "_HeightOffset",  Params_HeightOffset,
        "_Color",         Params_Color
        )
};

IMPLEMENT_SHADER_TYPE(template<>, TPMUMarchingSquaresMapWriteCellCaseCS<0>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUMarchingSquaresCS.usf"), TEXT("CellWriteCaseKernel"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, TPMUMarchingSquaresMapWriteCellCaseCS<1>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUMarchingSquaresCS.usf"), TEXT("CellWriteCaseKernel"), SF_Compute);

IMPLEMENT_SHADER_TYPE(, FPMUMarchingSquaresMapWriteCellCompactIdCS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUMarchingSquaresCS.usf"), TEXT("CellWriteCompactIdKernel"), SF_Compute);

IMPLEMENT_SHADER_TYPE(template<>, TPMUMarchingSquaresMapTriangulateFillCellCS<0>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUMarchingSquaresCS.usf"), TEXT("TriangulateFillCell"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, TPMUMarchingSquaresMapTriangulateFillCellCS<1>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUMarchingSquaresCS.usf"), TEXT("TriangulateFillCell"), SF_Compute);

IMPLEMENT_SHADER_TYPE(template<>, TPMUMarchingSquaresMapTriangulateEdgeCellCS<0>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUMarchingSquaresCS.usf"), TEXT("TriangulateEdgeCell"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, TPMUMarchingSquaresMapTriangulateEdgeCellCS<1>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUMarchingSquaresCS.usf"), TEXT("TriangulateEdgeCell"), SF_Compute);

void FPMUMarchingSquaresMap::SetDimension(FIntPoint InDimension)
{
    if (Dimension_GT != InDimension)
    {
        Dimension_GT = InDimension;
    }
}

void FPMUMarchingSquaresMap::SetHeightMap(FTexture2DRHIParamRef InHeightMap)
{
    HeightMap = InHeightMap;
}

void FPMUMarchingSquaresMap::ClearMap()
{
    ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
        FMarchingSquaresMap_ClearMap,
        FPMUMarchingSquaresMap*, Map, this,
        {
            Map->ClearMap_RT(RHICmdList);
        } );
}

void FPMUMarchingSquaresMap::InitializeVoxelData()
{
    check(HasValidDimension());

    ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
        FMarchingSquaresMap_InitializeVoxelData,
        FPMUMarchingSquaresMap*, Map, this,
        FIntPoint, InDimension, Dimension_GT,
        {
            Map->InitializeVoxelData_RT(RHICmdList, InDimension);
        } );
}

void FPMUMarchingSquaresMap::ClearMap_RT(FRHICommandListImmediate& RHICmdList)
{
    VoxelStateData.Release();
    VoxelFeatureData.Release();

    DebugRTTRHI = nullptr;
	DebugTextureRHI.SafeRelease();
    DebugTextureUAV.SafeRelease();
}

void FPMUMarchingSquaresMap::InitializeVoxelData_RT(FRHICommandListImmediate& RHICmdList, FIntPoint InDimension)
{
    check(IsInRenderingThread());

    // Update dimension and clear previous voxel data if required

    if (Dimension_RT != InDimension)
    {
        VoxelStateData.Release();
        VoxelFeatureData.Release();

        Dimension_RT = InDimension;
    }

    check(HasValidDimension_RT());

    FIntPoint Dimension = Dimension_RT;
    int32 VoxelCount = GetVoxelCount_RT();

    // Create UAV compatible debug texture if debug RTT is valid

    if (DebugRTT && ! DebugTextureRHI)
    {
        DebugRTTRHI = DebugRTT->GetRenderTargetResource()->TextureRHI;

        if (DebugRTTRHI && DebugRTT->GetFormat() == PF_FloatRGBA)
        {
            FRHIResourceCreateInfo CreateInfo;
            DebugTextureRHI = RHICreateTexture2D(Dimension.X, Dimension.Y, PF_FloatRGBA, 1, 1, TexCreate_ShaderResource | TexCreate_UAV, CreateInfo);
            DebugTextureUAV = RHICreateUnorderedAccessView(DebugTextureRHI);
        }
    }

    typedef TResourceArray<FAlignedUint, VERTEXBUFFER_ALIGNMENT> FVoxelData;

    // Construct voxel state data

    if (! VoxelStateData.IsValid())
    {
        FVoxelData VoxelStateDefaultData(false);
        VoxelStateDefaultData.SetNumZeroed(VoxelCount);

        VoxelStateData.Initialize(
            sizeof(FVoxelData::ElementType),
            VoxelCount,
            PF_R32_UINT,
            &VoxelStateDefaultData,
            BUF_Static,
            TEXT("VoxelStateData")
            );
    }

    // Construct cell feature data

    if (! VoxelFeatureData.IsValid())
    {
        FVoxelData VoxelFeatureDefaultData(false);
        VoxelFeatureDefaultData.SetNumUninitialized(VoxelCount);
        FMemory::Memset(VoxelFeatureDefaultData.GetData(), 0xFF, VoxelFeatureDefaultData.GetResourceDataSize());

        VoxelFeatureData.Initialize(
            sizeof(FVoxelData::ElementType),
            VoxelCount,
            PF_R32_UINT,
            &VoxelFeatureDefaultData,
            BUF_Static,
            TEXT("VoxelFeatureData")
            );
    }
}

void FPMUMarchingSquaresMap::BuildMap(int32 FillType, bool bGenerateWalls)
{
    uint32 BuildFillType = FMath::Max(0, FillType);
    bool bCallResult = BuildMapExec(BuildFillType, bGenerateWalls);

    // Call failed, broadcast build map done event
    if (! bCallResult)
    {
        BuildMapDoneEvent.Broadcast(false, BuildFillType);
    }
}

bool FPMUMarchingSquaresMap::BuildMapExec(uint32 FillType, bool bGenerateWalls)
{
    if (! HasValidDimension())
    {
        UE_LOG(LogMSq,Warning, TEXT("FPMUMarchingSquaresMap::BuildMap() ABORTED - Invalid map dimension"));
        return false;
    }

    ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
        FMarchingSquaresMap_BuildMap,
        FPMUMarchingSquaresMap*, Map, this,
        uint32, FillType, FillType,
        bool, bGenerateWalls, bGenerateWalls,
        {
            Map->BuildMap_RT(RHICmdList, FillType, bGenerateWalls, GMaxRHIFeatureLevel);
        } );

    return true;
}

void FPMUMarchingSquaresMap::BuildMap_RT(FRHICommandListImmediate& RHICmdList, uint32 FillType, bool bGenerateWalls, ERHIFeatureLevel::Type InFeatureLevel)
{
    checkf(VoxelStateData.IsValid()  , TEXT("FPMUMarchingSquaresMap::BuildMap() ABORTED - Dimension has been updated and InitializeVoxelData() has not been called"));
    checkf(VoxelFeatureData.IsValid(), TEXT("FPMUMarchingSquaresMap::BuildMap() ABORTED - Dimension has been updated and InitializeVoxelData() has not been called"));
    check(IsInRenderingThread());
    check(HasValidDimension_RT());

    RHICmdListPtr = &RHICmdList;
    RHIShaderMap  = GetGlobalShaderMap(InFeatureLevel);

    check(RHIShaderMap != nullptr);

    GenerateMarchingCubes_RT(FillType, bGenerateWalls);

    // Copy resolve debug texture to debug rtt

    if (DebugRTTRHI && DebugTextureRHI)
    {
        RHICmdList.CopyToResolveTarget(
            DebugTextureRHI,
            DebugRTTRHI,
            FResolveParams()
            );

        DebugTextureUAV.SafeRelease();
        DebugTextureRHI.SafeRelease();
        DebugRTTRHI = nullptr;
    }

    RHICmdListPtr = nullptr;
    RHIShaderMap  = nullptr;

    BuildMapDoneEvent.Broadcast(true, FillType);
}

void FPMUMarchingSquaresMap::GenerateMarchingCubes_RT(uint32 FillType, bool bInGenerateWalls)
{
    check(IsInRenderingThread());
    check(RHICmdListPtr != nullptr);
    check(VoxelStateData.IsValid());
    check(VoxelFeatureData.IsValid());
    check(HasValidDimension_RT());

    FIntPoint Dimension = Dimension_RT;
    int32 VoxelCount = GetVoxelCount();

    FRHICommandListImmediate& RHICmdList(*RHICmdListPtr);

    const bool bUseDualMesh = ! bInGenerateWalls;

    typedef TResourceArray<FAlignedUint, VERTEXBUFFER_ALIGNMENT> FIndexData;

    // Construct cell case data

    FPMURWBuffer CellCaseData;
    CellCaseData.Initialize(
        sizeof(FIndexData::ElementType),
        VoxelCount,
        PF_R32_UINT,
        BUF_Static,
        TEXT("CellCaseData")
        );

    // Construct cell geometry count data

    typedef FAlignedUintVector4 FGeomCountDataType;
    typedef TResourceArray<FGeomCountDataType, VERTEXBUFFER_ALIGNMENT> FGeomCountData;

    FGeomCountData GeomCountDefaultData(false);
    GeomCountDefaultData.SetNumZeroed(VoxelCount);
    
    FPMURWBufferStructured GeomCountData;
    GeomCountData.Initialize(
        sizeof(FGeomCountData::ElementType),
        VoxelCount,
        &GeomCountDefaultData,
        BUF_Static,
        TEXT("GeomCountData")
        );

    // Write cell case data

    {
        TPMUMarchingSquaresMapWriteCellCaseCS<0>::FBaseType* ComputeShader;

        if (bUseDualMesh)
        {
            ComputeShader = *TShaderMapRef<TPMUMarchingSquaresMapWriteCellCaseCS<0>>(RHIShaderMap);
        }
        else
        {
            ComputeShader = *TShaderMapRef<TPMUMarchingSquaresMapWriteCellCaseCS<1>>(RHIShaderMap);
        }

        ComputeShader->SetShader(RHICmdList);
        ComputeShader->BindSRV(RHICmdList, TEXT("VoxelStateData"), VoxelStateData.SRV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutCellCaseData"), CellCaseData.UAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutGeomCountData"), GeomCountData.UAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutDebugTexture"), DebugTextureUAV);
        ComputeShader->SetParameter(RHICmdList, TEXT("_GDim"), Dimension);
        ComputeShader->SetParameter(RHICmdList, TEXT("_LDim"), FIntPoint(BlockSize, BlockSize));
        ComputeShader->SetParameter(RHICmdList, TEXT("_FillType"), FillType);
        ComputeShader->DispatchAndClear(RHICmdList, Dimension.X, Dimension.Y, 1);
    }

    // Scan cell geometry count data to generate geometry offset and sum data

    FPMURWBufferStructured OffsetData;
    FPMURWBufferStructured SumData;

    const int32 ScanBlockCount = FPMUPrefixSumScan::ExclusiveScan<4>(
        GeomCountData.SRV,
        sizeof(FGeomCountData::ElementType),
        VoxelCount,
        OffsetData,
        SumData,
        BUF_Static
        );

    // Get geometry count scan sum data

    check(ScanBlockCount > 0);
    check(SumData.Buffer->GetStride() > 0);

    const int32 SumDataCount = SumData.Buffer->GetSize() / SumData.Buffer->GetStride();
    FGeomCountData SumArr;
    SumArr.SetNumUninitialized(SumDataCount);

    check(SumDataCount > 0);

    void* SumDataPtr = RHILockStructuredBuffer(SumData.Buffer, 0, SumData.Buffer->GetSize(), RLM_ReadOnly);
    FMemory::Memcpy(SumArr.GetData(), SumDataPtr, SumData.Buffer->GetSize());
    RHIUnlockStructuredBuffer(SumData.Buffer);

    FGeomCountDataType BufferSum = SumArr[ScanBlockCount];

    UE_LOG(UntMSq,Warning, TEXT("FPMUMarchingSquaresMap::GenerateMarchingCubes_RT() BufferSum: X=%d,Y=%d,Z=%d,W=%d"), BufferSum.X,BufferSum.Y,BufferSum.Z,BufferSum.W);

    // Calculate geometry allocation sizes

    const int32 BlockOffset = FPMUPrefixSumScan::GetBlockOffsetForSize(BlockSize*BlockSize);

    const int32 VCount = BufferSum.X;
    const int32 ICount = BufferSum.Y;

    const int32 FillCellCount = BufferSum.Z;
    const int32 EdgeCellCount = BufferSum.W;

    int32 TotalVCount = VCount;
    int32 TotalICount = ICount;

    if (bUseDualMesh)
    {
        TotalVCount *= 2;
        TotalICount *= 2;
    }
    
    UE_LOG(UntMSq,Warning, TEXT("FPMUMarchingSquaresMap::GenerateMarchingCubes_RT() BlockOffset: %d"), BlockOffset);
    UE_LOG(UntMSq,Warning, TEXT("FPMUMarchingSquaresMap::GenerateMarchingCubes_RT() VCount: %d"), VCount);
    UE_LOG(UntMSq,Warning, TEXT("FPMUMarchingSquaresMap::GenerateMarchingCubes_RT() ICount: %d"), ICount);
    UE_LOG(UntMSq,Warning, TEXT("FPMUMarchingSquaresMap::GenerateMarchingCubes_RT() FillCellCount: %d"), FillCellCount);
    UE_LOG(UntMSq,Warning, TEXT("FPMUMarchingSquaresMap::GenerateMarchingCubes_RT() EdgeCellCount: %d"), EdgeCellCount);
    UE_LOG(UntMSq,Warning, TEXT("FPMUMarchingSquaresMap::GenerateMarchingCubes_RT() TotalVCount: %d"), TotalVCount);
    UE_LOG(UntMSq,Warning, TEXT("FPMUMarchingSquaresMap::GenerateMarchingCubes_RT() TotalICount: %d"), TotalICount);

    // Empty map, return
    if (TotalVCount < 3 || TotalICount < 3)
    {
        return;
    }

    // Generate compact triangulation data
    
    FPMURWBuffer FillCellIdData;
    FPMURWBuffer EdgeCellIdData;

    if (FillCellCount > 0)
    {
        FillCellIdData.Initialize(
            sizeof(FIndexData::ElementType),
            FillCellCount,
            PF_R32_UINT,
            BUF_Static,
            TEXT("FillCellIdData")
            );
    }

    if (EdgeCellCount > 0)
    {
        EdgeCellIdData.Initialize(
            sizeof(FIndexData::ElementType),
            EdgeCellCount,
            PF_R32_UINT,
            BUF_Static,
            TEXT("EdgeCellIdData")
            );
    }

    TShaderMapRef<FPMUMarchingSquaresMapWriteCellCompactIdCS> CellWriteCompactIdCS(RHIShaderMap);
    CellWriteCompactIdCS->SetShader(RHICmdList);
    CellWriteCompactIdCS->BindSRV(RHICmdList, TEXT("GeomCountData"), GeomCountData.SRV);
    CellWriteCompactIdCS->BindSRV(RHICmdList, TEXT("OffsetData"), OffsetData.SRV);
    CellWriteCompactIdCS->BindUAV(RHICmdList, TEXT("OutFillCellIdData"), FillCellIdData.UAV);
    CellWriteCompactIdCS->BindUAV(RHICmdList, TEXT("OutEdgeCellIdData"), EdgeCellIdData.UAV);
    CellWriteCompactIdCS->SetParameter(RHICmdList, TEXT("_GDim"), Dimension);
    CellWriteCompactIdCS->SetParameter(RHICmdList, TEXT("_LDim"), FIntPoint(BlockSize, BlockSize));
    CellWriteCompactIdCS->DispatchAndClear(RHICmdList, Dimension.X, Dimension.Y, 1);

    FPMURWBufferStructured VData;
    FPMURWBuffer           IData;

    VData.Initialize(
        sizeof(FPMUPackedVertex),
        TotalVCount,
        BUF_Static,
        TEXT("Vertex Data")
        );

    IData.Initialize(
        sizeof(FIndexData::ElementType),
        TotalICount,
        PF_R32_UINT,
        BUF_Static,
        TEXT("Index Data")
        );

    if (FillCellCount > 0)
    {
        TPMUMarchingSquaresMapTriangulateFillCellCS<0>::FBaseType* ComputeShader;

        if (bUseDualMesh)
        {
            ComputeShader = *TShaderMapRef<TPMUMarchingSquaresMapTriangulateFillCellCS<0>>(RHIShaderMap);
        }
        else
        {
            ComputeShader = *TShaderMapRef<TPMUMarchingSquaresMapTriangulateFillCellCS<1>>(RHIShaderMap);
        }

        FSamplerStateRHIParamRef HeightMapSampler = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

        FVector2D HeightScale;
        HeightScale.X = SurfaceHeightScale;
        HeightScale.Y = ExtrudeHeightScale;

        FIntPoint GeomCount;
        GeomCount.X = VCount;
        GeomCount.Y = ICount;

        float  HeightOffset = BaseHeightOffset;
        uint32 SampleLevel  = FMath::Max(0, HeightMapMipLevel);

        ComputeShader->SetShader(RHICmdList);
        ComputeShader->BindTexture(RHICmdList, TEXT("HeightMap"), TEXT("samplerHeightMap"), HeightMap, HeightMapSampler);
        ComputeShader->BindSRV(RHICmdList, TEXT("OffsetData"),     OffsetData.SRV);
        ComputeShader->BindSRV(RHICmdList, TEXT("SumData"),        SumData.SRV);
        ComputeShader->BindSRV(RHICmdList, TEXT("FillCellIdData"), FillCellIdData.SRV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutVertexData"), VData.UAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutIndexData"),  IData.UAV);
        ComputeShader->SetParameter(RHICmdList, TEXT("_GDim"),          Dimension);
        ComputeShader->SetParameter(RHICmdList, TEXT("_LDim"),          FIntPoint(BlockSize, BlockSize));
        ComputeShader->SetParameter(RHICmdList, TEXT("_GeomCount"),     GeomCount);
        ComputeShader->SetParameter(RHICmdList, TEXT("_SampleLevel"),   SampleLevel);
        ComputeShader->SetParameter(RHICmdList, TEXT("_FillCellCount"), FillCellCount);
        ComputeShader->SetParameter(RHICmdList, TEXT("_BlockOffset"),   BlockOffset);
        ComputeShader->SetParameter(RHICmdList, TEXT("_HeightScale"),   HeightScale);
        ComputeShader->SetParameter(RHICmdList, TEXT("_HeightOffset"),  HeightOffset);
        ComputeShader->SetParameter(RHICmdList, TEXT("_Color"),         FVector4(1,0,0,1));
        ComputeShader->DispatchAndClear(RHICmdList, FillCellCount, 1, 1);
    }

    if (EdgeCellCount > 0)
    {
        TPMUMarchingSquaresMapTriangulateEdgeCellCS<0>::FBaseType* ComputeShader;

        if (bUseDualMesh)
        {
            ComputeShader = *TShaderMapRef<TPMUMarchingSquaresMapTriangulateEdgeCellCS<0>>(RHIShaderMap);
        }
        else
        {
            ComputeShader = *TShaderMapRef<TPMUMarchingSquaresMapTriangulateEdgeCellCS<1>>(RHIShaderMap);
        }

        FSamplerStateRHIParamRef HeightMapSampler = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

        FVector2D HeightScale;
        HeightScale.X = SurfaceHeightScale;
        HeightScale.Y = ExtrudeHeightScale;

        FIntPoint GeomCount;
        GeomCount.X = VCount;
        GeomCount.Y = ICount;

        float  HeightOffset = BaseHeightOffset;
        uint32 SampleLevel  = FMath::Max(0, HeightMapMipLevel);

        ComputeShader->SetShader(RHICmdList);
        ComputeShader->BindTexture(RHICmdList, TEXT("HeightMap"), TEXT("samplerHeightMap"), HeightMap, HeightMapSampler);
        ComputeShader->BindSRV(RHICmdList, TEXT("VoxelFeatureData"), VoxelFeatureData.SRV);
        ComputeShader->BindSRV(RHICmdList, TEXT("OffsetData"),       OffsetData.SRV);
        ComputeShader->BindSRV(RHICmdList, TEXT("SumData"),          SumData.SRV);
        ComputeShader->BindSRV(RHICmdList, TEXT("EdgeCellIdData"),   EdgeCellIdData.SRV);
        ComputeShader->BindSRV(RHICmdList, TEXT("CellCaseData"),     CellCaseData.SRV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutVertexData"), VData.UAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutIndexData"),  IData.UAV);
        ComputeShader->SetParameter(RHICmdList, TEXT("_GDim"),          Dimension);
        ComputeShader->SetParameter(RHICmdList, TEXT("_LDim"),          FIntPoint(BlockSize, BlockSize));
        ComputeShader->SetParameter(RHICmdList, TEXT("_GeomCount"),     GeomCount);
        ComputeShader->SetParameter(RHICmdList, TEXT("_SampleLevel"),   SampleLevel);
        ComputeShader->SetParameter(RHICmdList, TEXT("_EdgeCellCount"), EdgeCellCount);
        ComputeShader->SetParameter(RHICmdList, TEXT("_BlockOffset"),   BlockOffset);
        ComputeShader->SetParameter(RHICmdList, TEXT("_HeightScale"),   HeightScale);
        ComputeShader->SetParameter(RHICmdList, TEXT("_HeightOffset"),  HeightOffset);
        ComputeShader->SetParameter(RHICmdList, TEXT("_Color"),         FVector4(1,0,0,1));
        ComputeShader->DispatchAndClear(RHICmdList, EdgeCellCount, 1, 1);
    }

    // Construct mesh sections

    int32 GridCountX = (Dimension.X / BlockSize);
    int32 GridCountY = (Dimension.Y / BlockSize);
    int32 GridCount  = (GridCountX * GridCountY);
    int32 TotalGridCount = GridCount;

    if (bUseDualMesh)
    {
        TotalGridCount *= 2;
    }

    if (! SectionGroups.IsValidIndex(FillType))
    {
        SectionGroups.SetNum(FillType + 1, false);
    }

    TArray<FPMUMeshSectionResource>& MeshSections(SectionGroups[FillType]);

    MeshSections.Reset(TotalGridCount);
    MeshSections.SetNum(TotalGridCount);

    uint8* VDataPtr = reinterpret_cast<uint8*>(RHILockStructuredBuffer(VData.Buffer, 0, VData.Buffer->GetSize(), RLM_ReadOnly));
    uint8* IDataPtr = reinterpret_cast<uint8*>(RHILockVertexBuffer(IData.Buffer, 0, IData.Buffer->GetSize(), RLM_ReadOnly));
    const int32 IDataStride = sizeof(FIndexData::ElementType);

    for (int32 gy=0; gy<GridCountY; ++gy)
    for (int32 gx=0; gx<GridCountX; ++gx)
    {
        int32 i  = gx + gy*GridCountX;
        int32 i0 =  i    * BlockOffset;
        int32 i1 = (i+1) * BlockOffset;

        FGeomCountDataType Sum0 = SumArr[i0];
        FGeomCountDataType Sum1 = SumArr[i1];

        uint32 GVOffset = Sum0[0];
        uint32 GIOffset = Sum0[1];

        uint32 GVCount = Sum1[0] - GVOffset;
        uint32 GICount = Sum1[1] - GIOffset;

        UE_LOG(UntMSq,Warning, TEXT("FPMUMarchingSquaresMap::GenerateMarchingCubes_RT() [%d] Grid Geometry Count (%d, %d)"), i, i0, i1);
        UE_LOG(UntMSq,Warning, TEXT("FPMUMarchingSquaresMap::GenerateMarchingCubes_RT() [%d] GVOffset: %u"), i, GVOffset);
        UE_LOG(UntMSq,Warning, TEXT("FPMUMarchingSquaresMap::GenerateMarchingCubes_RT() [%d] GIOffset: %u"), i, GIOffset);
        UE_LOG(UntMSq,Warning, TEXT("FPMUMarchingSquaresMap::GenerateMarchingCubes_RT() [%d] GVCount: %u"), i, GVCount);
        UE_LOG(UntMSq,Warning, TEXT("FPMUMarchingSquaresMap::GenerateMarchingCubes_RT() [%d] GICount: %u"), i, GICount);

        bool bValidSection = (GVCount >= 3 && GICount >= 3);

        // Skip empty sections
        if (! bValidSection)
        {
            continue;
        }

        // Calculate local bounds

        float BoundsSurfaceZ =  BaseHeightOffset + SurfaceHeightScale + 1.f;
        float BoundsExtrudeZ = -BaseHeightOffset + ExtrudeHeightScale - 1.f;

        if (bOverrideBoundsZ)
        {
            if (BoundsSurfaceOverrideZ > KINDA_SMALL_NUMBER)
            {
                BoundsSurfaceZ =  BaseHeightOffset + BoundsSurfaceOverrideZ + 1.f;
            }

            if (BoundsExtrudeOverrideZ > KINDA_SMALL_NUMBER)
            {
                BoundsExtrudeZ = -BaseHeightOffset - BoundsExtrudeOverrideZ - 1.f;
            }
        }

        FBox LocalBounds(ForceInitToZero);
        LocalBounds.Min = FVector(gx*BlockSize, gy*BlockSize, 0);
        LocalBounds.Max = LocalBounds.Min + FVector(BlockSize, BlockSize, 0);
        LocalBounds.Min.Z = BoundsExtrudeZ;
        LocalBounds.Max.Z = BoundsSurfaceZ;
        LocalBounds = LocalBounds.ShiftBy(-FVector(Dimension.X,Dimension.Y,0)/2.f);

        // Copy surface geometry

        {
            FPMUMeshSectionResource& Section(MeshSections[i]);

            Section.bSectionVisible = bValidSection;
            Section.VertexCount = GVCount;
            Section.IndexCount  = GICount;

            auto& SectionVData(Section.Buffer.GetVBArray());
            auto& SectionIData(Section.Buffer.GetIBArray());

            SectionVData.SetNumUninitialized(GVCount);
            SectionIData.SetNumUninitialized(GICount);

            void* SectionVDataPtr = SectionVData.GetData();
            void* SectionIDataPtr = SectionIData.GetData();

            uint32 VByteOffset = GVOffset * VData.Buffer->GetStride();
            uint32 IByteOffset = GIOffset * IDataStride;

            uint32 VByteCount = GVCount * VData.Buffer->GetStride();
            uint32 IByteCount = GICount * IDataStride;

            FMemory::Memcpy(SectionVDataPtr, VDataPtr+VByteOffset, VByteCount);
            FMemory::Memcpy(SectionIDataPtr, IDataPtr+IByteOffset, IByteCount);

            Section.LocalBounds = LocalBounds;
        }

        // Copy extrude geometry if generating dual mesh

        if (bUseDualMesh)
        {
            FPMUMeshSectionResource& Section(MeshSections[i+GridCount]);

            Section.bSectionVisible = bValidSection;
            Section.VertexCount = GVCount;
            Section.IndexCount  = GICount;

            auto& SectionVData(Section.Buffer.GetVBArray());
            auto& SectionIData(Section.Buffer.GetIBArray());

            SectionVData.SetNumUninitialized(GVCount);
            SectionIData.SetNumUninitialized(GICount);

            void* SectionVDataPtr = SectionVData.GetData();
            void* SectionIDataPtr = SectionIData.GetData();

            uint32 VByteOffset = (GVOffset+VCount) * VData.Buffer->GetStride();
            uint32 IByteOffset = (GIOffset+ICount) * IDataStride;

            uint32 VByteCount = GVCount * VData.Buffer->GetStride();
            uint32 IByteCount = GICount * IDataStride;

            FMemory::Memcpy(SectionVDataPtr, VDataPtr+VByteOffset, VByteCount);
            FMemory::Memcpy(SectionIDataPtr, IDataPtr+IByteOffset, IByteCount);

            Section.LocalBounds = LocalBounds;
        }
    }

    RHIUnlockVertexBuffer(IData.Buffer);
    RHIUnlockStructuredBuffer(VData.Buffer);
}

bool FPMUMarchingSquaresMap::IsPrefabValid(int32 PrefabIndex, int32 LODIndex, int32 SectionIndex) const
{
    //if (! HasPrefab(PrefabIndex))
    //{
    //    return false;
    //}

    //const UStaticMesh* Mesh = MeshPrefabs[PrefabIndex];

    //if (Mesh->bAllowCPUAccess &&
    //    Mesh->RenderData      &&
    //    Mesh->RenderData->LODResources.IsValidIndex(LODIndex) && 
    //    Mesh->RenderData->LODResources[LODIndex].Sections.IsValidIndex(SectionIndex)
    //    )
    //{
    //    return true;
    //}

    return false;
}

bool FPMUMarchingSquaresMap::HasIntersectingBounds(const TArray<FBox2D>& Bounds) const
{
    //if (Bounds.Num() > 0)
    //{
    //    for (const FPrefabData& PrefabData : AppliedPrefabs)
    //    {
    //        const TArray<FBox2D>& AppliedBounds(PrefabData.Bounds);

    //        for (const FBox2D& Box0 : Bounds)
    //        for (const FBox2D& Box1 : AppliedBounds)
    //        {
    //            if (Box0.Intersect(Box1))
    //            {
    //                // Skip exactly intersecting box

    //                if (FMath::IsNearlyEqual(Box0.Min.X, Box1.Max.X, 1.e-3f) ||
    //                    FMath::IsNearlyEqual(Box1.Min.X, Box0.Max.X, 1.e-3f))
    //                {
    //                    continue;
    //                }

    //                if (FMath::IsNearlyEqual(Box0.Min.Y, Box1.Max.Y, 1.e-3f) ||
    //                    FMath::IsNearlyEqual(Box1.Min.Y, Box0.Max.Y, 1.e-3f))
    //                {
    //                    continue;
    //                }

    //                return true;
    //            }
    //        }
    //    }
    //}

    return false;
}

bool FPMUMarchingSquaresMap::TryPlacePrefabAt(int32 PrefabIndex, const FVector2D& Center)
{
    /*
    TArray<FBox2D> Bounds;
    GetPrefabBounds(PrefabIndex, Bounds);

    // Offset prefab bounds towards the specified center location
    for (FBox2D& Box : Bounds)
    {
        Box = Box.ShiftBy(Center);
    }

    if (! HasIntersectingBounds(Bounds))
    {
        AppliedPrefabs.Emplace(Bounds);
        return true;
    }
    */

    return false;
}

void FPMUMarchingSquaresMap::GetPrefabBounds(int32 PrefabIndex, TArray<FBox2D>& Bounds) const
{
    /*
    Bounds.Empty();

    if (! HasPrefab(PrefabIndex))
    {
        return;
    }

    const UStaticMesh& Mesh(*MeshPrefabs[PrefabIndex]);
    const TArray<UStaticMeshSocket*>& Sockets(Mesh.Sockets);

    typedef TKeyValuePair<UStaticMeshSocket*, UStaticMeshSocket*> FBoundsPair;
    TArray<FBoundsPair> BoundSockets;

    const FString MIN_PREFIX(TEXT("Bounds_MIN_"));
    const FString MAX_PREFIX(TEXT("Bounds_MAX_"));
    const int32 PREFIX_LEN = MIN_PREFIX.Len();

    for (UStaticMeshSocket* Socket0 : Sockets)
    {
        // Invalid socket, skip
        if (! IsValid(Socket0))
        {
            continue;
        }

        FString MinSocketName(Socket0->SocketName.ToString());
        FString MaxSocketName(MAX_PREFIX);

        // Not a min bounds socket, skip
        if (! MinSocketName.StartsWith(*MIN_PREFIX, ESearchCase::IgnoreCase))
        {
            continue;
        }

        MaxSocketName += MinSocketName.RightChop(PREFIX_LEN);

        for (UStaticMeshSocket* Socket1 : Sockets)
        {
            if (IsValid(Socket1) && Socket1->SocketName.ToString() == MaxSocketName)
            {
                BoundSockets.Emplace(Socket0, Socket1);
                break;
            }
        }
    }

    for (FBoundsPair BoundsPair : BoundSockets)
    {
        FBox2D Box;
        const FVector& Min(BoundsPair.Key->RelativeLocation);
        const FVector& Max(BoundsPair.Value->RelativeLocation);
        const float MinX = FMath::RoundHalfFromZero(Min.X);
        const float MinY = FMath::RoundHalfFromZero(Min.Y);
        const float MaxX = FMath::RoundHalfFromZero(Max.X);
        const float MaxY = FMath::RoundHalfFromZero(Max.Y);
        Box += FVector2D(MinX, MinY);
        Box += FVector2D(MaxX, MaxY);
        Bounds.Emplace(Box);
    }
    */
}

TArray<FBox2D> FPMUMarchingSquaresMap::GetPrefabBounds(int32 PrefabIndex) const
{
    TArray<FBox2D> Bounds;
    GetPrefabBounds(PrefabIndex, Bounds);
    return Bounds;
}

bool FPMUMarchingSquaresMap::ApplyPrefab(
    int32 PrefabIndex,
    int32 SectionIndex,
    FVector Center,
    bool bApplyHeightMap,
    bool bInverseHeightMap,
    FPMUMeshSection& OutSection
    )
{
    /*
    int32 LODIndex = 0;

    // Check prefab validity
    if (! IsPrefabValid(PrefabIndex, LODIndex, SectionIndex))
    {
        return false;
    }

    UStaticMesh* InMesh = MeshPrefabs[PrefabIndex];
    const FStaticMeshLODResources& LOD(InMesh->RenderData->LODResources[LODIndex]);

    // Place prefab only if the prefab would not intersect with applied prefabs
    if (! TryPlacePrefabAt(PrefabIndex, FVector2D(Center)))
    {
        return false;
    }

    bool bApplySuccess = false;

    // Map from vert buffer for whole mesh to vert buffer for section of interest
    TMap<int32, int32> MeshToSectionVertMap;
    TSet<int32> BorderVertSet;

    const FStaticMeshSection& Section = LOD.Sections[SectionIndex];
    const uint32 OnePastLastIndex = Section.FirstIndex + Section.NumTriangles * 3;

    // Empty output buffers
    FIndexArrayView Indices = LOD.IndexBuffer.GetArrayView();
    const int32 VertexCountOffset = OutSection.GetVertexCount();

    // Iterate over section index buffer, copying verts as needed
    for (uint32 i = Section.FirstIndex; i < OnePastLastIndex; i++)
    {
        uint32 MeshVertIndex = Indices[i];

        // See if we have this vert already in our section vert buffer,
        // copy vert in if not 
        int32 SectionVertIndex = GetNewIndexForOldVertIndex(
            MeshVertIndex,
            MeshToSectionVertMap,
            BorderVertSet,
            Center,
            ! bApplyHeightMap,
            &LOD.PositionVertexBuffer,
            &LOD.VertexBuffer,
            &LOD.ColorVertexBuffer,
            OutSection
            );

        // Add to index buffer
        OutSection.IndexBuffer.Emplace(SectionVertIndex);
    }

    // Mark success
    bApplySuccess = true;

    if (! bApplyHeightMap || ! GridData)
    {
        return bApplySuccess;
    }

    const FPMUGridData* GD = GridData;
    int32 ShapeHeightMapId   = -1;
    int32 SurfaceHeightMapId = -1;
    int32 ExtrudeHeightMapId = -1;
    uint8 HeightMapType = 0;

    if (GD)
    {
        ShapeHeightMapId   = GridData->GetNamedHeightMapId(TEXT("PMU_VOXEL_SHAPE_HEIGHT_MAP"));
        SurfaceHeightMapId = GridData->GetNamedHeightMapId(TEXT("PMU_VOXEL_SURFACE_HEIGHT_MAP"));
        ExtrudeHeightMapId = GridData->GetNamedHeightMapId(TEXT("PMU_VOXEL_EXTRUDE_HEIGHT_MAP"));
        const bool bHasShapeHeightMap   = ShapeHeightMapId   >= 0;
        const bool bHasSurfaceHeightMap = SurfaceHeightMapId >= 0;
        const bool bHasExtrudeHeightMap = ExtrudeHeightMapId >= 0;
        HeightMapType = (bHasShapeHeightMap ? 1 : 0) | ((bHasSurfaceHeightMap ? 1 : 0) << 1) | ((bHasExtrudeHeightMap ? 1 : 0) << 2);
    }

    float voxelSize     = 1.0f;
    float voxelSizeInv  = 1.0f;
    float voxelSizeHalf = voxelSize / 2.f;
    const int32 VertexCount = OutSection.GetVertexCount();

    const FIntPoint& Dim(GridData->Dimension);
    FVector2D GridRangeMin = FVector2D(1.f, 1.f);
    FVector2D GridRangeMax = FVector2D(Dim.X-2, Dim.Y-2);

    for (int32 i=VertexCountOffset; i<VertexCount; ++i)
    {
        FVector& Position(OutSection.VertexBuffer[i].Position);
        FVector& VNormal(OutSection.VertexBuffer[i].Normal);

        const float PX = Position.X + voxelSizeHalf;
        const float PY = Position.Y + voxelSizeHalf;
        const float GridX = FMath::Clamp(PX, GridRangeMin.X, GridRangeMax.X);
        const float GridY = FMath::Clamp(PY, GridRangeMin.Y, GridRangeMax.Y);
        const int32 IX = GridX;
        const int32 IY = GridY;

        const float Z = Position.Z;
        float Height = Z;
        FVector Normal(0.f, 0.f, bInverseHeightMap ? -1.f : 1.f);

        if (HeightMapType != 0)
        {
            check(GD != nullptr);

            if (bInverseHeightMap)
            {
                switch (HeightMapType & 5)
                {
                    case 1:
                        UPMUGridHeightMapUtility::GetHeightNormal(GridX, GridY, IX, IY, *GD, ShapeHeightMapId, Height, Normal);
                        break;

                    case 4:
                    case 5:
                        UPMUGridHeightMapUtility::GetHeightNormal(GridX, GridY, IX, IY, *GD, ExtrudeHeightMapId, Height, Normal);
                        break;
                }

                Height += Z;
                Normal = -Normal;
            }
            else
            {
                switch (HeightMapType & 3)
                {
                    case 1:
                        UPMUGridHeightMapUtility::GetHeightNormal(GridX, GridY, IX, IY, *GD, ShapeHeightMapId, Height, Normal);
                        break;

                    case 2:
                    case 3:
                        UPMUGridHeightMapUtility::GetHeightNormal(GridX, GridY, IX, IY, *GD, SurfaceHeightMapId, Height, Normal);
                        break;
                }
            }
        }

        Position.Z += Height;
        VNormal = BorderVertSet.Contains(i)
            ? Normal
            : (VNormal+Normal).GetSafeNormal();

        OutSection.LocalBox += Position;
    }

    return bApplySuccess;
    */
    return false;
}
