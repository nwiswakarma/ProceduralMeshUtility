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
#include "DistanceField/PMUDFGeometry.h"
#include "PMUDFGeometryUtility.generated.h"

UCLASS(BlueprintType)
class PROCEDURALMESHUTILITY_API UPMUDFGeometryUtility : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

    // Geometry generator functions

    UFUNCTION(BlueprintCallable)
    static void CreateLine(UPARAM(ref) FPMUDFGeometryDataRef& GeometryRef, int32 GroupId, const FVector2D& P0, const FVector2D& P1, float Radius, bool bReversed);

    UFUNCTION(BlueprintCallable)
    static void CreateLines(UPARAM(ref) FPMUDFGeometryDataRef& GeometryRef, int32 GroupId, const TArray<FVector2D>& Lines, float Radius, bool bReversed);

    UFUNCTION(BlueprintCallable)
    static void FilterByPoint(UPARAM(ref) FPMUDFGeometryDataRef& GeometryRef, int32 GroupId, const FVector2D& Point, float Radius);

    UFUNCTION(BlueprintCallable)
    static void FilterByPoints(UPARAM(ref) FPMUDFGeometryDataRef& GeometryRef, int32 GroupId, const TArray<FVector2D>& Points, float Radius);
};
