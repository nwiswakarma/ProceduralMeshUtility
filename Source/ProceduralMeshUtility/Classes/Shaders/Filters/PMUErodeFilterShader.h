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
#include "PMUErodeFilterShader.generated.h"

class FPMUErodeFilterShaderRenderThreadWorker : public FPMUFilterRenderThreadWorkerBase
{
public:

    FPMUShaderTextureParameterInputResource SourceTexture;
    FPMUShaderTextureParameterInputResource WeightTexture;

    float InclineFactor;
    bool bUseBlendIncline;
    bool bUseInclineFactor;
    bool bUseInvertIncline;
    bool bUseSampleAX;
    bool bUseDilateFilter;

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
class PROCEDURALMESHUTILITY_API UPMUErodeFilterShader : public UPMUFilterShaderBase
{
	GENERATED_BODY()

public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FPMUShaderTextureParameterInput SourceTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FPMUShaderTextureParameterInput WeightTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InclineFactor = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseBlendIncline = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseInclineFactor = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseInvertIncline = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseSampleAX = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseDilateFilter = false;

    UFUNCTION(BlueprintCallable)
    void SetParameters(
        FPMUShaderTextureParameterInput InSourceTexture,
        FPMUShaderTextureParameterInput InWeightTexture,
        int32 InRepeatCount = 0,
        float InInclineFactor = 0.2f,
        bool bInUseBlendIncline  = false,
        bool bInUseInclineFactor = false,
        bool bInUseInvertIncline = false,
        bool bInUseSampleAX      = false,
        bool bInUseDilateFilter  = false
        )
    {
        SourceTexture = InSourceTexture;
        WeightTexture = InWeightTexture;
        RepeatCount   = InRepeatCount;
        InclineFactor = InInclineFactor;
        bUseBlendIncline  = bInUseBlendIncline;
        bUseInclineFactor = bInUseInclineFactor;
        bUseInvertIncline = bInUseInvertIncline;
        bUseSampleAX      = bInUseSampleAX;
        bUseDilateFilter  = bInUseDilateFilter;
    }

    virtual TSharedPtr<FPMUFilterRenderThreadWorkerBase> GetRenderThreadWorker() override
    {
        FPMUErodeFilterShaderRenderThreadWorker* Worker(new FPMUErodeFilterShaderRenderThreadWorker);
        TSharedPtr<FPMUFilterRenderThreadWorkerBase> WorkerShared(Worker);

        Worker->SourceTexture     = SourceTexture.GetResource_GT();
        Worker->WeightTexture     = WeightTexture.GetResource_GT();
        Worker->RepeatCount       = RepeatCount;
        Worker->InclineFactor     = InclineFactor;
        Worker->bUseBlendIncline  = bUseBlendIncline;
        Worker->bUseInclineFactor = bUseInclineFactor;
        Worker->bUseInvertIncline = bUseInvertIncline;
        Worker->bUseSampleAX      = bUseSampleAX;
        Worker->bUseDilateFilter  = bUseDilateFilter;
        
        return WorkerShared;
    }
};
