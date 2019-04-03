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
#include "PMUComputeFilterShaderBase.generated.h"

class FPMUComputeFilterRenderThreadWorkerBase
{
public:

    int32 RepeatCount;

    FPMUComputeFilterRenderThreadWorkerBase()
    {
    }

    virtual ~FPMUComputeFilterRenderThreadWorkerBase()
    {
    }

    virtual bool IsMultiPass() const
    {
        return false;
    }

    virtual int32 GetRepeatCount() const
    {
        return RepeatCount;
    }

    virtual FComputeShaderRHIParamRef GetComputeShader(ERHIFeatureLevel::Type FeatureLevel) = 0;
    virtual void BindSwapTexture(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, FTextureRHIParamRef SwapTexture) = 0;
    virtual void BindComputeShaderParameters(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel) = 0;
    virtual void UnbindComputeShaderParameters(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel) = 0;
};

UCLASS(BlueprintType, Blueprintable)
class PROCEDURALMESHUTILITY_API UPMUComputeFilterShaderBase : public UObject
{
	GENERATED_BODY()

public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 RepeatCount = 1;

    virtual TSharedPtr<FPMUComputeFilterRenderThreadWorkerBase> GetRenderThreadWorker()
    {
        return nullptr;
    }
};
