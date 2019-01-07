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
#include "Shaders/PMUShaderParameters.h"
#include "Shaders/Filters/PMUFilterShaderBase.h"
#include "PMUDirectionalWarpFilterShader.generated.h"

class FPMUDirectionalWarpFilterShaderRenderThreadWorker : public FPMUFilterRenderThreadWorkerBase
{
public:

    FPMUShaderTextureParameterInputResource SourceTexture;
    FPMUShaderTextureParameterInputResource WeightTexture;
    FPMUShaderTextureParameterInputResource DirectionalTexture;

    float RadialDirection;
    float Strength;
    float StrengthDual;
    bool bUseBlendIncline;
    bool bUseDirectionalMap;
    bool bUseDualSampling;
    bool bUseSampleMinimum;
    bool bUseSampleMinimumDual;

    virtual bool IsMultiPass() const override
    {
        return true;
    }

    virtual FPixelShaderRHIParamRef GetPixelShader(ERHIFeatureLevel::Type FeatureLevel) override;
    virtual void BindSwapTexture(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, FTextureRHIParamRef SwapTexture) override;
    virtual void BindPixelShaderParameters(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel) override;
    virtual void UnbindPixelShaderParameters(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel) override;
};

UCLASS()
class PROCEDURALMESHUTILITY_API UPMUDirectionalWarpFilterShader : public UPMUFilterShaderBase
{
	GENERATED_BODY()

public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FPMUShaderTextureParameterInput SourceTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FPMUShaderTextureParameterInput WeightTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FPMUShaderTextureParameterInput DirectionalTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RadialDirection = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Strength = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StrengthDual = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseBlendIncline = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseDualSampling = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseSampleMinimum = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseSampleMinimumDual = false;

    UFUNCTION(BlueprintCallable)
    void SetParameters(
        FPMUShaderTextureParameterInput InSourceTexture,
        FPMUShaderTextureParameterInput InWeightTexture,
        FPMUShaderTextureParameterInput InDirectionalTexture,
        int32 InRepeatCount = 0,
        float InRadialDirection = 0.f,
        float InStrength = 0.5f,
        float InStrengthDual = 0.5f,
        bool bInUseBlendIncline = false,
        bool bInUseDualSampling = false,
        bool bInUseSampleMinimum = false,
        bool bInUseSampleMinimumDual = false
        )
    {
        SourceTexture = InSourceTexture;
        WeightTexture = InWeightTexture;
        DirectionalTexture = InDirectionalTexture;
        RepeatCount = InRepeatCount;
        RadialDirection = InRadialDirection;
        Strength = InStrength;
        bUseSampleMinimum = bInUseSampleMinimum;
        bUseBlendIncline = bInUseBlendIncline;

        // Dual configuration
        StrengthDual = InStrengthDual;
        bUseDualSampling = bInUseDualSampling;
        bUseSampleMinimumDual = bInUseSampleMinimumDual;
    }

    virtual TSharedPtr<FPMUFilterRenderThreadWorkerBase> GetRenderThreadWorker() override
    {
        FPMUDirectionalWarpFilterShaderRenderThreadWorker* Worker(new FPMUDirectionalWarpFilterShaderRenderThreadWorker);
        TSharedPtr<FPMUFilterRenderThreadWorkerBase> WorkerShared(Worker);

        Worker->SourceTexture = SourceTexture.GetResource_GT();
        Worker->WeightTexture = WeightTexture.GetResource_GT();
        Worker->DirectionalTexture = DirectionalTexture.GetResource_GT();
        Worker->RepeatCount = RepeatCount;
        Worker->RadialDirection = RadialDirection;
        Worker->Strength = Strength;
        Worker->bUseBlendIncline = bUseBlendIncline;
        Worker->bUseSampleMinimum = bUseSampleMinimum;
        Worker->bUseDirectionalMap = Worker->DirectionalTexture.HasValidResource();

        // Dual configuration
        Worker->StrengthDual = StrengthDual;
        Worker->bUseDualSampling = bUseDualSampling;
        Worker->bUseSampleMinimumDual = bUseSampleMinimumDual;

        return WorkerShared;
    }
};
