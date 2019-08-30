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

#include "Mesh/PMUMeshGridUtility.h"

void UPMUMeshGridUtility::GenerateLineData(TArray<FVector2D>& Tangents, TArray<float>& Distances, const TArray<FVector2D>& Points, bool bClosedPoly)
{
    const int32 PointCount = Points.Num();

#if 1
    Tangents.Reserve(PointCount);
    Distances.Reserve(PointCount);

    float Distance = 0.f;

    for (int32 i=1; i<PointCount; ++i)
    {
        const FVector2D& P0(Points[i-1]);
        const FVector2D& P1(Points[i]);
        const FVector2D P01(P1-P0);
        FVector2D Tangent;
        float Length;
        P01.ToDirectionAndLength(Tangent, Length);
        Tangents.Emplace(Tangent);
        Distances.Emplace(Distance);
        Distance += Length;
    }

    // Add last line tangent and distance
    if (bClosedPoly)
    {
        Tangents.Emplace(Tangents[0]);
        Distances.Emplace(Distance);
    }
    else
    {
        Tangents.Emplace(Tangents.Last());
        Distances.Emplace(Distance);
    }

    check(PointCount == Tangents.Num());
    check(PointCount == Distances.Num());
#else
    TArray<FVector2D> Tangents;
    TArray<float> Distances;

    Tangents.Reserve(PointCount);
    Distances.Reserve(PointCount);

    // Generate non-normalized line data

    float DistSq = 0.f;

    for (int32 i=1; i<PointCount; ++i)
    {
        const FVector2D& P0(Points[i-1]);
        const FVector2D& P1(Points[i]);
        const FVector2D P01(P1-P0);

        Tangents.Emplace(P01);
        Distances.Emplace(DistSq);

        DistSq += P01.SizeSquared();
    }

    // Add end point line tangent and distance

    if (bClosedPoly)
    {
        Tangents.Emplace(Tangents[0]);
    }
    else
    {
        Tangents.Emplace(Tangents.Last());
    }

    Distances.Emplace(DistSq);

    // Ensure line data match point position count
    check(PointCount == Tangents.Num());
    check(PointCount == Distances.Num());

    // Normalize line data

    float Distance = FMath::Sqrt(DistSq);
    float InvDist = (Distance>0.f) ? (1.f/Distance) : 1.f;
    float LastDist = 0.f;

    for (int32 i=1; i<PointCount; ++i)
    {
        Distances[i] *= InvDist;
        Tangents[i-1] /= Distances[i]-Distances[i-1];
    }

    Tangents.Last() = bClosedPoly ? Tangents[0] : Tangents.Last();
#endif
}

void UPMUMeshGridUtility::GenerateMeshAlongLine(FPMUMeshSectionRef OutSectionRef, const TArray<FVector2D>& Points, float Offset, int32 StepCount, bool bClosedPoly)
{
    const int32 PointCount = Points.Num();

    // Validate section output
    if (! OutSectionRef.HasValidSection())
    {
        return;
    }

    // Validate geometry input
    if ((bClosedPoly && PointCount < 3) || (!bClosedPoly && PointCount < 2) || FMath::Abs(Offset) < KINDA_SMALL_NUMBER)
    {
        return;
    }

    TArray<FVector2D> Tangents;
    TArray<float> Distances;

    GenerateLineData(Tangents, Distances, Points, bClosedPoly);

    FPMUMeshSection& OutSection(*OutSectionRef.SectionPtr);
    StepCount = FMath::Max(0, StepCount);

    GenerateMeshAlongLine(OutSection, Points, Tangents, Distances, Offset, StepCount);
}

void UPMUMeshGridUtility::GenerateMeshAlongLineUniform(FPMUMeshSectionRef OutSectionRef, const TArray<FVector2D>& Points, float GridSize, float Offset, int32 StepCount, bool bClosedPoly)
{
    const int32 PointCount = Points.Num();

    // Validate section output
    if (! OutSectionRef.HasValidSection())
    {
        return;
    }

    // Validate geometry input
    if ((bClosedPoly && PointCount < 3) || (!bClosedPoly && PointCount < 2))
    {
        return;
    }

    // Validate generation parameters
    if (GridSize < KINDA_SMALL_NUMBER || FMath::Abs(Offset) < KINDA_SMALL_NUMBER)
    {
        return;
    }

    TArray<FVector2D> Tangents;
    TArray<float> Distances;

    GenerateLineData(Tangents, Distances, Points, bClosedPoly);

    FPMUMeshSection& OutSection(*OutSectionRef.SectionPtr);
    StepCount = FMath::Max(0, StepCount);

    GenerateMeshAlongLineUniform(OutSection, Points, Tangents, Distances, GridSize, Offset, StepCount, bClosedPoly);
}

