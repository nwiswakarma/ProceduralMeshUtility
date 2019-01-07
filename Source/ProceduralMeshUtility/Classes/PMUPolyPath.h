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
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PMUGridData.h"
#include "PMUPolyPath.generated.h"

class FPMUPolyPath
{
    struct FPathGroup
    {
        TArray<FPMUPolyPath> Data;

        int32 Num() const
        {
            return Data.Num();
        }

        bool IsEmpty() const
        {
            return Data.Num() <= 0;
        }
    };

    TArray<FPathGroup> PathGroups;
    FPMUGridData GridData;

    void GeneratePolyIslandImpl(const FPMUPolyIslandParams& PolyIslandParams, const TArray<FVector2D>& PolyPoints, float ShapeOffset, uint8 Flags);

public:

    FVector2D Dimension;
    TArray<FVector2D> Points;

    FPMUPolyPath() = default;

    FPMUPolyPath(const FVector2D& InDimension, const TArray<FVector2D>& InPoints)
        : Dimension(InDimension)
        , Points(InPoints)
        , GridData(FIntPoint(InDimension.X, InDimension.Y))
    {
    }

    void GeneratePolyIsland(const FPMUPolyIslandParams& PolyIslandParams, float ShapeOffset = -1.f, uint8 Flags = 0);
    void GeneratePolyIsland(const FPMUPolyIslandParams& PolyIslandParams, const TArray<FVector2D>& InitialPoints, float ShapeOffset = -1.f, uint8 Flags = 0);

    FORCEINLINE bool HasPath(const int32 PathGroupIndex, const int32 PathIndex) const
    {
        return PathGroups.IsValidIndex(PathGroupIndex)
            ? PathGroups[PathGroupIndex].Data.IsValidIndex(PathIndex)
            : false;
    }

    FORCEINLINE int32 GetPathGroupCount() const
    {
        return PathGroups.Num();
    }

    FORCEINLINE int32 GetPathCount(const int32 PathGroupIndex) const
    {
        return PathGroups.IsValidIndex(PathGroupIndex)
            ? PathGroups[PathGroupIndex].Num()
            : 0;
    }

    FORCEINLINE FPMUPolyPath* GetPath(const int32 PathGroupIndex, const int32 PathIndex)
    {
        return HasPath(PathGroupIndex, PathIndex)
            ? &PathGroups[PathGroupIndex].Data[PathIndex]
            : nullptr;
    }
};

UCLASS(BlueprintType)
class PROCEDURALMESHUTILITY_API UPMUPolyPathRef : public UObject
{
    GENERATED_BODY()

    TSharedPtr<FPMUPolyPath> PolyPath = nullptr;

public:

    FORCEINLINE void ResetPath()
    {
        PolyPath.Reset();
    }

    FORCEINLINE bool IsPathValid() const
    {
        return PolyPath.IsValid();
    }

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Reset Path"))
    void K2_ResetPath()
    {
        ResetPath();
    }

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Is Path Valid"))
    bool K2_IsPathValid() const
    {
        return IsPathValid();
    }

    UFUNCTION(BlueprintCallable)
    int32 GetPathGroupCount() const
    {
        if (IsPathValid())
        {
            return PolyPath->GetPathGroupCount();
        }

        return 0;
    }

    UFUNCTION(BlueprintCallable)
    int32 GetPathCount(int32 PathGroupIndex) const
    {
        if (IsPathValid())
        {
            return PolyPath->GetPathCount(PathGroupIndex);
        }

        return 0;
    }

    UFUNCTION(BlueprintCallable)
    TArray<FVector2D> GetPoints(int32 PathGroupIndex, int32 PathIndex) const;

    UFUNCTION(BlueprintCallable)
    void GeneratePolyIsland(const FPMUPolyIslandParams& PolyIslandParams, float ShapeOffset = -1.f, uint8 Flags = 0);

    UFUNCTION(BlueprintCallable)
    void GeneratePolyIslandFromPoints(const FPMUPolyIslandParams& PolyIslandParams, const TArray<FVector2D>& Points, float ShapeOffset = -1.f, uint8 Flags = 0);
};
