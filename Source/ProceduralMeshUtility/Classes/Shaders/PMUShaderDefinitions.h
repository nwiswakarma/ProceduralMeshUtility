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

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "MaterialShaderType.h"
#include "MaterialShader.h"
#include "ShaderParameters.h"
#include "ShaderCore.h"
#include "UniformBuffer.h"

template<int32 ShaderFrequency>
class FPMUBaseGlobalShader : public FGlobalShader
{
private:

    void SetTextureTyped(FRHICommandList& RHICmdList, const FShaderResourceParameter& Parameter, FTextureRHIParamRef TextureParameter)
    {
        switch (ShaderFrequency)
        {
            case SF_Vertex:
                SetTextureParameter(RHICmdList, GetVertexShader(), Parameter, TextureParameter);
                break;

            case SF_Pixel:
                SetTextureParameter(RHICmdList, GetPixelShader(), Parameter, TextureParameter);
                break;

            case SF_Compute:
                SetTextureParameter(RHICmdList, GetComputeShader(), Parameter, TextureParameter);
                break;
        }
    }

    void SetTextureTyped(
        FRHICommandList& RHICmdList,
        const FShaderResourceParameter& TextureParamRes,
        const FShaderResourceParameter& SamplerParamRes,
        FTextureRHIParamRef TextureParameter,
        FSamplerStateRHIParamRef SamplerParameter
        )
    {
        switch (ShaderFrequency)
        {
            case SF_Vertex:
                SetTextureParameter(RHICmdList, GetVertexShader(), TextureParamRes, SamplerParamRes, SamplerParameter, TextureParameter);
                break;

            case SF_Pixel:
                SetTextureParameter(RHICmdList, GetPixelShader(), TextureParamRes, SamplerParamRes, SamplerParameter, TextureParameter);
                break;

            case SF_Compute:
                SetTextureParameter(RHICmdList, GetComputeShader(), TextureParamRes, SamplerParamRes, SamplerParameter, TextureParameter);
                break;
        }
    }

    void SetSRVTyped(FRHICommandList& RHICmdList, int32 ParameterBaseIndex, FShaderResourceViewRHIParamRef SRVParameter)
    {
        switch (ShaderFrequency)
        {
            case SF_Vertex:
                RHICmdList.SetShaderResourceViewParameter(GetVertexShader(), ParameterBaseIndex, SRVParameter);
                break;

            case SF_Pixel:
                RHICmdList.SetShaderResourceViewParameter(GetPixelShader(), ParameterBaseIndex, SRVParameter);
                break;

            case SF_Compute:
                RHICmdList.SetShaderResourceViewParameter(GetComputeShader(), ParameterBaseIndex, SRVParameter);
                break;
        }
    }

    void SetUAVTyped(FRHICommandList& RHICmdList, int32 ParameterBaseIndex, FUnorderedAccessViewRHIParamRef UAVParameter)
    {
        switch (ShaderFrequency)
        {
            case SF_Compute:
                RHICmdList.SetUAVParameter(GetComputeShader(), ParameterBaseIndex, UAVParameter);
                break;
        }
    }

    template<typename FParameterType>
    void SetShaderValueTyped(FRHICommandList& RHICmdList, FShaderParameter& Parameter, FParameterType& ParameterValue)
    {
        switch (ShaderFrequency)
        {
            case SF_Vertex:
                SetShaderValue(RHICmdList, GetVertexShader(), Parameter, ParameterValue);
                break;

            case SF_Pixel:
                SetShaderValue(RHICmdList, GetPixelShader(), Parameter, ParameterValue);
                break;

            case SF_Compute:
                SetShaderValue(RHICmdList, GetComputeShader(), Parameter, ParameterValue);
                break;
        }
    }

protected:

    typedef TPair<FString, FShaderResourceParameter*> FResourceId;
    typedef TPair<FString, FShaderParameter*>         FParameterId;

    typedef TMap<FName, FResourceId>  FResourceMapType;
    typedef TMap<FName, FParameterId> FParameterMapType;

    FResourceMapType  TextureMap;
    FResourceMapType  SamplerMap;
    FResourceMapType  SRVMap;
    FResourceMapType  UAVMap;
    FParameterMapType ParameterMap;

public:

    typedef ShaderMetaType FPMUShaderMetaType;

