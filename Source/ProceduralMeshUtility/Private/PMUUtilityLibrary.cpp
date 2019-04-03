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

#include "PMUUtilityLibrary.h"

#include "Shaders/PMUShaderParameters.h"

#include "EarcutTypes.h"
#include "GWTAsyncTypes.h"

#ifdef PMU_SUBSTANCE_ENABLED
#include "SubstanceImageInput.h"
#include "SubstanceUtility.h"
#endif

// Utility Library

void UPMUUtilityLibrary::FindBorders(TSet<int32>& BorderIndices, const FPointMask& GridMasks, int32 SizeX, int32 SizeY, const uint8 MaskThreshold, const int32 Iteration)
{
    const int32 Size = SizeX * SizeY;
    const int32 SizeInnerLo = SizeX+1;
    const int32 SizeInnerHi = Size-SizeInnerLo;
    const int32 MaskThresholdLo = MaskThreshold + 1;
    const int32 MaskThresholdHi = MaskThreshold;

    if (Iteration < 1 || SizeX != SizeY || Size <= 0 || GridMasks.Num() != Size)
    {
        return;
    }

    const FIntPoint Offsets[8] = {
        { -1,  0 }, // W
        { -1, -1 }, // NW
        {  0, -1 }, // N
        {  1, -1 }, // NE
        {  1,  0 }, // E
        {  1,  1 }, // SE
        {  0,  1 }, // S
        { -1,  1 }  // SW
        };

    const int32 BorderOffsets[8] = {
        Offsets[0].X + Offsets[0].Y*SizeX,
        Offsets[1].X + Offsets[1].Y*SizeX,
        Offsets[2].X + Offsets[2].Y*SizeX,
        Offsets[3].X + Offsets[3].Y*SizeX,
        Offsets[4].X + Offsets[4].Y*SizeX,
        Offsets[5].X + Offsets[5].Y*SizeX,
        Offsets[6].X + Offsets[6].Y*SizeX,
        Offsets[7].X + Offsets[7].Y*SizeX
        };

    TSet<int32> IndexSet0;
    IndexSet0.Reserve(Size);

    // Gather border indices
    for (int32 y=0, i=0; y<SizeY; ++y)
    for (int32 x=0; x<SizeX; ++x, ++i)
    {
        if (GridMasks[i] > MaskThreshold)
        {
            bool bIsBorder = false;

            if (i > SizeInnerLo && i < SizeInnerHi)
            {
                if (GridMasks[i+BorderOffsets[0]] < MaskThresholdLo ||
                    GridMasks[i+BorderOffsets[1]] < MaskThresholdLo ||
                    GridMasks[i+BorderOffsets[2]] < MaskThresholdLo ||
                    GridMasks[i+BorderOffsets[3]] < MaskThresholdLo ||
                    GridMasks[i+BorderOffsets[4]] < MaskThresholdLo ||
                    GridMasks[i+BorderOffsets[5]] < MaskThresholdLo ||
                    GridMasks[i+BorderOffsets[6]] < MaskThresholdLo ||
                    GridMasks[i+BorderOffsets[7]] < MaskThresholdLo)
                {
                    bIsBorder = true;
                }
            }
            else
            {
                bIsBorder = true;
            }

            if (bIsBorder)
            {
                IndexSet0.Emplace(i);
            }
        }
    }

    for (int32 It=1; It<Iteration; ++It)
    {
        TSet<int32> IndexSet1 = IndexSet0;

        for (int32 BorderIndex : IndexSet1)
        {
            for (int32 Dir=0; Dir<8; ++Dir)
            {
                const int32 Index = BorderIndex+BorderOffsets[Dir];

                if (Index > SizeInnerLo && Index < SizeInnerHi)
                {
                    if (GridMasks[Index] > MaskThreshold && ! IndexSet0.Contains(Index))
                    {
                        IndexSet0.Emplace(Index);
                    }
                }
            }
        }
    }

    BorderIndices = MoveTemp(IndexSet0);
}

