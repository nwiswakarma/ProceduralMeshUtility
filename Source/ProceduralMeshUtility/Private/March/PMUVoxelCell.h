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
#include "PMUVoxelFeaturePoint.h"

class FPMUVoxelCell
{
public:

    FPMUVoxel a;
    FPMUVoxel b;
    FPMUVoxel c;
    FPMUVoxel d;
    
    int32 i;
    
    float sharpFeatureLimit;
    float parallelLimit;

    FORCEINLINE FVector2D GetAverageNESW() const
    {
        return (a.GetXEdgePoint() + a.GetYEdgePoint() +
                b.GetYEdgePoint() + c.GetXEdgePoint()) * 0.25f;
    }

    FORCEINLINE FPMUVoxelFeaturePoint GetFeatureSW() const
    {
        return GetSharpFeature(a.GetXEdgePoint(), a.xNormal, a.GetYEdgePoint(), a.yNormal);
    }
    
    FORCEINLINE FPMUVoxelFeaturePoint GetFeatureSE() const
    {
        return GetSharpFeature(a.GetXEdgePoint(), a.xNormal, b.GetYEdgePoint(), b.yNormal);
    }
    
    FORCEINLINE FPMUVoxelFeaturePoint GetFeatureNW() const
    {
        return GetSharpFeature(a.GetYEdgePoint(), a.yNormal, c.GetXEdgePoint(), c.xNormal);
    }
    
    FORCEINLINE FPMUVoxelFeaturePoint GetFeatureNE() const
    {
        return GetSharpFeature(c.GetXEdgePoint(), c.xNormal, b.GetYEdgePoint(), b.yNormal);
    }
    
    FORCEINLINE FPMUVoxelFeaturePoint GetFeatureNS() const
    {
        return GetSharpFeature(a.GetXEdgePoint(), a.xNormal, c.GetXEdgePoint(), c.xNormal);
    }
    
    FORCEINLINE FPMUVoxelFeaturePoint GetFeatureEW() const
    {
        return GetSharpFeature(a.GetYEdgePoint(), a.yNormal, b.GetYEdgePoint(), b.yNormal);
    }

    FORCEINLINE FPMUVoxelFeaturePoint GetFeatureNEW() const
    {
        FPMUVoxelFeaturePoint f = FPMUVoxelFeaturePoint::Average(
            GetFeatureEW(), GetFeatureNE(), GetFeatureNW());
        if (!f.exists)
        {
            f.position = (a.GetYEdgePoint() + b.GetYEdgePoint() + c.GetXEdgePoint()) / 3.f;
            f.exists = true;
        }
        return f;
    }

    FORCEINLINE FPMUVoxelFeaturePoint GetFeatureNSE() const
    {
        FPMUVoxelFeaturePoint f = FPMUVoxelFeaturePoint::Average(
            GetFeatureNS(), GetFeatureSE(), GetFeatureNE());
        if (!f.exists)
        {
            f.position = (a.GetXEdgePoint() + b.GetYEdgePoint() + c.GetXEdgePoint()) / 3.f;
            f.exists = true;
        }
        return f;
    }
    
    FORCEINLINE FPMUVoxelFeaturePoint GetFeatureNSW() const
    {
        FPMUVoxelFeaturePoint f = FPMUVoxelFeaturePoint::Average(
            GetFeatureNS(), GetFeatureNW(), GetFeatureSW());
        if (!f.exists)
        {
            f.position = (a.GetXEdgePoint() + a.GetYEdgePoint() + c.GetXEdgePoint()) / 3.f;
            f.exists = true;
        }
        return f;
    }
    
    FORCEINLINE FPMUVoxelFeaturePoint GetFeatureSEW() const
    {
        FPMUVoxelFeaturePoint f = FPMUVoxelFeaturePoint::Average(
            GetFeatureEW(), GetFeatureSE(), GetFeatureSW());
        if (!f.exists)
        {
            f.position = (a.GetXEdgePoint() + a.GetYEdgePoint() + b.GetYEdgePoint()) / 3.f;
            f.exists = true;
        }
        return f;
    }

    bool HasConnectionAD(const FPMUVoxelFeaturePoint& fA, const FPMUVoxelFeaturePoint& fD)
    {
        bool flip = (a.state < b.state) == (a.state < c.state);
        if (IsParallel(a.xNormal, a.yNormal, flip) ||
            IsParallel(c.xNormal, b.yNormal, flip))
        {
            return true;
        }
        if (fA.exists)
        {
            if (fD.exists)
            {
                if (IsBelowLine(fA.position, b.GetYEdgePoint(), fD.position))
                {
                    if (IsBelowLine(fA.position, fD.position, c.GetXEdgePoint()) ||
                        IsBelowLine(fD.position, fA.position, a.GetXEdgePoint()))
                    {
                        return true;
                    }
                }
                else if (IsBelowLine(fA.position, fD.position, c.GetXEdgePoint()) &&
                         IsBelowLine(fD.position, a.GetYEdgePoint(), fA.position))
                {
                    return true;
                }
                return false;
            }
            return IsBelowLine(fA.position, b.GetYEdgePoint(), c.GetXEdgePoint());
        }
        return fD.exists &&
            IsBelowLine(fD.position, a.GetYEdgePoint(), a.GetXEdgePoint());
    }
    
