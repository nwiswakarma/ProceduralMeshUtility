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

#include "Grid/HeightMap/PMUGridDiamondSquareHeightMapGenerator.h"

float Noise_GetGridPoint(TArray<float>& map, int32 x, int32 y, int32 sx, int32 sy, TArray<float>& vOverflowBuffer, TArray<float>& hOverflowBuffer)
{
	// The value is outside the right border of the map - mirror the coordinate, so a realistically behaving value is used
	if (x < 0)
    {
		x = -x;
	}
	// The value is outside the right border - use value from the overflow buffer
	else
    if (x >= sx)
    {
		return vOverflowBuffer[FMath::Max(FMath::Min(y, sy), 0)];
	}

	// The value is outside the top border of the map - mirror the coordinate, so a realistically behaving value is used
	if (y < 0)
    {
		y = -y;
	}
	// The value is outside the bottom border - use value from the overflow buffer
	else
    if (y >= sy)
    {
		return hOverflowBuffer[x];
	}

	// The point is within the map
	return map[x + sx * y];
}

float Noise_BicubicInterpolation(float p[4][4])
{
	// Prepare coefficients for the bicubic polynomial
	float a00 = p[1][1];
	float a01 = -.5*p[1][0] + .5*p[1][2];
	float a02 = p[1][0] - 2.5*p[1][1] + 2*p[1][2] - .5*p[1][3];
	float a03 = -.5*p[1][0] + 1.5*p[1][1] - 1.5*p[1][2] + .5*p[1][3];
	float a10 = -.5*p[0][1] + .5*p[2][1];
	float a11 = .25*p[0][0] - .25*p[0][2] - .25*p[2][0] + .25*p[2][2];
	float a12 = -.5*p[0][0] + 1.25*p[0][1] - p[0][2] + .25*p[0][3] + .5*p[2][0] - 1.25*p[2][1] + p[2][2] - .25*p[2][3];
	float a13 = .25*p[0][0] - .75*p[0][1] + .75*p[0][2] - .25*p[0][3] - .25*p[2][0] + .75*p[2][1] - .75*p[2][2] + .25*p[2][3];
	float a20 = p[0][1] - 2.5*p[1][1] + 2*p[2][1] - .5*p[3][1];
	float a21 = -.5*p[0][0] + .5*p[0][2] + 1.25*p[1][0] - 1.25*p[1][2] - p[2][0] + p[2][2] + .25*p[3][0] - .25*p[3][2];
	float a22 = p[0][0] - 2.5*p[0][1] + 2*p[0][2] - .5*p[0][3] - 2.5*p[1][0] + 6.25*p[1][1] - 5*p[1][2] + 1.25*p[1][3] + 2*p[2][0] - 5*p[2][1] + 4*p[2][2] - p[2][3] - .5*p[3][0] + 1.25*p[3][1] - p[3][2] + .25*p[3][3];
	float a23 = -.5*p[0][0] + 1.5*p[0][1] - 1.5*p[0][2] + .5*p[0][3] + 1.25*p[1][0] - 3.75*p[1][1] + 3.75*p[1][2] - 1.25*p[1][3] - p[2][0] + 3*p[2][1] - 3*p[2][2] + p[2][3] + .25*p[3][0] - .75*p[3][1] + .75*p[3][2] - .25*p[3][3];
	float a30 = -.5*p[0][1] + 1.5*p[1][1] - 1.5*p[2][1] + .5*p[3][1];
	float a31 = .25*p[0][0] - .25*p[0][2] - .75*p[1][0] + .75*p[1][2] + .75*p[2][0] - .75*p[2][2] - .25*p[3][0] + .25*p[3][2];
	float a32 = -.5*p[0][0] + 1.25*p[0][1] - p[0][2] + .25*p[0][3] + 1.5*p[1][0] - 3.75*p[1][1] + 3*p[1][2] - .75*p[1][3] - 1.5*p[2][0] + 3.75*p[2][1] - 3*p[2][2] + .75*p[2][3] + .5*p[3][0] - 1.25*p[3][1] + p[3][2] - .25*p[3][3];
	float a33 = .25*p[0][0] - .75*p[0][1] + .75*p[0][2] - .25*p[0][3] - .75*p[1][0] + 2.25*p[1][1] - 2.25*p[1][2] + .75*p[1][3] + .75*p[2][0] - 2.25*p[2][1] + 2.25*p[2][2] - .75*p[2][3] - .25*p[3][0] + .75*p[3][1] - .75*p[3][2] + .25*p[3][3];

	// Calculate value of the bicubic polynomial.
	float value =
        (a00 + a01 * 0.5 + a02 * 0.5 * 0.5 + a03 * 0.5 * 0.5 * 0.5) +
		(a10 + a11 * 0.5 + a12 * 0.5 * 0.5 + a13 * 0.5 * 0.5 * 0.5) * 0.5 +
		(a20 + a21 * 0.5 + a22 * 0.5 * 0.5 + a23 * 0.5 * 0.5 * 0.5) * 0.5 * 0.5 +
		(a30 + a31 * 0.5 + a32 * 0.5 * 0.5 + a33 * 0.5 * 0.5 * 0.5) * 0.5 * 0.5 * 0.5;

	return value;
}

