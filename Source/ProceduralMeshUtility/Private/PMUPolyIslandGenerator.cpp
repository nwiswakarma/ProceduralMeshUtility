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

#include "PMUPolyIslandGenerator.h"

bool UPMUPolyIslandGenerator::GeneratePoly(const FPMUPolyIslandParams& Params, FRandomStream& Rand, TArray<FVector2D>& OutVerts)
{
    typedef FPlatformMath FPM;

    const FVector2D& displRange( Params.DisplacementRange );
    const float displMin( displRange.X );
    const float displMax( displRange.Y );

    const int32 sideCount = Params.SideCount;
    const float polyAngleOffset = Params.PolyAngleOffset;
    const float tau = 2*PI;
    const float sideAngle = tau/sideCount;
    float angle = (polyAngleOffset/360.f)*tau;

    FPolyList PolyList;
    FBox2D Bounds(ForceInitToZero);

    // Generates random initial poly verts
    for (int32 i=0; i<sideCount; ++i, angle+=sideAngle)
    {
        FRoughV2D pt(FPM::Cos(angle), FPM::Sin(angle), Rand, displMin, displMax);
        Bounds += pt.Pos;
        PolyList.AddTail(pt);
    }
    PolyList.AddTail(PolyList.GetHead()->GetValue());

    return GeneratePoly(PolyList, Bounds, Params, Rand, OutVerts);
}

bool UPMUPolyIslandGenerator::GeneratePoly(const FPMUPolyIslandParams& Params, const TArray<FVector2D>& InitialPoints, FRandomStream& Rand, TArray<FVector2D>& OutVerts)
{
    typedef FPlatformMath FPM;

    const FVector2D& displRange( Params.DisplacementRange );
    const float displMin( displRange.X );
    const float displMax( displRange.Y );

    FPolyList PolyList;
    FBox2D Bounds(ForceInitToZero);

    // Generate rough vector from passed initial points
    for (const FVector2D& Point : InitialPoints)
    {
        FRoughV2D pt(Point.X, Point.Y, Rand, displMin, displMax);
        Bounds += pt.Pos;
        PolyList.AddTail(pt);
    }
    PolyList.AddTail(PolyList.GetHead()->GetValue());

    return GeneratePoly(PolyList, Bounds, Params, Rand, OutVerts);
}

bool UPMUPolyIslandGenerator::GeneratePoly(FPolyList& PolyList, FBox2D& Bounds, const FPMUPolyIslandParams& Params, FRandomStream& Rand, TArray<FVector2D>& OutVerts)
{
    // Input parameter invalid, abort
    if (! Params.IsValid())
    {
        return false;
    }

    const int32 subdivCount = Params.SubdivCount;
    const float subdivLimit = Params.SubdivLimit;
    const float polyScale = Params.PolyScale;

    const FVector2D& size( Params.Size );
    const FVector2D exts = size/2.f;

    FPolyList& poly(PolyList);

    // Subdivides poly edges
    for (int32 i=0; i<subdivCount; ++i)
    {
        int32 subdivPerformed = 0;
        FPolyNode* h( poly.GetHead() );
        FPolyNode* n( h->GetNextNode() );

        while (n)
        {
            const FRoughV2D& r0( n->GetPrevNode()->GetValue() );
            const FRoughV2D& r1( n->GetValue() );
            const FVector2D& v0( r0.Pos );
            const FVector2D& v1( r1.Pos );
            if (FVector2D::DistSquared(v0, v1) > subdivLimit)
            {
                float mx = (v0.X+v1.X)*.5f;
                float my = (v0.Y+v1.Y)*.5f;
                float vx = -(v0.Y-v1.Y);
                float vy = v0.X-v1.X;
                float b = r1.Balance;
                float o = r1.MaxOffset;
                float d = (Rand.GetFraction()-b)*o;
                FRoughV2D pt(mx+d*vx, my+d*vy, b, o);
                poly.InsertNode(pt, n);
                Bounds += pt.Pos;
                ++subdivPerformed;
            }
            n = n->GetNextNode();
        }

        if (subdivPerformed < 1)
        {
            break;
        }
    }

    // Fit points to bounds & write output vertices
    {
        const FVector2D& Unit( FVector2D::UnitVector );
        FVector2D Offset( Unit-(Unit+Bounds.Min) );
        FVector2D ScaledOffset( (1.f-polyScale)*exts );
        float Scale = ( size/Bounds.GetSize() ).GetMin() * polyScale;

        OutVerts.Reset(poly.Num());

        for (const FRoughV2D& v : poly)
        {
            OutVerts.Emplace(ScaledOffset+(Offset+v.Pos)*Scale);
        }
    }

    return true;
}

