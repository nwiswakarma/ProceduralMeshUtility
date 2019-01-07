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

#include "Grid/Tasks/PMUGridIslandHeightMapGenerationTask.h"

bool UPMUGridIslandHeightMapGenerationTask::SetupTask(FGridData& GD)
{
    if (UPMUGridGenerationTask::SetupTask(GD))
    {
        return MapInfo.DstID >= 0;
    }

    return false;
}

void UPMUGridIslandHeightMapGenerationTask::ExecuteTask_Impl(FGridData& GD)
{
    const int32 MapSize = GD.CalcSize();
    const int32 HVar = HeightVariance;
    const bool bUseHVar = HVar > 1;
    FRandomStream rs(Seed);

    // Land iteration queue
    TQueue<int32> tvQ;
    // Land sorting height values (not actual height map values)
    TArray<int32> tvHeight;
    // Index set to prevent multiple elevation entries
    FPointSet tvQueueSet;

    tvHeight.SetNumZeroed(MapSize);
    tvQueueSet.Reserve(MapSize);

    // Assign initial elevation from island borders
    for (int32 i : GD.PointSet)
    {
        if (GD.IsBorder(i))
        {
            tvQ.Enqueue(i);
            tvQueueSet.Emplace(i);
            tvHeight[i] = 0;
        }
        else
        {
            check(GD.IsSolid(i));
            tvHeight[i] = 1000;
        }
    }

    // Assign elevation values,
    // elevations are higher towards island center
    while (! tvQ.IsEmpty())
    {
        int32 tvi0;
        tvQ.Dequeue(tvi0);

        for (int32 ni=0; ni<FPMUGridInfo::NCOUNT; ++ni)
        {
            int32 tvi1 = GD.GetIdx(tvi0, ni);

            if (! GD.IsValidIndex(tvi1))
                continue;

            int32 e0 = tvHeight[tvi0];
            int32 e1 = tvHeight[tvi1];
            int32 eOffset = bUseHVar ? rs.RandRange(1, HVar) : 1;

            e0 += eOffset;

            if (e0 < e1)
            {
                tvHeight[tvi1] = e0;
                tvQ.Enqueue(tvi1);
                tvQueueSet.Emplace(tvi1);
            }
        }
    }

    // Assign set to an array for further modifications
    TArray<int32> tvIndices( tvQueueSet.Array() );

    // Sort indices by elevation if using height variance
    if (bUseHVar)
    {
        tvIndices.Sort( [&tvHeight](const int32& i0, const int32& i1) {
            return tvHeight[i0] < tvHeight[i1];
        } );
    }

    // Clears initial sorting containers
    tvHeight.Empty();
    tvQueueSet.Empty();

    // Redistribute elevation values,
    // high elevations should occur less often
    TMap<int32, float> elevationMap;
    int32 tvNum = tvIndices.Num();
    float maxElevation = -1.0f;
    float maxElevationInv = -1.0f;

    elevationMap.Reserve( tvNum );

    // Find maximum elevation value
    {
        float y = (tvNum-1) / (tvNum - 1.0f);
        float x = FMath::Sqrt(1.1f) - FMath::Sqrt(1.1f * (1.0f - y));
        maxElevation = x;
        maxElevationInv = 1 / maxElevation;
    }

    // Calculate elevation value using height redistribution formula
    for (int32 i=0; i<tvNum; i++)
    {
        // Let y(x) be the total area that we want at elevation <= x.
        // We want the higher elevations to occur less than lower
        // ones, and set the area to be y(x) = 1 - (1-x)^2.
        float y = i / (tvNum - 1.0f);

        // Since the data is sorted by elevation, this will linearly
        // increase the elevation as the loop goes on.
        float x = FMath::Sqrt(1.1f) - FMath::Sqrt(1.1f * (1.0f - y));

        elevationMap.Emplace(tvIndices[i], x*maxElevationInv);
    }

    // Max elevation under minimum threshold, abort
    if (maxElevation <= .00001f)
        return;

    // Creates transient height map
    TArray<float> HeightMap;
    HeightMap.SetNumZeroed(MapSize);

    const float avgInv = 1.f/(FPMUGridInfo::NCOUNT+1);

    for (int32 tvi0 : tvIndices)
    {
        float e = elevationMap.FindChecked(tvi0);

        for (int32 ni=0; ni<FPMUGridInfo::NCOUNT; ++ni)
        {
            int32 tvi1 = GD.GetIdx(tvi0, ni);

            if (! GD.IsValidIndex(tvi1))
                continue;

            if (elevationMap.Contains(tvi1))
                e += elevationMap.FindChecked(tvi1);
        }

        e *= avgInv;

        HeightMap[tvi0] = e;
    }

    GD.ApplyHeightBlend(HeightMap, MapInfo);

    HeightMap.Empty();
}