    static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
        OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
    }

    FPMUBaseGlobalShader() = default;

    FPMUBaseGlobalShader(const FPMUShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FGlobalShader(Initializer)
    {
    }

    void BindShaderParameters(const FPMUShaderMetaType::CompiledShaderInitializerType& Initializer)
    {
        for (auto& ResourcePair : TextureMap)
        {
            auto& ResourceId(ResourcePair.Value);

            if (ResourceId.Value)
            {
                ResourceId.Value->Bind(Initializer.ParameterMap, *ResourceId.Key);
            }
        }

        for (auto& ResourcePair : SamplerMap)
        {
            auto& ResourceId(ResourcePair.Value);

            if (ResourceId.Value)
            {
                ResourceId.Value->Bind(Initializer.ParameterMap, *ResourceId.Key);
            }
        }

        for (auto& ResourcePair : SRVMap)
        {
            auto& ResourceId(ResourcePair.Value);

            if (ResourceId.Value)
            {
                ResourceId.Value->Bind(Initializer.ParameterMap, *ResourceId.Key);
            }
        }

        for (auto& ResourcePair : UAVMap)
        {
            auto& ResourceId(ResourcePair.Value);

            if (ResourceId.Value)
            {
                ResourceId.Value->Bind(Initializer.ParameterMap, *ResourceId.Key);
            }
        }

        for (auto& ResourcePair : ParameterMap)
        {
            auto& ResourceId(ResourcePair.Value);

            if (ResourceId.Value)
            {
                ResourceId.Value->Bind(Initializer.ParameterMap, *ResourceId.Key);
            }
        }
    }

    void MapTextureParameters(TArray<FResourceId> Parameters)
    {
        for (FResourceId& Parameter : Parameters)
        {
            if (Parameter.Value)
            {
                TextureMap.Emplace(FName(*Parameter.Key), Parameter);
            }
        }
    }

    void MapSamplerParameters(TArray<FResourceId> Parameters)
    {
        for (FResourceId& Parameter : Parameters)
        {
            if (Parameter.Value)
            {
                SamplerMap.Emplace(FName(*Parameter.Key), Parameter);
            }
        }
    }

    void MapSRVParameters(TArray<FResourceId> Parameters)
    {
        for (FResourceId& Parameter : Parameters)
        {
            if (Parameter.Value)
            {
                SRVMap.Emplace(FName(*Parameter.Key), Parameter);
            }
        }
    }

    void MapUAVParameters(TArray<FResourceId> Parameters)
    {
        for (FResourceId& Parameter : Parameters)
        {
            if (Parameter.Value)
            {
                UAVMap.Emplace(FName(*Parameter.Key), Parameter);
            }
        }
    }

    void MapValueParameters(TArray<FParameterId> Parameters)
    {
        for (FParameterId& Parameter : Parameters)
        {
            if (Parameter.Value)
            {
                ParameterMap.Emplace(FName(*Parameter.Key), Parameter);
            }
        }
    }

    void BindTexture(FRHICommandList& RHICmdList, FName TextureName, FTextureRHIParamRef TextureParameter)
    {
        if (TextureMap.Contains(TextureName))
        {
            FShaderResourceParameter* Parameter(TextureMap.FindChecked(TextureName).Value);

            UE_LOG(UntPMU,Warning, TEXT("BindTexture() %s : (Parameter != nullptr)=%d, Parameter->IsBound()=%d"),
                *TextureMap.FindChecked(TextureName).Key,
                (Parameter != nullptr),
                (Parameter && Parameter->IsBound())
                );

            if (Parameter && Parameter->IsBound())
            {
                SetTextureTyped(RHICmdList, *Parameter, TextureParameter);
            }
        }
    }

    void BindTexture(FRHICommandList& RHICmdList, FName TextureName, FName SamplerName, FTextureRHIParamRef TextureParameter, FSamplerStateRHIParamRef SamplerParameter)
    {
        if (TextureMap.Contains(TextureName) && SamplerMap.Contains(SamplerName))
        {
            FShaderResourceParameter* TextureParamRes(TextureMap.FindChecked(TextureName).Value);
            FShaderResourceParameter* SamplerParamRes(SamplerMap.FindChecked(SamplerName).Value);

            UE_LOG(UntPMU,Warning, TEXT("BindTexture() %s : (TextureParamRes != nullptr)=%d, TextureParamRes->IsBound()=%d"),
                *TextureMap.FindChecked(TextureName).Key,
                (TextureParamRes != nullptr),
                (TextureParamRes && TextureParamRes->IsBound())
                );

            UE_LOG(UntPMU,Warning, TEXT("BindTexture() %s : (SamplerParamRes != nullptr)=%d, SamplerParamRes->IsBound()=%d"),
                *SamplerMap.FindChecked(SamplerName).Key,
                (SamplerParamRes != nullptr),
                (SamplerParamRes && SamplerParamRes->IsBound())
                );

            if (TextureParamRes && SamplerParamRes && TextureParamRes->IsBound())
            {
                SetTextureTyped(RHICmdList, *TextureParamRes, *SamplerParamRes, TextureParameter, SamplerParameter);
            }
        }
    }

    void BindSRV(FRHICommandList& RHICmdList, FName SRVName, FShaderResourceViewRHIParamRef SRVParameter)
    {
        if (SRVMap.Contains(SRVName))
        {
            FShaderResourceParameter* Parameter(SRVMap.FindChecked(SRVName).Value);

            UE_LOG(UntPMU,Warning, TEXT("BindSRV() %s : (Parameter != nullptr)=%d, Parameter->IsBound()=%d"),
                *SRVMap.FindChecked(SRVName).Key,
                (Parameter != nullptr),
                (Parameter && Parameter->IsBound())
                );

            if (Parameter && Parameter->IsBound())
            {
                SetSRVTyped(RHICmdList, Parameter->GetBaseIndex(), SRVParameter);
            }
        }
    }

    void BindUAV(FRHICommandList& RHICmdList, FName UAVName, FUnorderedAccessViewRHIParamRef UAVParameter)
    {
        if (UAVMap.Contains(UAVName))
        {
            FShaderResourceParameter* Parameter(UAVMap.FindChecked(UAVName).Value);

            UE_LOG(UntPMU,Warning, TEXT("BindUAV() %s : (Parameter != nullptr)=%d, Parameter->IsBound()=%d"),
                *UAVMap.FindChecked(UAVName).Key,
                (Parameter != nullptr),
                (Parameter && Parameter->IsBound())
                );

            if (Parameter && Parameter->IsBound())
            {
                SetUAVTyped(RHICmdList, Parameter->GetBaseIndex(), UAVParameter);
            }
        }
    }

    template<typename FParameterType>
    void SetParameter(FRHICommandList& RHICmdList, FName ParameterName, FParameterType ParameterValue)
    {
        if (ParameterMap.Contains(ParameterName))
        {
            FShaderParameter* Parameter(ParameterMap.FindChecked(ParameterName).Value);

            UE_LOG(UntPMU,Warning, TEXT("SetParameter() %s : (Parameter != nullptr)=%d, Parameter->IsBound()=%d"),
                *ParameterMap.FindChecked(ParameterName).Key,
                (Parameter != nullptr),
                (Parameter && Parameter->IsBound())
                );

            if (Parameter && Parameter->IsBound())
            {
                SetShaderValueTyped(RHICmdList, *Parameter, ParameterValue);
            }
        }
    }

    void UnbindBuffers(FRHICommandList& RHICmdList)
    {
        for (auto& ResourcePair : TextureMap)
        {
            FShaderResourceParameter* Parameter(ResourcePair.Value.Value);

            if (Parameter && Parameter->IsBound())
            {
                SetTextureTyped(RHICmdList, *Parameter, FTextureRHIParamRef());
            }
        }

        for (auto& ResourcePair : SRVMap)
        {
            FShaderResourceParameter* Parameter(ResourcePair.Value.Value);

            if (Parameter && Parameter->IsBound())
            {
                SetSRVTyped(RHICmdList, Parameter->GetBaseIndex(), FShaderResourceViewRHIParamRef());
            }
        }

        for (auto& ResourcePair : UAVMap)
        {
            FShaderResourceParameter* Parameter(ResourcePair.Value.Value);

            if (Parameter && Parameter->IsBound())
            {
                SetUAVTyped(RHICmdList, Parameter->GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
            }
        }

        UE_LOG(UntPMU,Warning, TEXT("UnbindBuffers()"));
    }
};

template<int32 ShaderFrequency>
class FPMUBaseMaterialShader : public FMaterialShader
{
private:

    void SetTextureTyped(FRHICommandList& RHICmdList, const FShaderResourceParameter& Parameter, FTextureRHIParamRef TextureParameter)
    {
        switch (ShaderFrequency)
        {
            case SF_Vertex:
                SetTextureParameter(RHICmdList, GetVertexShader(), Parameter, TextureParameter);
                break;

            case SF_Pixel:
                SetTextureParameter(RHICmdList, GetPixelShader(), Parameter, TextureParameter);
                break;

            case SF_Compute:
                SetTextureParameter(RHICmdList, GetComputeShader(), Parameter, TextureParameter);
                break;
        }
    }

    void SetTextureTyped(
        FRHICommandList& RHICmdList,
        const FShaderResourceParameter& TextureParamRes,
        const FShaderResourceParameter& SamplerParamRes,
        FTextureRHIParamRef TextureParameter,
        FSamplerStateRHIParamRef SamplerParameter
        )
    {
        switch (ShaderFrequency)
        {
            case SF_Vertex:
                SetTextureParameter(RHICmdList, GetVertexShader(), TextureParamRes, SamplerParamRes, SamplerParameter, TextureParameter);
                break;

            case SF_Pixel:
                SetTextureParameter(RHICmdList, GetPixelShader(), TextureParamRes, SamplerParamRes, SamplerParameter, TextureParameter);
                break;

            case SF_Compute:
                SetTextureParameter(RHICmdList, GetComputeShader(), TextureParamRes, SamplerParamRes, SamplerParameter, TextureParameter);
                break;
        }
    }

    void SetSRVTyped(FRHICommandList& RHICmdList, int32 ParameterBaseIndex, FShaderResourceViewRHIParamRef SRVParameter)
    {
        switch (ShaderFrequency)
        {
            case SF_Vertex:
                RHICmdList.SetShaderResourceViewParameter(GetVertexShader(), ParameterBaseIndex, SRVParameter);
                break;

            case SF_Pixel:
                RHICmdList.SetShaderResourceViewParameter(GetPixelShader(), ParameterBaseIndex, SRVParameter);
                break;

            case SF_Compute:
                RHICmdList.SetShaderResourceViewParameter(GetComputeShader(), ParameterBaseIndex, SRVParameter);
                break;
        }
    }

    void SetUAVTyped(FRHICommandList& RHICmdList, int32 ParameterBaseIndex, FUnorderedAccessViewRHIParamRef UAVParameter)
    {
        switch (ShaderFrequency)
        {
            case SF_Compute:
                RHICmdList.SetUAVParameter(GetComputeShader(), ParameterBaseIndex, UAVParameter);
                break;
        }
    }

    template<typename FParameterType>
    void SetShaderValueTyped(FRHICommandList& RHICmdList, FShaderParameter& Parameter, FParameterType& ParameterValue)
    {
        switch (ShaderFrequency)
        {
            case SF_Vertex:
                SetShaderValue(RHICmdList, GetVertexShader(), Parameter, ParameterValue);
                break;

            case SF_Pixel:
                SetShaderValue(RHICmdList, GetPixelShader(), Parameter, ParameterValue);
                break;

            case SF_Compute:
                SetShaderValue(RHICmdList, GetComputeShader(), Parameter, ParameterValue);
                break;
        }
    }

protected:

    typedef TPair<FString, FShaderResourceParameter*> FResourceId;
    typedef TPair<FString, FShaderParameter*>         FParameterId;

    typedef TMap<FName, FResourceId>  FResourceMapType;
    typedef TMap<FName, FParameterId> FParameterMapType;

    FResourceMapType  TextureMap;
    FResourceMapType  SamplerMap;
    FResourceMapType  SRVMap;
    FResourceMapType  UAVMap;
    FParameterMapType ParameterMap;

public:

    typedef FMaterialShaderType FPMUShaderMetaType;

    static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
    {
        FMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
    }

    FPMUBaseMaterialShader() = default;

    FPMUBaseMaterialShader(const FPMUShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FMaterialShader(Initializer)
    {
    }

    void BindShaderParameters(const FPMUShaderMetaType::CompiledShaderInitializerType& Initializer)
    {
        for (auto& ResourcePair : TextureMap)
        {
            auto& ResourceId(ResourcePair.Value);

            if (ResourceId.Value)
            {
                ResourceId.Value->Bind(Initializer.ParameterMap, *ResourceId.Key);
            }
        }

        for (auto& ResourcePair : SamplerMap)
        {
            auto& ResourceId(ResourcePair.Value);

            if (ResourceId.Value)
            {
                ResourceId.Value->Bind(Initializer.ParameterMap, *ResourceId.Key);
            }
        }

        for (auto& ResourcePair : SRVMap)
        {
            auto& ResourceId(ResourcePair.Value);

            if (ResourceId.Value)
            {
                ResourceId.Value->Bind(Initializer.ParameterMap, *ResourceId.Key);
            }
        }

        for (auto& ResourcePair : UAVMap)
        {
            auto& ResourceId(ResourcePair.Value);

            if (ResourceId.Value)
            {
                ResourceId.Value->Bind(Initializer.ParameterMap, *ResourceId.Key);
            }
        }

        for (auto& ResourcePair : ParameterMap)
        {
            auto& ResourceId(ResourcePair.Value);

            if (ResourceId.Value)
            {
                ResourceId.Value->Bind(Initializer.ParameterMap, *ResourceId.Key);
            }
        }
    }

    void MapTextureParameters(TArray<FResourceId> Parameters)
    {
        for (FResourceId& Parameter : Parameters)
        {
            if (Parameter.Value)
            {
                TextureMap.Emplace(FName(*Parameter.Key), Parameter);
            }
        }
    }

    void MapSamplerParameters(TArray<FResourceId> Parameters)
    {
        for (FResourceId& Parameter : Parameters)
        {
            if (Parameter.Value)
            {
                SamplerMap.Emplace(FName(*Parameter.Key), Parameter);
            }
        }
    }

    void MapSRVParameters(TArray<FResourceId> Parameters)
    {
        for (FResourceId& Parameter : Parameters)
        {
            if (Parameter.Value)
            {
                SRVMap.Emplace(FName(*Parameter.Key), Parameter);
            }
        }
    }

    void MapUAVParameters(TArray<FResourceId> Parameters)
    {
        for (FResourceId& Parameter : Parameters)
        {
            if (Parameter.Value)
            {
                UAVMap.Emplace(FName(*Parameter.Key), Parameter);
            }
        }
    }

    void MapValueParameters(TArray<FParameterId> Parameters)
    {
        for (FParameterId& Parameter : Parameters)
        {
            if (Parameter.Value)
            {
                ParameterMap.Emplace(FName(*Parameter.Key), Parameter);
            }
        }
    }

    void BindTexture(FRHICommandList& RHICmdList, FName TextureName, FTextureRHIParamRef TextureParameter)
    {
        if (TextureMap.Contains(TextureName))
        {
            FShaderResourceParameter* Parameter(TextureMap.FindChecked(TextureName).Value);

            UE_LOG(UntPMU,Warning, TEXT("BindTexture() %s : (Parameter != nullptr)=%d, Parameter->IsBound()=%d"),
                *TextureMap.FindChecked(TextureName).Key,
                (Parameter != nullptr),
                (Parameter && Parameter->IsBound())
                );

            if (Parameter && Parameter->IsBound())
            {
                SetTextureTyped(RHICmdList, *Parameter, TextureParameter);
            }
        }
    }

    void BindTexture(FRHICommandList& RHICmdList, FName TextureName, FName SamplerName, FTextureRHIParamRef TextureParameter, FSamplerStateRHIParamRef SamplerParameter)
    {
        if (TextureMap.Contains(TextureName) && SamplerMap.Contains(SamplerName))
        {
            FShaderResourceParameter* TextureParamRes(TextureMap.FindChecked(TextureName).Value);
            FShaderResourceParameter* SamplerParamRes(SamplerMap.FindChecked(SamplerName).Value);

            UE_LOG(UntPMU,Warning, TEXT("BindTexture() %s : (TextureParamRes != nullptr)=%d, TextureParamRes->IsBound()=%d"),
                *TextureMap.FindChecked(TextureName).Key,
                (TextureParamRes != nullptr),
                (TextureParamRes && TextureParamRes->IsBound())
                );

            UE_LOG(UntPMU,Warning, TEXT("BindTexture() %s : (SamplerParamRes != nullptr)=%d, SamplerParamRes->IsBound()=%d"),
                *SamplerMap.FindChecked(SamplerName).Key,
                (SamplerParamRes != nullptr),
                (SamplerParamRes && SamplerParamRes->IsBound())
                );

            if (TextureParamRes && SamplerParamRes && TextureParamRes->IsBound())
            {
                SetTextureTyped(RHICmdList, *TextureParamRes, *SamplerParamRes, TextureParameter, SamplerParameter);
            }
        }
    }

    void BindSRV(FRHICommandList& RHICmdList, FName SRVName, FShaderResourceViewRHIParamRef SRVParameter)
    {
        if (SRVMap.Contains(SRVName))
        {
            FShaderResourceParameter* Parameter(SRVMap.FindChecked(SRVName).Value);

            UE_LOG(UntPMU,Warning, TEXT("BindSRV() %s : (Parameter != nullptr)=%d, Parameter->IsBound()=%d"),
                *SRVMap.FindChecked(SRVName).Key,
                (Parameter != nullptr),
                (Parameter && Parameter->IsBound())
                );

            if (Parameter && Parameter->IsBound())
            {
                SetSRVTyped(RHICmdList, Parameter->GetBaseIndex(), SRVParameter);
            }
        }
    }

    void BindUAV(FRHICommandList& RHICmdList, FName UAVName, FUnorderedAccessViewRHIParamRef UAVParameter)
    {
        if (UAVMap.Contains(UAVName))
        {
            FShaderResourceParameter* Parameter(UAVMap.FindChecked(UAVName).Value);

            UE_LOG(UntPMU,Warning, TEXT("BindUAV() %s : (Parameter != nullptr)=%d, Parameter->IsBound()=%d"),
                *UAVMap.FindChecked(UAVName).Key,
                (Parameter != nullptr),
                (Parameter && Parameter->IsBound())
                );

            if (Parameter && Parameter->IsBound())
            {
                SetUAVTyped(RHICmdList, Parameter->GetBaseIndex(), UAVParameter);
            }
        }
    }

    template<typename FParameterType>
    void SetParameter(FRHICommandList& RHICmdList, FName ParameterName, FParameterType ParameterValue)
    {
        if (ParameterMap.Contains(ParameterName))
        {
            FShaderParameter* Parameter(ParameterMap.FindChecked(ParameterName).Value);

            UE_LOG(UntPMU,Warning, TEXT("SetParameter() %s : (Parameter != nullptr)=%d, Parameter->IsBound()=%d"),
                *ParameterMap.FindChecked(ParameterName).Key,
                (Parameter != nullptr),
                (Parameter && Parameter->IsBound())
                );

            if (Parameter && Parameter->IsBound())
            {
                SetShaderValueTyped(RHICmdList, *Parameter, ParameterValue);
            }
        }
    }

    void UnbindBuffers(FRHICommandList& RHICmdList)
    {
        for (auto& ResourcePair : TextureMap)
        {
            FShaderResourceParameter* Parameter(ResourcePair.Value.Value);

            if (Parameter && Parameter->IsBound())
            {
                SetTextureTyped(RHICmdList, *Parameter, FTextureRHIParamRef());
            }
        }

        for (auto& ResourcePair : SRVMap)
        {
            FShaderResourceParameter* Parameter(ResourcePair.Value.Value);

            if (Parameter && Parameter->IsBound())
            {
                SetSRVTyped(RHICmdList, Parameter->GetBaseIndex(), FShaderResourceViewRHIParamRef());
            }
        }

        for (auto& ResourcePair : UAVMap)
        {
            FShaderResourceParameter* Parameter(ResourcePair.Value.Value);

            if (Parameter && Parameter->IsBound())
            {
                SetUAVTyped(RHICmdList, Parameter->GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
            }
        }

        UE_LOG(UntPMU,Warning, TEXT("UnbindBuffers()"));
    }
};

class FPMUBaseVertexShader : public FPMUBaseGlobalShader<SF_Vertex>
{
public:

    static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
    {
        FPMUBaseGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
    }

    FPMUBaseVertexShader() = default;

    FPMUBaseVertexShader(const FPMUShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FPMUBaseGlobalShader(Initializer)
    {
    }
};

class FPMUBasePixelShader : public FPMUBaseGlobalShader<SF_Pixel>
{
public:

    static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
    {
        FPMUBaseGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
    }

    FPMUBasePixelShader() = default;

    FPMUBasePixelShader(const FPMUShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FPMUBaseGlobalShader(Initializer)
    {
    }
};

template<int32 ThreadSizeX=1, int32 ThreadSizeY=1, int32 ThreadSizeZ=1>
class FPMUBaseComputeShader : public FPMUBaseGlobalShader<SF_Compute>
{
public:

    static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
    {
        FPMUBaseGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
        SetThreadCountDefines(Platform, OutEnvironment);
    }

    static void SetThreadCountDefines(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
    {
        OutEnvironment.SetDefine(TEXT("THREAD_SIZE_X"), ThreadSizeX);
        OutEnvironment.SetDefine(TEXT("THREAD_SIZE_Y"), ThreadSizeY);
        OutEnvironment.SetDefine(TEXT("THREAD_SIZE_Z"), ThreadSizeZ);
    }

    FPMUBaseComputeShader() = default;

    FPMUBaseComputeShader(const FPMUShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FPMUBaseGlobalShader(Initializer)
    {
    }

    void SetShader(FRHICommandList& RHICmdList)
    {
        UE_LOG(UntPMU,Warning, TEXT("SetShader()"));
        UE_LOG(UntPMU,Warning, TEXT("TextureMap.Num(): %d"), TextureMap.Num());
        UE_LOG(UntPMU,Warning, TEXT("SamplerMap.Num(): %d"), SamplerMap.Num());
        UE_LOG(UntPMU,Warning, TEXT("SRVMap.Num(): %d"), SRVMap.Num());
        UE_LOG(UntPMU,Warning, TEXT("UAVMap.Num(): %d"), UAVMap.Num());
        UE_LOG(UntPMU,Warning, TEXT("ParameterMap.Num(): %d"), ParameterMap.Num());
        RHICmdList.SetComputeShader(GetComputeShader());
    }

    FIntPoint GetDispatchCount(FIntPoint Dim) const
    {
        FIntPoint DispatchCount;
        DispatchCount.X = FMath::DivideAndRoundUp(Dim.X, ThreadSizeX);
        DispatchCount.Y = FMath::DivideAndRoundUp(Dim.Y, ThreadSizeY);
        return DispatchCount;
    }

    void Dispatch(FRHICommandList& RHICmdList, int32 DimX, int32 DimY, int32 DimZ, bool bUnbindBuffers)
    {
        FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
        int32 GroupCountX = FMath::DivideAndRoundUp(DimX, ThreadSizeX);
        int32 GroupCountY = FMath::DivideAndRoundUp(DimY, ThreadSizeY);
        int32 GroupCountZ = FMath::DivideAndRoundUp(DimZ, ThreadSizeZ);
        DispatchComputeShader(RHICmdList, this, GroupCountX, GroupCountY, GroupCountZ);

        UE_LOG(UntPMU,Warning, TEXT("Dispatch(%d, %d, %d) GroupCountX=%d, GroupCountY=%d, GroupCountZ=%d"), DimX, DimY, DimZ, GroupCountX, GroupCountY, GroupCountZ);

        if (bUnbindBuffers)
        {
            UnbindBuffers(RHICmdList);
        }
    }

    void DispatchAndClear(FRHICommandList& RHICmdList, int32 DimX, int32 DimY, int32 DimZ)
    {
        Dispatch(RHICmdList, DimX, DimY, DimZ, true);
    }
};

class FPMUBasePixelMaterialShader : public FPMUBaseMaterialShader<SF_Pixel>
{
public:

    static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
    {
        FPMUBaseMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
    }

    FPMUBasePixelMaterialShader() = default;

    FPMUBasePixelMaterialShader(const FPMUShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FPMUBaseMaterialShader(Initializer)
    {
    }
};

#define PMU_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS(ClassName, ShaderType, CacheCheck)\
    DECLARE_SHADER_TYPE(ClassName, ShaderType);\
    PMU_DECLARE_SHADER_DEFAULT_STATICS(ClassName, CacheCheck)\
    PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER(ClassName)

#define PMU_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS_WITH_TEXTURE(ClassName, ShaderType, CacheCheck)\
    DECLARE_SHADER_TYPE(ClassName, ShaderType);\
    PMU_DECLARE_SHADER_DEFAULT_STATICS(ClassName, CacheCheck)\
    PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER_WITH_TEXTURE(ClassName)

#define PMU_DECLARE_SHADER_DEFAULT_STATICS(ClassName, CacheCheck)\
    static bool ShouldCache(EShaderPlatform Platform)\
    {\
        return CacheCheck;\
    }\
    static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)\
    {\
        FBaseType::ModifyCompilationEnvironment(Platform, OutEnvironment);\
    }

#define PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER(ClassName)\
    PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER_PREFIX3(ClassName, SRV, UAV, Value)

#define PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER_WITH_TEXTURE(ClassName)\
    PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER_PREFIX5(ClassName, Texture, Sampler, SRV, UAV, Value)

#define PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER_PREFIX3(ClassName, Prefix0, Prefix1, Prefix2)\
    public:\
    ClassName() : FBaseType()\
    {\
        MapLocal##Prefix0##Parameters();\
        MapLocal##Prefix1##Parameters();\
        MapLocal##Prefix2##Parameters();\
    }\
    ClassName(const FPMUShaderMetaType::CompiledShaderInitializerType& Initializer) : FBaseType(Initializer)\
    {\
        MapLocal##Prefix0##Parameters();\
        MapLocal##Prefix1##Parameters();\
        MapLocal##Prefix2##Parameters();\
        BindShaderParameters(Initializer);\
    }\
    virtual bool Serialize(FArchive& Ar) override\
    {\
        bool bShaderHasOutdatedParameters = FBaseType::Serialize(Ar);\
        Serialize##Prefix0##Parameters(Ar);\
        Serialize##Prefix1##Parameters(Ar);\
        Serialize##Prefix2##Parameters(Ar);\
        return bShaderHasOutdatedParameters;\
    }

#define PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER_PREFIX4(ClassName, Prefix0, Prefix1, Prefix2, Prefix3)\
    public:\
    ClassName() : FBaseType()\
    {\
        MapLocal##Prefix0##Parameters();\
        MapLocal##Prefix1##Parameters();\
        MapLocal##Prefix2##Parameters();\
        MapLocal##Prefix3##Parameters();\
    }\
    ClassName(const FPMUShaderMetaType::CompiledShaderInitializerType& Initializer) : FBaseType(Initializer)\
    {\
        MapLocal##Prefix0##Parameters();\
        MapLocal##Prefix1##Parameters();\
        MapLocal##Prefix2##Parameters();\
        MapLocal##Prefix3##Parameters();\
        BindShaderParameters(Initializer);\
    }\
    virtual bool Serialize(FArchive& Ar) override\
    {\
        bool bShaderHasOutdatedParameters = FBaseType::Serialize(Ar);\
        Serialize##Prefix0##Parameters(Ar);\
        Serialize##Prefix1##Parameters(Ar);\
        Serialize##Prefix2##Parameters(Ar);\
        Serialize##Prefix3##Parameters(Ar);\
        return bShaderHasOutdatedParameters;\
    }

#define PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER_PREFIX5(ClassName, Prefix0, Prefix1, Prefix2, Prefix3, Prefix4)\
    public:\
    ClassName() : FBaseType()\
    {\
        MapLocal##Prefix0##Parameters();\
        MapLocal##Prefix1##Parameters();\
        MapLocal##Prefix2##Parameters();\
        MapLocal##Prefix3##Parameters();\
        MapLocal##Prefix4##Parameters();\
    }\
    ClassName(const FPMUShaderMetaType::CompiledShaderInitializerType& Initializer) : FBaseType(Initializer)\
    {\
        MapLocal##Prefix0##Parameters();\
        MapLocal##Prefix1##Parameters();\
        MapLocal##Prefix2##Parameters();\
        MapLocal##Prefix3##Parameters();\
        MapLocal##Prefix4##Parameters();\
        BindShaderParameters(Initializer);\
    }\
    virtual bool Serialize(FArchive& Ar) override\
    {\
        bool bShaderHasOutdatedParameters = FBaseType::Serialize(Ar);\
        Serialize##Prefix0##Parameters(Ar);\
        Serialize##Prefix1##Parameters(Ar);\
        Serialize##Prefix2##Parameters(Ar);\
        Serialize##Prefix3##Parameters(Ar);\
        Serialize##Prefix4##Parameters(Ar);\
        return bShaderHasOutdatedParameters;\
    }

#define PMU_DECLARE_SHADER_PARAMETERS_0(Prefix, PropertyType, IdType)\
    private:\
    void MapLocal##Prefix##Parameters() {}\
    void Serialize##Prefix##Parameters(FArchive& Ar) {}\

#define PMU_DECLARE_SHADER_PARAMETERS_1(Prefix, PropertyType, IdType, N0, V0)\
    private:\
    PropertyType V0;\
    void MapLocal##Prefix##Parameters()\
    {\
        Map##Prefix##Parameters( {\
            IdType(TEXT(N0), &V0)\
            } );\
    }\
    void Serialize##Prefix##Parameters(FArchive& Ar)\
    {\
        Ar << V0;\
    }

#define PMU_DECLARE_SHADER_PARAMETERS_2(Prefix, PropertyType, IdType, N0, V0, N1, V1)\
    private:\
    PropertyType V0;\
    PropertyType V1;\
    void MapLocal##Prefix##Parameters()\
    {\
        Map##Prefix##Parameters( {\
            IdType(TEXT(N0), &V0),\
            IdType(TEXT(N1), &V1)\
            } );\
    }\
    void Serialize##Prefix##Parameters(FArchive& Ar)\
    {\
        Ar << V0;\
        Ar << V1;\
    }

#define PMU_DECLARE_SHADER_PARAMETERS_3(Prefix, PropertyType, IdType, N0, V0, N1, V1, N2, V2)\
    private:\
    PropertyType V0;\
    PropertyType V1;\
    PropertyType V2;\
    void MapLocal##Prefix##Parameters()\
    {\
        Map##Prefix##Parameters( {\
            IdType(TEXT(N0), &V0),\
            IdType(TEXT(N1), &V1),\
            IdType(TEXT(N2), &V2)\
            } );\
    }\
    void Serialize##Prefix##Parameters(FArchive& Ar)\
    {\
        Ar << V0;\
        Ar << V1;\
        Ar << V2;\
    }

#define PMU_DECLARE_SHADER_PARAMETERS_4(Prefix, PropertyType, IdType, N0, V0, N1, V1, N2, V2, N3, V3)\
    private:\
    PropertyType V0;\
    PropertyType V1;\
    PropertyType V2;\
    PropertyType V3;\
    void MapLocal##Prefix##Parameters()\
    {\
        Map##Prefix##Parameters( {\
            IdType(TEXT(N0), &V0),\
            IdType(TEXT(N1), &V1),\
            IdType(TEXT(N2), &V2),\
            IdType(TEXT(N3), &V3)\
            } );\
    }\
    void Serialize##Prefix##Parameters(FArchive& Ar)\
    {\
        Ar << V0;\
        Ar << V1;\
        Ar << V2;\
        Ar << V3;\
    }

