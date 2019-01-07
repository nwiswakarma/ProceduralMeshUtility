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
#include "Grid/Tasks/PMUGridGenerationTask.h"
#include "PMUUtilityLibrary.h"
#include "PMUGridLandSetGenerationTask.generated.h"

/**
 * 
 */
UCLASS()
class PROCEDURALMESHUTILITY_API UPMUGridLandSetGenerationTask : public UPMUGridGenerationTask
{
	GENERATED_BODY()

protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bPrintLandCount = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bGenerateBorderSet = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bFilterBorderStrands = false;

	virtual void ExecuteTask_Impl(FGridData& GD) override
    {
        const int32 TexelCount = GD.CalcSize();

        // Generate border set if required

        if (bGenerateBorderSet)
        {
            FIntPoint& Dim(GD.Dimension);
            FPointSet& BorderSet(GD.BorderSet);
            FGridData::FMaskMap& PointMask(GD.PointMask);

            // Clear existing border set
            BorderSet.Empty();

            // Generate border set
            UPMUUtilityLibrary::FindBorders(
                BorderSet,
                PointMask,
                Dim.X,
                Dim.Y,
                FPMUGridInfo::MASK_THRESHOLD,
                1
                );
        }

        // Construct solid point set

        FPointSet& PointSet(GD.PointSet);
        PointSet.Reset();
        PointSet.Reserve(TexelCount);

        for (int32 i=0; i<TexelCount; ++i)
        {
            if (GD.IsSolid(i))
            {
                PointSet.Emplace(i);
            }
        }

        PointSet.Compact();
        PointSet.Shrink();

        if (bPrintLandCount)
        {
            UE_LOG(LogTemp,Warning, TEXT("PointSet: %d"), PointSet.Num());
        }
    }
};