void UPMUPolyIslandGenerator::FitPoints(TArray<FVector2D>& Points, const FVector2D& Dimension, float FitScale)
{
    // Generate point bounds

    FBox2D Bounds(ForceInitToZero);

    for (const FVector2D& Point : Points)
    {
        Bounds += Point;
    }

    // Scale points

    const FVector2D& Unit(FVector2D::UnitVector);
    const FVector2D& size(Dimension);
    const FVector2D exts = size/2.f;

    const FVector2D Offset = Unit-(Unit+Bounds.Min);
    const FVector2D ScaledOffset = (1.f-FitScale) * exts;
    const float Scale = (size/Bounds.GetSize()).GetMin() * FitScale;

    for (FVector2D& Point : Points)
    {
        Point = ScaledOffset + (Offset+Point)*Scale;
    }
}

TArray<FVector2D> UPMUPolyIslandGenerator::K2_GeneratePoly(FPMUPolyIslandParams Params)
{
    FRandomStream Rand(Params.RandomSeed);
    TArray<FVector2D> Vertices;

    GeneratePoly(Params, Rand, Vertices);

    return MoveTemp(Vertices);
}

TArray<FVector2D> UPMUPolyIslandGenerator::K2_GeneratePolyWithInitialPoints(FPMUPolyIslandParams Params, const TArray<FVector2D>& Points)
{
    FRandomStream Rand(Params.RandomSeed);
    TArray<FVector2D> Vertices;

    GeneratePoly(Params, Points, Rand, Vertices);

    return MoveTemp(Vertices);
}

TArray<FVector2D> UPMUPolyIslandGenerator::K2_GeneratePointOffsets(int32 Seed, const TArray<FVector2D>& Points, float PointRadius)
{
    FRandomStream Rand(Seed);
    FBox2D Bounds(ForceInitToZero);
    TArray<FVector2D> NewPoints(Points);

    for (FVector2D& Point : NewPoints)
    {
		FVector2D UnitRand;
		float L;

		do
		{
			// Check random vectors in the unit sphere so result is statistically uniform.
			UnitRand.X = Rand.GetFraction() * 2.f - 1.f;
			UnitRand.Y = Rand.GetFraction() * 2.f - 1.f;
			L = UnitRand.SizeSquared();
		}
		while (L > 1.f || L < KINDA_SMALL_NUMBER);

		Point += UnitRand.GetSafeNormal() * PointRadius;
    }

    return NewPoints;
}

TArray<FVector2D> UPMUPolyIslandGenerator::K2_FitPoints(const TArray<FVector2D>& Points, FVector2D Dimension, float FitScale)
{
    // Invalid dimension, abort
    if (Dimension.X < KINDA_SMALL_NUMBER || Dimension.Y < KINDA_SMALL_NUMBER)
    {
        return Points;
    }

    TArray<FVector2D> NewPoints(Points);

    FitPoints(NewPoints, Dimension, FitScale);

    return NewPoints;
}

TArray<FVector2D> UPMUPolyIslandGenerator::K2_FlipPoints(const TArray<FVector2D>& Points, FVector2D Dimension)
{
    // Invalid dimension, abort
    if (Dimension.X < KINDA_SMALL_NUMBER || Dimension.Y < KINDA_SMALL_NUMBER)
    {
        return Points;
    }

    TArray<FVector2D> NewPoints(Points);

    for (FVector2D& Point : NewPoints)
    {
        Point = Dimension - Point;
    }

    return NewPoints;
}
