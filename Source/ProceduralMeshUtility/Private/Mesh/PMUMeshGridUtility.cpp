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
#include "ProceduralMeshUtility.h"

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

void UPMUMeshGridUtility::GenerateMeshAlongLineUniform(FPMUMeshSectionRef OutSectionRef, const TArray<FVector2D>& Points, float GridSize, float Offset, float ExtrudeOffset, int32 StepCount, bool bClosedPoly)
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
    ExtrudeOffset = FMath::Max(0.f, ExtrudeOffset);

    GenerateMeshAlongLineUniform(OutSection, Points, Tangents, Distances, GridSize, Offset, ExtrudeOffset, StepCount, bClosedPoly);
}

void UPMUMeshGridUtility::GenerateMeshAlongLineUniform(FPMUMeshSection& OutSection, const TArray<FVector2D>& Positions, const TArray<FVector2D>& Tangents, const TArray<float>& Distances, float GridSize, float Offset, float ExtrudeOffset, int32 StepCount, bool bClosedPoly)
{
    check(GridSize > KINDA_SMALL_NUMBER);
    check(FMath::Abs(Offset) > KINDA_SMALL_NUMBER);
    check(StepCount >= 0);
    check(Positions.Num() > 1);

    const int32 LinePointCount = Positions.Num();
    const float LineLength = Distances.Last();

    const int32 SegmentCount = FMath::CeilToInt(LineLength / GridSize);
    const int32 PointCount = SegmentCount+1;

    const bool bGenerateExtrude = ExtrudeOffset > KINDA_SMALL_NUMBER;
    const int32 ExtrudeVertexCount = bGenerateExtrude ? 2 : 0;

    const int32 VertexCountZ = 2 + StepCount;
    const int32 VertexZLastIndex = VertexCountZ-1;
    const int32 VertexCountPerPoint = VertexCountZ + ExtrudeVertexCount;
    const int32 VertexCount = PointCount*VertexCountPerPoint;
    const int32 QuadPerSegment = VertexCountPerPoint-1;

    const float SegmentLength = LineLength / SegmentCount;
    const float VertOffsetZ = Offset / (VertexCountZ-1);
    const float InvDist = 1.f/Distances.Last();
    const float InvUVY = 1.f/(VertexCountPerPoint-1);

    const int32 IndexCount = SegmentCount*QuadPerSegment*6;

    TArray<FVector>& OutPositions(OutSection.Positions);
    TArray<FVector2D>& OutUVs(OutSection.UVs);
    TArray<uint32>& OutTangents(OutSection.Tangents);
    TArray<FColor>& OutColors(OutSection.Colors);
    TArray<uint32>& OutIndices(OutSection.Indices);
    FBox& Bounds(OutSection.SectionLocalBox);

    OutSection.Reset();
    OutPositions.Reserve(VertexCount);
    OutUVs.Reserve(VertexCount);
    OutTangents.Reserve(VertexCount*2);
    OutColors.Reserve(VertexCount);
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

        FVector2D Tangent = Tangent0;
        FVector2D Normal(Tangent.Y, -Tangent.X);
        FVector4 TangentX(Tangent.X, Tangent.Y, 0,0);
        FVector4 TangentZ(Normal.X, Normal.Y, 0,1);

        FVector2D OffsetXY = Normal*ExtrudeOffset;
        FVector2D Position = Point;

        float UVX = Distance * InvDist;
        float UVY = 0.f;

        if (bGenerateExtrude)
        {
            OutPositions.Emplace(Position, 0.f);
            OutUVs.Emplace(UVX, UVY);
            OutTangents.Emplace(FPackedNormal(FVector4(1,0,0,0)).Vector.Packed);
            OutTangents.Emplace(FPackedNormal(FVector4(0,0,1,1)).Vector.Packed);
            OutColors.Emplace(0,0,0,0);
            UVY += InvUVY;
            Bounds += OutPositions.Last();
        }

        for (int32 zi=0; zi<VertexCountZ; ++zi)
        {
            OutPositions.Emplace(OffsetXY+Position, zi*VertOffsetZ);
            OutUVs.Emplace(UVX, UVY);
            OutTangents.Emplace(FPackedNormal(TangentX).Vector.Packed);
            OutTangents.Emplace(FPackedNormal(TangentZ).Vector.Packed);
            OutColors.Emplace(FColor::White);
            UVY += InvUVY;
            Bounds += OutPositions.Last();
        }

        if (bGenerateExtrude)
        {
            OutPositions.Emplace(Position, VertexZLastIndex*VertOffsetZ);
            OutUVs.Emplace(UVX, UVY);
            OutTangents.Emplace(FPackedNormal(FVector4(1,0, 0,0)).Vector.Packed);
            OutTangents.Emplace(FPackedNormal(FVector4(0,0,-1,1)).Vector.Packed);
            OutColors.Emplace(0,0,0,0);
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
        FVector2D Normal(Tangent.Y, -Tangent.X);
        FVector4 TangentX(Tangent.X, Tangent.Y, 0,0);
        FVector4 TangentZ(Normal.X, Normal.Y, 0,1);

        FVector2D OffsetXY = Normal*ExtrudeOffset;
        FVector2D Position = Point+Tangent*DistDelta;

        float UVX = CurrentDistance * InvDist;
        float UVY = 0.f;

        if (bGenerateExtrude)
        {
            OutPositions.Emplace(Position, 0.f);
            OutUVs.Emplace(UVX, UVY);
            OutTangents.Emplace(FPackedNormal(FVector4(1,0,0,0)).Vector.Packed);
            OutTangents.Emplace(FPackedNormal(FVector4(0,0,1,1)).Vector.Packed);
            OutColors.Emplace(0,0,0,0);
            UVY += InvUVY;
            Bounds += OutPositions.Last();
        }

        for (int32 zi=0; zi<VertexCountZ; ++zi)
        {
            OutPositions.Emplace(OffsetXY+Position, zi*VertOffsetZ);
            OutUVs.Emplace(UVX, UVY);
            OutTangents.Emplace(FPackedNormal(TangentX).Vector.Packed);
            OutTangents.Emplace(FPackedNormal(TangentZ).Vector.Packed);
            OutColors.Emplace(FColor::White);
            UVY += InvUVY;
            Bounds += OutPositions.Last();
        }

        if (bGenerateExtrude)
        {
            OutPositions.Emplace(Position, VertexZLastIndex*VertOffsetZ);
            OutUVs.Emplace(UVX, UVY);
            OutTangents.Emplace(FPackedNormal(FVector4(1,0, 0,0)).Vector.Packed);
            OutTangents.Emplace(FPackedNormal(FVector4(0,0,-1,1)).Vector.Packed);
            OutColors.Emplace(0,0,0,0);
            UVY += InvUVY;
            Bounds += OutPositions.Last();
        }

        // Generate faces

        int32 VertexIndex0 = (vi-1) * VertexCountPerPoint;
        int32 VertexIndex1 = (vi  ) * VertexCountPerPoint;

        for (int32 qi=0; qi<QuadPerSegment; ++qi)
        {
            const uint32 vi0 = VertexIndex0+qi;
            const uint32 vi1 = VertexIndex1+qi;
            const uint32 vi2 = vi0+1;
            const uint32 vi3 = vi1+1;

            if (bFlipIndexOrder)
            {
                OutIndices.Emplace(vi0);
                OutIndices.Emplace(vi1);
                OutIndices.Emplace(vi2);

                OutIndices.Emplace(vi2);
                OutIndices.Emplace(vi1);
                OutIndices.Emplace(vi3);
            }
            else
            {
                OutIndices.Emplace(vi0);
                OutIndices.Emplace(vi2);
                OutIndices.Emplace(vi1);

                OutIndices.Emplace(vi2);
                OutIndices.Emplace(vi3);
                OutIndices.Emplace(vi1);
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

        FVector2D Tangent = bClosedPoly
            ? (Tangents[LinePointCount-2]+Tangents[0])*.5f
            : Tangents[LinePointCount-1];

        Tangent.Normalize();

        FVector2D Normal(Tangent.Y, -Tangent.X);
        FVector4 TangentX(Tangent.X, Tangent.Y, 0,0);
        FVector4 TangentZ(Normal.X, Normal.Y, 0,1);

        FVector2D OffsetXY = Normal*ExtrudeOffset;
        FVector2D Position = Point;

        float UVX = Distance * InvDist;
        float UVY = 0.f;

        if (bGenerateExtrude)
        {
            OutPositions.Emplace(Position, 0.f);
            OutUVs.Emplace(UVX, UVY);
            OutTangents.Emplace(FPackedNormal(FVector4(1,0,0,0)).Vector.Packed);
            OutTangents.Emplace(FPackedNormal(FVector4(0,0,1,1)).Vector.Packed);
            OutColors.Emplace(0,0,0,0);
            UVY += InvUVY;
            Bounds += OutPositions.Last();
        }

        for (int32 zi=0; zi<VertexCountZ; ++zi)
        {
            OutPositions.Emplace(OffsetXY+Position, zi*VertOffsetZ);
            OutUVs.Emplace(UVX, UVY);
            OutTangents.Emplace(FPackedNormal(TangentX).Vector.Packed);
            OutTangents.Emplace(FPackedNormal(TangentZ).Vector.Packed);
            OutColors.Emplace(FColor::White);
            UVY += InvUVY;
            Bounds += OutPositions.Last();
        }

        if (bGenerateExtrude)
        {
            OutPositions.Emplace(Position, VertexZLastIndex*VertOffsetZ);
            OutUVs.Emplace(UVX, UVY);
            OutTangents.Emplace(FPackedNormal(FVector4(1,0, 0,0)).Vector.Packed);
            OutTangents.Emplace(FPackedNormal(FVector4(0,0,-1,1)).Vector.Packed);
            OutColors.Emplace(0,0,0,0);
            UVY += InvUVY;
            Bounds += OutPositions.Last();
        }

        // Generate faces

        int32 VertexIndex0 = (PointCount-2) * VertexCountPerPoint;
        int32 VertexIndex1 = (PointCount-1) * VertexCountPerPoint;

        for (int32 qi=0; qi<QuadPerSegment; ++qi)
        {
            const uint32 vi0 = VertexIndex0+qi;
            const uint32 vi1 = VertexIndex1+qi;
            const uint32 vi2 = vi0+1;
            const uint32 vi3 = vi1+1;

            if (bFlipIndexOrder)
            {
                OutIndices.Emplace(vi0);
                OutIndices.Emplace(vi1);
                OutIndices.Emplace(vi2);

                OutIndices.Emplace(vi2);
                OutIndices.Emplace(vi1);
                OutIndices.Emplace(vi3);
            }
            else
            {
                OutIndices.Emplace(vi0);
                OutIndices.Emplace(vi2);
                OutIndices.Emplace(vi1);

                OutIndices.Emplace(vi2);
                OutIndices.Emplace(vi3);
                OutIndices.Emplace(vi1);
            }
        }

        CurrentDistance += SegmentLength;
    }
}

void UPMUMeshGridUtility::GenerateGrids(FPMUMeshSectionRef OutSectionRef, const TArray<FIntPoint>& GridOrigins, int32 DimensionX, int32 DimensionY, float UnitScale)
{
    if (! OutSectionRef.HasValidSection())
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshGridUtility::GenerateGrids() ABORTED, INVALID MESH SECTION"));
        return;
    }
    else
    if (GridOrigins.Num() < 1)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshGridUtility::GenerateGrids() ABORTED, EMPTY GRID ORIGINS"));
        return;
    }
    else
    if (DimensionX < 1 || DimensionY < 1)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshGridUtility::GenerateGrids() ABORTED, INVALID DIMENSION"));
        return;
    }
    else
    if (UnitScale < KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshGridUtility::GenerateGrids() ABORTED, INVALID UNIT SCALE"));
        return;
    }

    FPMUMeshSection& Section(*OutSectionRef.SectionPtr);

	TArray<FVector>& Positions(Section.Positions);
	TArray<uint32>& Tangents(Section.Tangents);
	TArray<FVector2D>& UVs(Section.UVs);
	TArray<uint32>& Indices(Section.Indices);

    FBox& Bounds(Section.SectionLocalBox);

    int32 DX = DimensionX;
    int32 DY = DimensionY;
    int32 BX = DX+1;
    int32 BY = DY+1;
    float fDX = static_cast<float>(DX);
    float fDY = static_cast<float>(DY);

    int32 VertexOffset = Positions.Num();

    //UE_LOG(LogTemp,Warning, TEXT("VertexOffset: %d"), VertexOffset);

    for (const FIntPoint& Offset : GridOrigins)
    {
        FVector2D fOffset(Offset.X*DX*UnitScale, Offset.Y*DY*UnitScale);

		for (int32 y=0; y<BY; ++y)
        for (int32 x=0; x<BX; ++x)
        {
            float fx = fOffset.X + static_cast<float>(x)*UnitScale;
            float fy = fOffset.Y + static_cast<float>(y)*UnitScale;

            Positions.Emplace(fx, fy, 0);
            UVs.Emplace(fx/fDX, fy/fDY);
        }

        Bounds += Positions[VertexOffset];
        Bounds += Positions.Last();

		for (int32 y=0; y<DY; ++y)
        for (int32 x=0; x<DX; ++x)
        {
            int32 idx = VertexOffset + (x + (y * BX));

            Indices.Emplace(idx);
            Indices.Emplace(idx + BX);
            Indices.Emplace(idx + 1);

            Indices.Emplace(idx + 1);
            Indices.Emplace(idx + BX);
            Indices.Emplace(idx + BX + 1);
        }

        VertexOffset = Positions.Num();
    }

    //UE_LOG(LogTemp,Warning, TEXT("GridOrigins.Num(): %d"), GridOrigins.Num());
    //UE_LOG(LogTemp,Warning, TEXT("Positions.Num(): %d"), Positions.Num());
    //UE_LOG(LogTemp,Warning, TEXT("Indices.Num(): %d"), Indices.Num());
    //UE_LOG(LogTemp,Warning, TEXT("Bounds: %s"), *Bounds.ToString());
}