#define PMU_DECLARE_SHADER_PARAMETERS_5(Prefix, PropertyType, IdType, N0, V0, N1, V1, N2, V2, N3, V3, N4, V4)\
    private:\
    PropertyType V0;\
    PropertyType V1;\
    PropertyType V2;\
    PropertyType V3;\
    PropertyType V4;\
    void MapLocal##Prefix##Parameters()\
    {\
        Map##Prefix##Parameters( {\
            IdType(TEXT(N0), &V0),\
            IdType(TEXT(N1), &V1),\
            IdType(TEXT(N2), &V2),\
            IdType(TEXT(N3), &V3),\
            IdType(TEXT(N4), &V4)\
            } );\
    }\
    void Serialize##Prefix##Parameters(FArchive& Ar)\
    {\
        Ar << V0;\
        Ar << V1;\
        Ar << V2;\
        Ar << V3;\
        Ar << V4;\
    }

#define PMU_DECLARE_SHADER_PARAMETERS_6(Prefix, PropertyType, IdType, N0, V0, N1, V1, N2, V2, N3, V3, N4, V4, N5, V5)\
    private:\
    PropertyType V0;\
    PropertyType V1;\
    PropertyType V2;\
    PropertyType V3;\
    PropertyType V4;\
    PropertyType V5;\
    void MapLocal##Prefix##Parameters()\
    {\
        Map##Prefix##Parameters( {\
            IdType(TEXT(N0), &V0),\
            IdType(TEXT(N1), &V1),\
            IdType(TEXT(N2), &V2),\
            IdType(TEXT(N3), &V3),\
            IdType(TEXT(N4), &V4),\
            IdType(TEXT(N5), &V5)\
            } );\
    }\
    void Serialize##Prefix##Parameters(FArchive& Ar)\
    {\
        Ar << V0;\
        Ar << V1;\
        Ar << V2;\
        Ar << V3;\
        Ar << V4;\
        Ar << V5;\
    }

