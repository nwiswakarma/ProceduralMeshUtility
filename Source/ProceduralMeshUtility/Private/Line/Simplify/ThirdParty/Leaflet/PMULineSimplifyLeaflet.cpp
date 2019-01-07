// BSD 2-Clause License
// 
// Copyright (c) 2017, SINTEF-9012
// Copyright (c) 2018, Nuraga Wiswakarma
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "PMULineSimplifyLeaflet.h"

float getSqSegDist(const FVector2D& p, const FVector2D& p1, const FVector2D& p2)
{
	float x = p1.X;
	float y = p1.Y;
	float dx = p2.X - x;
	float dy = p2.Y - y;

	if (dx != 0.0f || dy != 0.0f)
    {
		float t = ((p.X - x) * dx + (p.Y - y) * dy) / (dx * dx + dy * dy);

		if (t > 1.0f)
        {
			x = p2.X;
			y = p2.Y;

		}
		else if (t > 0.0f)
        {
			x += dx * t;
			y += dy * t;
		}
	}

	dx = p.X - x;
	dy = p.Y - y;

	return dx * dx + dy * dy;
}

void simplifyRadialDist(TArray<FVector2D>& newPoints, const TArray<FVector2D>& points, float sqTolerance)
{
	FVector2D prevPoint = points[0];
	newPoints.Emplace(prevPoint);
	FVector2D point;

	for (int32 i=1; i<points.Num(); ++i)
    {
		point = points[i];
		if (FVector2D::DistSquared(point, prevPoint) > sqTolerance)
        {
			newPoints.Emplace(point);
			prevPoint = point;
		}
	}

	if (! prevPoint.Equals(point))
    {
        newPoints.Emplace(point);
    }
}

void simplifyDPStep(const TArray<FVector2D>& points, int32 first, int32 last, float sqTolerance, TArray<FVector2D>& simplified)
{
	float maxSqDist = sqTolerance;
	int32 index = -1;

	for (int32 i=first+1; i<last; ++i)
    {
		float sqDist = getSqSegDist(points[i], points[first], points[last]);

		if (sqDist > maxSqDist)
        {
			index = i;
			maxSqDist = sqDist;
		}
	}

	if (maxSqDist > sqTolerance)
    {
		if ((index - first) > 1) simplifyDPStep(points, first, index, sqTolerance, simplified);
		simplified.Emplace(points[index]);
		if ((last - index) > 1) simplifyDPStep(points, index, last, sqTolerance, simplified);
	}
}

void simplifyDouglasPeucker(TArray<FVector2D>& simplified, const TArray<FVector2D>& points, float sqTolerance)
{
	const int32 last = points.Num() - 1;

	simplified.Emplace(points[0]);
	simplifyDPStep(points, 0, last, sqTolerance, simplified);
	simplified.Emplace(points[last]);
}

void UPMULineSimplifyLeaflet::Simplify(TArray<FVector2D>& OutPoints, const TArray<FVector2D>& InPoints, float Tolerance /*= 1.0f*/, bool bHighestQuality/* = false*/)
{
    // Not enough point count to simplify, assign input and return
	if (InPoints.Num() < 3)
    {
        OutPoints = InPoints;
        return;
    }

	float sqTolerance = Tolerance * Tolerance;

    OutPoints.Empty(InPoints.Num());

    // Simplify with radial distance & Douglas-Peucker simplification 
	if (bHighestQuality)
    {
        TArray<FVector2D> TempPoints;
        TempPoints.Reserve(InPoints.Num());

		simplifyRadialDist(TempPoints, InPoints, sqTolerance);
		simplifyDouglasPeucker(OutPoints, TempPoints, sqTolerance);
	}
    // Simplify with Douglas-Peucker simplification 
	else
    {
		simplifyDouglasPeucker(OutPoints, InPoints, sqTolerance);
	}

    OutPoints.Shrink();
}
