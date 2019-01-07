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

#include "PMUMarchingSquaresStencilCircle.h"

#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "ShaderParameters.h"
#include "ShaderCore.h"
#include "ShaderParameterUtils.h"
#include "UniformBuffer.h"

#include "ProceduralMeshUtility.h"
#include "RHI/PMURWBuffer.h"
#include "Shaders/PMUShaderDefinitions.h"

// COMPUTE SHADER DEFINITIONS

class FPMUMSqStencilCircleWriteVoxelStateCS : public FPMUBaseComputeShader<16,16,1>
{
    typedef FPMUBaseComputeShader<16,16,1> FBaseType;

    PMU_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS(
        FPMUMSqStencilCircleWriteVoxelStateCS,
        Global,
        RHISupportsComputeShaders(Platform)
        )

    PMU_DECLARE_SHADER_PARAMETERS_0(SRV,,)

    PMU_DECLARE_SHADER_PARAMETERS_2(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutVoxelStateData", OutVoxelStateData,
        "OutDebugTexture",   OutDebugTexture
        )

    PMU_DECLARE_SHADER_PARAMETERS_4(
        Value,
        FShaderParameter,
        FParameterId,
        "_MapDim",        Params_MapDimension,
        "_StencilCenter", Params_StencilCenter,
        "_StencilRadius", Params_StencilRadius,
        "_FillType",      Params_FillType
        )
};

class FPMUMSqStencilCircleWriteVoxelFeatureCS : public FPMUBaseComputeShader<16,16,1>
{
    typedef FPMUBaseComputeShader<16,16,1> FBaseType;

    PMU_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS(
        FPMUMSqStencilCircleWriteVoxelFeatureCS,
        Global,
        RHISupportsComputeShaders(Platform)
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "StencilTexture", StencilTexture,
        "VoxelStateData", VoxelStateData
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutVoxelFeatureData", OutVoxelFeatureData,
        "OutDebugTexture",     OutDebugTexture
        )

    PMU_DECLARE_SHADER_PARAMETERS_4(
        Value,
        FShaderParameter,
        FParameterId,
        "_MapDim",        Params_MapDimension,
        "_StencilCenter", Params_StencilCenter,
        "_StencilRadius", Params_StencilRadius,
        "_FillType",      Params_FillType
        )
};

IMPLEMENT_SHADER_TYPE(, FPMUMSqStencilCircleWriteVoxelStateCS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUMarchingSquaresStencilCircleCS.usf"), TEXT("VoxelWriteStateKernel"), SF_Compute);

IMPLEMENT_SHADER_TYPE(, FPMUMSqStencilCircleWriteVoxelFeatureCS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUMarchingSquaresStencilCircleCS.usf"), TEXT("VoxelWriteFeatureKernel"), SF_Compute);

void FPMUMarchingSquaresStencilCircle::GenerateVoxelFeatures_RT(FPMUMarchingSquaresMap& Map, uint32 FillType)
{
    const int32 VoxelCount(Map.GetVoxelCount());
    const FIntPoint Dimension(Map.GetDimension());

    check(IsInRenderingThread());
    check(Dimension.X > 1);
    check(Dimension.Y > 1);
    check(VoxelCount > 1);

    FRHICommandListImmediate& RHICmdList(GRHICommandList.GetImmediateCommandList());
    TShaderMap<FGlobalShaderType>* RHIShaderMap(GetGlobalShaderMap(GMaxRHIFeatureLevel));

    FPMURWBuffer VoxelStateData(Map.GetVoxelStateData());
    FPMURWBuffer VoxelFeatureData(Map.GetVoxelFeatureData());

    FShaderResourceViewRHIParamRef  VoxelStateDataSRV = VoxelStateData.SRV;
    FUnorderedAccessViewRHIParamRef VoxelStateDataUAV = VoxelStateData.UAV;

    FShaderResourceViewRHIParamRef  VoxelFeatureDataSRV = VoxelFeatureData.SRV;
    FUnorderedAccessViewRHIParamRef VoxelFeatureDataUAV = VoxelFeatureData.UAV;

    FUnorderedAccessViewRHIRef& DebugTextureUAV(Map.GetDebugRTTUAV());

    // Write voxel state data

    {
        TShaderMapRef<FPMUMSqStencilCircleWriteVoxelStateCS> ComputeShader(RHIShaderMap);
        ComputeShader->SetShader(RHICmdList);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutVoxelStateData"), VoxelStateDataUAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutDebugTexture"), DebugTextureUAV);
        ComputeShader->SetParameter(RHICmdList, TEXT("_FillType"), FillType);
        ComputeShader->SetParameter(RHICmdList, TEXT("_MapDim"), Dimension);
        ComputeShader->SetParameter(RHICmdList, TEXT("_StencilCenter"), StencilCenter);
        ComputeShader->SetParameter(RHICmdList, TEXT("_StencilRadius"), StencilRadius);
        ComputeShader->DispatchAndClear(RHICmdList, Dimension.X, Dimension.Y, 1);
    }

    // Write cell feature data

    {
        TShaderMapRef<FPMUMSqStencilCircleWriteVoxelFeatureCS> ComputeShader(RHIShaderMap);
        ComputeShader->SetShader(RHICmdList);
        ComputeShader->BindSRV(RHICmdList, TEXT("VoxelStateData"), VoxelStateDataSRV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutVoxelFeatureData"), VoxelFeatureDataUAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutDebugTexture"), DebugTextureUAV);
        ComputeShader->SetParameter(RHICmdList, TEXT("_FillType"), FillType);
        ComputeShader->SetParameter(RHICmdList, TEXT("_MapDim"), Dimension);
        ComputeShader->SetParameter(RHICmdList, TEXT("_StencilCenter"), StencilCenter);
        ComputeShader->SetParameter(RHICmdList, TEXT("_StencilRadius"), StencilRadius);
        ComputeShader->DispatchAndClear(RHICmdList, Dimension.X, Dimension.Y, 1);
    }
}

void FPMUMarchingSquaresStencilCircle::GenerateVoxelFeatures(FPMUMarchingSquaresMap& Map, uint32 FillType)
{
    check(Map.HasValidDimension());

    Map.InitializeVoxelData();

    ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
        FPMUMarchingSquaresStencilCircle_GenerateVoxelFeatures,
        FPMUMarchingSquaresStencilCircle*, Stencil, this,
        FPMUMarchingSquaresMap&, Map, Map,
        uint32, FillType, FillType,
        {
            Stencil->GenerateVoxelFeatures_RT(Map, FillType);
        } );
}

void UPMUMarchingSquaresStencilCircleRef::ApplyStencilToMap(UPMUMarchingSquaresMapRef* MapRef, int32 FillType)
{
    if (IsValid(MapRef) && MapRef->HasValidMap() && FillType >= 0)
    {
        FPMUMarchingSquaresMap& Map(MapRef->GetMap());

        Stencil.StencilCenter = StencilCenter;
        Stencil.StencilRadius = StencilRadius;
        Stencil.GenerateVoxelFeatures(Map, FillType);
    }
}