#define PMU_DECLARE_SHADER_PARAMETERS_7(Prefix, PropertyType, IdType, N0, V0, N1, V1, N2, V2, N3, V3, N4, V4, N5, V5, N6, V6)\
    private:\
    PropertyType V0;\
    PropertyType V1;\
    PropertyType V2;\
    PropertyType V3;\
    PropertyType V4;\
    PropertyType V5;\
    PropertyType V6;\
    void MapLocal##Prefix##Parameters()\
    {\
        Map##Prefix##Parameters( {\
            IdType(TEXT(N0), &V0),\
            IdType(TEXT(N1), &V1),\
            IdType(TEXT(N2), &V2),\
            IdType(TEXT(N3), &V3),\
            IdType(TEXT(N4), &V4),\
            IdType(TEXT(N5), &V5),\
            IdType(TEXT(N6), &V6)\
            } );\
    }\
    void Serialize##Prefix##Parameters(FArchive& Ar)\
    {\
        Ar << V0;\
        Ar << V1;\
        Ar << V2;\
        Ar << V3;\
        Ar << V4;\
        Ar << V5;\
        Ar << V6;\
    }

#define PMU_DECLARE_SHADER_PARAMETERS_8(Prefix, PropertyType, IdType, N0, V0, N1, V1, N2, V2, N3, V3, N4, V4, N5, V5, N6, V6, N7, V7)\
    private:\
    PropertyType V0;\
    PropertyType V1;\
    PropertyType V2;\
    PropertyType V3;\
    PropertyType V4;\
    PropertyType V5;\
    PropertyType V6;\
    PropertyType V7;\
    void MapLocal##Prefix##Parameters()\
    {\
        Map##Prefix##Parameters( {\
            IdType(TEXT(N0), &V0),\
            IdType(TEXT(N1), &V1),\
            IdType(TEXT(N2), &V2),\
            IdType(TEXT(N3), &V3),\
            IdType(TEXT(N4), &V4),\
            IdType(TEXT(N5), &V5),\
            IdType(TEXT(N6), &V6),\
            IdType(TEXT(N7), &V7)\
            } );\
    }\
    void Serialize##Prefix##Parameters(FArchive& Ar)\
    {\
        Ar << V0;\
        Ar << V1;\
        Ar << V2;\
        Ar << V3;\
        Ar << V4;\
        Ar << V5;\
        Ar << V6;\
        Ar << V7;\
    }

