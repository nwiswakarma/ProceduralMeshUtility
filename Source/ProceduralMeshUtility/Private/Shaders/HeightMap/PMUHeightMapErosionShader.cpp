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

#include "Shaders/HeightMap/PMUHeightMapErosionShader.h"

#include "GlobalShader.h"
#include "UniformBuffer.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"
#include "RenderResource.h"
#include "PipelineStateCache.h"
#include "ShaderParameterUtils.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"

#include "ProceduralMeshUtility.h"
#include "Shaders/PMUShaderDefinitions.h"

class FPMUHeightMapErosion_ApplyWaterSourcesCS : public FPMUBaseComputeShader<16,16,1>
{
    typedef FPMUBaseComputeShader<16,16,1> FBaseType;

    PMU_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS(
        FPMUHeightMapErosion_ApplyWaterSourcesCS,
        Global,
        RHISupportsComputeShaders(Parameters.Platform)
        )

    PMU_DECLARE_SHADER_PARAMETERS_1(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "DeltaTData", DeltaTData
        )

    PMU_DECLARE_SHADER_PARAMETERS_1(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutWaterMap", OutWaterMap
        )

    PMU_DECLARE_SHADER_PARAMETERS_3(
        Value,
        FShaderParameter,
        FParameterId,
        "_Dim",          Params_Dim,
        "_DeltaTId",     Params_DeltaTId,
        "_SourceAmount", Params_SourceAmount
        )
};

class FPMUHeightMapErosion_ComputeFluxCS : public FPMUBaseComputeShader<16,16,1>
{
    typedef FPMUBaseComputeShader<16,16,1> FBaseType;

    PMU_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS(
        FPMUHeightMapErosion_ComputeFluxCS,
        Global,
        RHISupportsComputeShaders(Parameters.Platform)
        )

    PMU_DECLARE_SHADER_PARAMETERS_3(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "DeltaTData", DeltaTData,
        "WaterMap",   WaterMap,
        "HeightMap",  HeightMap
        )

    PMU_DECLARE_SHADER_PARAMETERS_1(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutFluxMap", OutFluxMap
        )

    PMU_DECLARE_SHADER_PARAMETERS_3(
        Value,
        FShaderParameter,
        FParameterId,
        "_Dim",           Params_Dim,
        "_DeltaTId",      Params_DeltaTId,
        "_FlowConstants", Params_FlowConstants
        )
};

class FPMUHeightMapErosion_SimulateFlowCS : public FPMUBaseComputeShader<16,16,1>
{
    typedef FPMUBaseComputeShader<16,16,1> FBaseType;

    PMU_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS(
        FPMUHeightMapErosion_SimulateFlowCS,
        Global,
        RHISupportsComputeShaders(Parameters.Platform)
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "DeltaTData", DeltaTData,
        "FluxMap",    FluxMap
        )

    PMU_DECLARE_SHADER_PARAMETERS_3(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutWaterMap", OutWaterMap,
        "OutFlowMap",  OutFlowMap,
        "OutDeltaTScanData", OutDeltaTScanData
        )

    PMU_DECLARE_SHADER_PARAMETERS_4(
        Value,
        FShaderParameter,
        FParameterId,
        "_Dim",           Params_Dim,
        "_DeltaTId",      Params_DeltaTId,
        "_FlowConstants", Params_FlowConstants,
        "_DispatchCount", Params_DispatchCount
        )
};

class FPMUHeightMapErosion_SimulateErosionCS : public FPMUBaseComputeShader<16,16,1>
{
    typedef FPMUBaseComputeShader<16,16,1> FBaseType;

    PMU_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS(
        FPMUHeightMapErosion_SimulateErosionCS,
        Global,
        RHISupportsComputeShaders(Parameters.Platform)
        )

    PMU_DECLARE_SHADER_PARAMETERS_3(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "DeltaTData",  DeltaTData,
        "FlowMap",     FlowMap,
        "SedimentMap", SedimentMap
        )

    PMU_DECLARE_SHADER_PARAMETERS_3(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutWaterMap",            OutWaterMap,
        "OutSedimentTransferMap", OutSedimentTransferMap,
        "OutHeightMap",           OutHeightMap
        )

    PMU_DECLARE_SHADER_PARAMETERS_3(
        Value,
        FShaderParameter,
        FParameterId,
        "_Dim",              Params_Dim,
        "_DeltaTId",         Params_DeltaTId,
        "_ErosionConstants", Params_ErosionConstants
        )
};

