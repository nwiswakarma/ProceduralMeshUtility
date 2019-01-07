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
#include "ProceduralMeshUtility.h"
#include "PMUUtilityLibrary.h"
#include "Mesh/PMUMeshTypes.h"
#include "Shaders/PMUShaderParameters.h"
#include "GWTTickUtilities.h"
#include "PMUGridData.generated.h"

class UGWTPromiseObject;

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUGridDataRef
{
    GENERATED_BODY()

    struct FPMUGridData* Grid;

    FPMUGridDataRef() : Grid(nullptr)
    {
    }

    FPMUGridDataRef(struct FPMUGridData& InGrid) : Grid(&InGrid)
    {
    }
};

struct FPMUGridData
{
    using FIndices        = TArray<int32>;
    using FIndexContainer = TArray<FIndices>;

    using FPointPath  = TArray<FVector2D>;
    using FPointPaths = TArray<FPointPath>;

    typedef TArray<uint8> FMaskMap;
    typedef TSet<int32>   FPointSet;

    typedef TArray<float>      FHeightMap;
    typedef TArray<FHeightMap> FHeightMapContainer;

    typedef FPMUMapGenerationInfo FMapInfo;

    FIntPoint Dimension;
    TArray<int32, TFixedAllocator<8>> NDIR;

    // Mask Maps
    FMaskMap PointMask;

    // Height Maps
    FHeightMapContainer HeightMaps;
    TMap<FName, int32> NamedHeightMap;

    // Set Data
    FPointSet PointSet;
    FPointSet BorderSet;

#ifdef PMU_VOXEL_USE_OCL

    // GPU Height Maps
    TArray<TSharedPtr<boost::compute::image2d>> GPUHeightMaps;

#endif // PMU_VOXEL_USE_OCL

    FPMUGridData() = default;

    FPMUGridData(const FIntPoint& InDimension)
    {
        SetDimension(InDimension);
    }

    void SetDimension(const FIntPoint& InDimension)
    {
        Dimension = InDimension;
        NDIR = {
            FPMUGridInfo::NDIR[0][0] + FPMUGridInfo::NDIR[0][1]*InDimension.X,
            FPMUGridInfo::NDIR[1][0] + FPMUGridInfo::NDIR[1][1]*InDimension.X,
            FPMUGridInfo::NDIR[2][0] + FPMUGridInfo::NDIR[2][1]*InDimension.X,
            FPMUGridInfo::NDIR[3][0] + FPMUGridInfo::NDIR[3][1]*InDimension.X,
            FPMUGridInfo::NDIR[4][0] + FPMUGridInfo::NDIR[4][1]*InDimension.X,
            FPMUGridInfo::NDIR[5][0] + FPMUGridInfo::NDIR[5][1]*InDimension.X,
            FPMUGridInfo::NDIR[6][0] + FPMUGridInfo::NDIR[6][1]*InDimension.X,
            FPMUGridInfo::NDIR[7][0] + FPMUGridInfo::NDIR[7][1]*InDimension.X
            };
    }

    void Reset()
    {
        PointMask.Reset(CalcSize());
        PointMask.SetNumZeroed(CalcSize());

        HeightMaps.Empty();
    }

    void Empty();

    FORCEINLINE int32 CalcSize() const
    {
        return Dimension.X * Dimension.Y;
    }

    FORCEINLINE bool HasValidPointMask() const
    {
        return CalcSize() > 0 && PointMask.Num() == CalcSize();
    }

    FORCEINLINE bool IsValidIndex(int32 i) const
    {
        return PointMask.IsValidIndex(i);
    }

    FORCEINLINE bool HasTexel(int32 X, int32 Y) const
    {
        return HasTexel(X + Y*Dimension.X);
    }

    FORCEINLINE bool HasTexel(int32 i) const
    {
        check(IsValidIndex(i));
        return PointMask[i] > FPMUGridInfo::MASK_THRESHOLD;
    }

    FORCEINLINE bool IsSolid(int32 i) const
    {
        return HasTexel(i);
    }

    FORCEINLINE bool IsBorder(int32 i) const
    {
        return BorderSet.Contains(i);
    }

    FORCEINLINE int32 GetIdx(int32 i, int32 ni) const
    {
        return i+NDIR[ni];
    }

    FORCEINLINE bool HasValidSize(int32 MapId) const
    {
        check(HeightMaps.IsValidIndex(MapId));
        const FHeightMap& HeightMap(HeightMaps[MapId]);
        return HeightMap.Num() > 0 && HeightMap.Num() == CalcSize();
    }

    FORCEINLINE bool HasHeightMap(int32 MapId) const
    {
        return HeightMaps.IsValidIndex(MapId) ? HasValidSize(MapId) : false;
    }

