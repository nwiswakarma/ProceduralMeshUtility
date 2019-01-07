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

#include "DistanceField/PMUDFGeometryUtility.h"
#include "PMUDFGeometryLine.h"

void UPMUDFGeometryUtility::CreateLine(FPMUDFGeometryDataRef& GeometryRef, int32 GroupId, const FVector2D& P0, const FVector2D& P1, float Radius, bool bReversed)
{
    if (FPMUDFGeometryGroup* GeometryGroup = GeometryRef.GetGroup(GroupId))
    {
        GeometryGroup->Emplace(new FPMUDFGeometryLine(P0, P1, Radius, bReversed));
    }
}

void UPMUDFGeometryUtility::CreateLines(FPMUDFGeometryDataRef& GeometryRef, int32 GroupId, const TArray<FVector2D>& Lines, float Radius, bool bReversed)
{
    if (Lines.Num() < 2)
    {
        return;
    }

    if (FPMUDFGeometryGroup* GeometryGroup = GeometryRef.GetGroup(GroupId))
    {
        if (bReversed)
        {
            for (int32 i=(Lines.Num()-1); i>=1; ++i)
            {
                const FVector2D& P0(Lines[i]);
                const FVector2D& P1(Lines[i-1]);
                GeometryGroup->Emplace(new FPMUDFGeometryLine(P0, P1, Radius, false));
            }
        }
        else
        {
            for (int32 i=1; i<Lines.Num(); ++i)
            {
                const FVector2D& P0(Lines[i-1]);
                const FVector2D& P1(Lines[i]);
                GeometryGroup->Emplace(new FPMUDFGeometryLine(P0, P1, Radius, false));
            }
        }
    }
}

void UPMUDFGeometryUtility::FilterByPoint(FPMUDFGeometryDataRef& GeometryRef, int32 GroupId, const FVector2D& Point, float Radius)
{
    if (FPMUDFGeometryGroup* GeometryGroup = GeometryRef.GetGroup(GroupId))
    {
        const float ClampedRadius = FMath::Max(0.f, Radius);
        const float RadiusSq = ClampedRadius * ClampedRadius;

        // Remove all geometry that does not have intersecting bound radius
        GeometryGroup->RemoveAllSwap(
            [&](const FPSPMUDFGeometry& Geometry)
            {
                if (Geometry.IsValid())
                {
                    const float DistSq = Geometry->GetBoundingCenterDistanceSquared(Point);
                    return (DistSq-RadiusSq) > Geometry->GetBoundingRadiusSquared();
                }
                return false;
            }, true);
    }
}

void UPMUDFGeometryUtility::FilterByPoints(FPMUDFGeometryDataRef& GeometryRef, int32 GroupId, const TArray<FVector2D>& Points, float Radius)
{
    if (Points.Num() <= 0)
    {
        return;
    }

    if (FPMUDFGeometryGroup* GeometryGroup = GeometryRef.GetGroup(GroupId))
    {
        const float ClampedRadius = FMath::Max(0.f, Radius);
        const float RadiusSq = ClampedRadius * ClampedRadius;

        // Remove all geometry that does not have intersecting bound radius
        GeometryGroup->RemoveAllSwap(
            [&](const FPSPMUDFGeometry& Geometry)
            {
                bool bIsWithinRadius = false;

                if (Geometry.IsValid())
                {
                    for (const FVector2D& Point : Points)
                    {
                        const float DistSq = Geometry->GetBoundingCenterDistanceSquared(Point);
                        if ((DistSq-RadiusSq) <= Geometry->GetBoundingRadiusSquared())
                        {
                            bIsWithinRadius = true;
                            break;
                        }
                    }
                }

                return !bIsWithinRadius;
            }, true);
    }
}
