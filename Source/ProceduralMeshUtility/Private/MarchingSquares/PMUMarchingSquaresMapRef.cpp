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

#include "PMUMarchingSquaresMapRef.h"

#include "ProceduralMeshUtility.h"

#include "GenericWorkerThread.h"
#include "GWTTickManager.h"

#ifdef PMU_SUBSTANCE_ENABLED
#include "SubstanceTexture2D.h"
#include "SubstanceTexture2DDynamicResource.h"
#endif

UPMUMarchingSquaresMapRef::UPMUMarchingSquaresMapRef(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Register build map callback
    Map.OnBuildMapDone().AddUObject(this, &UPMUMarchingSquaresMapRef::OnBuildMapDoneCallback);
}

// MAP SETTINGS FUNCTIONS

void UPMUMarchingSquaresMapRef::ApplyMapSettings()
{
    Map.SetDimension(FIntPoint(DimX, DimY));
    Map.BlockSize = BlockSize;

    Map.bOverrideBoundsZ = bOverrideBoundsZ;
    Map.BoundsSurfaceOverrideZ = BoundsSurfaceOverrideZ;
    Map.BoundsExtrudeOverrideZ = BoundsExtrudeOverrideZ;

    Map.SurfaceHeightScale = SurfaceHeightScale;
    Map.ExtrudeHeightScale = ExtrudeHeightScale;

    Map.BaseHeightOffset  = BaseHeightOffset;
    Map.HeightMapMipLevel = HeightMapMipLevel;

    Map.MeshPrefabs = MeshPrefabs;
    Map.DebugRTT = DebugRTT;
}

void UPMUMarchingSquaresMapRef::SetHeightMap(FPMUShaderTextureParameterInput TextureInput)
{
    FPMUShaderTextureParameterInputResource TextureResource(TextureInput.GetResource_GT());

    ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
        UPMUMarchingSquaresMapRef_SetHeightMap,
        FPMUMarchingSquaresMap*, Map, &Map,
        FPMUShaderTextureParameterInputResource, TextureResource, TextureResource,
        {
            Map->SetHeightMap(TextureResource.GetTextureParamRef_RT());
        } );
}

void UPMUMarchingSquaresMapRef::SetHeightMapFromUTexture2D(UTexture2D* HeightMap)
{
    if (IsValid(HeightMap))
    {
        FTexture2DResource* TextureResource = static_cast<FTexture2DResource*>(HeightMap->Resource);

        if (! TextureResource)
        {
            UE_LOG(LogPMU,Warning, TEXT("UPMUMarchingSquaresMapRef::SetHeightMapFromUTexture2D() ABORTED - Invalid HeightMap resource"));
            return;
        }

        ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
            UPMUMarchingSquaresMapRef_SetHeightMapFromUTexture2D,
            FPMUMarchingSquaresMap*, Map, &Map,
            FTexture2DResource*, TextureResource, TextureResource,
            {
                FTexture2DRHIParamRef Texture2DRHI = nullptr;

                if (TextureResource)
                {
                    Texture2DRHI = TextureResource->GetTexture2DRHI();
                }

                Map->SetHeightMap(Texture2DRHI);
            } );
    }
}

void UPMUMarchingSquaresMapRef::SetHeightMapFromRTT(UTextureRenderTarget2D* HeightMap)
{
    if (IsValid(HeightMap))
    {
        FTextureRenderTarget2DResource* TextureResource = static_cast<FTextureRenderTarget2DResource*>(HeightMap->GameThread_GetRenderTargetResource());

        if (! TextureResource)
        {
            UE_LOG(LogPMU,Warning, TEXT("UPMUMarchingSquaresMapRef::SetHeightMapFromRTT() ABORTED - Invalid HeightMap resource"));
            return;
        }

        ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
            UPMUMarchingSquaresMapRef_SetHeightMapFromUTexture2D,
            FPMUMarchingSquaresMap*, Map, &Map,
            FTextureRenderTarget2DResource*, TextureResource, TextureResource,
            {
                FTexture2DRHIParamRef Texture2DRHI = nullptr;

                if (TextureResource)
                {
                    Texture2DRHI = TextureResource->GetTextureRHI();
                }

                Map->SetHeightMap(Texture2DRHI);
            } );
    }
}