    FORCEINLINE const FHeightMap* GetHeightMap(int32 MapId) const
    {
        if (HeightMaps.IsValidIndex(MapId))
        {
            const FHeightMap& HeightMap(HeightMaps[MapId]);
            return (HeightMap.Num() == CalcSize()) ? &HeightMap : nullptr;
        }

        return nullptr;
    }

    FORCEINLINE FHeightMap& GetHeightMapChecked(int32 MapId)
    {
        return HeightMaps[MapId];
    }

    FORCEINLINE const FHeightMap& GetHeightMapChecked(int32 MapId) const
    {
        return HeightMaps[MapId];
    }

    FORCEINLINE bool HasNamedHeightMap(const FName& MapName) const
    {
        return NamedHeightMap.Contains(MapName)
            ? HasHeightMap(NamedHeightMap.FindChecked(MapName))
            : false;
    }

    FORCEINLINE const FHeightMap* GetNamedHeightMap(const FName& MapName) const
    {
        return HasNamedHeightMap(MapName)
            ? GetHeightMap(NamedHeightMap.FindChecked(MapName))
            : nullptr;
    }

    FORCEINLINE int32 GetNamedHeightMapId(const FName& MapName) const
    {
        return HasNamedHeightMap(MapName)
            ? NamedHeightMap.FindChecked(MapName)
            : -1;
    }

    FORCEINLINE FHeightMap& GetNamedHeightMapChecked(const FName& MapName)
    {
        return GetHeightMapChecked(NamedHeightMap.FindChecked(MapName));
    }

    FORCEINLINE const FHeightMap& GetNamedHeightMapChecked(const FName& MapName) const
    {
        return GetHeightMapChecked(NamedHeightMap.FindChecked(MapName));
    }

    int32 CreateHeightMap()
    {
        return CreateHeightMap(HeightMaps.Num());
    }

    int32 CreateHeightMap(const int32 MapId)
    {
        const int32 MapSize = CalcSize();

        if (MapSize > 0)
        {
            if (! HeightMaps.IsValidIndex(MapId))
            {
                HeightMaps.SetNum(MapId+1, false);
            }

            HeightMaps[MapId].SetNumZeroed(MapSize, true);

            return MapId;
        }

        return -1;
    }

    void SetHeightMapNum(int32 MapNum, bool bShrink = false)
    {
        HeightMaps.SetNum(MapNum, bShrink);
    }

    void ApplyHeightBlend(const FHeightMap& InHeightMap, const FMapInfo& MapInfo)
    {
        const int32 MapId = MapInfo.DstID;
        const int32 MapSize = CalcSize();
        const EPMUHeightBlendType BlendType = MapInfo.BlendType;

        // Validate map properties
        if (MapId < 0 || InHeightMap.Num() != MapSize)
        {
            UE_LOG(LogPMU,Error, TEXT("FPMUGridData::ApplyHeightBlend(): Invalid height map info"));
            return;
        }

        // If target map does not exist expand the map pool to create a new one
        if (! HeightMaps.IsValidIndex(MapId))
        {
            HeightMaps.SetNum(MapId+1);
        }

        const TArray<float>& Src(InHeightMap);
        TArray<float>& Dst(HeightMaps[MapId]);

        // Target map have been created but uninitialized, initialize it now
        if (Src.Num() != Dst.Num())
        {
            Dst.SetNumZeroed(MapSize, true);
        }

        check(Src.Num() == Dst.Num());

        switch (BlendType)
        {
            case EPMUHeightBlendType::ADD:
                for (int32 i=0; i<Src.Num(); ++i)
                    Dst[i] += Src[i];
                break;

            case EPMUHeightBlendType::MUL:
                for (int32 i=0; i<Src.Num(); ++i)
                    Dst[i] *= Src[i];
                break;

            case EPMUHeightBlendType::REPLACE:
                FMemory::Memcpy(
                    Dst.GetData(),
                    Src.GetData(),
                    Src.Num()*Src.GetTypeSize());
                break;

            case EPMUHeightBlendType::MAX:
            default:
                for (int32 i=0; i<Src.Num(); ++i)
                    Dst[i] = FMath::Max(Src[i], Dst[i]);
                break;
        }
    }

#ifdef PMU_VOXEL_USE_OCL

    bool UploadToGPU(int32 MapId);
    bool HasGPUHeightMap(int32 MapId) const;
    boost::compute::image2d* GetGPUHeightMap(int32 MapId) const;
    boost::compute::image2d& GetGPUHeightMapChecked(int32 MapId) const;

#endif // PMU_VOXEL_USE_OCL
};

UCLASS(BlueprintType)
class PROCEDURALMESHUTILITY_API UPMUGridInstance : public UObject
{
    GENERATED_BODY()

public:

    FPMUGridData GridData;