TArray<int32> UPMUUtilityLibrary::FindGridIslands(TArray<TDoubleLinkedList<int32>>& BorderPaths, const FPointMask& GridMasks, int32 SizeX, int32 SizeY, const uint8 MaskThreshold, int32 MaxIslandCount, bool bFilterStrands)
{
    const int32 Size = SizeX * SizeY;
    FIndices FilteredIndices;

    if (SizeX != SizeY || Size <= 0 || GridMasks.Num() != Size)
    {
        return FilteredIndices;
    }

    enum { NEIGHBOUR_NUM = 4 };
    enum { NEIGHBOUR_SEARCH_START = (NEIGHBOUR_NUM/2) + 1 };

    const FIntPoint SizeRangeLo(0, 0);
    const FIntPoint SizeRangeHi(SizeX-1, SizeY-1);
    const int32 SizeInnerLo = SizeX+1;
    const int32 SizeInnerHi = Size-SizeInnerLo;
    const int32 MaskThresholdLo = MaskThreshold + 1;
    const int32 MaskThresholdHi = MaskThreshold;

    const FIntPoint Offsets[8] = {
        { -1,  0 }, // W
        { -1, -1 }, // NW
        {  0, -1 }, // N
        {  1, -1 }, // NE
        {  1,  0 }, // E
        {  1,  1 }, // SE
        {  0,  1 }, // S
        { -1,  1 }  // SW
        };

    const FIntPoint NeighbourOffsets[4] = {
        Offsets[0],
        Offsets[2],
        Offsets[4],
        Offsets[6]
        };

    const int32 BorderOffsets[8] = {
        Offsets[0].X + Offsets[0].Y*SizeX,
        Offsets[1].X + Offsets[1].Y*SizeX,
        Offsets[2].X + Offsets[2].Y*SizeX,
        Offsets[3].X + Offsets[3].Y*SizeX,
        Offsets[4].X + Offsets[4].Y*SizeX,
        Offsets[5].X + Offsets[5].Y*SizeX,
        Offsets[6].X + Offsets[6].Y*SizeX,
        Offsets[7].X + Offsets[7].Y*SizeX
        };

    TSet<int32> BorderIndexSetRaw;
    BorderIndexSetRaw.Reserve(Size);

    // Gather border indices

    for (int32 y=0, i=0; y<SizeY; ++y)
    for (int32 x=0; x<SizeX; ++x, ++i)
    {
        if (GridMasks[i] > MaskThreshold)
        {
            bool bIsBorder = false;

            if (i > SizeInnerLo && i < SizeInnerHi)
            {
                if (GridMasks[i+BorderOffsets[0]] < MaskThresholdLo ||
                    GridMasks[i+BorderOffsets[1]] < MaskThresholdLo ||
                    GridMasks[i+BorderOffsets[2]] < MaskThresholdLo ||
                    GridMasks[i+BorderOffsets[3]] < MaskThresholdLo ||
                    GridMasks[i+BorderOffsets[4]] < MaskThresholdLo ||
                    GridMasks[i+BorderOffsets[5]] < MaskThresholdLo ||
                    GridMasks[i+BorderOffsets[6]] < MaskThresholdLo ||
                    GridMasks[i+BorderOffsets[7]] < MaskThresholdLo)
                {
                    bIsBorder = true;
                }
            }
            else
            {
                bIsBorder = true;
            }

            if (bIsBorder)
            {
                BorderIndexSetRaw.Emplace(i);
            }
        }
    }

    // Filter strand indices

    TSet<int32> BorderIndexSet;
    BorderIndexSet.Reserve(BorderIndexSetRaw.Num());

    if (bFilterStrands)
    {
        for (int32 i : BorderIndexSetRaw)
        {
            // Only include texel with neighbouring non-border solid texel
            for (int32 ni=0; ni<8; ++i)
            {
                int32 idx = i+BorderOffsets[ni];

                if (idx >= 0 && idx < Size)
                {
                    if (! BorderIndexSetRaw.Contains(idx) && GridMasks[idx] > MaskThresholdHi)
                    {
                        BorderIndexSet.Emplace(i);
                        break;
                    }
                }
            }
        }

        BorderIndexSet.Shrink();

        FilteredIndices = BorderIndexSetRaw.Difference(BorderIndexSet).Array();
    }
    else
    {
        BorderIndexSet = MoveTemp(BorderIndexSetRaw);
    }

    // Empty border set, abort
    if (BorderIndexSet.Num() < 2)
    {
        return MoveTemp(FilteredIndices);
    }

    const int32 IterationCount = FMath::Abs(MaxIslandCount);

    for (int32 Iteration=0; Iteration<IterationCount; ++Iteration)
    {
        BorderPaths.SetNum(Iteration+1, false);

        TDoubleLinkedList<int32>& List(BorderPaths[Iteration]);
        List.AddTail(*BorderIndexSet.CreateIterator());

        int32 SearchIndex = List.GetTail()->GetValue();
        int32 SearchStart = 0;
        bool bNeighbourFound = false;

        check(BorderIndexSet.Contains(SearchIndex));
        BorderIndexSet.Remove(SearchIndex);

        do
        {
            bNeighbourFound = false;

            for (int32 i=0; i<NEIGHBOUR_NUM; ++i)
            {
                int32 NeighbourId = SearchStart + i;
                int32 NeighbourWrapId = NeighbourId % NEIGHBOUR_NUM;

                const FIntPoint& NeighbourOffset(NeighbourOffsets[NeighbourWrapId]);
                int32 NeighbourX = (SearchIndex % SizeX) + NeighbourOffset.X;
                int32 NeighbourY = (SearchIndex / SizeX) + NeighbourOffset.Y;
                int32 NeighbourIndex = NeighbourX + NeighbourY * SizeX;

                // Make sure neighbour position is within grid dimension
                if (NeighbourX < SizeRangeLo.X || NeighbourX > SizeRangeHi.X || NeighbourY < SizeRangeLo.Y || NeighbourY > SizeRangeHi.Y)
                {
                    continue;
                }

                if (BorderIndexSet.Contains(NeighbourIndex))
                {
                    BorderIndexSet.Remove(NeighbourIndex);
                    List.AddTail(NeighbourIndex);

                    bNeighbourFound = true;
                    SearchIndex = NeighbourIndex;

                    // Search start direction for the next iteration is the direction pointing
                    // from the neighbour cell towards the current cell + 1
                    SearchStart = (NeighbourId + NEIGHBOUR_SEARCH_START) % NEIGHBOUR_NUM;

                    break;
                }
            }
        }
        while (bNeighbourFound);

        // No index left in the index set, break
        if (BorderIndexSet.Num() <= 0)
        {
            break;
        }
    }

    return MoveTemp(FilteredIndices);
}

