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

struct FPMUVoxelFeaturePoint
{
	FVector2D position;
	bool exists;

    FPMUVoxelFeaturePoint() = default;

    FPMUVoxelFeaturePoint(const FVector2D& inPosition)
        : position(inPosition)
    {
    }

	static FPMUVoxelFeaturePoint Average(const FPMUVoxelFeaturePoint& a, const FPMUVoxelFeaturePoint& b, const FPMUVoxelFeaturePoint& c)
    {
		
		FPMUVoxelFeaturePoint average(FVector2D::ZeroVector);
		float features = 0.f;
		if (a.exists)
        {
			average.position += a.position;
			features += 1.f;
		}
		if (b.exists)
        {
			average.position += b.position;
			features += 1.f;
		}
		if (c.exists)
        {
			average.position += c.position;
			features += 1.f;
		}
		if (features > 0.f)
        {
			average.position /= features;
			average.exists = true;
		}
		else {
			average.exists = false;
		}
		return average;
	}

	static FPMUVoxelFeaturePoint Average(const FPMUVoxelFeaturePoint& a, const FPMUVoxelFeaturePoint& b, const FPMUVoxelFeaturePoint& c, const FPMUVoxelFeaturePoint& d)
    {
		FPMUVoxelFeaturePoint average(FVector2D::ZeroVector);
		float features = 0.f;
		if (a.exists)
        {
			average.position += a.position;
			features += 1.f;
		}
		if (b.exists)
        {
			average.position += b.position;
			features += 1.f;
		}
		if (c.exists)
        {
			average.position += c.position;
			features += 1.f;
		}
		if (d.exists)
        {
			average.position += d.position;
			features += 1.f;
		}
		if (features > 0.f)
        {
			average.position /= features;
			average.exists = true;
		}
		else
        {
			average.exists = false;
		}
		return average;
	}
};
