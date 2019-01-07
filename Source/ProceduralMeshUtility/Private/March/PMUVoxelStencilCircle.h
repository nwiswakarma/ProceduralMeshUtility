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
#include "PMUVoxelStencilSquare.h"
#include "PMUVoxelStencilCircle.generated.h"

class FPMUVoxelStencilCircle : public FPMUVoxelStencilSquare
{
private:

	float sqrRadius;

	FVector2D ComputeNormal(float x, float y, const FPMUVoxel& other) const
    {
		if (fillType > other.state)
        {
			return FVector2D(x - centerX, y - centerY).GetSafeNormal();
		}
		else
        {
			return FVector2D(centerX - x, centerY - y).GetSafeNormal();
		}
	}

protected:

    virtual void FindHorizontalCrossing(FPMUVoxel& xMin, const FPMUVoxel& xMax) const override
    {
		float y2 = xMin.position.Y - centerY;
		y2 *= y2;

		if (xMin.state == fillType)
        {
			float x = xMin.position.X - centerX;
			if (x * x + y2 <= sqrRadius)
            {
				x = centerX + FMath::Sqrt(sqrRadius - y2);
				if (xMin.xEdge == TNumericLimits<float>::Lowest() || xMin.xEdge < x)
                {
					xMin.xEdge = x;
					xMin.xNormal = ComputeNormal(x, xMin.position.Y, xMax);
				}
                else
                {
					ValidateHorizontalNormal(xMin, xMax);
                }
			}
		}
		else if (xMax.state == fillType)
        {
			float x = xMax.position.X - centerX;
			if (x * x + y2 <= sqrRadius)
            {
				x = centerX - FMath::Sqrt(sqrRadius - y2);
				if (xMin.xEdge == TNumericLimits<float>::Lowest() || xMin.xEdge > x)
                {
					xMin.xEdge = x;
					xMin.xNormal = ComputeNormal(x, xMin.position.Y, xMin);
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
		float x2 = yMin.position.X - centerX;
		x2 *= x2;

		if (yMin.state == fillType)
        {
			float y = yMin.position.Y - centerY;
			if (y * y + x2 <= sqrRadius)
            {
				y = centerY + FMath::Sqrt(sqrRadius - x2);
				if (yMin.yEdge == TNumericLimits<float>::Lowest() || yMin.yEdge < y)
                {
					yMin.yEdge = y;
					yMin.yNormal = ComputeNormal(yMin.position.X, y, yMax);
				}
                else
                {
					ValidateVerticalNormal(yMin, yMax);
                }
			}
		}
		else if (yMax.state == fillType)
        {
			float y = yMax.position.Y - centerY;
			if (y * y + x2 <= sqrRadius)
            {
				y = centerY - FMath::Sqrt(sqrRadius - x2);
				if (yMin.yEdge == TNumericLimits<float>::Lowest() || yMin.yEdge > y)
                {
					yMin.yEdge = y;
					yMin.yNormal = ComputeNormal(yMin.position.X, y, yMin);
				}
                else
                {
					ValidateVerticalNormal(yMin, yMax);
                }
			}
		}
	}

public:
	
    virtual void Initialize(const FPMUVoxelMap& VoxelMap) override
    {
        FPMUVoxelStencilSquare::Initialize(VoxelMap);
		sqrRadius = radius * radius;
	}
	
    FORCEINLINE virtual void ApplyVoxel(FPMUVoxel& voxel) const override
    {
		float x = voxel.position.X - centerX;
		float y = voxel.position.Y - centerY;

		if (x * x + y * y <= sqrRadius)
        {
			voxel.state = fillType;
		}
	}
};

UCLASS(BlueprintType)
class UPMUVoxelStencilCircleRef : public UObject
{
    GENERATED_BODY()

    FPMUVoxelStencilCircle Stencil;

public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
    float Radius = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
    int32 FillType = 0;

    UFUNCTION(BlueprintCallable)
    void EditMap(UPMUVoxelMapRef* MapRef, FVector2D Center)
    {
        if (IsValid(MapRef) && MapRef->IsInitialized())
        {
            Stencil.RadiusSetting = Radius;
            Stencil.FillTypeSetting = FillType;
            Stencil.EditMap(MapRef->GetMap(), Center);
        }
    }

    UFUNCTION(BlueprintCallable)
    TArray<int32> GetChunkIndices(UPMUVoxelMapRef* MapRef, FVector2D Center)
    {
        TArray<int32> ChunkIndices;

        if (IsValid(MapRef) && MapRef->IsInitialized())
        {
            Stencil.RadiusSetting = Radius;
            Stencil.FillTypeSetting = FillType;
            Stencil.GetChunkIndices(MapRef->GetMap(), Center, ChunkIndices);
        }

        return ChunkIndices;
    }
};
