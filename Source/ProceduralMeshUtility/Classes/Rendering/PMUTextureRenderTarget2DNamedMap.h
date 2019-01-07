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
#include "PMUTextureRenderTarget2DNamedMap.generated.h"

UCLASS(BlueprintType, Blueprintable)
class PROCEDURALMESHUTILITY_API UPMUTextureRenderTarget2DNamedMap : public UObject
{
	GENERATED_BODY()

public:

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TMap<FName, UTextureRenderTarget2D*> NamedRenderTargetMap;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=TextureRenderTarget2D)
    int32 Width;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=TextureRenderTarget2D)
    int32 Height;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=TextureRenderTarget2D)
    TEnumAsByte<enum ETextureRenderTargetFormat> RenderTargetFormat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=TextureRenderTarget2D)
    TEnumAsByte<enum TextureAddress> AddressX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=TextureRenderTarget2D)
    TEnumAsByte<enum TextureAddress> AddressY;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=TextureRenderTarget2D)
    bool bForceLinearGamma;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=TextureRenderTarget2D)
    bool bGPUSharedFlag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=TextureRenderTarget2D)
    bool bAutoGenerateMips;

    UFUNCTION(BlueprintCallable)
    void SetRenderTargetProperties(
        int32 InWidth,
        int32 InHeight,
        TEnumAsByte<enum ETextureRenderTargetFormat> InRenderTargetFormat,
        TEnumAsByte<enum TextureAddress> InAddressX,
        TEnumAsByte<enum TextureAddress> InAddressY,
        bool bInForceLinearGamma,
        bool bInGPUSharedFlag,
        bool bInAutoGenerateMips
        );

    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject"))
    UTextureRenderTarget2D* CreateRenderTarget2D(UObject* WorldContextObject, FName RenderTargetName);

    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject"))
    void CreateRenderTarget2DByNames(UObject* WorldContextObject, TArray<FName> RenderTargetNames);

    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject"))
    bool RemoveRenderTarget(FName RenderTargetName);

    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject"))
    bool RemoveAndReleaseRenderTarget(FName RenderTargetName);

    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject"))
    void ClearAndReleaseRenderTargets();

    UFUNCTION(BlueprintCallable)
    UTextureRenderTarget2D* GetRenderTarget2D(FName RenderTargetName) const;
};