void UPMUMeshGridUtility::GenerateMeshAlongLine(FPMUMeshSection& OutSection, const TArray<FVector2D>& Positions, const TArray<FVector2D>& Tangents, const TArray<float>& Distances, float Offset, int32 StepCount)
{
    check(FMath::Abs(Offset) > KINDA_SMALL_NUMBER);
    check(StepCount >= 0);

    const int32 PointCount = Positions.Num();
    const int32 VertexCountZ = 2 + StepCount;
    const int32 VertexCount = PointCount*VertexCountZ;

    const int32 QuadPerSegment = (StepCount+1);
    const int32 SegmentCount = PointCount-1;
    const float VertOffsetZ = Offset / QuadPerSegment;
    const float InvDist = 1.f/Distances.Last();
    const float InvUVY = 1.f/(VertexCountZ);

    const int32 IndexCount = SegmentCount*QuadPerSegment*2;
    const bool bFlipIndexOrder = Offset < 0.f;

    TArray<FVector>& OutPositions(OutSection.Positions);
    TArray<FVector2D>& OutUVs(OutSection.UVs);
    TArray<uint32>& OutTangents(OutSection.Tangents);
    TArray<uint32>& OutIndices(OutSection.Indices);
    FBox& Bounds(OutSection.SectionLocalBox);

    OutSection.Reset();
    OutPositions.Reserve(VertexCount);
    OutUVs.Reserve(VertexCount);
    OutTangents.Reserve(VertexCount*2);
    OutIndices.Reserve(IndexCount);

    // Generate start point vertices
    {
        // Generate vertices

        const FVector2D& Position(Positions[0]);
        const FVector2D& Tangent(Tangents[0]);

        float UVX = Distances[0] * InvDist;
        float UVY = 0.f;

        FVector4 TangentX(Tangent.X, Tangent.Y, 0,0);
        FVector4 TangentZ(-Tangent.Y, Tangent.X, 0,1);

        float OffsetZ = 0.f;

        for (int32 zi=0; zi<VertexCountZ; ++zi)
        {
            OutPositions.Emplace(Position, OffsetZ);
            OutUVs.Emplace(UVX, UVY);
            OutTangents.Emplace(FPackedNormal(TangentX).Vector.Packed);
            OutTangents.Emplace(FPackedNormal(TangentZ).Vector.Packed);

            OffsetZ += VertOffsetZ;
            UVY += InvUVY;
            Bounds += OutPositions.Last();
        }
    }

    // Generate vertices and faces

    for (int32 pi=1; pi<PointCount; ++pi)
    {
        // Generate vertices

        const FVector2D& Position(Positions[pi]);
        const FVector2D& Tangent(Tangents[pi]);

        float UVX = Distances[pi] * InvDist;
        float UVY = 0.f;

        FVector4 TangentX(Tangent.X, Tangent.Y, 0,0);
        FVector4 TangentZ(-Tangent.Y, Tangent.X, 0,1);

        float OffsetZ = 0.f;

        for (int32 zi=0; zi<VertexCountZ; ++zi)
        {
            OutPositions.Emplace(Position, OffsetZ);
            OutUVs.Emplace(UVX, UVY);
            OutTangents.Emplace(FPackedNormal(TangentX).Vector.Packed);
            OutTangents.Emplace(FPackedNormal(TangentZ).Vector.Packed);

            OffsetZ += VertOffsetZ;
            UVY += InvUVY;
            Bounds += OutPositions.Last();
        }

        // Generate faces

        int32 VertexIndex0 = (pi-1) * VertexCountZ;
        int32 VertexIndex1 = (pi  ) * VertexCountZ;

        for (int32 qi=0; qi<QuadPerSegment; ++qi)
        {
            const uint32 vi0 = VertexIndex0+qi;
            const uint32 vi1 = VertexIndex1+qi;
            const uint32 vi2 = vi0+1;
            const uint32 vi3 = vi1+1;

            if (bFlipIndexOrder)
            {
                OutIndices.Emplace(vi0);
                OutIndices.Emplace(vi2);
                OutIndices.Emplace(vi1);

                OutIndices.Emplace(vi2);
                OutIndices.Emplace(vi3);
                OutIndices.Emplace(vi1);
            }
            else
            {
                OutIndices.Emplace(vi0);
                OutIndices.Emplace(vi1);
                OutIndices.Emplace(vi2);

                OutIndices.Emplace(vi2);
                OutIndices.Emplace(vi1);
                OutIndices.Emplace(vi3);
            }
        }
    }
}

