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
#include "March/PMUVoxelStencil.h"
#include "PMUVoxelStencilTri.generated.h"

class FPMUVoxelStencilTri : public FPMUVoxelStencil
{
private:

    FVector Offsets[3];
    FVector Pos[3];
    FVector2D Nrm[3];

    FVector2D Shift;
    FBox2D Bounds;

    bool FindIntersection(const FVector& SegmentStart, const FVector& SegmentEnd, FVector& Intersection, FVector2D& Normal) const
    {
        if (FMath::SegmentIntersection2D(SegmentStart, SegmentEnd, Pos[0], Pos[1], Intersection))
        {
            Normal = Nrm[0];
            return true;
        }

        if (FMath::SegmentIntersection2D(SegmentStart, SegmentEnd, Pos[1], Pos[2], Intersection))
        {
            Normal = Nrm[1];
            return true;
        }

        if (FMath::SegmentIntersection2D(SegmentStart, SegmentEnd, Pos[2], Pos[0], Intersection))
        {
            Normal = Nrm[2];
            return true;
        }

        return false;
    }

	FORCEINLINE FVector2D ComputeNormal(const FVector2D& Normal, const FPMUVoxel& other) const
    {
        return (fillType > other.state) ? -Normal : Normal;
	}

protected:

    virtual void FindHorizontalCrossing(FPMUVoxel& xMin, const FPMUVoxel& xMax) const override
    {
        FVector Segment0(xMin.position, 0.f);
        FVector Segment1(xMax.position.X, Segment0.Y, 0.f);

        FVector Intersection;
        FVector2D Normal;

        if (FindIntersection(Segment0, Segment1, Intersection, Normal))
        {
            if (xMin.state == fillType)
            {
                float x = Intersection.X;
                if (xMin.xEdge == TNumericLimits<float>::Lowest() || xMin.xEdge < x)
                {
                    xMin.xEdge = x;
                    xMin.xNormal = ComputeNormal(Normal, xMax); //fillType ? Normal : -Normal;
                }
                else
                {
					ValidateHorizontalNormal(xMin, xMax);
                }
            }
            else if (xMax.state == fillType)
            {
                float x = Intersection.X;
                if (xMin.xEdge == TNumericLimits<float>::Lowest() || xMin.xEdge > x)
                {
                    xMin.xEdge = x;
                    xMin.xNormal = ComputeNormal(Normal, xMin); //fillType ? Normal : -Normal;
                }
                else
                {
					ValidateHorizontalNormal(xMin, xMax);
                }
            }
        }
    }

    virtual void FindVerticalCrossing(FPMUVoxel& yMin, const FPMUVoxel& yMax) const override
    {
        FVector Segment0(yMin.position, 0.f);
        FVector Segment1(Segment0.X, yMax.position.Y, 0.f);

        FVector Intersection;
        FVector2D Normal;

        if (FindIntersection(Segment0, Segment1, Intersection, Normal))
        {
            if (yMin.state == fillType)
            {
                float y = Intersection.Y;
                if (yMin.yEdge == TNumericLimits<float>::Lowest() || yMin.yEdge < y)
                {
                    yMin.yEdge = y;
                    yMin.yNormal = ComputeNormal(Normal, yMax); //fillType ? Normal : -Normal;
                }
                else
                {
					ValidateVerticalNormal(yMin, yMax);
                }
            }
            else if (yMax.state == fillType)
            {
                float y = Intersection.Y;
                if (yMin.yEdge == TNumericLimits<float>::Lowest() || yMin.yEdge > y)
                {
                    yMin.yEdge = y;
                    yMin.yNormal = ComputeNormal(Normal, yMin); //fillType ? Normal : -Normal;
                }
                else
                {
					ValidateVerticalNormal(yMin, yMax);
                }
            }
        }
    }

public:

    FPMUVoxelStencilTri() = default;

    FPMUVoxelStencilTri(const FVector& Pos0, const FVector& Pos1, const FVector& Pos2)
    {
        SetPositions(Pos0, Pos1, Pos2);
    }

    FPMUVoxelStencilTri(const FVector& Pos0, const FVector& Pos1, const FVector& Pos2, const int32 InFillType)
    {
        SetPositions(Pos0, Pos1, Pos2);
        FillTypeSetting = InFillType;
    }

    FORCEINLINE const FBox2D& GetBounds() const
    {
        return Bounds;
    }

    FORCEINLINE const FVector2D& GetShift() const
    {
        return Shift;
    }

    FORCEINLINE FVector2D GetShiftedBoundsCenter() const
    {
        return Shift + Bounds.GetCenter();
    }

    FORCEINLINE virtual float GetXStart() const
    {
        return centerX - Bounds.GetExtent().X;
    }
    