TArray<int32> UPMUUtilityLibrary::FindGridIslands(FIndexContainer& IndexContainer, const FPointMask& GridMasks, int32 SizeX, int32 SizeY, const uint8 MaskThreshold, int32 MaxIslandCount, bool bFilterStrands)
{
    TArray<TDoubleLinkedList<int32>> BorderPaths;
    FIndices FilteredIndices = FindGridIslands(BorderPaths, GridMasks, SizeX, SizeY, MaskThreshold, MaxIslandCount, bFilterStrands);

    for (int32 IslandIndex=0; IslandIndex<BorderPaths.Num(); ++IslandIndex)
    {
        const TDoubleLinkedList<int32>& List(BorderPaths[IslandIndex]);

        if (List.Num() > 0)
        {
            int32 ContainerIndex = IndexContainer.Num();
            IndexContainer.SetNum(ContainerIndex+1);

            FIndices& Indices(IndexContainer[ContainerIndex]);
            Indices.Reserve(List.Num());

            for (int32 i : List)
            {
                Indices.Emplace(i);
            }
        }
    }

    return MoveTemp(FilteredIndices);
}

void UPMUUtilityLibrary::PolyClip(const TArray<FVector2D>& Points, TArray<int32>& OutIndices, bool bInversed)
{
    FECPolygon Polygon({ Points });
    FECUtils::Earcut(Polygon, OutIndices, bInversed);
}

void UPMUUtilityLibrary::PolyClipGroups(const TArray<FPMUPoints>& PolyGroups, TArray<int32>& OutIndices, FPMUPoints& OutPoints, bool bCombinePoints, bool bInversed)
{
    FECPolygon Polygon;
    int32 PointCount = 0;

    for (const FPMUPoints& Poly : PolyGroups)
    {
        Polygon.Data.Emplace(Poly.Data);
        PointCount += Poly.Data.Num();
    }

    if (bCombinePoints)
    {
        OutPoints.Data.Reset(PointCount);

        for (const FPMUPoints& Poly : PolyGroups)
        {
            OutPoints.Data.Append(Poly.Data);
        }
    }

    FECUtils::Earcut(Polygon, OutIndices, bInversed);
}

TArray<int32> UPMUUtilityLibrary::MaskPoints(const TArray<FVector2D>& Points, const TArray<uint8>& Mask, const int32 SizeX, const int32 SizeY, const uint8 Threshold)
{
    TArray<int32> Indices;

    // Invlaid dimension, abort
    if (SizeX <= 0 || SizeY <= 0 || Mask.Num() < (SizeX*SizeY))
    {
        return Indices;
    }

    for (int32 i=0; i<Points.Num(); ++i)
    {
        const int32 X = Points[i].X;
        const int32 Y = Points[i].Y;

        if (X >= 0 && X < SizeX && Y >= 0 && Y < SizeY)
        {
            if (Mask[X + Y*SizeX] >= Threshold)
            {
                Indices.Emplace(i);
            }
        }
    }

    return Indices;
}

void UPMUUtilityLibrary::GeneratePolyCircle(TArray<FVector2D>& Points, const FVector2D& Center, float Radius, int32 SegmentCount)
{
    // Need at least 4 segments
    SegmentCount = FMath::Max(4, SegmentCount);
    const float AngleStep = 2.f * PI / float(SegmentCount);

    Points.Empty(SegmentCount);

    float Angle = 0.f;
    while (SegmentCount--)
    {
        Points.Emplace(Center + Radius * FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)));
        Angle += AngleStep;
    }
}