void UPMUMeshGridUtility::GenerateMeshAlongLineUniform(FPMUMeshSection& OutSection, const TArray<FVector2D>& Positions, const TArray<FVector2D>& Tangents, const TArray<float>& Distances, float GridSize, float Offset, int32 StepCount, bool bClosedPoly)
{
    check(GridSize > KINDA_SMALL_NUMBER);
    check(FMath::Abs(Offset) > KINDA_SMALL_NUMBER);
    check(StepCount >= 0);
    check(Positions.Num() > 1);

    const int32 LinePointCount = Positions.Num();

    const float LineLength = Distances.Last();

    const int32 SegmentCount = FMath::CeilToInt(LineLength / GridSize);
    const int32 PointCount = SegmentCount+1;

    const int32 VertexCountZ = 2 + StepCount;
    const int32 VertexCount = PointCount*VertexCountZ;
    const int32 QuadPerSegment = (StepCount+1);

    const float SegmentLength = LineLength / SegmentCount;
    const float VertOffsetZ = Offset / QuadPerSegment;
    const float InvDist = 1.f/Distances.Last();
    const float InvUVY = 1.f/(VertexCountZ-1);

    const int32 IndexCount = SegmentCount*QuadPerSegment*6;

    TArray<FVector>& OutPositions(OutSection.Positions);
    TArray<FVector2D>& OutUVs(OutSection.UVs);
    TArray<uint32>& OutTangents(OutSection.Tangents);
    TArray<uint32>& OutIndices(OutSection.Indices);
    FBox& Bounds(OutSection.SectionLocalBox);

    OutSection.Reset();
    OutPositions.Reserve(VertexCount);
    OutUVs.Reserve(VertexCount);
    OutTangents.Reserve(VertexCount*2);
    OutIndices.Reserve(IndexCount);

    float CurrentDistance = 0.f;
    bool bFlipIndexOrder = Offset < 0.f;

    int32 PointIndex = 0;
    int32 PointIndexNew = PointIndex;

    FVector2D Point;
    FVector2D Tangent0;
    FVector2D Tangent1;
    float Distance;
    float Length;

    // Generate start point vertices

    {
        // Generate vertices

        Point = Positions[0];
        Distance = Distances[0];
        Length = Distances[1]-Distances[0];

        if (bClosedPoly)
        {
            Tangent0 = (Tangents[LinePointCount-2]+Tangents[0])*.5f;
            Tangent1 = (Tangents[0]+Tangents[1])*.5f;

            Tangent0.Normalize();
            Tangent1.Normalize();
        }
        else
        {
            Tangent0 = Tangents[0];
            Tangent1 = (Tangents[0]+Tangents[1])*.5f;

            Tangent1.Normalize();
        }

        FVector2D Position = Point;
        FVector2D Tangent = Tangent0;
        FVector4 TangentX(Tangent.X, Tangent.Y, 0,0);
        FVector4 TangentZ(-Tangent.Y, Tangent.X, 0,1);

        float UVX = Distance * InvDist;
        float UVY = 0.f;

        float OffsetZ = 0.f;

        for (int32 zi=0; zi<VertexCountZ; ++zi)
        {
            OutPositions.Emplace(Position, OffsetZ);
            OutUVs.Emplace(UVX, UVY);
            OutTangents.Emplace(FPackedNormal(TangentX).Vector.Packed);
            OutTangents.Emplace(FPackedNormal(TangentZ).Vector.Packed);

            OffsetZ += VertOffsetZ;
            UVY += InvUVY;
            Bounds += OutPositions.Last();
        }

        CurrentDistance += SegmentLength;
    }

    // Generate vertices and faces

    for (int32 vi=1; vi<PointCount-1; ++vi)
    {
        const float DistanceThreshold = CurrentDistance + KINDA_SMALL_NUMBER;

        for (int32 di=PointIndex; (di<LinePointCount && DistanceThreshold>Distances[di]); ++di)
        {
            PointIndexNew = di;
        }

        // Generate vertices

        if (PointIndex != PointIndexNew)
        {
            int32 PointIndexPrev = PointIndex;
            int32 PointIndexNext = PointIndexNew+1;

            PointIndex = PointIndexNew;

            Point = Positions[PointIndex];
            Distance = Distances[PointIndex];
            Length = Distances[PointIndexNext]-Distance;

            Tangent0 = (Tangents[PointIndexPrev]+Tangents[PointIndex])*.5f;
            Tangent1 = (Tangents[PointIndex]+Tangents[PointIndexNext])*.5f;

            Tangent0.Normalize();
            Tangent1.Normalize();
        }

        float DistDelta = (CurrentDistance-Distance);
        float DistAlpha = DistDelta / Length;

        FVector2D Tangent = FMath::LerpStable(Tangent0, Tangent1, DistAlpha).GetSafeNormal();
        FVector2D Position = Point+Tangent*DistDelta;
        FVector4 TangentX(Tangent.X, Tangent.Y, 0,0);
        FVector4 TangentZ(-Tangent.Y, Tangent.X, 0,1);

        float UVX = CurrentDistance * InvDist;
        float UVY = 0.f;

        float OffsetZ = 0.f;

        for (int32 zi=0; zi<VertexCountZ; ++zi)
        {
            OutPositions.Emplace(Position, OffsetZ);
            OutUVs.Emplace(UVX, UVY);
            OutTangents.Emplace(FPackedNormal(TangentX).Vector.Packed);
            OutTangents.Emplace(FPackedNormal(TangentZ).Vector.Packed);

            OffsetZ += VertOffsetZ;
            UVY += InvUVY;
            Bounds += OutPositions.Last();
        }

        // Generate faces

        int32 VertexIndex0 = (vi-1) * VertexCountZ;
        int32 VertexIndex1 = (vi  ) * VertexCountZ;

        for (int32 qi=0; qi<QuadPerSegment; ++qi)
        {
            const uint32 vi0 = VertexIndex0+qi;
            const uint32 vi1 = VertexIndex1+qi;
            const uint32 vi2 = vi0+1;
            const uint32 vi3 = vi1+1;

            if (bFlipIndexOrder)
            {
                OutIndices.Emplace(vi0);
                OutIndices.Emplace(vi2);
                OutIndices.Emplace(vi1);

                OutIndices.Emplace(vi2);
                OutIndices.Emplace(vi3);
                OutIndices.Emplace(vi1);
            }
            else
            {
                OutIndices.Emplace(vi0);
                OutIndices.Emplace(vi1);
                OutIndices.Emplace(vi2);

                OutIndices.Emplace(vi2);
                OutIndices.Emplace(vi1);
                OutIndices.Emplace(vi3);
            }
        }

        CurrentDistance += SegmentLength;
    }

    // Generate end point vertices and faces

    {
        PointIndex = LinePointCount-1;

        // Generate vertices

        Point = Positions[PointIndex];
        Distance = Distances[PointIndex];

        FVector2D Position = Point;
        FVector2D Tangent = bClosedPoly
            ? (Tangents[LinePointCount-2]+Tangents[0])*.5f
            : Tangents[LinePointCount-1];

        Tangent.Normalize();

        FVector4 TangentX(Tangent.X, Tangent.Y, 0,0);
        FVector4 TangentZ(-Tangent.Y, Tangent.X, 0,1);

        float UVX = Distance * InvDist;
        float UVY = 0.f;

        float OffsetZ = 0.f;

        for (int32 zi=0; zi<VertexCountZ; ++zi)
        {
            OutPositions.Emplace(Position, OffsetZ);
            OutUVs.Emplace(UVX, UVY);
            OutTangents.Emplace(FPackedNormal(TangentX).Vector.Packed);
            OutTangents.Emplace(FPackedNormal(TangentZ).Vector.Packed);

            OffsetZ += VertOffsetZ;
            UVY += InvUVY;
            Bounds += OutPositions.Last();
        }

        // Generate faces

        int32 VertexIndex0 = (PointCount-2) * VertexCountZ;
        int32 VertexIndex1 = (PointCount-1) * VertexCountZ;

        for (int32 qi=0; qi<QuadPerSegment; ++qi)
        {
            const uint32 vi0 = VertexIndex0+qi;
            const uint32 vi1 = VertexIndex1+qi;
            const uint32 vi2 = vi0+1;
            const uint32 vi3 = vi1+1;

            if (bFlipIndexOrder)
            {
                OutIndices.Emplace(vi0);
                OutIndices.Emplace(vi2);
                OutIndices.Emplace(vi1);

                OutIndices.Emplace(vi2);
                OutIndices.Emplace(vi3);
                OutIndices.Emplace(vi1);
            }
            else
            {
                OutIndices.Emplace(vi0);
                OutIndices.Emplace(vi1);
                OutIndices.Emplace(vi2);

                OutIndices.Emplace(vi2);
                OutIndices.Emplace(vi1);
                OutIndices.Emplace(vi3);
            }
        }

        CurrentDistance += SegmentLength;
    }
}