#define PMU_DECLARE_SHADER_PARAMETERS_9(Prefix, PropertyType, IdType, N0, V0, N1, V1, N2, V2, N3, V3, N4, V4, N5, V5, N6, V6, N7, V7, N8, V8)\
    private:\
    PropertyType V0;\
    PropertyType V1;\
    PropertyType V2;\
    PropertyType V3;\
    PropertyType V4;\
    PropertyType V5;\
    PropertyType V6;\
    PropertyType V7;\
    PropertyType V8;\
    void MapLocal##Prefix##Parameters()\
    {\
        Map##Prefix##Parameters( {\
            IdType(TEXT(N0), &V0),\
            IdType(TEXT(N1), &V1),\
            IdType(TEXT(N2), &V2),\
            IdType(TEXT(N3), &V3),\
            IdType(TEXT(N4), &V4),\
            IdType(TEXT(N5), &V5),\
            IdType(TEXT(N6), &V6),\
            IdType(TEXT(N7), &V7),\
            IdType(TEXT(N8), &V8)\
            } );\
    }\
    void Serialize##Prefix##Parameters(FArchive& Ar)\
    {\
        Ar << V0;\
        Ar << V1;\
        Ar << V2;\
        Ar << V3;\
        Ar << V4;\
        Ar << V5;\
        Ar << V6;\
        Ar << V7;\
        Ar << V8;\
    }

