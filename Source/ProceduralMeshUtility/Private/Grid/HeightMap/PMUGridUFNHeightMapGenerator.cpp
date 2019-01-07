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

#include "Grid/HeightMap/PMUGridUFNHeightMapGenerator.h"
#include "UFNNoiseGenerator.h"

void FPMUGridUFNHeightMapGenerator::GenerateHeightMapImpl(FPMUGridData& GD, int32 MapId)
{
    // Invalid noise generator, abort
    if (! NoiseGenerator || ! GD.HasHeightMap(MapId))
    {
        return;
    }

    const int32 MapSize = GD.CalcSize();
    const FIntPoint Dimension = GD.Dimension;

    FPMUGridData::FHeightMap& HeightMap(GD.GetHeightMapChecked(MapId));

    // Reset height map values
    HeightMap.Reset(MapSize);
    HeightMap.SetNumZeroed(MapSize);

    UUFNNoiseGenerator& NG(*NoiseGenerator);

    for (int32 y=0, i=0; y<Dimension.Y; ++y)
    for (int32 x=0;      x<Dimension.X; ++x, ++i)
    {
        HeightMap[i] = FMath::Clamp(NG.GetNoise2D(x, y), 0.f, 1.f);
    }
}
