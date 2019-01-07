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
#include "DistanceField/PMUDFGeometry.h"

class FPMUDFGeometryLine : public FPMUDFGeometry
{
    FVector2D P0;
    FVector2D P1;
    FVector2D SignDir;
    float Radius;
    float RadiusSqInv;

    FBox2D Bounds;
    float BoundingRadius;

public:

    FPMUDFGeometryLine() = default;

    FPMUDFGeometryLine(const FVector2D& InP0, const FVector2D& InP1, const float InRadius, bool bReverseOrder)
        : P0(InP0)
        , P1(InP1)
    {
        Set(InP0, InP1, InRadius, bReverseOrder);
    }

    void Set(const FVector2D& InP0, const FVector2D& InP1, const float InRadius, bool bReverseOrder)
    {
        check(InRadius > 0.f);

        P0 = bReverseOrder ? InP1 : InP0;
        P1 = bReverseOrder ? InP0 : InP1;

        FVector2D Dir((P1-P0).GetSafeNormal());
        SignDir = FVector2D(Dir.Y, -Dir.X);

        Radius = FMath::Max(InRadius, KINDA_SMALL_NUMBER);
        RadiusSqInv = 1.f / (Radius * Radius);

        Bounds.Init();
        Bounds += P0;
        Bounds += P1;
        Bounds = Bounds.ExpandBy(Radius);

        BoundingRadius = (Bounds.Max-Bounds.GetCenter()).Size();
    }

    FORCEINLINE virtual bool IsValid() const
    {
        return Bounds.bIsValid && Radius > 0.f;
    }

    FORCEINLINE virtual FPSPMUDFGeometry Copy() const
    {
        return MakeShareable(new FPMUDFGeometryLine(P0, P1, Radius, false));
    }

    FORCEINLINE virtual bool IsWithinBoundingRadius(const FVector2D& Point) const
    {
        check(IsValid());
        return (Point-Bounds.GetCenter()).SizeSquared() < GetBoundingRadiusSquared();
    }

    FORCEINLINE virtual bool Intersect(const FBox2D& Box) const
    {
        check(IsValid());
        return Bounds.Intersect(Box);
    }

    FORCEINLINE virtual float GetBoundingRadius() const
    {
        check(IsValid());
        return BoundingRadius;
    }

    FORCEINLINE virtual float GetBoundingRadiusSquared() const
    {
        check(IsValid());
        return BoundingRadius * BoundingRadius;
    }

    FORCEINLINE virtual float GetBoundingCenterDistance(const FVector2D& Point) const
    {
        check(IsValid());
        return (Point-Bounds.GetCenter()).Size();
    }

    FORCEINLINE virtual float GetBoundingCenterDistanceSquared(const FVector2D& Point) const
    {
        check(IsValid());
        return (Point-Bounds.GetCenter()).SizeSquared();
    }

    FORCEINLINE virtual FBox2D GetBoundingBox() const
    {
        check(IsValid());
        return Bounds;
    }

    FORCEINLINE virtual float GetRadius() const
    {
        check(IsValid());
        return Radius;
    }

    FORCEINLINE virtual float GetRadiusSquared() const
    {
        check(IsValid());
        return Radius * Radius;
    }

    FORCEINLINE virtual float GetDistance(const FVector2D& Point) const
    {
        check(IsValid());
        return FMath::Sqrt(GetDistanceSquared(Point));
    }

    FORCEINLINE virtual float GetDistanceSquared(const FVector2D& Point) const
    {
        check(IsValid());
        return (Point-FMath::ClosestPointOnSegment2D(Point, P0, P1)).SizeSquared();
    }

    virtual float GetSign(const FVector2D& Point) const
    {
        check(IsValid());
        return (Point-((P0+P1)*0.5f)) | SignDir;
    }

    virtual float GetClampedDistanceRatio(const FVector2D& Point) const
    {
        check(IsValid());
        return FMath::Clamp((GetDistanceSquared(Point)*RadiusSqInv), 0.f, 1.f);
    }

    virtual bool IsReversed(const FVector2D& Point) const
    {
        check(IsValid());
        return GetSign(Point) < 0.f;
    }
};
