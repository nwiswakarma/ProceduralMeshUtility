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
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PMURenderingLibrary.generated.h"

UCLASS()
class PROCEDURALMESHUTILITY_API UPMURenderingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
    static UTextureRenderTarget2D* CreateRenderTarget2D(
        UObject* WorldContextObject,
        int32 Width,
        int32 Height,
        TEnumAsByte<enum TextureAddress> AddressX,
        TEnumAsByte<enum TextureAddress> AddressY,
        TEnumAsByte<enum ETextureRenderTargetFormat> RenderTargetFormat,
        bool bForceLinearGamma,
        bool bGPUSharedFlag,
        bool bAutoGenerateMips
        );

    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
    static bool IsRenderTargetDimensionAndFormatEqual(UTextureRenderTarget2D* RenderTarget0, UTextureRenderTarget2D* RenderTarget1);

    static bool IsRenderTargetSwapCapable_RT(FTexture2DRHIParamRef Texture0, FTexture2DRHIParamRef Texture1);
};