void UPMUMeshGridUtility::GenerateGridByPoints(
    FPMUMeshSectionRef OutSectionRef,
    const TArray<FIntPoint>& InPoints,
    FIntPoint GridOrigin,
    FIntPoint GridOffset,
    int32 DimensionX,
    int32 DimensionY,
    float UnitScale,
    bool bClearSection
    )
{
    if (! OutSectionRef.HasValidSection())
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshGridUtility::GenerateGridByPoints() ABORTED, INVALID MESH SECTION"));
        return;
    }
    else
    if (DimensionX < 2 || DimensionY < 2)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshGridUtility::GenerateGridByPoints() ABORTED, INVALID DIMENSION"));
        return;
    }
    else
    if (UnitScale < KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshGridUtility::GenerateGridByPoints() ABORTED, INVALID UNIT SCALE"));
        return;
    }

    FPMUMeshSection& Section(*OutSectionRef.SectionPtr);

	TArray<FVector>& Positions(Section.Positions);
	TArray<uint32>& Tangents(Section.Tangents);
	TArray<FVector2D>& UVs(Section.UVs);
	TArray<uint32>& Indices(Section.Indices);
    FBox& Bounds(Section.SectionLocalBox);

    // Clear section if required
    if (bClearSection)
    {
        Section.Reset();
    }

    // No points specified, abort
    if (InPoints.Num() < 1)
    {
        return;
    }

    const int32 DX = DimensionX;
    const int32 DY = DimensionY;
    const int32 BX = DX+1;
    const int32 BY = DY+1;
    const float fDX = static_cast<float>(DX);
    const float fDY = static_cast<float>(DY);

    const FVector2D NW(0        , 0        );
    const FVector2D NE(UnitScale, 0        );
    const FVector2D SW(0        , UnitScale);
    const FVector2D SE(UnitScale, UnitScale);

    const int32 GridVertexCount = BX*BY;

    const int32 OX = GridOrigin.X*DX;
    const int32 OY = GridOrigin.Y*DY;

    const int32 FX = GridOffset.X*DX;
    const int32 FY = GridOffset.Y*DY;

    const FVector2D fOrigin((FX+OX)*UnitScale, (FY+OY)*UnitScale);

    TArray<FIntPoint> SortedPoints(
        InPoints.FilterByPredicate([&](const FIntPoint& Point)
        {
            return (
                Point.X >= OX && Point.X < OX+DX &&
                Point.Y >= OY && Point.Y < OY+DY
                );
        } ) );

    // Filter does not return any points, abort
    if (SortedPoints.Num() < 1)
    {
        return;
    }

    // Sort points
    SortedPoints.Sort([](const FIntPoint& P0, const FIntPoint& P1)
    {
        return (P0.Y < P1.Y)
            ? true
            : ((P0.Y == P1.Y) ? (P0.X < P1.X) : false);
    } );

    // Vertex index map
    TArray<uint32> VertexIndices;

    // Invalidate indices
    VertexIndices.SetNumUninitialized(GridVertexCount);
	FMemory::Memset(VertexIndices.GetData(), 0xFF, VertexIndices.Num()*VertexIndices.GetTypeSize());

    bool bSkipFirstPoint = false;

    if ((SortedPoints[0].X == OX) && (SortedPoints[0].Y == OY))
    {
        uint32 pi[4];
        pi[0] = VertexIndices[0   ] = Positions.Emplace(fOrigin+NW, 0);
        pi[1] = VertexIndices[1   ] = Positions.Emplace(fOrigin+NE, 0);
        pi[2] = VertexIndices[BX  ] = Positions.Emplace(fOrigin+SW, 0);
        pi[3] = VertexIndices[BX+1] = Positions.Emplace(fOrigin+SE, 0);

        Indices.Emplace(pi[0]); //.Emplace(vi);
        Indices.Emplace(pi[2]); //.Emplace(vi + BX);
        Indices.Emplace(pi[1]); //.Emplace(vi + 1);

        Indices.Emplace(pi[1]); //.Emplace(vi + 1);
        Indices.Emplace(pi[2]); //.Emplace(vi + BX);
        Indices.Emplace(pi[3]); //.Emplace(vi + BX + 1);

        bSkipFirstPoint = true;
    }

    for (int32 i=bSkipFirstPoint?1:0; i<SortedPoints.Num(); ++i)
    {
        const FIntPoint& Point(SortedPoints[i]);
        bool bFirstX = (Point.X == OX);
        bool bFirstY = (Point.Y == OY);
        int32 Index = (Point.X-OX) + (Point.Y-OY)*BX;

        FVector2D fPoint((FX+Point.X)*UnitScale, (FY+Point.Y)*UnitScale);

        uint32 pi[4];

        if (bFirstY)
        {
            pi[0] = VertexIndices[Index   ] = (VertexIndices[Index   ] == ~0U) ? Positions.Emplace(fPoint+NW, 0) : VertexIndices[Index   ];
            pi[1] = VertexIndices[Index+1 ] =                                    Positions.Emplace(fPoint+NE, 0);
            pi[2] = VertexIndices[Index+BX] = (VertexIndices[Index+BX] == ~0U) ? Positions.Emplace(fPoint+SW, 0) : VertexIndices[Index+BX];
        }
        else
        if (bFirstX)
        {
            pi[0] = VertexIndices[Index   ] = (VertexIndices[Index   ] == ~0U) ? Positions.Emplace(fPoint+NW, 0) : VertexIndices[Index   ];
            pi[1] = VertexIndices[Index+1 ] = (VertexIndices[Index+1 ] == ~0U) ? Positions.Emplace(fPoint+NE, 0) : VertexIndices[Index+1 ];
            pi[2] = VertexIndices[Index+BX] =                                    Positions.Emplace(fPoint+SW, 0);
        }
        else
        {
            pi[0] = VertexIndices[Index   ] = (VertexIndices[Index   ] == ~0U) ? Positions.Emplace(fPoint+NW, 0) : VertexIndices[Index   ];
            pi[1] = VertexIndices[Index+1 ] = (VertexIndices[Index+1 ] == ~0U) ? Positions.Emplace(fPoint+NE, 0) : VertexIndices[Index+1 ];
            pi[2] = VertexIndices[Index+BX] = (VertexIndices[Index+BX] == ~0U) ? Positions.Emplace(fPoint+SW, 0) : VertexIndices[Index+BX];
        }

        pi[3] = VertexIndices[Index+BX+1] = Positions.Emplace(fPoint+SE, 0);

        //UE_LOG(LogTemp,Warning, TEXT("%d %d %d %d"), pi[0], pi[1], pi[2], pi[3]);

        Indices.Emplace(pi[0]); //.Emplace(vi);
        Indices.Emplace(pi[2]); //.Emplace(vi + BX);
        Indices.Emplace(pi[1]); //.Emplace(vi + 1);

        Indices.Emplace(pi[1]); //.Emplace(vi + 1);
        Indices.Emplace(pi[2]); //.Emplace(vi + BX);
        Indices.Emplace(pi[3]); //.Emplace(vi + BX + 1);
    }

    Bounds += FVector(fOrigin, 0.f);
    Bounds += FVector(fOrigin.X+DX*UnitScale, fOrigin.Y+DY*UnitScale, 0.f);

    //UE_LOG(LogTemp,Warning, TEXT("Bounds: %s"), *Bounds.ToString());
}

