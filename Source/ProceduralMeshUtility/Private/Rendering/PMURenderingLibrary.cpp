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

#include "Rendering/PMURenderingLibrary.h"

#include "Engine/TextureRenderTarget2D.h"

#include "ProceduralMeshUtility.h"

UTextureRenderTarget2D* UPMURenderingLibrary::CreateRenderTarget2D(
    UObject* WorldContextObject,
    int32 Width,
    int32 Height,
    TEnumAsByte<enum TextureAddress> AddressX,
    TEnumAsByte<enum TextureAddress> AddressY,
    TEnumAsByte<enum ETextureRenderTargetFormat> RenderTargetFormat,
    bool bForceLinearGamma,
    bool bGPUSharedFlag,
    bool bAutoGenerateMips
    )
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

	if (Width > 0 && Height > 0 && World && FApp::CanEverRender())
	{
		UTextureRenderTarget2D* NewRenderTarget2D = NewObject<UTextureRenderTarget2D>(WorldContextObject);
		check(NewRenderTarget2D);
        NewRenderTarget2D->AddressX = AddressX;
        NewRenderTarget2D->AddressY = AddressY;
        NewRenderTarget2D->RenderTargetFormat = RenderTargetFormat;
        NewRenderTarget2D->bForceLinearGamma = bForceLinearGamma;
        NewRenderTarget2D->bGPUSharedFlag = bGPUSharedFlag;
        NewRenderTarget2D->bAutoGenerateMips = bAutoGenerateMips;
		NewRenderTarget2D->InitAutoFormat(Width, Height);
		NewRenderTarget2D->UpdateResourceImmediate(true);
		return NewRenderTarget2D; 
	}

	return nullptr;
}

bool UPMURenderingLibrary::IsRenderTargetDimensionAndFormatEqual(UTextureRenderTarget2D* RenderTarget0, UTextureRenderTarget2D* RenderTarget1)
{
    return (
        IsValid(RenderTarget0) &&
        IsValid(RenderTarget1) &&
        RenderTarget0->SizeX == RenderTarget1->SizeX &&
        RenderTarget0->SizeY == RenderTarget1->SizeY &&
        RenderTarget0->GetFormat() == RenderTarget1->GetFormat()
        );
}

bool UPMURenderingLibrary::IsRenderTargetSwapCapable_RT(FTexture2DRHIParamRef Texture0, FTexture2DRHIParamRef Texture1)
{

    return (
        Texture0 &&
        Texture1 &&
        Texture0->GetSizeX() == Texture1->GetSizeX() &&
        Texture0->GetSizeY() == Texture1->GetSizeY() &&
        Texture0->GetFormat() == Texture1->GetFormat() &&
        (Texture0->GetFlags() & TexCreate_RenderTargetable) != 0 &&
        (Texture1->GetFlags() & TexCreate_RenderTargetable) != 0 &&
        Texture0->GetNumSamples() && Texture1->GetNumSamples()
        );
}