void UPMUUtilityLibrary::CollapseAccuteAngles(TArray<FVector2D>& OutPoints, const TArray<FVector2D>& Points, float Threshold)
{
    if (Points.Num() <= 3)
    {
        OutPoints = Points;
        return;
    }

    if (Threshold > 1.f)
    {
        OutPoints = Points;
        return;
    }

    typedef TDoubleLinkedList<FVector2D>      FPointList;
    typedef FPointList::TDoubleLinkedListNode FPointNode;

    FPointList PointList;

    for (int32 i=0; i<Points.Num(); ++i)
    {
        PointList.AddTail(Points[i]);
    }

    FPointNode* NH = PointList.GetHead();
    FPointNode* NT = PointList.GetTail();

    FPointNode* N0 = NH;
    FPointNode* N1 = nullptr;
    FPointNode* N2 = nullptr;

    for (int32 i=0; i<Points.Num(); ++i)
    {
        N1 = N0->GetNextNode();
        N2 = N1->GetNextNode();

        check(N1 != nullptr);
        check(N2 != nullptr);

        // Reached the last point angle, break
        if (N2 == NT)
        {
            break;
        }

        const FVector2D& P0(N0->GetValue());
        const FVector2D& P1(N1->GetValue());
        const FVector2D& P2(N2->GetValue());

        const FVector2D D0 = (P1-P0).GetSafeNormal();
        const FVector2D D1 = (P2-P1).GetSafeNormal();
        const float DotAngle = D0 | D1;

        // Remove P1 if dot angle is below threshold
        if (DotAngle < Threshold)
        {
            PointList.RemoveNode(N1);

            // Start the next iteration from the previous node unless we're on the first node
            if (N0 != NH)
            {
                N0 = N0->GetPrevNode();
            }
        }
        // Otherwise, iterate the point list
        else
        {
            N0 = N1;
        }
    }

    // Write output

    OutPoints.Reserve(PointList.Num());

    N0 = NH;

    while (N0 != nullptr)
    {
        OutPoints.Emplace(N0->GetValue());
        N0 = N0->GetNextNode();
    }
}