void UPMUMeshGridUtility::GenerateGridVertices(
    TArray<FVector>& OutPositions,
    const FIntPoint& GridOrigin,
    int32 DimensionX,
    int32 DimensionY,
    float UnitScale,
    float ZOffset,
    TFunction<void(int32, int32, int32)> GenerateVertexCallback
    )
{
    if (DimensionX < 1 || DimensionY < 1)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshGridUtility::GenerateGrids() ABORTED, INVALID DIMENSION"));
        return;
    }
    else
    if (UnitScale < KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshGridUtility::GenerateGrids() ABORTED, INVALID UNIT SCALE"));
        return;
    }

    int32 DX = DimensionX;
    int32 DY = DimensionY;
    int32 BX = DX+1;
    int32 BY = DY+1;
    float fDX = static_cast<float>(DX);
    float fDY = static_cast<float>(DY);
    FVector2D fOffset(GridOrigin.X*DX*UnitScale, GridOrigin.Y*DY*UnitScale);

    int32 VertexOffset = OutPositions.Num();
    int32 GridVertexCount = BX*BY;

    OutPositions.AddUninitialized(GridVertexCount);

    for (int32 y=0, i=VertexOffset; y<BY; ++y)
    for (int32 x=0                ; x<BX; ++x, ++i)
    {
        float fx = fOffset.X + static_cast<float>(x)*UnitScale;
        float fy = fOffset.Y + static_cast<float>(y)*UnitScale;

        OutPositions[i] = FVector(fx, fy, ZOffset);

        if (GenerateVertexCallback)
        {
            GenerateVertexCallback(x, y, i);
        }
    }
}

