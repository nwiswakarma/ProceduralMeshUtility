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

#include "Shaders/Filters/PMUDirectionalWarpFilterShader.h"

#include "ProceduralMeshUtility.h"
#include "Shaders/PMUShaderDefinitions.h"

template<uint32 BuildFlags = 0>
class TPMUDirectionalWarpFilterPS : public FPMUBasePixelShader
{
public:

    typedef FPMUBasePixelShader FBaseType;

    DECLARE_SHADER_TYPE(TPMUDirectionalWarpFilterPS, Global);

public:

    static bool ShouldCache(EShaderPlatform Platform)
    {
        return true;
    }

    static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
    {
        FBaseType::ModifyCompilationEnvironment(Platform, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("USE_BLEND_INCLINE")  , ((BuildFlags & (1<<0)) != 0) ? 1 : 0);
        OutEnvironment.SetDefine(TEXT("USE_DIRECTIONAL_MAP"), ((BuildFlags & (1<<1)) != 0) ? 1 : 0);
        OutEnvironment.SetDefine(TEXT("USE_DUAL_SAMPLING")  , ((BuildFlags & (1<<2)) != 0) ? 1 : 0);
    }


    PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER_WITH_TEXTURE(TPMUDirectionalWarpFilterPS)

    PMU_DECLARE_SHADER_PARAMETERS_3(
        Texture,
        FShaderResourceParameter,
        FResourceId,
        "SourceMap", SourceMap,
        "WeightMap", WeightMap,
        "DirectionalMap", DirectionalMap
        )

    PMU_DECLARE_SHADER_PARAMETERS_3(
        Sampler,
        FShaderResourceParameter,
        FResourceId,
        "SourceMapSampler", SourceMapSampler,
        "WeightMapSampler", WeightMapSampler,
        "DirectionalMapSampler", DirectionalMapSampler
        )

    PMU_DECLARE_SHADER_PARAMETERS_0(SRV,,)
    PMU_DECLARE_SHADER_PARAMETERS_0(UAV,,)

    PMU_DECLARE_SHADER_PARAMETERS_3(
        Value,
        FShaderParameter,
        FParameterId,
        "_Direction", Params_Direction,
        "_Strength", Params_Strength,
        "_bSampleMinimum", Params_bSampleMinimum
        )

public:

    static FBaseType* GetShader(ERHIFeatureLevel::Type FeatureLevel, bool bUseBlendIncline, bool bUseDirectionalMap, bool bUseDualSampling)
    {
        TShaderMap<FGlobalShaderType>* RHIShaderMap(GetGlobalShaderMap(FeatureLevel));

        uint32 Flags = 0;
        Flags |= bUseBlendIncline   ? (1 << 0) : 0;
        Flags |= bUseDirectionalMap ? (1 << 1) : 0;
        Flags |= bUseDualSampling   ? (1 << 2) : 0;

        switch (Flags)
        {
            case 0: return *TShaderMapRef<TPMUDirectionalWarpFilterPS<0>>(RHIShaderMap); break;
            case 1: return *TShaderMapRef<TPMUDirectionalWarpFilterPS<1>>(RHIShaderMap); break;
            case 2: return *TShaderMapRef<TPMUDirectionalWarpFilterPS<2>>(RHIShaderMap); break;
            case 3: return *TShaderMapRef<TPMUDirectionalWarpFilterPS<3>>(RHIShaderMap); break;
            case 4: return *TShaderMapRef<TPMUDirectionalWarpFilterPS<4>>(RHIShaderMap); break;
            case 5: return *TShaderMapRef<TPMUDirectionalWarpFilterPS<5>>(RHIShaderMap); break;
            case 6: return *TShaderMapRef<TPMUDirectionalWarpFilterPS<6>>(RHIShaderMap); break;
            case 7: return *TShaderMapRef<TPMUDirectionalWarpFilterPS<7>>(RHIShaderMap); break;
        }

        return nullptr;
    }
};

IMPLEMENT_SHADER_TYPE(template<>, TPMUDirectionalWarpFilterPS<0>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUDirectionalWarpFilterPS.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TPMUDirectionalWarpFilterPS<1>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUDirectionalWarpFilterPS.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TPMUDirectionalWarpFilterPS<2>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUDirectionalWarpFilterPS.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TPMUDirectionalWarpFilterPS<3>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUDirectionalWarpFilterPS.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TPMUDirectionalWarpFilterPS<4>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUDirectionalWarpFilterPS.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TPMUDirectionalWarpFilterPS<5>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUDirectionalWarpFilterPS.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TPMUDirectionalWarpFilterPS<6>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUDirectionalWarpFilterPS.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TPMUDirectionalWarpFilterPS<7>, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUDirectionalWarpFilterPS.usf"), TEXT("MainPS"), SF_Pixel);

FPixelShaderRHIParamRef FPMUDirectionalWarpFilterShaderRenderThreadWorker::GetPixelShader(ERHIFeatureLevel::Type FeatureLevel)
{
    return GETSAFERHISHADER_PIXEL(TPMUDirectionalWarpFilterPS<>::GetShader(FeatureLevel, bUseBlendIncline, bUseDirectionalMap, bUseDualSampling));
}

void FPMUDirectionalWarpFilterShaderRenderThreadWorker::BindSwapTexture(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, FTextureRHIParamRef SwapTexture)
{
    FPMUBasePixelShader* PSShader(TPMUDirectionalWarpFilterPS<>::GetShader(FeatureLevel, bUseBlendIncline, bUseDirectionalMap, bUseDualSampling));
    FSamplerStateRHIParamRef TextureSampler = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
    PSShader->BindTexture(RHICmdList, TEXT("SourceMap"), TEXT("SourceMapSampler"), SwapTexture, TextureSampler);
}

void FPMUDirectionalWarpFilterShaderRenderThreadWorker::BindPixelShaderParameters(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel)
{
    FPMUBasePixelShader* PSShader(TPMUDirectionalWarpFilterPS<>::GetShader(FeatureLevel, bUseBlendIncline, bUseDirectionalMap, bUseDualSampling));
    FSamplerStateRHIParamRef TextureSampler = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
    PSShader->BindTexture(RHICmdList, TEXT("SourceMap"), TEXT("SourceMapSampler"), SourceTexture.GetTextureParamRef_RT(), TextureSampler);
    PSShader->BindTexture(RHICmdList, TEXT("WeightMap"), TEXT("WeightMapSampler"), WeightTexture.GetTextureParamRef_RT(), TextureSampler);
    PSShader->BindTexture(RHICmdList, TEXT("DirectionalMap"), TEXT("DirectionalMapSampler"), DirectionalTexture.GetTextureParamRef_RT(), TextureSampler);
    PSShader->SetParameter(RHICmdList, TEXT("_Direction"), RadialDirection);
    PSShader->SetParameter(RHICmdList, TEXT("_Strength"), FVector2D(Strength, StrengthDual));
    PSShader->SetParameter(RHICmdList, TEXT("_bSampleMinimum"), FIntPoint(bUseSampleMinimum, bUseSampleMinimumDual));
}

void FPMUDirectionalWarpFilterShaderRenderThreadWorker::UnbindPixelShaderParameters(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel)
{
    FPMUBasePixelShader* PSShader(TPMUDirectionalWarpFilterPS<>::GetShader(FeatureLevel, bUseBlendIncline, bUseDirectionalMap, bUseDualSampling));
    PSShader->UnbindBuffers(RHICmdList);
}