class FPMUHeightMapErosion_TransportSedimentCS : public FPMUBaseComputeShader<16,16,1>
{
    typedef FPMUBaseComputeShader<16,16,1> FBaseType;

    PMU_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS(
        FPMUHeightMapErosion_TransportSedimentCS,
        Global,
        RHISupportsComputeShaders(Parameters.Platform)
        )

    PMU_DECLARE_SHADER_PARAMETERS_3(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "DeltaTData",          DeltaTData,
        "FlowMap",             FlowMap,
        "SedimentTransferMap", SedimentMap
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutWaterMap",    OutWaterMap,
        "OutSedimentMap", OutSedimentMap
        )

    PMU_DECLARE_SHADER_PARAMETERS_3(
        Value,
        FShaderParameter,
        FParameterId,
        "_Dim",           Params_Dim,
        "_DeltaTId",      Params_DeltaTId,
        "_FlowConstants", Params_FlowConstants
        )
};

class FPMUHeightMapErosion_ComputeThermalWeatheringCS : public FPMUBaseComputeShader<16,16,1>
{
    typedef FPMUBaseComputeShader<16,16,1> FBaseType;

    PMU_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS(
        FPMUHeightMapErosion_ComputeThermalWeatheringCS,
        Global,
        RHISupportsComputeShaders(Parameters.Platform)
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "DeltaTData", DeltaTData,
        "HeightMap",  HeightMap
        )

    PMU_DECLARE_SHADER_PARAMETERS_3(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutHeightMap",          OutHeightMap,
        "OutThermalTransferMapAA", OutThermalTransferMapAA,
        "OutThermalTransferMapAX", OutThermalTransferMapAX
        )

    PMU_DECLARE_SHADER_PARAMETERS_3(
        Value,
        FShaderParameter,
        FParameterId,
        "_Dim",                        Params_Dim,
        "_DeltaTId",                   Params_DeltaTId,
        "_ThermalWeatheringConstants", Params_ThermalWeatheringConstants
        )
};

class FPMUHeightMapErosion_TransferThermalWeatheringCS : public FPMUBaseComputeShader<16,16,1>
{
    typedef FPMUBaseComputeShader<16,16,1> FBaseType;

    PMU_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS(
        FPMUHeightMapErosion_TransferThermalWeatheringCS,
        Global,
        RHISupportsComputeShaders(Parameters.Platform)
        )

    PMU_DECLARE_SHADER_PARAMETERS_4(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "DeltaTData",         DeltaTData,
        "HeightMap",          HeightMap,
        "ThermalTransferMapAA", ThermalTransferMapAA,
        "ThermalTransferMapAX", ThermalTransferMapAX
        )

    PMU_DECLARE_SHADER_PARAMETERS_1(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutHeightMap", OutHeightMap
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        Value,
        FShaderParameter,
        FParameterId,
        "_Dim",      Params_Dim,
        "_DeltaTId", Params_DeltaTId
        )
};

class FPMUHeightMapErosion_ScanDeltaTGroupCS : public FPMUBaseComputeShader<128,1,1>
{
    typedef FPMUBaseComputeShader<128,1,1> FBaseType;

    PMU_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS(
        FPMUHeightMapErosion_ScanDeltaTGroupCS,
        Global,
        RHISupportsComputeShaders(Parameters.Platform)
        )

    PMU_DECLARE_SHADER_PARAMETERS_1(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "DeltaTScanData", DeltaTScanData
        )

    PMU_DECLARE_SHADER_PARAMETERS_1(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutDeltaTData", OutDeltaTData
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        Value,
        FShaderParameter,
        FParameterId,
        "_ScanElementCount", Params_ScanElementCount,
        "_DispatchCount",    Params_DispatchCount
        )
};

class FPMUHeightMapErosion_ScanDeltaTTopLevelCS : public FPMUBaseComputeShader<128,1,1>
{
    typedef FPMUBaseComputeShader<128,1,1> FBaseType;

    PMU_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS(
        FPMUHeightMapErosion_ScanDeltaTTopLevelCS,
        Global,
        RHISupportsComputeShaders(Parameters.Platform)
        )

    PMU_DECLARE_SHADER_PARAMETERS_0(SRV,,)

    PMU_DECLARE_SHADER_PARAMETERS_1(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutDeltaTData", OutDeltaTData
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        Value,
        FShaderParameter,
        FParameterId,
        "_DeltaTId",         Params_DeltaTId,
        "_ScanElementCount", Params_ScanElementCount
        )
};

