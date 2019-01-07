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
#include "TextureResource.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "PMUShaderParameters.generated.h"

UENUM(BlueprintType)
enum class EPMUShaderTextureType : uint8
{
    PMU_STT_Texture2D,
    PMU_STT_TextureRenderTarget2D,
    PMU_STT_Unknown
};

struct FPMUShaderTextureParameterInputResource
{
    EPMUShaderTextureType           TextureType = EPMUShaderTextureType::PMU_STT_Unknown;
    FTexture2DResource*             Texture2DResource = nullptr;
    FTextureRenderTarget2DResource* TextureRenderTarget2DResource = nullptr;

    bool HasValidResource() const
    {
        switch (TextureType)
        {
            case EPMUShaderTextureType::PMU_STT_Texture2D:
                return Texture2DResource != nullptr;

            case EPMUShaderTextureType::PMU_STT_TextureRenderTarget2D:
                return TextureRenderTarget2DResource != nullptr;
        }
        return false;
    }

    FTexture2DRHIParamRef GetTextureParamRef_RT()
    {
        check(IsInRenderingThread());

        FTexture2DRHIParamRef ParamRef = nullptr;

        switch (TextureType)
        {
            case EPMUShaderTextureType::PMU_STT_Texture2D:
                if (Texture2DResource)
                {
                    ParamRef = Texture2DResource->GetTexture2DRHI();
                }
                break;

            case EPMUShaderTextureType::PMU_STT_TextureRenderTarget2D:
                if (TextureRenderTarget2DResource)
                {
                    ParamRef = TextureRenderTarget2DResource->GetTextureRHI();
                }
                break;
        }

        return ParamRef;
    }
};

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUShaderTextureParameterInput
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EPMUShaderTextureType TextureType = EPMUShaderTextureType::PMU_STT_Unknown;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture2D* Texture2D = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTextureRenderTarget2D* TextureRenderTarget2D = nullptr;

    FPMUShaderTextureParameterInput() = default;

    FPMUShaderTextureParameterInput(UTexture2D* InTexture2D)
        : Texture2D(InTexture2D)
        , TextureType(EPMUShaderTextureType::PMU_STT_Texture2D)
    {
    }

    FPMUShaderTextureParameterInput(UTextureRenderTarget2D* InTextureRenderTarget2D)
        : TextureRenderTarget2D(InTextureRenderTarget2D)
        , TextureType(EPMUShaderTextureType::PMU_STT_TextureRenderTarget2D)
    {
    }

    FPMUShaderTextureParameterInputResource GetResource_GT()
    {
        FPMUShaderTextureParameterInputResource Resource;
        Resource.TextureType = TextureType;

        switch (TextureType)
        {
            case EPMUShaderTextureType::PMU_STT_Texture2D:
                if (IsValid(Texture2D))
                {
                    Resource.Texture2DResource = static_cast<FTexture2DResource*>(Texture2D->Resource);
                }
                break;

            case EPMUShaderTextureType::PMU_STT_TextureRenderTarget2D:
                if (IsValid(TextureRenderTarget2D))
                {
                    Resource.TextureRenderTarget2DResource = static_cast<FTextureRenderTarget2DResource*>(TextureRenderTarget2D->GameThread_GetRenderTargetResource());
                }
                break;
        }

        return Resource;
    }
};