void FPMUGridDiamondSquareHeightMapGenerator::GenerateHeightMapImpl(FPMUGridData& GD, int32 MapId)
{
    check(GD.HasHeightMap(MapId));

    const int32 MapSize = GD.CalcSize();
    const FIntPoint Dim = GD.Dimension;
    const float Stride = Dim.X;

    FPMUGridData::FHeightMap& HeightMap(GD.GetHeightMapChecked(MapId));

    // Reset height map values
    HeightMap.Reset(MapSize);
    HeightMap.SetNumZeroed(MapSize);

    // Seed random generator
    Rand = FRandomStream(Seed);

    /*
    int32 gridPoints = FMath::Max(Dimension.X, Dimension.Y);
    int32 gridSpacing = gridPoints;
    float octaveAmplitude = Amplitude * gridSpacing;

    for (int32 sideLength=gridPoints-1; sideLength>=2; sideLength/=2)
    {
        int32 half = sideLength/2;
        int32 currentGridSize = gridPoints/sideLength;

        // Displace midpoints
        for (int32 i=0; i<currentGridSize; i++)
        for (int32 j=0; j<currentGridSize; j++)
        {
            int32 bli[2] = { i      * sideLength, j       * sideLength}; //bottom left
            int32 bri[2] = {(i + 1) * sideLength, j       * sideLength}; //bottom right
            int32 tli[2] = { i      * sideLength, (j + 1) * sideLength}; //top left
            int32 tri[2] = {(i + 1) * sideLength, (j + 1) * sideLength}; //top right

            float bl = HeightMap[bli[0]+bli[1]*Stride]; //bottom left
            float br = HeightMap[bri[0]+bri[1]*Stride]; //bottom right
            float tl = HeightMap[tli[0]+tli[1]*Stride]; //top left
            float tr = HeightMap[tri[0]+tri[1]*Stride]; //top right
            float average = (bl + br + tl + tr) / 4;

            int32 index[5] = {
                i*sideLength+half + j*sideLength+half * Stride,
                i*sideLength+half + j*sideLength      * Stride,
                i*sideLength      + j*sideLength+half * Stride,
                i*sideLength+half + (j+1)*sideLength  * Stride,
                (i+1)*sideLength  + j*sideLength+half * Stride
                };

            HeightMap[index[0]] = average+WhiteNoise()*octaveAmplitude; // * sqrt(2));

            HeightMap[index[1]] = (bl+br)/2+WhiteNoise()*octaveAmplitude; //bottom
            HeightMap[index[2]] = (tl+bl)/2+WhiteNoise()*octaveAmplitude; //left

            if (j==currentGridSize-1) HeightMap[index[1]] = (tl+tr)/2+WhiteNoise()*octaveAmplitude; //top
            if (i==currentGridSize-1) HeightMap[index[2]] = (tr+br)/2+WhiteNoise()*octaveAmplitude; //right
        }
        octaveAmplitude *= Gain;
    }
    */

    if (! (0 <= MinFeatureSize && MinFeatureSize < FMath::Max(Dim.X, Dim.Y)) ||
        ! (0 <= MaxFeatureSize && MaxFeatureSize < FMath::Max(Dim.X, Dim.Y)))
    {
        UE_LOG(LogTemp,Warning, TEXT("CHECK"));
        return;
    }

    // The diamond-square algorithm by definition works on square maps only with side length equal to power  
    // of two plus one. But only two rows/columns of points are enough to interpolate the points near the right 
    // and bottom borders (top and bottom borders are aligned with the grid, so they can be interpolated in much
    // simpler fashion) - thanks to the fact that this is mid-point displacement algorithm. The vertical buffer
    // stands for any points to outside the right border, the horizontal buffer then analogously serves as all 
    // points below the bottom border. The points outside both right and bottom border are represented by last
    // point in the vertical buffer.

    float amplitudes[13] = {
        (3    * 15.f) / 32767.f,
        (7    * 15.f) / 32767.f,
        (10   * 15.f) / 32767.f,
        (20   * 15.f) / 32767.f,
        (50   * 15.f) / 32767.f,
        (75   * 15.f) / 32767.f,
        (150  * 15.f) / 32767.f,
        (250  * 15.f) / 32767.f,
        (400  * 15.f) / 32767.f,
        (600  * 15.f) / 32767.f,
        (1000 * 15.f) / 32767.f,
        (1400 * 15.f) / 32767.f,
        (2000 * 15.f) / 32767.f
        };

    TArray<float> vOverflowBuffer;
    TArray<float> hOverflowBuffer;

    vOverflowBuffer.SetNumUninitialized(Dim.Y+1);
    hOverflowBuffer.SetNumUninitialized(Dim.X);

    // The supplied wave length is likely not a power of two. Convert the number to the nearest lesser power of two.
    uint8 waveLengthLog2 = (uint8) FMath::Log2(MaxFeatureSize);
    int32 waveLength = 1 << waveLengthLog2;
    float amplitude = amplitudes[waveLengthLog2];

    // Generate the initial seed values (square corners).
    for (int32 y = 0; ; y += waveLength)
    {
        if (y < Dim.Y)
        {
            for (int32 x = 0; ; x += waveLength)
            {
                if (x < Dim.X)
                {
                    HeightMap[x + Dim.X * y] = Rand.FRandRange(-amplitude, amplitude);
                }
                else
                {
                    vOverflowBuffer[y] = Rand.FRandRange(-amplitude, amplitude);
                    break;
                }
            }
        }
        else {
            for (int32 x = 0; ; x += waveLength)
            {
                if (x < Dim.X)
                {
                    hOverflowBuffer[x] = Rand.FRandRange(-amplitude, amplitude);
                }
                else
                {
                    vOverflowBuffer[Dim.Y] = Rand.FRandRange(-amplitude, amplitude);
                    break;
                }
            }
            break;
        }
    }

    amplitude = amplitudes[waveLengthLog2 - 1];

    // Keep interpolating until there are uninterpolated tiles..
    while (waveLength > 1)
    {
        int32 halfWaveLength = waveLength / 2;

        // The square step - put a randomly generated point into center of each square.
        bool breakY = false;
        for (int32 y = halfWaveLength; breakY == false; y += waveLength)
        {
            for (int32 x = halfWaveLength; ; x += waveLength)
            {
                // Prepare the 4x4 value matrix for bicubic interpolation.
                int32 x0 = (int32) x - (int32) halfWaveLength - (int32) waveLength;
                int32 x1 = (int32) x - (int32) halfWaveLength;
                int32 x2 = (int32) x + (int32) halfWaveLength;
                int32 x3 = (int32) x + (int32) halfWaveLength + (int32) waveLength;

                int32 y0 = (int32) y - (int32) halfWaveLength - (int32) waveLength;
                int32 y1 = (int32) y - (int32) halfWaveLength;
                int32 y2 = (int32) y + (int32) halfWaveLength;
                int32 y3 = (int32) y + (int32) halfWaveLength + (int32) waveLength;

                float data[4][4] = {
                    {
                        Noise_GetGridPoint(HeightMap, x0, y0, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x1, y0, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x2, y0, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x3, y0, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                    },
                    {
                        Noise_GetGridPoint(HeightMap, x0, y1, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x1, y1, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x2, y1, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x3, y1, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                    },
                    {
                        Noise_GetGridPoint(HeightMap, x0, y2, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x1, y2, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x2, y2, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x3, y2, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                    },
                    {
                        Noise_GetGridPoint(HeightMap, x0, y3, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x1, y3, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x2, y3, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x3, y3, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                    },
                };

                // Interpolate
                float interpolatedHeight = Noise_BicubicInterpolation(data) + (waveLength < MinFeatureSize ? 0 : Rand.FRandRange(-amplitude, amplitude));

                // Place the value into one of the overflow buffers, if it is outside the map.
                if (x >= Dim.X)
                {
                    vOverflowBuffer[FMath::Min(y, Dim.Y)] = interpolatedHeight;
                    break;
                }
                else
                if (y >= Dim.Y)
                {
                    hOverflowBuffer[x] = interpolatedHeight;
                    breakY = true;
                }
                else
                {
                    HeightMap[x + Dim.X * y] = interpolatedHeight;
                }
            }
        }

        // The diamond step - add point into middle of each diamond so each square from the square step is composed of 4 smaller squares.
        breakY = false;
        bool evenRow = true;
        for (int32 y = 0; breakY == false; y += halfWaveLength)
        {
            for (int32 x = evenRow ? halfWaveLength : 0; ; x += waveLength)
            {
                // Prepare the 4x4 value matrix for bicubic interpolation (this time rotated by 45 degrees).
                int32 x0 = (int32) x - (int32) halfWaveLength - (int32) waveLength;
                int32 x1 = (int32) x - (int32) waveLength;
                int32 x2 = (int32) x - (int32) halfWaveLength;
                int32 x3 = (int32) x;
                int32 x4 = (int32) x + (int32) halfWaveLength;
                int32 x5 = (int32) x + (int32) waveLength;
                int32 x6 = (int32) x + (int32) halfWaveLength + (int32) waveLength;

                int32 y0 = (int32) y - (int32) halfWaveLength - (int32) waveLength;
                int32 y1 = (int32) y - (int32) waveLength;
                int32 y2 = (int32) y - (int32) halfWaveLength;
                int32 y3 = (int32) y;
                int32 y4 = (int32) y + (int32) halfWaveLength;
                int32 y5 = (int32) y + (int32) waveLength;
                int32 y6 = (int32) y + (int32) halfWaveLength + (int32) waveLength;

                float data[4][4] = {
                    {
                        Noise_GetGridPoint(HeightMap, x0, y3, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x1, y4, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x2, y5, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x3, y6, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                    },
                    {
                        Noise_GetGridPoint(HeightMap, x1, y2, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x2, y3, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x3, y4, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x4, y5, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                    },
                    {
                        Noise_GetGridPoint(HeightMap, x2, y1, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x3, y2, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x4, y3, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x5, y4, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                    },
                    {
                        Noise_GetGridPoint(HeightMap, x3, y0, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x4, y1, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x5, y2, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                        Noise_GetGridPoint(HeightMap, x6, y3, Dim.X, Dim.Y, vOverflowBuffer, hOverflowBuffer),
                    },
                };

                // Interpolate
                float interpolatedHeight = Noise_BicubicInterpolation(data) + (waveLength < MinFeatureSize ? 0 : Rand.FRandRange(-amplitude, amplitude));

                // Place the value into one of the overflow buffers, if it is outside the map.
                if (x >= Dim.X)
                {
                    vOverflowBuffer[FMath::Min(y, Dim.Y)] = interpolatedHeight;
                    break;
                }
                else
                if (y >= Dim.Y)
                {
                    hOverflowBuffer[x] = interpolatedHeight;
                    breakY = true;
                }
                else
                {
                    HeightMap[x + Dim.X * y] = interpolatedHeight;
                }
            }

            // The X coordinates are shifted by waveLength/2 in even rows.
            evenRow = !evenRow;
        }

        // Decrease the wave length and amplitude.
        waveLength /= 2;
        if (waveLength > 1)
        {
            amplitude = amplitudes[(int32)FMath::Log2(waveLength / 2)];
        }
    }
}