    FORCEINLINE virtual float GetXEnd() const
    {
        return centerX + Bounds.GetExtent().X;
    }
    
    FORCEINLINE virtual float GetYStart() const
    {
        return centerY - Bounds.GetExtent().Y;
    }
    
    FORCEINLINE virtual float GetYEnd() const
    {
        return centerY + Bounds.GetExtent().Y;
    }

    virtual void SetCenter(float x, float y) override
    {
        FPMUVoxelStencil::SetCenter(x, y);

        // Set triangle vertices

        const float x0 = GetXStart();
        const float y0 = GetYStart();
        const float x1 = GetXEnd();
        const float y1 = GetYEnd();

        for (int32 i=0; i<3; ++i)
        {
            const FVector& Offset(Offsets[i]);
            FVector& PosRef(Pos[i]);
            PosRef.X = FMath::Clamp(x0+Offset.X, x0, x1);
            PosRef.Y = FMath::Clamp(y0+Offset.Y, y0, y1);
            PosRef.Z = 0.f;
        }
    }

    FORCEINLINE virtual void ApplyVoxel(FPMUVoxel& voxel) const override
    {
        if (UPMUUtilityLibrary::IsPointOnTri(FVector(voxel.position, 0.f), Pos[0], Pos[1], Pos[2]))
        {
            voxel.state = fillType;
        }
    }

    void SetPositions(const FVector& Pos0, const FVector& Pos1, const FVector& Pos2)
    {
        // Calculate tri bounds

        Bounds = FBox2D(ForceInit);
        Bounds += FVector2D(Pos0);
        Bounds += FVector2D(Pos1);
        Bounds += FVector2D(Pos2);

        Shift = Bounds.Min;
        Bounds = Bounds.ShiftBy(-Shift);

        // Calculate local vertices

        Offsets[0] = Pos0 - FVector(Shift, 0.f);
        Offsets[1] = Pos1 - FVector(Shift, 0.f);
        Offsets[2] = Pos2 - FVector(Shift, 0.f);

        // Calculate tri normals

        FVector Nrm01 = (Pos0 - Pos1).GetUnsafeNormal();
        FVector Nrm12 = (Pos1 - Pos2).GetUnsafeNormal();
        FVector Nrm20 = (Pos2 - Pos0).GetUnsafeNormal();

        Nrm[0].Set(-Nrm01.Y, Nrm01.X);
        Nrm[1].Set(-Nrm12.Y, Nrm12.X);
        Nrm[2].Set(-Nrm20.Y, Nrm20.X);
    }
};

UCLASS(BlueprintType)
class UPMUVoxelStencilTriRef : public UObject
{
    GENERATED_BODY()

    FPMUVoxelStencilTri Stencil;

public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
    int32 FillType = 0;

    UFUNCTION(BlueprintCallable)
    FVector2D GetShift() const
    {
        return Stencil.GetShift();
    }

    UFUNCTION(BlueprintCallable)
    FVector2D GetBoundsCenter() const
    {
        return Stencil.GetBounds().GetCenter();
    }

    UFUNCTION(BlueprintCallable)
    FVector2D GetShiftedBoundsCenter() const
    {
        return Stencil.GetShiftedBoundsCenter();
    }

    UFUNCTION(BlueprintCallable)
    void EditMap(UPMUVoxelMapRef* MapRef, FVector2D Center)
    {
        if (IsValid(MapRef) && MapRef->IsInitialized())
        {
            Stencil.FillTypeSetting = FillType;
            Stencil.EditMap(MapRef->GetMap(), Center);
        }
    }

    UFUNCTION(BlueprintCallable)
    void EditStates(UPMUVoxelMapRef* MapRef, FVector2D Center)
    {
        if (IsValid(MapRef) && MapRef->IsInitialized())
        {
            Stencil.FillTypeSetting = FillType;
            Stencil.EditStates(MapRef->GetMap(), Center);
        }
    }

    UFUNCTION(BlueprintCallable)
    void EditCrossings(UPMUVoxelMapRef* MapRef, FVector2D Center)
    {
        if (IsValid(MapRef) && MapRef->IsInitialized())
        {
            Stencil.FillTypeSetting = FillType;
            Stencil.EditCrossings(MapRef->GetMap(), Center);
        }
    }

    UFUNCTION(BlueprintCallable)
    void SetPositions(FVector Pos0, FVector Pos1, FVector Pos2, bool bInverse = false)
    {
        if (bInverse)
        {
            Stencil.SetPositions(Pos2, Pos1, Pos0);
        }
        else
        {
            Stencil.SetPositions(Pos0, Pos1, Pos2);
        }
    }
};