void UPMUUtilityLibrary::AssignInstancesAlongPoly(int32 Seed, const TArray<FVector>& Points, const TArray<FVector>& InstanceDimensions, float HeightOffsetMin, float HeightOffsetMax, TArray<int32>& InstanceIds, TArray<FVector>& Positions, TArray<FVector>& Directions)
{
    if (Points.Num() < 3)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityLibrary::AssignInstancesAlongPoly() ABORTED, POINTS < 3"));
        return;
    }

    if (InstanceDimensions.Num() < 1)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityLibrary::AssignInstancesAlongPoly() ABORTED, EMPTY INSTANCE DIMENSIONS"));
        return;
    }

    struct FInstanceGeom
    {
        int32 Index;
        FVector Dimension;
    };

    TArray<FInstanceGeom> Instances;

    for (int32 i=0; i<InstanceDimensions.Num(); ++i)
    {
        FVector Dimension = InstanceDimensions[i];

        if (Dimension.X < KINDA_SMALL_NUMBER ||
            Dimension.Y < KINDA_SMALL_NUMBER ||
            Dimension.Z < KINDA_SMALL_NUMBER
            )
        {
            continue;
        }

        FInstanceGeom Instance({ i, Dimension });
        Instances.Emplace(Instance);
    }

    if (Instances.Num() < 1)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityLibrary::AssignInstancesAlongPoly() ABORTED, NO VALID INSTANCE DIMENSION SPECIFIED"));
        return;
    }

    // Sort instances

    Instances.Sort(
        [&](const auto& A, const auto& B)
        {
            return A.Dimension.X < B.Dimension.X;
        } );

    FRandomStream Rand(Seed);
    const int32 InstanceCount = Instances.Num();
    const int32 PointCount = Points.Num();
    float DistAlongLine = 0.f;

    float ZOffsetMin = FMath::Min(HeightOffsetMin, HeightOffsetMax);
    float ZOffsetMax = FMath::Max(HeightOffsetMin, HeightOffsetMax);
    bool bUseRandomZOffset = ! FMath::IsNearlyEqual(ZOffsetMin, ZOffsetMax);

    for (int32 i=0; i<PointCount; ++i)
    {
        const int32 i0 = i;
        const int32 i1 = (i+1) % PointCount;
        const int32 i2 = (i+2) % PointCount;

        const FVector& P0(Points[i0]);
        const FVector& P1(Points[i1]);
        const FVector& P2(Points[i2]);

        const FVector PD = P1-P0;
        const FVector PT = PD.GetSafeNormal2D();
        const float LineDist = PD.Size();

        if (LineDist < KINDA_SMALL_NUMBER)
        {
            continue;
        }

        UE_LOG(UntPMU,Warning, TEXT("DistAlongLine: %f"), DistAlongLine);

        while (DistAlongLine < LineDist)
        {
            FVector PAlongLine = P0 + PD * (DistAlongLine / LineDist);

            if (bUseRandomZOffset)
            {
                PAlongLine.Z = Rand.FRandRange(ZOffsetMin, ZOffsetMax);
            }
            else
            {
                PAlongLine.Z = ZOffsetMin;
            }

            int32 InstanceIdx = (InstanceCount-1);
            float InstanceLength = 0.f;

            for (; InstanceIdx>=0; --InstanceIdx)
            {
                const float l = Instances[InstanceIdx].Dimension.X;
                const float t = l + DistAlongLine;

                if (t < LineDist)
                {
                    break;
                }
            }

            InstanceIdx = Rand.RandHelper(InstanceIdx+1);
            InstanceLength = Instances[InstanceIdx].Dimension.X;

            // Assign new instance

            InstanceIds.Emplace(Instances[InstanceIdx].Index);
            Positions.Emplace(PAlongLine);
            Directions.Emplace(PT);

            DistAlongLine += InstanceLength;
        }

        check(Positions.Num() > 0);

        if (i1 > 0)
        {
            // Compute segment intersection of the next line with the last instance bounds
            // to find T distance along the next line

            const FVector& LastDimension(InstanceDimensions[InstanceIds.Last()]);
            const float LastExtentX = LastDimension.X * .5f;
            const float LastExtentY = LastDimension.Y * .5f;
            const float LastDistAlongLine = DistAlongLine - LastExtentX;

            const FVector NextPT = (P2-P1).GetSafeNormal2D();
            const FVector NextPN(-NextPT.Y, NextPT.X, NextPT.Z);

            FVector IPT = PT;

            // If last instance extent went over half of the line end,
            // Replace last direction with the average of current and next line
            //if (LastDistAlongLine > LineDist)
            {
                IPT = (PT+NextPT).GetSafeNormal();
                Directions.Last() = IPT;
            }

            const FVector IPN(-IPT.Y, IPT.X, IPT.Z);

            const FVector IP0 = Positions.Last();
            const FVector IP1 = IP0 + IPT * LastDimension.X;
     
            const FVector ISegmentA0 = IP1 - IPN*LastExtentY;
            const FVector ISegmentA1 = ISegmentA0 + IPN*LastDimension.Y;

            const FVector ISegmentB0 = IP0 - IPN*LastExtentY;
            const FVector ISegmentB1 = ISegmentB0 + IPT*LastDimension.X;

            const FVector ISegmentC0 = IP0 + IPN*LastExtentY;
            const FVector ISegmentC1 = ISegmentC0 + IPT*LastDimension.X;

            float IntersectionT = 0.f;

            FVector Intersection;

            if (FMath::SegmentIntersection2D(P1, P2, ISegmentA0, ISegmentA1, Intersection))
            {
                float T = (Intersection-P1).Size();
                IntersectionT = T;

                // Pull T slightly towards line start if intersection point
                // is to close to the end of the test segment

                float DistAtSegment = (Intersection - ISegmentA0).Size();

                if (DistAtSegment < (LastDimension.X*.15f))
                {
                    FVector Projection = FVector::PointPlaneProject((ISegmentA0 + IPN*LastDimension.Y*.15f), P1, NextPN);
                    IntersectionT = (Projection-P1).Size();
                }
                else
                if (DistAtSegment > (LastDimension.X*.85f))
                {
                    FVector Projection = FVector::PointPlaneProject((ISegmentA0 + IPN*LastDimension.Y*.85f), P1, NextPN);
                    IntersectionT = (Projection-P1).Size();
                }
            }

            if (FMath::SegmentIntersection2D(P1, P2, ISegmentB0, ISegmentB1, Intersection))
            {
                float T = (Intersection-P1).Size();

                if (T > IntersectionT)
                {
                    IntersectionT = T;

                    // Pull T slightly towards line start if intersection point
                    // is to close to the end of the test segment

                    float DistAtSegment = (Intersection - ISegmentB0).Size();

                    if (DistAtSegment > (LastDimension.X*.85f))
                    {
                        //IntersectionT -= .1f;
                        FVector Projection = FVector::PointPlaneProject((ISegmentB0 + IPT*LastDimension.X*.85f), P1, NextPN);
                        IntersectionT = (Projection-P1).Size();
                    }
                }
            }
            else
            if (FMath::SegmentIntersection2D(P1, P2, ISegmentC0, ISegmentC1, Intersection))
            {
                float T = (Intersection-P1).Size();

                if (T > IntersectionT)
                {
                    IntersectionT = T;

                    // Pull T slightly towards line start if intersection point
                    // is to close to the end of the test segment

                    float DistAtSegment = (Intersection - ISegmentC0).Size();

                    if (DistAtSegment > (LastDimension.X*.85f))
                    {
                        //IntersectionT -= .1f;
                        FVector Projection = FVector::PointPlaneProject((ISegmentC0 + IPT*LastDimension.X*.85f), P1, NextPN);
                        IntersectionT = (Projection-P1).Size();
                    }
                }
            }

            DistAlongLine = IntersectionT;
        }
        else
        if (i1 == 0 && InstanceIds.Num() > 1)
        {
            // Remove last instance if its extent went past the last line

            FVector LastDimension = InstanceDimensions[InstanceIds.Last()];
            float LastExtentX = LastDimension.X * .5f;
            float LastDistAlongLine = DistAlongLine - LastExtentX;

            if (LastDistAlongLine > LineDist)
            {
                // Find smaller instance to replace, if any

                int32 LastInstanceId = InstanceIds.Last();
                int32 InstanceIdx = 0;

                for (; InstanceIdx<InstanceCount; ++InstanceIdx)
                {
                    if (Instances[InstanceIdx].Index == LastInstanceId)
                    {
                        break;
                    }
                }

                check(InstanceIdx < InstanceCount);

                // Smaller instance found, replace last instance
                if (InstanceIdx > 0)
                {
                    // Find smaller instance that does not have extent that went past the last line
                    for (InstanceIdx -= 1; InstanceIdx>=0; --InstanceIdx)
                    {
                        int32 NewIndex = Instances[InstanceIdx].Index;
                        FVector NewDimension = InstanceDimensions[NewIndex];
                        float NewExtent = NewDimension.X * .5f;
                        float NewDistAlongLine = DistAlongLine - NewExtent;

                        if (NewDistAlongLine < LineDist)
                        {
                            break;
                        }
                    }

                    InstanceIdx = FMath::Max(0, InstanceIdx);
                    InstanceIds.Last() = Instances[InstanceIdx-1].Index;
                }
                // Otherwise, simply remove instance
                else
                {
                    InstanceIds.Pop();
                    Positions.Pop();
                    Directions.Pop();

                    UE_LOG(UntPMU,Warning, TEXT("REMOVE LAST"));
                }
            }
        }
    }

    check(InstanceIds.Num() == Positions.Num());
    check(InstanceIds.Num() == Directions.Num());
}

