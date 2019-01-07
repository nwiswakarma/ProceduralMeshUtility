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

#include "Shaders/PMUDrawRadialGradientShader.h"

#include "ShaderParameters.h"
#include "ShaderCore.h"
#include "ShaderParameterUtils.h"
#include "UniformBuffer.h"

#include "Shaders/PMUShaderDefinitions.h"

class FPMUDrawRadialGradientShaderCS : public FPMUBaseComputeShader<16,16,1>
{
    typedef FPMUBaseComputeShader<16,16,1> FBaseType;

    PMU_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS(
        FPMUDrawRadialGradientShaderCS,
        Global,
        RHISupportsComputeShaders(Platform)
        )

    PMU_DECLARE_SHADER_PARAMETERS_0(SRV,,)

    PMU_DECLARE_SHADER_PARAMETERS_1(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutTexture", OutTexture
        )

    PMU_DECLARE_SHADER_PARAMETERS_3(
        Value,
        FShaderParameter,
        FParameterId,
        "_Center",    Params_Center,
        "_Radius",    Params_Radius,
        "_Value",     Params_Value
        )
};

IMPLEMENT_SHADER_TYPE(, FPMUDrawRadialGradientShaderCS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUDrawRadialGradientShaderCS.usf"), TEXT("MainCS"), SF_Compute);

void FPMUDrawRadialGradientShader::DrawRadialGradientCS_RT(
    FRHICommandListImmediate& RHICmdList,
    FUnorderedAccessViewRHIParamRef TextureUAV,
    FIntPoint Dimension,
    FVector2D Center,
    float Radius,
    float Value
    )
{
    TShaderMapRef<FPMUDrawRadialGradientShaderCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
    ComputeShader->SetShader(RHICmdList);
    ComputeShader->BindUAV(RHICmdList, TEXT("OutTexture"), TextureUAV);
    ComputeShader->SetParameter(RHICmdList, TEXT("_Center"), Center);
    ComputeShader->SetParameter(RHICmdList, TEXT("_Radius"), Radius);
    ComputeShader->SetParameter(RHICmdList, TEXT("_Value"), Value);
    DispatchComputeShader(RHICmdList, *ComputeShader, Dimension.X, Dimension.Y, 1);
    ComputeShader->UnbindBuffers(RHICmdList);
}

void FPMUDrawRadialGradientShader::CreateRadialGradientTextureCS_RT(
    FRHICommandListImmediate& RHICmdList,
    FTexture2DRHIRef& Texture,
    EPixelFormat Format,
    FIntPoint Dimension,
    FVector2D Center,
    float Radius,
    float Value
    )
{
    check(IsInRenderingThread());
    check(Dimension.X > 0);
    check(Dimension.Y > 0);

    if (! Texture.IsValid() || Texture->GetFormat() != Format || Texture->GetSizeXY() != Dimension)
    {
        FRHIResourceCreateInfo CreateInfo;
        Texture = RHICreateTexture2D(Dimension.X, Dimension.Y, Format, 1, 1, TexCreate_ShaderResource | TexCreate_UAV, CreateInfo);
    }

    if (Radius < KINDA_SMALL_NUMBER)
    {
        return;
    }

    FUnorderedAccessViewRHIRef TextureUAV = RHICreateUnorderedAccessView(Texture, 0);

    DrawRadialGradientCS_RT(RHICmdList, TextureUAV, Dimension, Center, Radius, Value);

    TextureUAV.SafeRelease();
}

void FPMUDrawRadialGradientShader::DrawRadialGradientCS(
    FUnorderedAccessViewRHIParamRef TextureUAV,
    FIntPoint Dimension,
    FVector2D Center,
    float Radius,
    float Value
    )
{
    if (Value < KINDA_SMALL_NUMBER || Dimension.X < 0 || Dimension.Y < 0)
    {
        return;
    }

    if (IsInRenderingThread())
    {
        FRHICommandListImmediate& RHICmdList = GRHICommandList.GetImmediateCommandList();
        DrawRadialGradientCS_RT(
            RHICmdList,
            TextureUAV,
            Dimension,
            Center,
            Radius,
            Value
            );
    }
    else
    {
        ENQUEUE_UNIQUE_RENDER_COMMAND_FIVEPARAMETER(
            FPMUDrawRadialGradientShader_DrawRadialGradientCS,
            FUnorderedAccessViewRHIParamRef, TextureUAV, TextureUAV,
            FIntPoint, Dimension, Dimension,
            FVector2D, Center, Center,
            float, Radius, Radius,
            float, Value, Value,
            {
                DrawRadialGradientCS_RT(
                    RHICmdList, 
                    TextureUAV, 
                    Dimension,
                    Center,
                    Radius,
                    Value
                    );
            } );
    }
}

void FPMUDrawRadialGradientShader::CreateRadialGradientTextureCS(
    FTexture2DRHIRef& Texture,
    EPixelFormat Format,
    FIntPoint Dimension,
    FVector2D Center,
    float Radius,
    float Value
    )
{
    if (Dimension.X < 0 || Dimension.Y < 0)
    {
        return;
    }

    if (IsInRenderingThread())
    {
        FRHICommandListImmediate& RHICmdList = GRHICommandList.GetImmediateCommandList();
        CreateRadialGradientTextureCS_RT(
            RHICmdList,
            Texture,
            Format,
            Dimension,
            Center,
            Radius,
            Value
            );
    }
    else
    {
        ENQUEUE_UNIQUE_RENDER_COMMAND_SIXPARAMETER(
            FPMUDrawRadialGradientShader_DrawRadialGradientCS,
            FTexture2DRHIRef&, Texture, Texture,
            EPixelFormat, Format, Format,
            FIntPoint, Dimension, Dimension,
            FVector2D, Center, Center,
            float, Radius, Radius,
            float, Value, Value,
            {
                CreateRadialGradientTextureCS_RT(
                    RHICmdList,
                    Texture,
                    Format,
                    Dimension,
                    Center,
                    Radius,
                    Value
                    );
            } );
    }
}
