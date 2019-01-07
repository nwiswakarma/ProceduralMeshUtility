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
#include "PMUUtilityLibrary.h"
#include "PMUDFGeometry.generated.h"

typedef TSharedPtr<class FPMUDFGeometry> FPSPMUDFGeometry;
typedef TArray<FPSPMUDFGeometry>         FPMUDFGeometryGroup;

class FPMUDFGeometry
{
public:

    virtual bool IsValid() const = 0;
    virtual FPSPMUDFGeometry Copy() const = 0;

    virtual bool Intersect(const FBox2D& Box) const = 0;
    virtual bool IsWithinBoundingRadius(const FVector2D& Point) const = 0;

    virtual float GetBoundingRadius() const = 0;
    virtual float GetBoundingRadiusSquared() const = 0;
    virtual float GetBoundingCenterDistance(const FVector2D& Point) const = 0;
    virtual float GetBoundingCenterDistanceSquared(const FVector2D& Point) const = 0;
    virtual FBox2D GetBoundingBox() const = 0;

    virtual float GetRadius() const = 0;
    virtual float GetRadiusSquared() const = 0;
    virtual float GetDistance(const FVector2D& Point) const = 0;
    virtual float GetDistanceSquared(const FVector2D& Point) const = 0;
    virtual float GetClampedDistanceRatio(const FVector2D& Point) const = 0;

    virtual float GetSign(const FVector2D& Point) const = 0;
    virtual bool IsReversed(const FVector2D& Point) const = 0;
};

class FPMUDFGeometryData
{
    friend class FPMUDFGeometryPartition;

    TArray<FPMUDFGeometryGroup> GeometryGroups;
    TMap<FName, int32> GeometryGroupNameMap;

public:

    void Empty()
    {
        GeometryGroups.Empty();
        GeometryGroupNameMap.Empty();
    }

    bool HasGeometry() const
    {
        bool bHasGeometry = false;

        for (const FPMUDFGeometryGroup& GeometryGroup : GeometryGroups)
        for (const FPSPMUDFGeometry& Geometry : GeometryGroup)
        {
            if (Geometry.IsValid())
            {
                bHasGeometry = true;
                break;
            }
        }

        return bHasGeometry;
    }

    FORCEINLINE int32 GetGroupCount() const
    {
        return GeometryGroups.Num();
    }

    FORCEINLINE bool HasGroup(int32 GroupId) const
    {
        return GeometryGroups.IsValidIndex(GroupId);
    }

    FORCEINLINE bool HasNamedGroup(FName GroupName) const
    {
        return ! GroupName.IsNone() && GeometryGroupNameMap.Contains(GroupName);
    }

    FORCEINLINE int32 GetGroupIdByName(FName GroupName) const
    {
        return HasNamedGroup(GroupName)
            ? GeometryGroupNameMap.FindChecked(GroupName)
            : -1;
    }

    FORCEINLINE FPMUDFGeometryGroup* GetGroup(int32 GroupId)
    {
        return HasGroup(GroupId) ? &GeometryGroups[GroupId] : nullptr;
    }

    FORCEINLINE const FPMUDFGeometryGroup* GetGroup(int32 GroupId) const
    {
        return HasGroup(GroupId) ? &GeometryGroups[GroupId] : nullptr;
    }

    FORCEINLINE FPMUDFGeometryGroup& GetGroupChecked(int32 GroupId)
    {
        return GeometryGroups[GroupId];
    }

    FORCEINLINE const FPMUDFGeometryGroup& GetGroupChecked(int32 GroupId) const
    {
        return GeometryGroups[GroupId];
    }

    FORCEINLINE void SetGroupNum(int32 GroupNum)
    {
        GeometryGroups.SetNum(GroupNum, false);
    }

    int32 CreateNewGroup(FName GroupName)
    {
        int32 GroupId = GeometryGroups.Num();
        CreateGroup(GroupId, GroupName);

        return GroupId;
    }

    int32 CreateGroup(int32 GroupId, FName GroupName)
    {
        if (! GeometryGroups.IsValidIndex(GroupId))
        {
            GeometryGroups.SetNum(GroupId+1, false);

            if (! GroupName.IsNone())
            {
                GeometryGroupNameMap.Emplace(GroupName, GroupId);
            }

            return GroupId;
        }

        return -1;
    }