void UPMUUtilityLibrary::AssignInstancesAlongPolyAlt(int32 Seed, const TArray<FVector>& Points, const TArray<FVector>& InstanceDimensions, float HeightOffsetMin, float HeightOffsetMax, TArray<int32>& InstanceIds, TArray<FVector>& Positions, TArray<FVector>& Directions)
{
    if (Points.Num() < 3)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityLibrary::AssignInstancesAlongPolyAlt() ABORTED, POINTS < 3"));
        return;
    }

    if (InstanceDimensions.Num() < 1)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityLibrary::AssignInstancesAlongPolyAlt() ABORTED, EMPTY INSTANCE DIMENSIONS"));
        return;
    }

    struct FInstanceGeom
    {
        int32 Index;
        FVector Dimension;
    };

    TArray<FInstanceGeom> Instances;

    for (int32 i=0; i<InstanceDimensions.Num(); ++i)
    {
        FVector Dimension = InstanceDimensions[i];

        if (Dimension.X < KINDA_SMALL_NUMBER ||
            Dimension.Y < KINDA_SMALL_NUMBER ||
            Dimension.Z < KINDA_SMALL_NUMBER
            )
        {
            continue;
        }

        FInstanceGeom Instance({ i, Dimension });
        Instances.Emplace(Instance);
    }

    if (Instances.Num() < 1)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityLibrary::AssignInstancesAlongPolyAlt() ABORTED, NO VALID INSTANCE DIMENSION SPECIFIED"));
        return;
    }

    // Sort instances

    Instances.Sort(
        [&](const auto& A, const auto& B)
        {
            return A.Dimension.X < B.Dimension.X;
        } );

    FRandomStream Rand(Seed);
    const int32 InstanceCount = Instances.Num();
    const int32 PointCount = Points.Num();
    float DistAlongLine = 0.f;

    float ZOffsetMin = FMath::Min(HeightOffsetMin, HeightOffsetMax);
    float ZOffsetMax = FMath::Max(HeightOffsetMin, HeightOffsetMax);
    bool bUseRandomZOffset = ! FMath::IsNearlyEqual(ZOffsetMin, ZOffsetMax);

    for (int32 PointIndex=0; PointIndex<PointCount; ++PointIndex)
    {
        const int32 i0 = PointIndex;
        const int32 i1 = (PointIndex+1) % PointCount;

        const FVector& P0(Points[i0]);
        const FVector& P1(Points[i1]);

        const FVector PD = P1-P0;
        const FVector PT = PD.GetSafeNormal2D();
        const float LineDist = PD.Size();

        if (LineDist < KINDA_SMALL_NUMBER)
        {
            continue;
        }

        while (DistAlongLine < LineDist)
        {
            FVector PAlongLine = P0 + PD * (DistAlongLine / LineDist);

            if (bUseRandomZOffset)
            {
                PAlongLine.Z = Rand.FRandRange(ZOffsetMin, ZOffsetMax);
            }
            else
            {
                PAlongLine.Z = ZOffsetMin;
            }

            int32 InstanceIdx = 0;

            for (int32 i=1; i<InstanceCount; ++i)
            {
                float l = Instances[i].Dimension.X;
                float t = l + DistAlongLine;

                if (t > LineDist)
                {
                    break;
                }

                InstanceIdx = i;
            }

            float InstanceLength = Instances[InstanceIdx].Dimension.X;

            // Assign new instance

            InstanceIds.Emplace(Instances[InstanceIdx].Index);
            Positions.Emplace(PAlongLine);
            Directions.Emplace(PT);

            DistAlongLine += InstanceLength;
        }

        // Last instance properties

        int32 LastId = InstanceIds.Last();
        const FVector& LastDimension(InstanceDimensions[LastId]);
        float LastExtentX = LastDimension.X * 0.5;
        float LastExtentY = LastDimension.Y * 0.5;

        const FVector& LastPosition(Positions.Last());
        const FVector& LastDirection(Directions.Last());

        const FVector LP0 = LastPosition;
        const FVector LP1 = LP0 + LastDirection * LastDimension.X;
        const FVector LExtent = FVector(-PT.Y, PT.X, 0.f) * LastExtentY;

        // Last instance segment points:
        //
        // B--P1--C
        // |      |
        // |  ^^  |
        // |  ||  |
        // |  ||  |
        // |  ||  |
        // |  ||  |
        // |      |
        // A--P0--D

        const FVector SPA = LP0 - LExtent;
        const FVector SPB = LP1 - LExtent;
        const FVector SPC = LP1 + LExtent;
        const FVector SPD = LP0 + LExtent;

        // Intersect last instance boundary points with next point lines
        // to find the next distance along lines

        int32 pni0 = i1;
        int32 pni1;
        float InstanceIntersectDist = -1.0;

        for (; pni0<PointCount; ++pni0)
        {
            pni1 = (pni0+1) % PointCount;

            const FVector& PNext0(Points[pni0]);
            const FVector& PNext1(Points[pni1]);
            bool bHasIntersection = false;
            FVector Intersection;

            // Intersect Segment BC
            if (FMath::SegmentIntersection2D(SPB, SPC, PNext0, PNext1, Intersection))
            {
                bHasIntersection = true;
            }
            // Intersect Segment AB
            else
            if (FMath::SegmentIntersection2D(SPA, SPB, PNext0, PNext1, Intersection))
            {
                bHasIntersection = true;
            }
            // Intersect Segment DC
            else
            if (FMath::SegmentIntersection2D(SPD, SPC, PNext0, PNext1, Intersection))
            {
                bHasIntersection = true;
            }
            // Intersect Segment AD
            else
            if (FMath::SegmentIntersection2D(SPA, SPD, PNext0, PNext1, Intersection))
            {
                bHasIntersection = true;
            }

            // Intersection found, break loop
            if (bHasIntersection)
            {
                InstanceIntersectDist = (Intersection-PNext0).Size();
                break;
            }
        }

        PointIndex = pni0-1;
        DistAlongLine = FMath::Max(0.f, InstanceIntersectDist);

        // Prevents cyclic loop
        if (PointIndex < i0)
        {
            break;
        }
    }

    check(InstanceIds.Num() == Positions.Num());
    check(InstanceIds.Num() == Directions.Num());
}