void UPMUMeshGridUtility::GenerateGridGeometryByPoints(
    TArray<FVector>& OutPositions,
    TArray<uint32>& OutIndices,
    const TArray<FIntPoint>& InPoints,
    FIntPoint GridOrigin,
    FIntPoint GridOffset,
    int32 DimensionX,
    int32 DimensionY,
    float UnitScale,
    float ZOffset,
    bool bRequireSorting,
    TFunction<void(int32, int32, int32)> GenerateVertexCallback
    )
{
    if (DimensionX < 2 || DimensionY < 2)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshGridUtility::GenerateGridByPoints() ABORTED, INVALID DIMENSION"));
        return;
    }
    else
    if (UnitScale < KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUMeshGridUtility::GenerateGridByPoints() ABORTED, INVALID UNIT SCALE"));
        return;
    }

    // No points specified, abort
    if (InPoints.Num() < 1)
    {
        return;
    }

    const int32 DX = DimensionX;
    const int32 DY = DimensionY;
    const int32 BX = DX+1;
    const int32 BY = DY+1;
    const float fDX = static_cast<float>(DX);
    const float fDY = static_cast<float>(DY);

    const FVector2D NW(0        , 0        );
    const FVector2D NE(UnitScale, 0        );
    const FVector2D SW(0        , UnitScale);
    const FVector2D SE(UnitScale, UnitScale);

    const int32 OX = GridOrigin.X*DX;
    const int32 OY = GridOrigin.Y*DY;

    const int32 FX = GridOffset.X*DX;
    const int32 FY = GridOffset.Y*DY;

    const int32 GridVertexCount = BX*BY;

    const TArray<FIntPoint>* SortedPointsPtr;
    TArray<FIntPoint> SortedPointsTmp;

    // Sort input points if required
    if (bRequireSorting)
    {
        // Filter out-of-bounds points
        SortedPointsTmp = InPoints.FilterByPredicate(
        [&](const FIntPoint& Point)
        {
            return (
                Point.X >= OX && Point.X < OX+DX &&
                Point.Y >= OY && Point.Y < OY+DY
                );
        } );

        // Sort points
        SortedPointsTmp.Sort([](const FIntPoint& P0, const FIntPoint& P1)
        {
            return (P0.Y < P1.Y)
                ? true
                : ((P0.Y == P1.Y) ? (P0.X < P1.X) : false);
        } );

        SortedPointsPtr = &SortedPointsTmp;
    }
    else
    {
        SortedPointsPtr = &InPoints;
    }

    const TArray<FIntPoint>& SortedPoints(*SortedPointsPtr);

    // Filtered and sorted points does not return any points, abort
    if (SortedPoints.Num() < 1)
    {
        return;
    }

    // Vertex index map
    TArray<uint32> VertexIndices;

    // Invalidate indices
    VertexIndices.SetNumUninitialized(GridVertexCount);
	FMemory::Memset(VertexIndices.GetData(), 0xFF, VertexIndices.Num()*VertexIndices.GetTypeSize());

    // Reserve geometry container
    OutPositions.Reserve(OutPositions.Num()+GridVertexCount);
    OutIndices.Reserve(OutIndices.Num()+DX*DY*6);

    // Create add vertex callback
    TFunctionRef<uint32(int32, int32, FVector)> AddVertex(
    [BX, &VertexIndices, &OutPositions, &GenerateVertexCallback](int32 X, int32 Y, FVector Position)
    {
        const int32 i = X + Y*BX;

        if (VertexIndices[i] == ~0U)
        {
            VertexIndices[i] = OutPositions.Emplace(Position);

            if (GenerateVertexCallback)
            {
                GenerateVertexCallback(X, Y, i);
            }
        }

        return VertexIndices[i];
    } );

    // Generate grid geometry
    for (int32 i=0; i<SortedPoints.Num(); ++i)
    {
        const FIntPoint& Point(SortedPoints[i]);
        int32 PX = Point.X;
        int32 PY = Point.Y;
        FVector2D fPoint((FX+PX)*UnitScale, (FY+PY)*UnitScale);

        uint32 pi[4];
        pi[0] = AddVertex(PX  , PY  , FVector(fPoint+NW, ZOffset));
        pi[1] = AddVertex(PX+1, PY  , FVector(fPoint+NE, ZOffset));
        pi[2] = AddVertex(PX  , PY+1, FVector(fPoint+SW, ZOffset));
        pi[3] = AddVertex(PX+1, PY+1, FVector(fPoint+SE, ZOffset));

        OutIndices.Emplace(pi[0]);
        OutIndices.Emplace(pi[2]);
        OutIndices.Emplace(pi[1]);

        OutIndices.Emplace(pi[1]);
        OutIndices.Emplace(pi[2]);
        OutIndices.Emplace(pi[3]);
    }

    // Shrink geometry container
    OutPositions.Shrink();
    OutIndices.Shrink();
}
