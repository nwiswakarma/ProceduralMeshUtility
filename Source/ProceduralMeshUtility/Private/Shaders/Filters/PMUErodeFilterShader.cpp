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

#include "Shaders/Filters/PMUErodeFilterShader.h"

#include "ProceduralMeshUtility.h"
#include "Shaders/PMUShaderDefinitions.h"

template<uint32 BuildFlags = 0>
class TPMUErodeFilterPS : public FPMUBasePixelShader
{
public:

    typedef FPMUBasePixelShader FBaseType;

    DECLARE_SHADER_TYPE(TPMUErodeFilterPS, Global);

public:

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return true;
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FBaseType::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("USE_BLEND_INCLINE"), ((BuildFlags & 0x01) != 0) ? 1 : 0);
        OutEnvironment.SetDefine(TEXT("USE_SAMPLE_AX")    , ((BuildFlags & 0x02) != 0) ? 1 : 0);
    }


    PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER_WITH_TEXTURE(TPMUErodeFilterPS)

    PMU_DECLARE_SHADER_PARAMETERS_2(
        Texture,
        FShaderResourceParameter,
        FResourceId,
        "SourceMap", SourceMap,
        "WeightMap", WeightMap
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        Sampler,
        FShaderResourceParameter,
        FResourceId,
        "SourceMapSampler", SourceMapSampler,
        "WeightMapSampler", WeightMapSampler
        )

    PMU_DECLARE_SHADER_PARAMETERS_0(SRV,,)
    PMU_DECLARE_SHADER_PARAMETERS_0(UAV,,)

    PMU_DECLARE_SHADER_PARAMETERS_2(
        Value,
        FShaderParameter,
        FParameterId,
        "_ComputeConstants", Params_ComputeConstants,
        "_InclineFactor",    Params_InclineFactor
        )

public:

    static FBaseType* GetShader(ERHIFeatureLevel::Type FeatureLevel, bool bUseBlendIncline, bool bUseSampleAX)
    {
        TShaderMap<FGlobalShaderType>* RHIShaderMap(GetGlobalShaderMap(FeatureLevel));

        uint32 Flags = 0;
        Flags |= bUseBlendIncline ? (1 << 0) : 0;
        Flags |= bUseSampleAX     ? (1 << 1) : 0;

        switch (Flags)
        {
            case 0: return *TShaderMapRef<TPMUErodeFilterPS<0>>(RHIShaderMap); break;
            case 1: return *TShaderMapRef<TPMUErodeFilterPS<1>>(RHIShaderMap); break;
            case 2: return *TShaderMapRef<TPMUErodeFilterPS<2>>(RHIShaderMap); break;
            case 3: return *TShaderMapRef<TPMUErodeFilterPS<3>>(RHIShaderMap); break;
        }

        return nullptr;
    }
};

IMPLEMENT_SHADER_TYPE(template<>, TPMUErodeFilterPS<0>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUErodeFilterPS.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TPMUErodeFilterPS<1>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUErodeFilterPS.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TPMUErodeFilterPS<2>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUErodeFilterPS.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TPMUErodeFilterPS<3>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUErodeFilterPS.usf"), TEXT("MainPS"), SF_Pixel);

FPixelShaderRHIParamRef FPMUErodeFilterShaderRenderThreadWorker::GetPixelShader(ERHIFeatureLevel::Type FeatureLevel)
{
    return GETSAFERHISHADER_PIXEL(TPMUErodeFilterPS<>::GetShader(FeatureLevel, bUseBlendIncline, bUseSampleAX));
}

void FPMUErodeFilterShaderRenderThreadWorker::BindSwapTexture(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, FTextureRHIParamRef SwapTexture)
{
    FPMUBasePixelShader* PSShader(TPMUErodeFilterPS<>::GetShader(FeatureLevel, bUseBlendIncline, bUseSampleAX));
    FSamplerStateRHIParamRef TextureSampler = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
    PSShader->BindTexture(RHICmdList, TEXT("SourceMap"), TEXT("SourceMapSampler"), SwapTexture, TextureSampler);
}

void FPMUErodeFilterShaderRenderThreadWorker::BindPixelShaderParameters(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel)
{
    FPMUBasePixelShader* PSShader(TPMUErodeFilterPS<>::GetShader(FeatureLevel, bUseBlendIncline, bUseSampleAX));

    const FIntVector ComputeConstants(bUseInclineFactor, bUseInvertIncline, bUseDilateFilter);
    FSamplerStateRHIParamRef TextureSampler = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

    PSShader->BindTexture(RHICmdList, TEXT("SourceMap"), TEXT("SourceMapSampler"), SourceTexture.GetTextureParamRef_RT(), TextureSampler);
    PSShader->BindTexture(RHICmdList, TEXT("WeightMap"), TEXT("WeightMapSampler"), WeightTexture.GetTextureParamRef_RT(), TextureSampler);
    PSShader->SetParameter(RHICmdList, TEXT("_ComputeConstants"), ComputeConstants);
    PSShader->SetParameter(RHICmdList, TEXT("_InclineFactor"), InclineFactor);
}

void FPMUErodeFilterShaderRenderThreadWorker::UnbindPixelShaderParameters(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel)
{
    FPMUBasePixelShader* PSShader(TPMUErodeFilterPS<>::GetShader(FeatureLevel, bUseBlendIncline, bUseSampleAX));
    PSShader->UnbindBuffers(RHICmdList);
}