UObject* UPMUUtilityLibrary::CreateSubstanceImageInputFromTexture2D(UObject* WorldContextObject, UTexture2D* SrcTexture, int32 MipLevel, FString InstanceName)
{
#ifdef PMU_SUBSTANCE_ENABLED

    if (! SrcTexture || ! SrcTexture->PlatformData || ! SrcTexture->PlatformData->Mips.IsValidIndex(MipLevel))
    {
        return nullptr;
    }

    FTexturePlatformData& PlatformData(*SrcTexture->PlatformData);
    FTexture2DMipMap& Mip(PlatformData.Mips[MipLevel]);
    int32 TextureBPP = -1;

    // Make sure texture format is supported
    switch (PlatformData.PixelFormat)
    {
        case EPixelFormat::PF_B8G8R8A8:
        {
            TextureBPP = 4;
        }
        break;
    }

    if (TextureBPP < 4)
    {
        return nullptr;
    }

    UObject* Outer = WorldContextObject ? WorldContextObject : GetTransientPackage();
    USubstanceImageInput* SubstanceImage = NewObject<USubstanceImageInput>(Outer, *InstanceName, RF_Transient);

    SubstanceImage->CompressionLevelRGB = 0;
    SubstanceImage->CompressionLevelAlpha = 0;
    SubstanceImage->NumComponents = 1;
    SubstanceImage->SizeX = Mip.SizeX;
    SubstanceImage->SizeY = Mip.SizeY;

    FByteBulkData& ImageRGB(SubstanceImage->ImageRGB);
    ImageRGB.RemoveBulkData();

    const void* MipData  = Mip.BulkData.Lock(LOCK_READ_ONLY);
    const int32 BufferSize = Mip.SizeX * Mip.SizeY * TextureBPP;

    ImageRGB.Lock(LOCK_READ_WRITE);
    void* ImagePtr = ImageRGB.Realloc(BufferSize);
    FMemory::Memcpy(ImagePtr, MipData, BufferSize);
    ImageRGB.Unlock();

    Mip.BulkData.Unlock();

    return SubstanceImage;

#else

    return nullptr;

#endif
}

