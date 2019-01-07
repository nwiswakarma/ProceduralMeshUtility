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
#include "PMUVoxel.h"
#include "PMUVoxelGrid.h"
#include "March/PMUVoxelMap.h"
#include "March/PMUVoxelStencil.h"
#include "PMUVoxelStencilBox.generated.h"

class FPMUVoxelStencilBox : public FPMUVoxelStencil
{
protected:

    FBox2D bounds;
    FVector2D boundsCenter;
    FVector2D boundsExtents;

    virtual void FindHorizontalCrossing(FPMUVoxel& xMin, const FPMUVoxel& xMax) const
    {
        if (xMin.position.Y < GetYStart() || xMin.position.Y > GetYEnd())
        {
            return;
        }

        if (xMin.state == fillType)
        {
            if (xMin.position.X <= GetXEnd() && xMax.position.X >= GetXEnd())
            {
                if (xMin.xEdge == TNumericLimits<float>::Lowest() || xMin.xEdge < GetXEnd())
                {
                    xMin.xEdge = GetXEnd();
                    xMin.xNormal = FVector2D(fillType ? 1.f : -1.f, 0.f);
                }
                else
                {
                    ValidateHorizontalNormal(xMin, xMax);
                }
            }
        }
        else if (xMax.state == fillType)
        {
            if (xMin.position.X <= GetXStart() && xMax.position.X >= GetXStart())
            {
                if (xMin.xEdge == TNumericLimits<float>::Lowest() || xMin.xEdge > GetXStart())
                {
                    xMin.xEdge = GetXStart();
                    xMin.xNormal = FVector2D(fillType ? -1.f : 1.f, 0.f);
                }
                else
                {
                    ValidateHorizontalNormal(xMin, xMax);
                }
            }
        }
    }
    
    virtual void FindVerticalCrossing(FPMUVoxel& yMin, const FPMUVoxel& yMax) const
    {
        if (yMin.position.X < GetXStart() || yMin.position.X > GetXEnd())
        {
            return;
        }

        if (yMin.state == fillType)
        {
            if (yMin.position.Y <= GetYEnd() && yMax.position.Y >= GetYEnd())
            {
                if (yMin.yEdge == TNumericLimits<float>::Lowest() || yMin.yEdge < GetYEnd())
                {
                    yMin.yEdge = GetYEnd();
                    yMin.yNormal = FVector2D(0.f, fillType ? 1.f : -1.f);
                }
                else
                {
                    ValidateVerticalNormal(yMin, yMax);
                }
            }
        }
        else if (yMax.state == fillType)
        {
            if (yMin.position.Y <= GetYStart() && yMax.position.Y >= GetYStart())
            {
                if (yMin.yEdge == TNumericLimits<float>::Lowest() || yMin.yEdge > GetYStart())
                {
                    yMin.yEdge = GetYStart();
                    yMin.yNormal = FVector2D(0.f, fillType ? -1.f : 1.f);
                }
                else
                {
                    ValidateVerticalNormal(yMin, yMax);
                }
            }
        }
    }

public:

    FBox2D BoundsSetting;

    FPMUVoxelStencilBox() = default;

    FPMUVoxelStencilBox(const FBox2D& InBounds)
    {
        SetBounds(InBounds);
    }

    FPMUVoxelStencilBox(const FBox2D& InBounds, const int32 InFillType)
    {
        SetBounds(InBounds);
        FillTypeSetting = InFillType;
    }

    FORCEINLINE const FBox2D& GetBounds() const
    {
        return bounds;
    }

    FORCEINLINE const FVector2D& GetShift() const
    {
        return bounds.Min;
    }

    FORCEINLINE const FVector2D& GetExtents() const
    {
        return boundsExtents;
    }

    FORCEINLINE const FVector2D& GetCenter() const
    {
        return boundsCenter;
    }

    FORCEINLINE virtual float GetXStart() const
    {
        return centerX - boundsExtents.X;
    }
    
    FORCEINLINE virtual float GetXEnd() const
    {
        return centerX + boundsExtents.X;
    }
    
    FORCEINLINE virtual float GetYStart() const
    {
        return centerY - boundsExtents.Y;
    }
    
    FORCEINLINE virtual float GetYEnd() const
    {
        return centerY + boundsExtents.Y;
    }

    void SetBounds(const FBox2D& InBounds)
    {
        bounds = InBounds;
        boundsExtents = bounds.GetExtent();
        bounds.GetCenterAndExtents(boundsCenter, boundsExtents);
    }

    virtual void Initialize(const FPMUVoxelMap& VoxelMap) override
    {
        FPMUVoxelStencil::Initialize(VoxelMap);

        if (! bounds.bIsValid)
        {
            SetBounds(BoundsSetting);
        }
    }
};

UCLASS(BlueprintType)
class UPMUVoxelStencilBoxRef : public UObject
{
    GENERATED_BODY()

    FPMUVoxelStencilBox Stencil;

public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
    FBox2D Bounds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
    int32 FillType = 0;

    UFUNCTION(BlueprintCallable)
    void EditMap(UPMUVoxelMapRef* MapRef, FVector2D Center)
    {
        if (IsValid(MapRef) && MapRef->IsInitialized())
        {
            Stencil.BoundsSetting = Bounds;
            Stencil.FillTypeSetting = FillType;
            Stencil.EditMap(MapRef->GetMap(), Center);
        }
    }
};

UCLASS(BlueprintType)
class UPMUVoxelStencilMultiBoxRef : public UObject
{
    GENERATED_BODY()

    TArray<FPMUVoxelStencilBox> Stencils;

public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
    int32 FillType = 0;

    void EditMap(FPMUVoxelMap& Map, const FVector2D& Center)
    {
        // Edit map states
        for (FPMUVoxelStencilBox& Stencil : Stencils)
        {
            FVector2D BoxCenter = Center + Stencil.GetCenter();
            Stencil.EditStates(Map, BoxCenter);
        }

        // Edit map voxel crossings
        for (FPMUVoxelStencilBox& Stencil : Stencils)
        {
            FVector2D BoxCenter = Center + Stencil.GetCenter();
            Stencil.EditCrossings(Map, BoxCenter);
        }

        Map.RefreshAllChunks();
    }

    UFUNCTION(BlueprintCallable)
    void EditMap(UPMUVoxelMapRef* MapRef, FVector2D Center)
    {
        if (IsValid(MapRef) && MapRef->IsInitialized())
        {
            EditMap(MapRef->GetMap(), Center);
        }
    }

    UFUNCTION(BlueprintCallable)
    void AddBox(FBox2D Bounds)
    {
        Stencils.Emplace(Bounds, FillType);
    }

    UFUNCTION(BlueprintCallable)
    void AddBoxes(const TArray<FBox2D>& Bounds)
    {
        for (int32 i=0; i<Bounds.Num(); ++i)
        {
            Stencils.Emplace(Bounds[i], FillType);
        }
    }
};