    UPROPERTY(BlueprintReadWrite)
    TArray<class UPMUGridGenerationTask*> GenerationTasks;

    UFUNCTION(BlueprintCallable)
    FPMUGridDataRef GetGridRef()
    {
        return FPMUGridDataRef(GridData);
    }

    UFUNCTION(BlueprintCallable)
    TArray<uint8> GetPointMask() const
    {
        return GridData.PointMask;
    }

    UFUNCTION(BlueprintCallable)
    TArray<float> GetHeightMap(int32 MapId) const
    {
        return GridData.HasHeightMap(MapId)
            ? GridData.GetHeightMapChecked(MapId)
            : TArray<float>();
    }

    UFUNCTION(BlueprintCallable)
    bool HasHeightMap(int32 MapId) const
    {
        return GridData.HasHeightMap(MapId);
    }

    UFUNCTION(BlueprintCallable)
    bool HasNamedHeightMap(FName MapName) const
    {
        return GridData.HasNamedHeightMap(MapName);
    }

    UFUNCTION(BlueprintCallable)
    int32 GetNamedHeightMapId(FName MapName) const
    {
        return GridData.GetNamedHeightMapId(MapName);
    }

    UFUNCTION(BlueprintCallable)
    TArray<float> GetNamedHeightMap(FName MapName) const
    {
        return HasNamedHeightMap(MapName)
            ? GridData.GetNamedHeightMapChecked(MapName)
            : TArray<float>();
    }

    UFUNCTION(BlueprintCallable)
    void CreateNamedHeightMap(FName MapName)
    {
        if (! HasNamedHeightMap(MapName))
        {
            SetHeightMapName(MapName, GridData.CreateHeightMap());
        }
    }

    UFUNCTION(BlueprintCallable)
    void SetHeightMapName(FName MapName, int32 MapId)
    {
        GridData.NamedHeightMap.Emplace(MapName, MapId);
    }

    UFUNCTION(BlueprintCallable)
    void RemoveHeightMapName(FName MapName)
    {
        GridData.NamedHeightMap.Remove(MapName);
    }

    UFUNCTION(BlueprintCallable)
    FIntPoint GetDimension() const
    {
        return GridData.Dimension;
    }


    UFUNCTION(BlueprintCallable)
    FVector2D GetVectorDimension() const
    {
        return FVector2D(GridData.Dimension);
    }

    UFUNCTION(BlueprintCallable)
    FIntPoint GetCenter() const
    {
        return GridData.Dimension / 2;
    }

    UFUNCTION(BlueprintCallable)
    FVector2D GetVectorCenter() const
    {
        return FVector2D(GridData.Dimension) / 2.f;
    }

    UFUNCTION(BlueprintCallable)
    void SetDimension(const FIntPoint& InDimension)
    {
        GridData.SetDimension(InDimension);
    }

    UFUNCTION(BlueprintCallable)
    void SetPointMask(const TArray<uint8>& InPointMask)
    {
        if (InPointMask.Num() == GridData.CalcSize())
        {
            GridData.PointMask = InPointMask;
        }
    }

    UFUNCTION(BlueprintCallable)
    void Reset()
    {
        GridData.Reset();
    }

    UFUNCTION(BlueprintCallable)
    void Empty()
    {
        GridData.Empty();
    }

    UFUNCTION(BlueprintCallable)
    FPMUMeshSection CreateMeshSection(int32 HeightMapId = -1, bool bWinding = false);

    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
    void CreateGPUMeshSection(
        UObject* WorldContextObject,
        FPMUMeshSectionResourceRef Section,
        FPMUShaderTextureParameterInput TextureInput,
        float HeightScale = 1.f,
        float BoundsHeightMin = -1.f,
        float BoundsHeightMax =  1.f,
        bool bReverseWinding = false,
        UGWTTickEvent* CallbackEvent = nullptr
        );

    UFUNCTION(BlueprintCallable)
    TArray<int32> FilterHeightMapPoints(const TArray<FVector2D>& Points, const int32 MapId, const float Threshold = 0.5f);

    UFUNCTION(BlueprintCallable)
    void DrawPointMask(const TArray<FVector2D>& Points);

    UFUNCTION(BlueprintCallable)
    UTexture2D* CreatePointMaskTexture(bool bFilterNearest = true);

    UFUNCTION(BlueprintCallable)
    void ExecuteTasks();

    UFUNCTION(BlueprintCallable)
    bool ExecuteTasksAsync(UPARAM(ref) FGWTAsyncTaskRef& TaskRef);

    UFUNCTION(BlueprintCallable)
    bool UploadToGPU(int32 MapId)
    {
#ifdef PMU_VOXEL_USE_OCL
        return GridData.UploadToGPU(MapId);
#else
        return false;
#endif // PMU_VOXEL_USE_OCL
    }
};