UObject* UPMUUtilityLibrary::CreateSubstanceImageInputFromRTT2D(UObject* WorldContextObject, UTextureRenderTarget2D* SrcTexture, uint8 MipLevel, FString InstanceName)
{
#ifdef PMU_SUBSTANCE_ENABLED

    if (! IsValid(SrcTexture) || MipLevel > SrcTexture->GetNumMips())
    {
        return nullptr;
    }

    int32 TextureBPP = -1;

    // Make sure texture format is supported

    switch (SrcTexture->GetFormat())
    {
        case EPixelFormat::PF_B8G8R8A8:
        {
            TextureBPP = 4;
        }
        break;
    }

    if (TextureBPP < 4)
    {
        return nullptr;
    }

    // Get texture resource from game thread

    FPMUShaderTextureParameterInput TextureParameter;
    TextureParameter.TextureType = EPMUShaderTextureType::PMU_STT_TextureRenderTarget2D;
    TextureParameter.TextureRenderTarget2D = SrcTexture;

    FPMUShaderTextureParameterInputResource TextureResource(TextureParameter.GetResource_GT());

    // Make sure texture resource is valid
    if (! TextureResource.HasValidResource())
    {
        return nullptr;
    }

    // Construct new substance image input object

    UObject* Outer = WorldContextObject ? WorldContextObject : GetTransientPackage();
    USubstanceImageInput* SubstanceImage = NewObject<USubstanceImageInput>(Outer, *InstanceName, RF_Transient);

    SubstanceImage->CompressionLevelRGB = 0;
    SubstanceImage->CompressionLevelAlpha = 0;
    SubstanceImage->NumComponents = 3;
    SubstanceImage->SizeX = SrcTexture->SizeX;
    SubstanceImage->SizeY = SrcTexture->SizeY;

    FByteBulkData* ImageData(&SubstanceImage->ImageRGB);

    // Enqueue image data copy operation on the render thread

    ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER(
        FMarchingSquaresMap_InitializeVoxelData,
        FPMUShaderTextureParameterInputResource, TextureResource, TextureResource,
        FByteBulkData*, ImageData, ImageData,
        uint8, MipLevel, MipLevel,
        FIntRect, Dimension, FIntRect(0, 0, SrcTexture->SizeX, SrcTexture->SizeY),
        {
            check(ImageData != nullptr);

            if (! TextureResource.HasValidResource())
            {
                return;
            }

            TArray<FColor> ColorArray;

            FReadSurfaceDataFlags ReadDataFlags;
            ReadDataFlags.SetLinearToGamma(false);
            ReadDataFlags.SetOutputStencil(false);
            ReadDataFlags.SetMip(MipLevel);

            RHICmdList.ReadSurfaceData(TextureResource.GetTextureParamRef_RT(), Dimension, ColorArray, ReadDataFlags);

            ImageData->Lock(LOCK_READ_WRITE);
            const int32 BufferSize = ColorArray.Num() * ColorArray.GetTypeSize();
            void* ImagePtr = ImageData->Realloc(BufferSize);
            FMemory::Memcpy(ImagePtr, ColorArray.GetData(), BufferSize);
            ImageData->Unlock();
        } );

    return SubstanceImage;

#else

    return nullptr;

#endif
}