IMPLEMENT_SHADER_TYPE(, FPMUHeightMapErosion_ApplyWaterSourcesCS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUHeightMapErosionCS.usf"), TEXT("ApplyWaterSources"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FPMUHeightMapErosion_ComputeFluxCS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUHeightMapErosionCS.usf"), TEXT("ComputeFlux"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FPMUHeightMapErosion_SimulateFlowCS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUHeightMapErosionCS.usf"), TEXT("SimulateFlow"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FPMUHeightMapErosion_SimulateErosionCS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUHeightMapErosionCS.usf"), TEXT("SimulateErosion"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FPMUHeightMapErosion_TransportSedimentCS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUHeightMapErosionCS.usf"), TEXT("TransportSediment"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FPMUHeightMapErosion_ComputeThermalWeatheringCS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUHeightMapErosionCS.usf"), TEXT("ComputeThermalWeathering"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FPMUHeightMapErosion_TransferThermalWeatheringCS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUHeightMapErosionCS.usf"), TEXT("TransferThermalWeathering"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FPMUHeightMapErosion_ScanDeltaTGroupCS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUHeightMapErosionCS.usf"), TEXT("ScanDeltaTGroup"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FPMUHeightMapErosion_ScanDeltaTTopLevelCS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUHeightMapErosionCS.usf"), TEXT("ScanDeltaTTopLevel"), SF_Compute);

void UPMUHeightMapErosionShader::ApplyErosionShader(
    UObject* WorldContextObject,
    UTextureRenderTarget2D* RenderTarget,
    // Erosion Configuration
    int32 MaxIteration,
    float MaxDuration,
    float WaterAmount,
    float ThermalWeatheringAmount,
    // Flow Constants
    float PipeCrossSectionArea,
    float PipeLength,
    float GravitationalAcceleration,
    float WaterErosionFactor,
    // Erosion Constants
    float MinimumComputedSurfaceTilt,
    float SedimentCapacityConstant,
    float DissolveConstant,
    float DepositConstant,
    // Game Thread Callback Event
    UGWTTickEvent* CallbackEvent
    )
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    FTextureRenderTarget2DResource* TextureResource = nullptr;

    if (! IsValid(World))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUHeightMapErosionShader::ApplyErosionShader() ABORTED, INVALID WORLD CONTEXT OBJECT"));
        return;
    }

    if (! World->Scene)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUHeightMapErosionShader::ApplyErosionShader() ABORTED, INVALID WORLD SCENE"));
        return;
    }

    if (! IsValid(RenderTarget))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUHeightMapErosionShader::ApplyErosionShader() ABORTED, INVALID RENDER TARGET"));
        return;
    }

    TextureResource = static_cast<FTextureRenderTarget2DResource*>(RenderTarget->GameThread_GetRenderTargetResource());

    if (! TextureResource)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUHeightMapErosionShader::ApplyErosionShader() ABORTED, INVALID RENDER TARGET TEXTURE RESOURCE"));
        return;
    }

    struct FRenderParameter
    {
        ERHIFeatureLevel::Type FeatureLevel;
        FTextureRenderTarget2DResource* TextureResource;
        FIntPoint Dimension;
        int32 MaxIteration;
        float MaxDuration;
        float WaterAmount;
        float ThermalWeatheringAmount;
        FVector4 FlowConstants;
        FVector4 ErosionConstants;
        FGWTTickEventRef CallbackRef;
    };

    FRenderParameter RenderParameter = {
        World->Scene->GetFeatureLevel(),
        TextureResource,
        { RenderTarget->SizeX, RenderTarget->SizeY },
        MaxIteration,
        MaxDuration,
        WaterAmount,
        ThermalWeatheringAmount,
        FVector4(
            PipeCrossSectionArea,
            PipeLength,
            GravitationalAcceleration,
            WaterErosionFactor
            ),
        FVector4(
            MinimumComputedSurfaceTilt,
            SedimentCapacityConstant,
            DissolveConstant,
            DepositConstant
            ),
        { CallbackEvent }
        };

    ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
        UPMUHeightMapErosionShader_ApplyErosionShader,
        FRenderParameter, RenderParameter, RenderParameter,
        {
            UPMUHeightMapErosionShader::ApplyErosionShader_RT(
                RHICmdList,
                RenderParameter.FeatureLevel,
                RenderParameter.TextureResource,
                RenderParameter.Dimension,
                RenderParameter.MaxIteration,
                RenderParameter.MaxDuration,
                RenderParameter.WaterAmount,
                RenderParameter.ThermalWeatheringAmount,
                RenderParameter.FlowConstants,
                RenderParameter.ErosionConstants
                );
            RenderParameter.CallbackRef.EnqueueCallback();
        } );
}

