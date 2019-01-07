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

#include "PMUPolyPath.h"
#include "ProceduralMeshUtility.h"

#include "AGGTypes.h"
#include "AGGTypedContext.h"
#include "AJCTypes.h"
#include "AJCUtilityLibrary.h"
#include "EarcutTypes.h"

#include "UnrealMathUtility.h"

// Poly Path Ref Functions

TArray<FVector2D> UPMUPolyPathRef::GetPoints(int32 PathGroupIndex, int32 PathIndex) const
{
    // Invalid current poly path, abort
    if (IsPathValid() && PolyPath->HasPath(PathGroupIndex, PathIndex))
    {
        return PolyPath->GetPath(PathGroupIndex, PathIndex)->Points;
    }

    return TArray<FVector2D>();
}

void UPMUPolyPathRef::GeneratePolyIsland(const FPMUPolyIslandParams& PolyIslandParams, float ShapeOffset, uint8 Flags)
{
    TSharedPtr<FPMUPolyPath> PolyPathPtr(new FPMUPolyPath);
    PolyPathPtr->GeneratePolyIsland(PolyIslandParams, ShapeOffset, Flags);

    ResetPath();
    PolyPath = MoveTemp(PolyPathPtr);
}

void UPMUPolyPathRef::GeneratePolyIslandFromPoints(const FPMUPolyIslandParams& PolyIslandParams, const TArray<FVector2D>& Points, float ShapeOffset, uint8 Flags)
{
    TSharedPtr<FPMUPolyPath> PolyPathPtr(new FPMUPolyPath);
    PolyPathPtr->GeneratePolyIsland(PolyIslandParams, Points, ShapeOffset, Flags);

    ResetPath();
    PolyPath = MoveTemp(PolyPathPtr);
}

// Poly Path Functions

void FPMUPolyPath::GeneratePolyIslandImpl(const FPMUPolyIslandParams& PolyIslandParams, const TArray<FVector2D>& PolyPoints, float ShapeOffset, uint8 Flags)
{
    // Not enough points to form an island, abort
    if (PolyPoints.Num() < 3)
    {
        return;
    }

    // Override shape size param to poly path dimension
    Dimension = PolyIslandParams.Size;

    const FAJCVectorPath& PolyVectorPath(PolyPoints);
    FAJCPointPaths PolyPointPaths;
    FAJCVectorPaths ResultPaths;
    FAJCOffsetClipperConfig ClipperConfig(ShapeOffset);

    FAJCPathRef PolyPointPath;
    PolyPointPath.JoinType = EAJCJoinType::JTSquare;
    PolyPointPath.EndType  = EAJCEndType::ETClosedPolygon;

    // Generate offset poly path

    FAJCUtilityLibrary::ConvertVectorPathToPointPath(PolyVectorPath, PolyPointPath);
    FAJCUtilityLibrary::OffsetClip(PolyPointPath, ClipperConfig, PolyPointPaths);
    FAJCUtilityLibrary::ConvertPointPathsToVectorPaths(PolyPointPaths, ResultPaths);

    // Construct new poly paths

    PathGroups.SetNum(PathGroups.Num() + 1);
    FPathGroup& PathGroup(PathGroups.Last());
    PathGroup.Data.Reserve(ResultPaths.Num());

    const float MinAreaSq = PolyIslandParams.GetMinAreaSq();
    const bool bOnlyLargest = (Flags & 1) != 0;

    if (bOnlyLargest)
    {
        const bool bFitIsland = (Flags & 2) != 0;
        int32 LargestPathIndex = -1;
        double LargestAreaSq = -1.f;

        for (int32 i=0; i<ResultPaths.Num(); ++i)
        {
            const FAJCVectorPath& PointPath(ResultPaths[i]);
            const double AreaSq = FMath::Abs(ClipperLib::Area(PolyPointPaths[i]));

            UE_LOG(LogTemp,Warning,
                TEXT("GeneratePoly() (Flags: %d) - ResultPath[%d]: (Num: %d) (Area: %f)"),
                Flags,
                i,
                PolyPointPaths[i].size(),
                AreaSq
                );

            if (PointPath.Num() > 0 && AreaSq > LargestAreaSq)
            {
                LargestPathIndex = i;
                LargestAreaSq = AreaSq;
            }
        }

        if (ResultPaths.IsValidIndex(LargestPathIndex))
        {
            UE_LOG(LogTemp,Warning, TEXT("GeneratePoly() LargestPathIndex: %d"), LargestPathIndex);

            FAJCVectorPath& PointPath(ResultPaths[LargestPathIndex]);

            if (bFitIsland)
            {
                UPMUPolyIslandGenerator::FitPoints(PointPath, Dimension, PolyIslandParams.PolyScale);
            }

            PathGroup.Data.Emplace(Dimension, PointPath);
        }
    }
    else
    {
        for (int32 i=0; i<ResultPaths.Num(); ++i)
        {
            const FAJCVectorPath& PointPath(ResultPaths[i]);
            const double AreaSq = ClipperLib::Area(PolyPointPaths[i]);

            UE_LOG(LogTemp,Warning,
                TEXT("GeneratePoly() (Flags: %d) - ResultPath[%d]: (Num: %d) (Area: %f) (AreaSq > MinAreaSq: %d)"),
                Flags,
                i,
                PolyPointPaths[i].size(),
                AreaSq,
                AreaSq > MinAreaSq
                );

            if (PointPath.Num() > 0 && AreaSq > MinAreaSq)
            {
                PathGroup.Data.Emplace(Dimension, PointPath);
            }
        }
    }
}

void FPMUPolyPath::GeneratePolyIsland(const FPMUPolyIslandParams& PolyIslandParams, float ShapeOffset, uint8 Flags)
{
    UE_LOG(LogTemp,Warning, TEXT("GeneratePoly(): (Seed: %d) (MinArea: %f (%f))"),
        PolyIslandParams.RandomSeed,
        PolyIslandParams.MinArea,
        PolyIslandParams.GetMinAreaSq()
        );

    FRandomStream Rand(PolyIslandParams.RandomSeed);
    TArray<FVector2D> Points;

    // Generate vector poly path
    UPMUPolyIslandGenerator::GeneratePoly(PolyIslandParams, Rand, Points);

    // Generate poly island
    FPMUPolyPath::GeneratePolyIslandImpl(PolyIslandParams, Points, ShapeOffset, Flags);
}

void FPMUPolyPath::GeneratePolyIsland(const FPMUPolyIslandParams& PolyIslandParams, const TArray<FVector2D>& InitialPoints, float ShapeOffset, uint8 Flags)
{
    FRandomStream Rand(PolyIslandParams.RandomSeed);
    TArray<FVector2D> Points;

    // Generate vector poly path
    UPMUPolyIslandGenerator::GeneratePoly(PolyIslandParams, InitialPoints, Rand, Points);

    // Generate poly island
    FPMUPolyPath::GeneratePolyIslandImpl(PolyIslandParams, Points, ShapeOffset, Flags);
}