    void CopyTo(FPMUDFGeometryData& DstData, const FBox2D& Rect) const
    {
        DstData.Empty();
        DstData.SetGroupNum(GeometryGroups.Num());

        for (int32 gi=0; gi<GeometryGroups.Num(); ++gi)
        {
            const FPMUDFGeometryGroup& SrcGeometryGroup(GeometryGroups[gi]);
            FPMUDFGeometryGroup& DstGeometryGroup(DstData.GeometryGroups[gi]);

            DstGeometryGroup.Reserve(SrcGeometryGroup.Num());

            for (const FPSPMUDFGeometry& Geometry : SrcGeometryGroup)
            {
                if (Geometry.IsValid() && Geometry->Intersect(Rect))
                {
                    FPSPMUDFGeometry GeometryCopy(Geometry->Copy());

                    if (GeometryCopy.IsValid())
                    {
                        DstGeometryGroup.Emplace(MoveTemp(GeometryCopy));
                    }
                }
            }

            DstGeometryGroup.Shrink();
        }
    }

    void CopyGroupTo(const int32 SrcGroupId, FPMUDFGeometryGroup& DstGeometryGroup, FBox2D FilterRect) const
    {
        if (HasGroup(SrcGroupId))
        {
            const FPMUDFGeometryGroup& SrcGeometryGroup(GeometryGroups[SrcGroupId]);
            const bool bUseFilterRect = FilterRect.bIsValid;

            DstGeometryGroup.Reserve(SrcGeometryGroup.Num());

            for (const FPSPMUDFGeometry& Geometry : SrcGeometryGroup)
            {
                if (Geometry.IsValid() && (! bUseFilterRect || Geometry->Intersect(FilterRect)))
                {
                    FPSPMUDFGeometry GeometryCopy(Geometry->Copy());

                    if (GeometryCopy.IsValid())
                    {
                        DstGeometryGroup.Emplace(MoveTemp(GeometryCopy));
                    }
                }
            }

            DstGeometryGroup.Shrink();
        }
    }
};

class FPMUDFGeometryPartition
{
    FPMUDFGeometryPartition* Partitions[4];
    TArray<const FPMUDFGeometry*> Data;

    FBox2D BoundingBox;
    FVector2D BoundsExtents;
    float BoundsSizeHalf    = 0.f;
    float BoundsSizeHalfInv = 0.f;
    int32 Depth = 0;

public:

    FPMUDFGeometryPartition()
    {
        for (int32 i=0; i<4; ++i)
        {
            Partitions[i] = nullptr;
        }
    }

    ~FPMUDFGeometryPartition()
    {
        Empty();
    }

    void CopyFrom(const FPMUDFGeometryPartition& Partition)
    {
        BoundingBox   = Partition.BoundingBox;
        BoundsExtents = Partition.BoundsExtents;
        BoundsSizeHalf    = Partition.BoundsSizeHalf;
        BoundsSizeHalfInv = Partition.BoundsSizeHalfInv;
        Depth = Partition.Depth;
        Data  = Partition.Data;

        for (int32 i=0; i<4; ++i)
        {
            if (Partition.Partitions[i])
            {
                Partitions[i] = new FPMUDFGeometryPartition;
                Partitions[i]->CopyFrom(*Partition.Partitions[i]);
            }
            else
            {
                Partitions[i] = nullptr;
            }
        }
    }

    void Empty()
    {
        for (int32 i=0; i<4; ++i)
        {
            if (Partitions[i])
            {
                delete Partitions[i];
                Partitions[i] = nullptr;
            }
        }

        Data.Empty();
    }

    void CheckPartitionGeometry() const
    {
        if (Depth > 0)
        {
            for (int32 i=0; i<4; ++i)
            {
                if (Partitions[i])
                {
                    Partitions[i]->CheckPartitionGeometry();
                }
            }
        }
        else
        {
            UE_LOG(LogTemp,Warning, TEXT("Partition (HasGeometry: %d) (BoundingBox: %s) (BoundsSize: %s) (Geometry Count: %d)"),
                Data.Num() > 0,
                *BoundingBox.ToString(),
                *FVector2D(BoundingBox.Max-BoundingBox.Min).ToString(),
                Data.Num());
        }
    }

    FORCEINLINE const FVector2D& GetBoundsOffset() const
    {
        return BoundingBox.Min;
    }

    FORCEINLINE int32 GetPartitionId(const FVector2D& Point) const
    {
        FVector2D PtId(Point-GetBoundsOffset());
        const int32 IX = FMath::Clamp(static_cast<int32>(PtId.X*BoundsSizeHalfInv), 0, 1);
        const int32 IY = FMath::Clamp(static_cast<int32>(PtId.Y*BoundsSizeHalfInv), 0, 1);
        return IX + IY*2;
    }

    FORCEINLINE const FPMUDFGeometryPartition* GetPartition(const FVector2D& Point) const
    {
        const FPMUDFGeometryPartition* Partition = this;

        for (int32 d=Depth; d>=1; --d)
        {
            Partition = Partition->Partitions[Partition->GetPartitionId(Point)];
            if (! Partition) break;
        }

        return Partition;
    }

