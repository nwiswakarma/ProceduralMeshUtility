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
#include "Mesh/PMUMeshTypes.h"
#include "PMUUtilityLibrary.h"
#include "PMUPolyLineExtrude.generated.h"

class FPMUPolyLineExtrude
{

private:

    bool bBuildStarted = false;
    int32 LastFlip = -1;
    FVector2D Normal;

    TArray<FVector2D> Positions;
    TArray<int32> Indices;

    void Reset(const int32 Reserve = 0);
    int32 CreateSegment(const int32 index, FVector2D prev, FVector2D cur, const FVector2D& next, const float halfThick, const bool bHasNext);
    float ComputeMiter(FVector2D& tangent, FVector2D& miter, const FVector2D& lineA, const FVector2D& lineB, const float halfThick);
    void AddExtrusion(const FVector2D& point, const FVector2D& normal, const float scale);

    FORCEINLINE static FVector2D GetPerpendicular(const FVector2D& LineDelta)
    {
        return FVector2D(-LineDelta.Y, LineDelta.X);
    }

public:

    float Thickness = 1.f;
    float MiterLimit = 10.f;
    bool bCapSquared = false;
    bool bJoinBevel = false;

    void Build(const TArray<FVector2D>& Points);

    FORCEINLINE const TArray<FVector2D>& GetPositions() const
    {
        return Positions;
    }

    FORCEINLINE const TArray<int32>& GetIndices() const
    {
        return Indices;
    }
};

UCLASS(BlueprintType, Blueprintable)
class UPMUPolyLineExtrude : public UObject
{
    GENERATED_BODY()

    FPMUPolyLineExtrude ExtrudeUtility;

public:

    UPROPERTY(BlueprintReadWrite, Category="Line Extrusion")
    float Thickness = 1.f;

    UPROPERTY(BlueprintReadWrite, Category="Line Extrusion")
    float MiterLimit = 10.f;

    UPROPERTY(BlueprintReadWrite, Category="Line Extrusion")
    bool bCapSquared = false;

    UPROPERTY(BlueprintReadWrite, Category="Line Extrusion")
    bool bJoinBevel = false;

	UFUNCTION(BlueprintCallable)
	FPMUMeshSection CreateLineExtrudeSection(const TArray<FVector2D>& Points);
};
