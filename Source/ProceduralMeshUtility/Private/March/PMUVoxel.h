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

enum class EPMUVoxelDT : uint8
{
	X,
	Y,
	XY
};

struct FPMUVoxel
{
    int32 state = 0;

    float xEdge;
    float yEdge;

    FVector2D position;
    FVector2D xNormal;
    FVector2D yNormal;

    FPMUVoxel() = default;

    FPMUVoxel(int32 x, int32 y, float size)
    {
        Set(x, y, size);
    }

    FORCEINLINE bool IsFilled() const
    {
        return state > 0;
    }

    FORCEINLINE FVector2D GetXEdgePoint() const
    {
        return FVector2D(xEdge, position.Y);
    }
    
    FORCEINLINE FVector2D GetYEdgePoint() const
    {
        return FVector2D(position.X, yEdge);
    }

    FORCEINLINE void Reset()
    {
        state = 0;
        xEdge = TNumericLimits<float>::Lowest();
        yEdge = TNumericLimits<float>::Lowest();
    }

    FORCEINLINE void Set(int32 x, int32 y, float size)
    {
        position.X = (x + 0.5f) * size;
        position.Y = (y + 0.5f) * size;

        xEdge = TNumericLimits<float>::Lowest();
        yEdge = TNumericLimits<float>::Lowest();

        state = 0;
    }

    FORCEINLINE void BecomeXDummyOf(const FPMUVoxel& voxel, float offset)
    {
        state = voxel.state;
        position = voxel.position;
        position.X += offset;
        xEdge = voxel.xEdge + offset;
        yEdge = voxel.yEdge;
        yNormal = voxel.yNormal;
    }
    
    FORCEINLINE void BecomeYDummyOf(const FPMUVoxel& voxel, float offset)
    {
        state = voxel.state;
        position = voxel.position;
        position.Y += offset;
        xEdge = voxel.xEdge;
        yEdge = voxel.yEdge + offset;
        xNormal = voxel.xNormal;
    }

    FORCEINLINE void BecomeXYDummyOf(const FPMUVoxel& voxel, float offset)
    {
        state = voxel.state;
        position = voxel.position;
        position.X += offset;
        position.Y += offset;
        xEdge = voxel.xEdge + offset;
        yEdge = voxel.yEdge + offset;
    }
};