void UPMUMarchingSquaresMapRef::SetHeightMapFromSubstanceTexture(UObject* HeightMap)
{
#ifdef PMU_SUBSTANCE_ENABLED
    USubstanceTexture2D* SubstanceTexture = Cast<USubstanceTexture2D>(HeightMap);

    if (IsValid(SubstanceTexture))
    {
        FSubstanceTexture2DDynamicResource* TextureResource = static_cast<FSubstanceTexture2DDynamicResource*>(SubstanceTexture->Resource);

        if (! TextureResource)
        {
            UE_LOG(LogPMU,Warning, TEXT("UPMUMarchingSquaresMapRef::SetHeightMapFromSubstanceTexture() ABORTED - Invalid SubstanceTexture resource"));
            return;
        }

        ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
            UPMUMarchingSquaresMapRef_SetHeightMapFromUTexture2D,
            FPMUMarchingSquaresMap*, Map, &Map,
            FSubstanceTexture2DDynamicResource*, TextureResource, TextureResource,
            {
                FTexture2DRHIParamRef Texture2DRHI = nullptr;

                if (TextureResource)
                {
                    Texture2DRHI = (FTexture2DRHIParamRef) TextureResource->TextureRHI.GetReference();
                }

                Map->SetHeightMap(Texture2DRHI);
            } );
    }
#endif
}

void UPMUMarchingSquaresMapRef::OnBuildMapDoneCallback(bool bBuildMapResult, uint32 FillType)
{
    FGWTTickManager& TickManager(IGenericWorkerThread::Get().GetTickManager());
    FGWTTickManager::FTickCallback TickCallback(
        [this, bBuildMapResult, FillType]()
        {
            OnBuildMapDone.Broadcast(bBuildMapResult, FillType);
        } );
    
    TickManager.EnqueueTickCallback(TickCallback);
}

void UPMUMarchingSquaresMapRef::BuildMap(int32 FillType, bool bGenerateWalls)
{
    if (Map.HasValidDimension())
    {
        Map.BuildMap(FMath::Max(0, FillType), bGenerateWalls);
    }
    else
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMarchingSquaresMapRef::BuildMap() ABORTED - Invalid map dimension"));
    }
}

void UPMUMarchingSquaresMapRef::ClearMap()
{
    Map.ClearMap();
}

// SECTION FUNCTIONS

bool UPMUMarchingSquaresMapRef::HasSection(int32 FillType, int32 Index) const
{
    return Map.HasSection(FillType, Index);
}

bool UPMUMarchingSquaresMapRef::HasSectionGeometry(int32 FillType, int32 Index) const
{
    if (HasSection(FillType, Index))
    {
        const FPMUMeshSectionResource& Resource(Map.GetSectionChecked(FillType, Index));
        return Resource.GetVertexCount() > 0 && Resource.GetIndexCount() > 0;
    }

    return false;
}

FPMUMeshSectionResourceRef UPMUMarchingSquaresMapRef::GetSectionResource(int32 FillType, int32 Index)
{
    return Map.HasSection(FillType, Index)
        ? FPMUMeshSectionResourceRef(Map.GetSectionChecked(FillType, Index))
        : FPMUMeshSectionResourceRef();
}

void UPMUMarchingSquaresMapRef::CopySectionResource(FPMUMeshSectionResourceRef TargetSection, int32 FillType, int32 Index) const
{
    if (TargetSection.IsValid() && Map.HasSection(FillType, Index))
    {
        (*TargetSection.SectionPtr) = Map.GetSectionChecked(FillType, Index);
    }
}

void UPMUMarchingSquaresMapRef::ClearStateSectionResource(int32 FillType)
{
    Map.ClearStateSection(FillType);
}

// PREFAB FUNCTIONS

TArray<FBox2D> UPMUMarchingSquaresMapRef::GetPrefabBounds(int32 PrefabIndex) const
{
    //return HasValidMap()
    //    ? Map.GetPrefabBounds(PrefabIndex)
    //    : TArray<FBox2D>();

    return TArray<FBox2D>();
}

bool UPMUMarchingSquaresMapRef::HasPrefab(int32 PrefabIndex) const
{
    //return HasValidMap() ? Map.HasPrefab(PrefabIndex) : false;

    return false;
}

bool UPMUMarchingSquaresMapRef::ApplyPrefab(
    int32 PrefabIndex,
    int32 SectionIndex,
    FVector Location,
    bool bApplyHeightMap,
    bool bInverseHeightMap,
    FPMUMeshSection& OutSection
    )
{
    //if (HasValidMap())
    //{
    //    return Map.ApplyPrefab(
    //        PrefabIndex,
    //        SectionIndex,
    //        Location,
    //        bApplyHeightMap,
    //        bInverseHeightMap,
    //        OutSection
    //        );
    //}

    return false;
}