#define PMU_DECLARE_SHADER_PARAMETERS_10(Prefix, PropertyType, IdType, N0, V0, N1, V1, N2, V2, N3, V3, N4, V4, N5, V5, N6, V6, N7, V7, N8, V8, N9, V9)\
    private:\
    PropertyType V0;\
    PropertyType V1;\
    PropertyType V2;\
    PropertyType V3;\
    PropertyType V4;\
    PropertyType V5;\
    PropertyType V6;\
    PropertyType V7;\
    PropertyType V8;\
    PropertyType V9;\
    void MapLocal##Prefix##Parameters()\
    {\
        Map##Prefix##Parameters( {\
            IdType(TEXT(N0), &V0),\
            IdType(TEXT(N1), &V1),\
            IdType(TEXT(N2), &V2),\
            IdType(TEXT(N3), &V3),\
            IdType(TEXT(N4), &V4),\
            IdType(TEXT(N5), &V5),\
            IdType(TEXT(N6), &V6),\
            IdType(TEXT(N7), &V7),\
            IdType(TEXT(N8), &V8),\
            IdType(TEXT(N9), &V9)\
            } );\
    }\
    void Serialize##Prefix##Parameters(FArchive& Ar)\
    {\
        Ar << V0;\
        Ar << V1;\
        Ar << V2;\
        Ar << V3;\
        Ar << V4;\
        Ar << V5;\
        Ar << V6;\
        Ar << V7;\
        Ar << V8;\
        Ar << V9;\
    }

