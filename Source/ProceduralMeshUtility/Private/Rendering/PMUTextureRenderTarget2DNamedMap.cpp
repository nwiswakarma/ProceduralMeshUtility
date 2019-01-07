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

#include "Rendering/PMUTextureRenderTarget2DNamedMap.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"

#include "ProceduralMeshUtility.h"
#include "Rendering/PMURenderingLibrary.h"

void UPMUTextureRenderTarget2DNamedMap::SetRenderTargetProperties(
    int32 InWidth,
    int32 InHeight,
    TEnumAsByte<enum ETextureRenderTargetFormat> InRenderTargetFormat,
    TEnumAsByte<enum TextureAddress> InAddressX,
    TEnumAsByte<enum TextureAddress> InAddressY,
    bool bInForceLinearGamma,
    bool bInGPUSharedFlag,
    bool bInAutoGenerateMips
    )
{
    bool bUpdateFormatAndDimension = 
        Width  != InWidth  ||
        Height != InHeight ||
        RenderTargetFormat != InRenderTargetFormat;

    if (bUpdateFormatAndDimension && NamedRenderTargetMap.Num() > 0)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUTextureRenderTarget2DNamedMap::SetRenderTargetProperties() ABORTED, UNABLE TO SET RENDER TARGET FORMAT AND DIMENSION, RENDER TARGET MAP MUST BE EMPTY"));
        return;
    }

    Width  = InWidth;
    Height = InHeight;

    RenderTargetFormat = InRenderTargetFormat;

    AddressX = InAddressX;
    AddressY = InAddressY;

    bForceLinearGamma = bInForceLinearGamma;
    bGPUSharedFlag    = bInGPUSharedFlag;
    bAutoGenerateMips = bInAutoGenerateMips;
}

UTextureRenderTarget2D* UPMUTextureRenderTarget2DNamedMap::CreateRenderTarget2D(UObject* WorldContextObject, FName RenderTargetName)
{
    UTextureRenderTarget2D* RenderTarget = nullptr;

    if (RenderTargetName.IsNone())
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUTextureRenderTarget2DNamedMap::CreateRenderTarget2D() ABORTED, INVALID RENDER TARGET NAME"));
        return RenderTarget;
    }

    if (NamedRenderTargetMap.Contains(RenderTargetName))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUTextureRenderTarget2DNamedMap::CreateRenderTarget2D() ABORTED, RENDER TARGET WITH THE SPECIFIED NAME ALREADY EXIST"));
        return RenderTarget;
    }

    RenderTarget = UPMURenderingLibrary::CreateRenderTarget2D(
        WorldContextObject,
        Width,
        Height,
        AddressX,
        AddressY,
        RenderTargetFormat,
        bForceLinearGamma,
        bGPUSharedFlag,
        bAutoGenerateMips
        );

    if (! RenderTarget)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUTextureRenderTarget2DNamedMap::CreateRenderTarget2D() ABORTED, RENDER TARGET CREATION FAILED"));
        return RenderTarget;
    }

    NamedRenderTargetMap.Emplace(RenderTargetName, RenderTarget);

    return RenderTarget;
}

void UPMUTextureRenderTarget2DNamedMap::CreateRenderTarget2DByNames(UObject* WorldContextObject, TArray<FName> RenderTargetNames)
{
    for (const FName& RenderTargetName : RenderTargetNames)
    {
        CreateRenderTarget2D(WorldContextObject, RenderTargetName);
    }
}

bool UPMUTextureRenderTarget2DNamedMap::RemoveRenderTarget(FName RenderTargetName)
{
    int32 NumRemoved = NamedRenderTargetMap.Remove(RenderTargetName);
    return NumRemoved > 0;
}

bool UPMUTextureRenderTarget2DNamedMap::RemoveAndReleaseRenderTarget(FName RenderTargetName)
{
    if (NamedRenderTargetMap.Contains(RenderTargetName))
    {
        UTextureRenderTarget2D* RenderTarget = NamedRenderTargetMap.FindChecked(RenderTargetName);

        UKismetRenderingLibrary::ReleaseRenderTarget2D(RenderTarget);

        NamedRenderTargetMap.FindAndRemoveChecked(RenderTargetName);

        return true;
    }

    return false;
}

void UPMUTextureRenderTarget2DNamedMap::ClearAndReleaseRenderTargets()
{
    for (const auto& RTTPair : NamedRenderTargetMap)
    {
        UKismetRenderingLibrary::ReleaseRenderTarget2D(RTTPair.Value);
    }

    NamedRenderTargetMap.Empty();
}

UTextureRenderTarget2D* UPMUTextureRenderTarget2DNamedMap::GetRenderTarget2D(FName RenderTargetName) const
{
    return NamedRenderTargetMap.Contains(RenderTargetName)
        ? NamedRenderTargetMap.FindChecked(RenderTargetName)
        : nullptr;
}
