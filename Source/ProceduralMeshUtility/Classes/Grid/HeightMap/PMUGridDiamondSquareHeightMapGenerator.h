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
#include "Grid/HeightMap/PMUGridHeightMapGenerator.h"
#include "Grid/HeightMap/PMUGridHeightMapUtility.h"
#include "PMUGridDiamondSquareHeightMapGenerator.generated.h"

class FPMUGridDiamondSquareHeightMapGenerator : public IPMUGridHeightMapGenerator
{
protected:

    virtual void GenerateHeightMapImpl(FPMUGridData& GridData, int32 MapId) override;

    FORCEINLINE float WhiteNoise() const
    {
        return Rand.GetFraction() - .5f;
    }

public:

    FRandomStream Rand;
    int32 MinFeatureSize = 2;
    int32 MaxFeatureSize = 4;
	float Seed = 0;
};

UCLASS(BlueprintType, Blueprintable)
class UPMUGridDiamondSquareHeightMapGeneratorInstance : public UObject
{
    GENERATED_BODY()

    FPMUGridDiamondSquareHeightMapGenerator Generator;

public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Seed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MinFeatureSize;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxFeatureSize;

	UFUNCTION(BlueprintCallable)
    FPMUGridHeightMapGeneratorData GetGeneratorRef()
    {
        Generator.Seed = Seed;
        Generator.MinFeatureSize = MinFeatureSize;
        Generator.MaxFeatureSize = MaxFeatureSize;
        return FPMUGridHeightMapGeneratorData(Generator);
    }
};