    FORCEINLINE const TArray<const FPMUDFGeometry*>& GetGeometry() const
    {
        return Data;
    }

    void Initialize(const FVector2D& Offset, const float BoundsSize, const int32 InDepth)
    {
        Depth = InDepth;
        BoundsSizeHalf    = BoundsSize / 2.f;;
        BoundsSizeHalfInv = 1.f / BoundsSizeHalf;
        BoundingBox  = FBox2D(Offset, Offset+FVector2D(BoundsSize, BoundsSize));
        BoundsExtents.Set(BoundsSizeHalf, BoundsSizeHalf);
    }

    bool Construct(const FPMUDFGeometryGroup& GeometryGroup)
    {
        check(BoundingBox.bIsValid);

        bool bGeometryAdded = false;

        for (const FPSPMUDFGeometry& Geometry : GeometryGroup)
        {
            if (Geometry.IsValid() && Geometry->Intersect(BoundingBox))
            {
                bGeometryAdded |= AddGeometry(*Geometry);
            }
        }

        return bGeometryAdded;
    }

    bool AddGeometry(const FPMUDFGeometry& Geometry)
    {
        bool bGeometryAdded = false;

        if (Depth > 0)
        {
            const FVector2D& Extent(BoundsExtents);

            // Add geometry to child partition
            for (int32 y=0, i=0; y<2; ++y)
            for (int32 x=0;      x<2; ++x, ++i)
            {
                const float px = x*BoundsSizeHalf;
                const float py = y*BoundsSizeHalf;

                FVector2D Offset(px, py);
                FBox2D PartitionBounds;
                PartitionBounds     = BoundingBox.ShiftBy(Offset);
                PartitionBounds.Max = PartitionBounds.Min + Extent;

                if (Geometry.Intersect(PartitionBounds))
                {
                    bGeometryAdded |= FindOrAdd(i, PartitionBounds.Min).AddGeometry(Geometry);
                }
            }
        }
        else
        {
            Data.Emplace(&Geometry);
            bGeometryAdded = true;
        }

        return bGeometryAdded;
    }

    FPMUDFGeometryPartition& FindOrAdd(const int32 PartitionId, const FVector2D& Offset)
    {
        if (! Partitions[PartitionId])
        {
            Partitions[PartitionId] = new FPMUDFGeometryPartition;
            Partitions[PartitionId]->Initialize(Offset, BoundsSizeHalf, Depth-1);
        }

        return *Partitions[PartitionId];
    }
};

USTRUCT(BlueprintType)
struct FPMUDFGeometryDataRef
{
    GENERATED_BODY()

    FPMUDFGeometryData* Data = nullptr;

    FPMUDFGeometryDataRef() = default;

    FPMUDFGeometryDataRef(FPMUDFGeometryData& InData)
        : Data(&InData)
    {
    }

    FORCEINLINE bool IsValid() const
    {
        return Data != nullptr;
    }

    FORCEINLINE FPMUDFGeometryGroup* GetGroup(int32 GroupId) const
    {
        return IsValid() ? Data->GetGroup(GroupId) : nullptr;
    }
};

UCLASS(BlueprintType)
class PROCEDURALMESHUTILITY_API UPMUDFGeometryDataObject : public UObject
{
    GENERATED_BODY()

public:

    FPMUDFGeometryData GeometryData;

    UFUNCTION(BlueprintCallable)
    FPMUDFGeometryDataRef GetDataRef()
    {
        return FPMUDFGeometryDataRef(GeometryData);
    }

    UFUNCTION(BlueprintCallable)
    void Empty()
    {
        GeometryData.Empty();
    }

    UFUNCTION(BlueprintCallable)
    bool HasGroup(int32 GroupId) const
    {
        return GeometryData.HasGroup(GroupId);
    }

    UFUNCTION(BlueprintCallable)
    bool HasNamedGroup(FName GroupName) const
    {
        return GeometryData.HasNamedGroup(GroupName);
    }

    UFUNCTION(BlueprintCallable)
    int32 GetGroupIdByName(FName GroupName) const
    {
        return GeometryData.GetGroupIdByName(GroupName);
    }

    UFUNCTION(BlueprintCallable)
    int32 CreateNewGroup(FName GroupName)
    {
        return GeometryData.CreateNewGroup(GroupName);
    }

    UFUNCTION(BlueprintCallable)
    int32 CreateGroup(int32 GroupId, FName GroupName)
    {
        return GeometryData.CreateGroup(GroupId, GroupName);
    }
};