    bool HasConnectionBC(const FPMUVoxelFeaturePoint& fB, const FPMUVoxelFeaturePoint& fC)
    {
        bool flip = (b.state < a.state) == (b.state < d.state);
        if (
            IsParallel(a.xNormal, b.yNormal, flip) ||
            IsParallel(c.xNormal, a.yNormal, flip))
        {
            return true;
        }
        if (fB.exists)
        {
            if (fC.exists)
            {
                if (IsBelowLine(fC.position, a.GetXEdgePoint(), fB.position))
                {
                    if (IsBelowLine(fC.position, fB.position, b.GetYEdgePoint()) ||
                        IsBelowLine(fB.position, fC.position, a.GetYEdgePoint()))
                    {
                        return true;
                    }
                }
                else if (IsBelowLine(fC.position, fB.position, b.GetYEdgePoint()) &&
                         IsBelowLine(fB.position, c.GetXEdgePoint(), fC.position))
                {
                    return true;
                }
                return false;
            }
            return IsBelowLine(fB.position, c.GetXEdgePoint(), a.GetYEdgePoint());
        }
        return fC.exists &&
            IsBelowLine(fC.position, a.GetXEdgePoint(), b.GetYEdgePoint());
    }

    FORCEINLINE bool IsInsideABD(const FVector2D& point)
    {
        return IsBelowLine(point, a.position, d.position);
    }
    
    FORCEINLINE bool IsInsideACD(const FVector2D& point)
    {
        return IsBelowLine(point, d.position, a.position);
    }

    FORCEINLINE bool IsInsideABC(const FVector2D& point)
    {
        return IsBelowLine(point, c.position, b.position);
    }

    FORCEINLINE bool IsInsideBCD(const FVector2D& point)
    {
        return IsBelowLine(point, b.position, c.position);
    }

private:

    FORCEINLINE static bool IsBelowLine(const FVector2D& p, const FVector2D& start, const FVector2D& end)
    {
        float determinant =
            (end.X - start.X) * (p.Y - start.Y) -
            (end.Y - start.Y) * (p.X - start.X);
        return determinant < 0.f;
    }

    FORCEINLINE static FVector2D GetIntersection(const FVector2D& p1, const FVector2D& n1, const FVector2D& p2, const FVector2D& n2)
    {
        
        FVector2D d2(-n2.Y, n2.X);
        float u2 = -FVector2D::DotProduct(n1, p2 - p1) / FVector2D::DotProduct(n1, d2);
        return p2 + d2 * u2;
    }

    FORCEINLINE FPMUVoxelFeaturePoint GetCheckedFeatureSW() const
    {
        FVector2D n2 = (a.state < b.state) == (a.state < c.state) ?  a.yNormal : -a.yNormal;
        return GetSharpFeature(a.GetXEdgePoint(), a.xNormal, a.GetYEdgePoint(), n2);
    }
    
    FORCEINLINE FPMUVoxelFeaturePoint GetCheckedFeatureSE() const
    {
        FVector2D n2 = (b.state < a.state) == (b.state < c.state) ?  b.yNormal : -b.yNormal;
        return GetSharpFeature(a.GetXEdgePoint(), a.xNormal, b.GetYEdgePoint(), n2);
    }
    
    FORCEINLINE FPMUVoxelFeaturePoint GetCheckedFeatureNW() const
    {
        FVector2D n2 = (c.state < a.state) == (c.state < d.state) ?  c.xNormal : -c.xNormal;
        return GetSharpFeature(a.GetYEdgePoint(), a.yNormal, c.GetXEdgePoint(), n2);
    }
    
    FORCEINLINE FPMUVoxelFeaturePoint GetCheckedFeatureNE() const
    {
        FVector2D n2 = (d.state < b.state) == (d.state < c.state) ?  b.yNormal : -b.yNormal;
        return GetSharpFeature(c.GetXEdgePoint(), c.xNormal, b.GetYEdgePoint(), n2);
    }
    
    FORCEINLINE FPMUVoxelFeaturePoint GetCheckedFeatureNS() const
    {
        FVector2D n2 = (a.state < b.state) == (c.state < d.state) ?  c.xNormal : -c.xNormal;
        return GetSharpFeature(a.GetXEdgePoint(), a.xNormal, c.GetXEdgePoint(), n2);
    }
    
    FORCEINLINE FPMUVoxelFeaturePoint GetCheckedFeatureEW() const
    {
        FVector2D n2 = (a.state < c.state) == (b.state < d.state) ?  b.yNormal : -b.yNormal;
        return GetSharpFeature(a.GetYEdgePoint(), a.yNormal, b.GetYEdgePoint(), n2);
    }

    FPMUVoxelFeaturePoint GetSharpFeature(const FVector2D& p1, const FVector2D& n1, const FVector2D& p2, const FVector2D& n2) const
    {
        FPMUVoxelFeaturePoint point;
        if (IsSharpFeature(n1, n2))
        {
            point.position = GetIntersection(p1, n1, p2, n2);
            point.exists = IsInsideCell(point.position);
        }
        else
        {
            point.position = FVector2D::ZeroVector;
            point.exists = false;
        }
        return point;
    }

    FORCEINLINE bool IsSharpFeature(const FVector2D& n1, const FVector2D& n2) const
    {
        float dot = FVector2D::DotProduct(n1, -n2);
        return dot >= sharpFeatureLimit && dot < 0.999f;
    }

    FORCEINLINE bool IsParallel(const FVector2D& n1, const FVector2D& n2, bool flip) const
    {
        return FVector2D::DotProduct(n1, flip ? -n2 : n2) > parallelLimit;
    }

    FORCEINLINE bool IsInsideCell(const FVector2D& point) const
    {
        return
            point.X > a.position.X && point.Y > a.position.Y &&
            point.X < d.position.X && point.Y < d.position.Y;
    }
};