void UPMUHeightMapErosionShader::ApplyErosionShader_RT(
    FRHICommandListImmediate& RHICmdList,
    ERHIFeatureLevel::Type FeatureLevel,
    FTextureRenderTarget2DResource* TextureResource,
    FIntPoint Dimension,
    int32 MaxIteration,
    float MaxDuration,
    float WaterAmount,
    float ThermalWeatheringAmount,
    // Flow Constants:
    //  .x Pipe Cross-Section Area
    //  .y Pipe Length
    //  .z Gravitational Acceleration
    //  .w Water Erosion Factor
    const FVector4 FlowConstants,
    // Erosion Constants:
    //  .x Minimum Computed Surface Tilt
    //  .y Sediment Capacity Constant
    //  .z Dissolve Constant
    //  .w Deposit Constant
    const FVector4 ErosionConstants
    )
{
    check(IsInRenderingThread());

    TShaderMap<FGlobalShaderType>* RHIShaderMap = GetGlobalShaderMap(FeatureLevel);

    if (! TextureResource || ! TextureResource->GetTextureRHI())
    {
        return;
    }

    // Data size enumerations

    enum { BLOCK_SIZE2 = 256 };

    enum { SIZE_FLOAT32    = sizeof(float) };
    enum { SIZE_FLOAT32_1D = SIZE_FLOAT32 * 1 };
    enum { SIZE_FLOAT32_2D = SIZE_FLOAT32 * 2 };
    enum { SIZE_FLOAT32_4D = SIZE_FLOAT32 * 4 };

    enum { SIZE_FLOAT16    = sizeof(FFloat16) };
    enum { SIZE_FLOAT16_1D = SIZE_FLOAT16 * 1 };
    enum { SIZE_FLOAT16_2D = SIZE_FLOAT16 * 2 };
    enum { SIZE_FLOAT16_4D = SIZE_FLOAT16 * 4 };

    const int32 MapSize = Dimension.X * Dimension.Y;

    const bool bThermalWeatheringEnabled = ThermalWeatheringAmount > KINDA_SMALL_NUMBER;

    // Erosion simulation data

    FPMURWBuffer WaterMap;
	FPMURWBuffer SedimentMap;
	FPMURWBuffer SedimentTransferMap;
    FPMURWBufferStructured FlowMap;
	FPMURWBufferStructured FluxMap;

	FPMURWBuffer DeltaTScanData;
	FPMURWBuffer DeltaTData;

	FTexture2DRHIRef DstHeightMap;
	FUnorderedAccessViewRHIRef DstHeightMapUAV;
	FShaderResourceViewRHIRef DstHeightMapSRV;

	FPMURWBuffer ThermalTransferMapAA;
	FPMURWBuffer ThermalTransferMapAX;

	FTexture2DRHIRef ThermalSwapHeightMap;
	FUnorderedAccessViewRHIRef ThermalSwapHeightMapUAV;
	FShaderResourceViewRHIRef ThermalSwapHeightMapSRV;

    // Height map RTT RHI texture

	FTexture2DRHIParamRef SrcHeightMap = TextureResource->GetTextureRHI();

    // Calculate block sizes for delta T calculation

    int32 BlockCount      = FMath::DivideAndRoundUp<int32>(MapSize, BLOCK_SIZE2);
    int32 BlockGroupCount = FMath::DivideAndRoundUp<int32>(BlockCount, BLOCK_SIZE2);

    int32 ScanBlockCount      = FPlatformMath::RoundUpToPowerOfTwo(BlockCount);
    int32 ScanBlockGroupCount = FPlatformMath::RoundUpToPowerOfTwo(BlockGroupCount);

    int32 DeltaTScanCount = ScanBlockCount;
    int32 DeltaTCount     = ScanBlockGroupCount + 1;

    int32 DeltaTId = DeltaTCount - 1;

    UE_LOG(UntPMU,Warning, TEXT("UPMUHeightMapErosionShader::ApplyErosionShader_RT() BlockCount: %d"), BlockCount);
    UE_LOG(UntPMU,Warning, TEXT("UPMUHeightMapErosionShader::ApplyErosionShader_RT() BlockGroupCount: %d"), BlockGroupCount);
    UE_LOG(UntPMU,Warning, TEXT("UPMUHeightMapErosionShader::ApplyErosionShader_RT() ScanBlockCount: %d"), ScanBlockCount);
    UE_LOG(UntPMU,Warning, TEXT("UPMUHeightMapErosionShader::ApplyErosionShader_RT() ScanBlockGroupCount: %d"), ScanBlockGroupCount);
    UE_LOG(UntPMU,Warning, TEXT("UPMUHeightMapErosionShader::ApplyErosionShader_RT() DeltaTScanCount: %d"), DeltaTScanCount);
    UE_LOG(UntPMU,Warning, TEXT("UPMUHeightMapErosionShader::ApplyErosionShader_RT() DeltaTCount: %d"), DeltaTCount);

    // Initialize compute resources

    {
        // Initialize computation buffers

        TResourceArray<uint8, VERTEXBUFFER_ALIGNMENT> DefaultValues161D;
        TResourceArray<uint8, VERTEXBUFFER_ALIGNMENT> DefaultValues324D;

        DefaultValues161D.SetNumZeroed(MapSize * SIZE_FLOAT16_1D);
        DefaultValues324D.SetNumZeroed(MapSize * SIZE_FLOAT32_4D);

        WaterMap.Initialize(
            SIZE_FLOAT16_1D,
            MapSize,
            PF_R16F,
            &DefaultValues161D,
            BUF_Static,
            TEXT("WaterMap")
            );

        SedimentTransferMap.Initialize(
            SIZE_FLOAT16_1D,
            MapSize,
            PF_R16F,
            BUF_Static,
            TEXT("SedimentTransferMap")
            );

        SedimentMap.Initialize(
            SIZE_FLOAT16_1D,
            MapSize,
            PF_R16F,
            &DefaultValues161D,
            BUF_Static,
            TEXT("SedimentMap")
            );

        FlowMap.Initialize(
            SIZE_FLOAT32_2D,
            MapSize,
            BUF_Static,
            TEXT("FlowMap")
            );

        FluxMap.Initialize(
            SIZE_FLOAT32_4D,
            MapSize,
            &DefaultValues324D,
            BUF_Static,
            TEXT("FluxMap")
            );

        // Initialize delta T buffers

        TResourceArray<float, VERTEXBUFFER_ALIGNMENT> DefaultDeltaTData;
        DefaultDeltaTData.SetNumUninitialized(DeltaTCount);
        DefaultDeltaTData.Last() = 0.005f;

        DeltaTScanData.Initialize(
            SIZE_FLOAT32_1D,
            DeltaTScanCount,
            PF_R32_FLOAT,
            BUF_Static,
            TEXT("DeltaTScanData")
            );

        DeltaTData.Initialize(
            SIZE_FLOAT32_1D,
            DeltaTCount,
            PF_R32_FLOAT,
            &DefaultDeltaTData,
            BUF_Static,
            TEXT("DeltaTData")
            );

        // Create result texture

        FRHIResourceCreateInfo CreateInfo;
        EPixelFormat PixelFormat = SrcHeightMap->GetFormat();
        uint32 TextureFlags = TexCreate_ShaderResource | TexCreate_UAV | TexCreate_ResolveTargetable;

        DstHeightMap = RHICmdList.CreateTexture2D(Dimension.X, Dimension.Y, PixelFormat, 1, 1, TextureFlags, CreateInfo);
        DstHeightMapUAV = RHICmdList.CreateUnorderedAccessView(DstHeightMap, 0);
        DstHeightMapSRV = RHICmdList.CreateShaderResourceView(DstHeightMap, 0);

        if (bThermalWeatheringEnabled)
        {
            ThermalSwapHeightMap = RHICmdList.CreateTexture2D(Dimension.X, Dimension.Y, PixelFormat, 1, 1, TextureFlags, CreateInfo);
            ThermalSwapHeightMapUAV = RHICmdList.CreateUnorderedAccessView(ThermalSwapHeightMap, 0);
            ThermalSwapHeightMapSRV = RHICmdList.CreateShaderResourceView(ThermalSwapHeightMap, 0);

            ThermalTransferMapAA.Initialize(
                SIZE_FLOAT16_4D,
                MapSize,
                PF_FloatRGBA,
                BUF_Static,
                TEXT("ThermalTransferMapAA")
                );

            ThermalTransferMapAX.Initialize(
                SIZE_FLOAT16_4D,
                MapSize,
                PF_FloatRGBA,
                BUF_Static,
                TEXT("ThermalTransferMapAX")
                );
        }
    }

    SetRenderTarget(RHICmdList, FTextureRHIParamRef(), FTextureRHIParamRef());

    // Copy source texture

    RHICmdList.CopyToResolveTarget(
        SrcHeightMap,
        DstHeightMap,
        FResolveParams()
        );

    // Erosion iteration

    for (int32 Iteration=0; Iteration<MaxIteration; ++Iteration)
    {
        // Apply Water Sources

        {
            TShaderMapRef<FPMUHeightMapErosion_ApplyWaterSourcesCS> ComputeShader(RHIShaderMap);
            ComputeShader->SetShader(RHICmdList);
            ComputeShader->BindSRV(RHICmdList, TEXT("DeltaTData"), DeltaTData.SRV);
            ComputeShader->BindUAV(RHICmdList, TEXT("OutWaterMap"), WaterMap.UAV);
            ComputeShader->SetParameter(RHICmdList, TEXT("_Dim"), Dimension);
            ComputeShader->SetParameter(RHICmdList, TEXT("_DeltaTId"), DeltaTId);
            ComputeShader->SetParameter(RHICmdList, TEXT("_SourceAmount"), WaterAmount * .05f);
            ComputeShader->DispatchAndClear(RHICmdList, Dimension.X, Dimension.Y, 1);
        }

        // Simulate Flow

        {
            TShaderMapRef<FPMUHeightMapErosion_ComputeFluxCS> ComputeShader(RHIShaderMap);
            ComputeShader->SetShader(RHICmdList);
            ComputeShader->BindSRV(RHICmdList, TEXT("DeltaTData"), DeltaTData.SRV);
            ComputeShader->BindSRV(RHICmdList, TEXT("WaterMap"), WaterMap.SRV);
            ComputeShader->BindSRV(RHICmdList, TEXT("HeightMap"), DstHeightMapSRV);
            ComputeShader->BindUAV(RHICmdList, TEXT("OutFluxMap"), FluxMap.UAV);
            ComputeShader->SetParameter(RHICmdList, TEXT("_Dim"), Dimension);
            ComputeShader->SetParameter(RHICmdList, TEXT("_DeltaTId"), DeltaTId);
            ComputeShader->SetParameter(RHICmdList, TEXT("_FlowConstants"), FlowConstants);
            ComputeShader->DispatchAndClear(RHICmdList, Dimension.X, Dimension.Y, 1);
        }

        {
            TShaderMapRef<FPMUHeightMapErosion_SimulateFlowCS> ComputeShader(RHIShaderMap);
            ComputeShader->SetShader(RHICmdList);
            ComputeShader->BindSRV(RHICmdList, TEXT("DeltaTData"), DeltaTData.SRV);
            ComputeShader->BindSRV(RHICmdList, TEXT("FluxMap"), FluxMap.SRV);
            ComputeShader->BindUAV(RHICmdList, TEXT("OutWaterMap"), WaterMap.UAV);
            ComputeShader->BindUAV(RHICmdList, TEXT("OutFlowMap"), FlowMap.UAV);
            ComputeShader->BindUAV(RHICmdList, TEXT("OutDeltaTScanData"), DeltaTScanData.UAV);
            ComputeShader->SetParameter(RHICmdList, TEXT("_Dim"), Dimension);
            ComputeShader->SetParameter(RHICmdList, TEXT("_DeltaTId"), DeltaTId);
            ComputeShader->SetParameter(RHICmdList, TEXT("_FlowConstants"), FlowConstants);
            ComputeShader->SetParameter(RHICmdList, TEXT("_DispatchCount"), ComputeShader->GetDispatchCount(Dimension));
            ComputeShader->DispatchAndClear(RHICmdList, Dimension.X, Dimension.Y, 1);
        }

        // Simulate Erosion

        {
            TShaderMapRef<FPMUHeightMapErosion_SimulateErosionCS> ComputeShader(RHIShaderMap);
            ComputeShader->SetShader(RHICmdList);
            ComputeShader->BindSRV(RHICmdList, TEXT("DeltaTData"), DeltaTData.SRV);
            ComputeShader->BindSRV(RHICmdList, TEXT("SedimentMap"), SedimentMap.SRV);
            ComputeShader->BindSRV(RHICmdList, TEXT("FlowMap"), FlowMap.SRV);
            ComputeShader->BindUAV(RHICmdList, TEXT("OutWaterMap"), WaterMap.UAV);
            ComputeShader->BindUAV(RHICmdList, TEXT("OutSedimentTransferMap"), SedimentTransferMap.UAV);
            ComputeShader->BindUAV(RHICmdList, TEXT("OutHeightMap"), DstHeightMapUAV);
            ComputeShader->SetParameter(RHICmdList, TEXT("_Dim"), Dimension);
            ComputeShader->SetParameter(RHICmdList, TEXT("_DeltaTId"), DeltaTId);
            ComputeShader->SetParameter(RHICmdList, TEXT("_ErosionConstants"), ErosionConstants);
            ComputeShader->DispatchAndClear(RHICmdList, Dimension.X, Dimension.Y, 1);
        }

        // Simulate sediment transport and water evaporation

        {
            TShaderMapRef<FPMUHeightMapErosion_TransportSedimentCS> ComputeShader(RHIShaderMap);
            ComputeShader->SetShader(RHICmdList);
            ComputeShader->BindSRV(RHICmdList, TEXT("DeltaTData"), DeltaTData.SRV);
            ComputeShader->BindSRV(RHICmdList, TEXT("FlowMap"), FlowMap.SRV);
            ComputeShader->BindSRV(RHICmdList, TEXT("SedimentTransferMap"), SedimentTransferMap.SRV);
            ComputeShader->BindUAV(RHICmdList, TEXT("OutWaterMap"), WaterMap.UAV);
            ComputeShader->BindUAV(RHICmdList, TEXT("OutSedimentMap"), SedimentMap.UAV);
            ComputeShader->SetParameter(RHICmdList, TEXT("_Dim"), Dimension);
            ComputeShader->SetParameter(RHICmdList, TEXT("_DeltaTId"), DeltaTId);
            ComputeShader->SetParameter(RHICmdList, TEXT("_FlowConstants"), FlowConstants);
            ComputeShader->DispatchAndClear(RHICmdList, Dimension.X, Dimension.Y, 1);
        }

        // Simulate thermal weathering if enabled

        if (bThermalWeatheringEnabled)
        {
            // Thermal Weathering Constants:
            //  .x Thermal Weathering Amount
            //  .y Talus Angle
            const FVector2D ThermalWeatheringConstants(ThermalWeatheringAmount, .5f);

            {
                TShaderMapRef<FPMUHeightMapErosion_ComputeThermalWeatheringCS> ComputeShader(RHIShaderMap);
                ComputeShader->SetShader(RHICmdList);
                ComputeShader->BindSRV(RHICmdList, TEXT("DeltaTData"), DeltaTData.SRV);
                ComputeShader->BindSRV(RHICmdList, TEXT("HeightMap"), DstHeightMapSRV);
                ComputeShader->BindUAV(RHICmdList, TEXT("OutHeightMap"), ThermalSwapHeightMapUAV);
                ComputeShader->BindUAV(RHICmdList, TEXT("OutThermalTransferMapAA"), ThermalTransferMapAA.UAV);
                ComputeShader->BindUAV(RHICmdList, TEXT("OutThermalTransferMapAX"), ThermalTransferMapAX.UAV);
                ComputeShader->SetParameter(RHICmdList, TEXT("_Dim"), Dimension);
                ComputeShader->SetParameter(RHICmdList, TEXT("_DeltaTId"), DeltaTId);
                ComputeShader->SetParameter(RHICmdList, TEXT("_ThermalWeatheringConstants"), ThermalWeatheringConstants);
                ComputeShader->DispatchAndClear(RHICmdList, Dimension.X, Dimension.Y, 1);
            }

            {
                TShaderMapRef<FPMUHeightMapErosion_TransferThermalWeatheringCS> ComputeShader(RHIShaderMap);
                ComputeShader->SetShader(RHICmdList);
                ComputeShader->BindSRV(RHICmdList, TEXT("DeltaTData"), DeltaTData.SRV);
                ComputeShader->BindSRV(RHICmdList, TEXT("HeightMap"), ThermalSwapHeightMapSRV);
                ComputeShader->BindSRV(RHICmdList, TEXT("ThermalTransferMapAA"), ThermalTransferMapAA.SRV);
                ComputeShader->BindSRV(RHICmdList, TEXT("ThermalTransferMapAX"), ThermalTransferMapAX.SRV);
                ComputeShader->BindUAV(RHICmdList, TEXT("OutHeightMap"), DstHeightMapUAV);
                ComputeShader->SetParameter(RHICmdList, TEXT("_Dim"), Dimension);
                ComputeShader->SetParameter(RHICmdList, TEXT("_DeltaTId"), DeltaTId);
                ComputeShader->DispatchAndClear(RHICmdList, Dimension.X, Dimension.Y, 1);
            }
        }

        // Scan delta T data for next iteration

        {
            TShaderMapRef<FPMUHeightMapErosion_ScanDeltaTGroupCS> ComputeShader(RHIShaderMap);
            ComputeShader->SetShader(RHICmdList);
            ComputeShader->BindSRV(RHICmdList, TEXT("DeltaTScanData"), DeltaTScanData.SRV);
            ComputeShader->BindUAV(RHICmdList, TEXT("OutDeltaTData"), DeltaTData.UAV);
            ComputeShader->SetParameter(RHICmdList, TEXT("_ScanElementCount"), BlockCount);
            ComputeShader->SetParameter(RHICmdList, TEXT("_DispatchCount"), FIntPoint(BlockGroupCount, 1));
            DispatchComputeShader(RHICmdList, *ComputeShader, BlockGroupCount, 1, 1);
            ComputeShader->UnbindBuffers(RHICmdList);
        }

        {
            TShaderMapRef<FPMUHeightMapErosion_ScanDeltaTTopLevelCS> ComputeShader(RHIShaderMap);
            ComputeShader->SetShader(RHICmdList);
            ComputeShader->BindUAV(RHICmdList, TEXT("OutDeltaTData"), DeltaTData.UAV);
            ComputeShader->SetParameter(RHICmdList, TEXT("_DeltaTId"), DeltaTId);
            ComputeShader->SetParameter(RHICmdList, TEXT("_ScanElementCount"), BlockGroupCount);
            DispatchComputeShader(RHICmdList, *ComputeShader, 1, 1, 1);
            ComputeShader->UnbindBuffers(RHICmdList);
        }

        //{
        //    uint32 Stride;
        //    const float* Height = reinterpret_cast<float*>(RHILockTexture2D(DstHeightMap, 0, RLM_ReadOnly, Stride, false));
        //    const float* Water = reinterpret_cast<float*>(RHILockVertexBuffer(WaterMap.Buffer, 0, WaterMap.Buffer->GetSize(), RLM_ReadOnly));
        //    const float* Sediment = reinterpret_cast<float*>(RHILockVertexBuffer(SedimentMap.Buffer, 0, SedimentMap.Buffer->GetSize(), RLM_ReadOnly));

        //    for (int32 i=255; i<MapSize; i+=64)
        //    {
        //        UE_LOG(LogTemp,Warning, TEXT("Map1[%d]: %f, %f, %f"), i, *(Height+i), *(Water+i), *(Sediment+i));
        //    }

        //    RHIUnlockVertexBuffer(SedimentMap.Buffer);
        //    RHIUnlockVertexBuffer(WaterMap.Buffer);
        //    RHIUnlockTexture2D(DstHeightMap, 0, false);
        //}

        //{
        //    const FVector2D* Flow = reinterpret_cast<FVector2D*>(RHILockStructuredBuffer(FlowMap.Buffer, 0, FlowMap.Buffer->GetSize(), RLM_ReadOnly));
        //    for (int32 i=255; i<MapSize; i+=64)
        //    {
        //        UE_LOG(LogTemp,Warning, TEXT("Map1[%d]: %s"), i, *(Flow+i)->ToString());
        //    }
        //    RHIUnlockStructuredBuffer(FlowMap.Buffer);
        //}

        //{
        //    //const float* Scan = reinterpret_cast<float*>(RHILockVertexBuffer(DeltaTScanData.Buffer, 0, DeltaTScanData.Buffer->GetSize(), RLM_ReadOnly));
        //    //for (int32 i=0; i<DeltaTScanCount; ++i)
        //    //{
        //    //    UE_LOG(LogTemp,Warning, TEXT("DeltaTScanData[%d]: %f"), i, *(Scan+i));
        //    //}
        //    //RHIUnlockVertexBuffer(DeltaTScanData.Buffer);

        //    const float* DeltaT = reinterpret_cast<float*>(RHILockVertexBuffer(DeltaTData.Buffer, 0, DeltaTData.Buffer->GetSize(), RLM_ReadOnly));
        //    //for (int32 i=0; i<DeltaTCount; ++i)
        //    //{
        //    //    UE_LOG(LogTemp,Warning, TEXT("DeltaTData[%d]: %f"), i, *(DeltaT+i));
        //    //}
        //    UE_LOG(LogTemp,Warning, TEXT("DeltaT: %f"), *(DeltaT+(DeltaTCount-1)));
        //    RHIUnlockVertexBuffer(DeltaTData.Buffer);
        //}
    }

    // Copy result texture to RTT

    RHICmdList.CopyToResolveTarget(
        DstHeightMap,
        SrcHeightMap,
        FResolveParams()
        );
}