#define PMU_DECLARE_SHADER_PARAMETERS_11(Prefix, PropertyType, IdType, N0, V0, N1, V1, N2, V2, N3, V3, N4, V4, N5, V5, N6, V6, N7, V7, N8, V8, N9, V9, N10, V10)\
    private:\
    PropertyType V0;\
    PropertyType V1;\
    PropertyType V2;\
    PropertyType V3;\
    PropertyType V4;\
    PropertyType V5;\
    PropertyType V6;\
    PropertyType V7;\
    PropertyType V8;\
    PropertyType V9;\
    PropertyType V10;\
    void MapLocal##Prefix##Parameters()\
    {\
        Map##Prefix##Parameters( {\
            IdType(TEXT(N0), &V0),\
            IdType(TEXT(N1), &V1),\
            IdType(TEXT(N2), &V2),\
            IdType(TEXT(N3), &V3),\
            IdType(TEXT(N4), &V4),\
            IdType(TEXT(N5), &V5),\
            IdType(TEXT(N6), &V6),\
            IdType(TEXT(N7), &V7),\
            IdType(TEXT(N8), &V8),\
            IdType(TEXT(N9), &V9),\
            IdType(TEXT(N10), &V10)\
            } );\
    }\
    void Serialize##Prefix##Parameters(FArchive& Ar)\
    {\
        Ar << V0;\
        Ar << V1;\
        Ar << V2;\
        Ar << V3;\
        Ar << V4;\
        Ar << V5;\
        Ar << V6;\
        Ar << V7;\
        Ar << V8;\
        Ar << V9;\
        Ar << V10;\
    }

