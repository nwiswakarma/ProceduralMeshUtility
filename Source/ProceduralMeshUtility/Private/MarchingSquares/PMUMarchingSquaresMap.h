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
#include "Mesh/PMUMeshTypes.h"
#include "RHI/PMURWBuffer.h"
#include "PMUMarchingSquaresTypes.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMSq, Verbose, All);
DECLARE_LOG_CATEGORY_EXTERN(UntMSq, Verbose, All);

class FPMUMarchingSquaresMap
{
private:

    struct FPrefabData
    {
        TArray<FBox2D> Bounds;

        FPrefabData(const TArray<FBox2D>& InBounds)
            : Bounds(InBounds)
        {
        }
    };

    TArray<FPrefabData> AppliedPrefabs;
    TArray< TArray<FPMUMeshSectionResource> > SectionGroups;

    // Build map properties

    FIntPoint Dimension_GT;
    FIntPoint Dimension_RT;
    bool bDimensionUpdated = true;

    FPMURWBuffer VoxelStateData;
    FPMURWBuffer VoxelFeatureData;

    FRHICommandListImmediate*      RHICmdListPtr = nullptr;
    TShaderMap<FGlobalShaderType>* RHIShaderMap  = nullptr;

    FTextureRHIRef             DebugRTTRHI;
	FTexture2DRHIRef           DebugTextureRHI;
    FUnorderedAccessViewRHIRef DebugTextureUAV;

    // Render thread functions

    void ClearMap_RT(FRHICommandListImmediate& RHICmdList);
    void InitializeVoxelData_RT(FRHICommandListImmediate& RHICmdList, FIntPoint InDimension);

    bool BuildMapExec(uint32 FillType, bool bGenerateWalls);
    void BuildMap_RT(FRHICommandListImmediate& RHICmdList, uint32 FillType, bool bGenerateWalls, ERHIFeatureLevel::Type InFeatureLevel);

    void GenerateMarchingCubes_RT(uint32 FillType, bool bGenerateWalls);

public:
    
    DECLARE_EVENT_TwoParams(FPMUMarchingSquaresMap, FBuildMapDone, bool, uint32);

    FORCEINLINE FBuildMapDone& OnBuildMapDone()
    {
        return BuildMapDoneEvent;
    }

private:

    FBuildMapDone BuildMapDoneEvent;

public:

    int32 BlockSize = 64;

    bool bOverrideBoundsZ = false;
    float BoundsSurfaceOverrideZ = 0.f;
    float BoundsExtrudeOverrideZ = 0.f;

	float BaseHeightOffset = 1.f;
	float SurfaceHeightScale = 1.f;
	float ExtrudeHeightScale = 1.f;
	float BaseOffsetScale = 1.f;
	float SurfaceOffsetScale = 1.f;
	float ExtrudeOffsetScale = 1.f;
    int32 SurfaceHeightTextureMipLevel = 0;
    int32 ExtrudeHeightTextureMipLevel = 0;
    int32 HeightOffsetTextureMipLevel = 0;
    FVector2D HeightOffsetSampleOffset;
    float HeightOffsetSampleScale = 1.f;

    TArray<class UStaticMesh*> MeshPrefabs;

    FTexture2DRHIParamRef SurfaceHeightTexture;
    FTexture2DRHIParamRef ExtrudeHeightTexture;
    FTexture2DRHIParamRef HeightOffsetTexture;

    UTextureRenderTarget2D* DebugRTT;

    // Game thread accessors

    FORCEINLINE int32 GetVoxelCount() const
    {
        return Dimension_GT.X * Dimension_GT.Y;
    }

    FORCEINLINE FIntPoint GetDimension() const
    {
        return Dimension_GT;
    }

    FORCEINLINE bool HasValidDimension() const
    {
        return Dimension_GT.X > 0 && Dimension_GT.Y > 0 && BlockSize > 0 && BlockSize < FMath::Max(Dimension_GT.X, Dimension_GT.Y);
    }

    // Render thread accessors

    FORCEINLINE int32 GetVoxelCount_RT() const
    {
        return Dimension_RT.X * Dimension_RT.Y;
    }

    FORCEINLINE FIntPoint GetDimension_RT() const
    {
        return Dimension_RT;
    }

    FORCEINLINE bool HasValidDimension_RT() const
    {
        return Dimension_RT.X > 0 && Dimension_RT.Y > 0 && BlockSize > 0 && BlockSize < FMath::Max(Dimension_RT.X, Dimension_RT.Y);
    }

    // MAP GENERATION FUNCTIONS

    void SetDimension(FIntPoint InDimension);
    void SetHeightTexture(FTexture2DRHIParamRef HeightTexture, TEnumAsByte<EPMUMarchingSquaresHeightTextureType::Type> HeightTextureType);
    void InitializeVoxelData();
    void BuildMap(int32 FillType, bool bGenerateWalls);
    void ClearMap();

    // RENDER THREAD FUNCTIONS

    FORCEINLINE FPMURWBuffer& GetVoxelStateData()
    {
        return VoxelStateData;
    }

    FORCEINLINE FPMURWBuffer& GetVoxelFeatureData()
    {
        return VoxelFeatureData;
    }

    FORCEINLINE FUnorderedAccessViewRHIRef& GetDebugRTTUAV()
    {
        return DebugTextureUAV;
    }

    // SECTION FUNCTIONS

    FORCEINLINE bool HasStateSection(int32 FillType) const
    {
        return SectionGroups.IsValidIndex(FillType);
    }

    FORCEINLINE bool HasSection(int32 FillType, int32 Index) const
    {
        return (SectionGroups.IsValidIndex(FillType) && SectionGroups[FillType].IsValidIndex(Index));
    }

    FORCEINLINE const FPMUMeshSectionResource& GetSectionChecked(int32 FillType, int32 Index) const
    {
        return SectionGroups[FillType][Index];
    }

    FORCEINLINE FPMUMeshSectionResource& GetSectionChecked(int32 FillType, int32 Index)
    {
        return SectionGroups[FillType][Index];
    }

    void ClearStateSection(int32 FillType)
    {
        if (HasStateSection(FillType))
        {
            SectionGroups[FillType].Empty();
        }
    }

    void ClearSections()
    {
        SectionGroups.Empty();
    }

    // PREFAB FUNCTIONS

	FORCEINLINE bool HasPrefab(int32 PrefabIndex) const
    {
        return MeshPrefabs.IsValidIndex(PrefabIndex) && IsValid(MeshPrefabs[PrefabIndex]);
    }

    bool IsPrefabValid(int32 PrefabIndex, int32 LODIndex, int32 SectionIndex) const;
    bool HasIntersectingBounds(const TArray<FBox2D>& Bounds) const;
	void GetPrefabBounds(int32 PrefabIndex, TArray<FBox2D>& Bounds) const;
	TArray<FBox2D> GetPrefabBounds(int32 PrefabIndex) const;

    bool TryPlacePrefabAt(int32 PrefabIndex, const FVector2D& Center);

	bool ApplyPrefab(
        int32 PrefabIndex,
        int32 SectionIndex,
        FVector Center,
        bool bApplyHeightMap,
        bool bInverseHeightMap,
        UPARAM(ref) FPMUMeshSection& OutSection
        );
};
