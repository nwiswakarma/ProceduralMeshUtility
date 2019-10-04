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
#include "PMUMeshGridUtility.generated.h"

UCLASS()
class PROCEDURALMESHUTILITY_API UPMUMeshGridUtility : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

    static void GenerateLineData(TArray<FVector2D>& Tangents, TArray<float>& Distances, const TArray<FVector2D>& Points, bool bClosedPoly);

public:

    UFUNCTION(BlueprintCallable)
    static void GenerateMeshAlongLine(FPMUMeshSectionRef OutSectionRef, const TArray<FVector2D>& Points, float Offset = 1.f, int32 StepCount = 1, bool bClosedPoly = true);

    UFUNCTION(BlueprintCallable, meta=(DisplayName="Generate Mesh Along Line (Uniform)"))
    static void K2_GenerateMeshAlongLineUniform(FPMUMeshSectionRef OutSectionRef, const TArray<FVector2D>& Points, float GridSize = 1.f, float Offset = 1.f, float ExtrudeOffset = 0.f, int32 StepCount = 1, bool bClosedPoly = true);

    static void GenerateMeshAlongLine(FPMUMeshSection& OutSection, const TArray<FVector2D>& Positions, const TArray<FVector2D>& Tangents, const TArray<float>& Distances, float Offset, int32 StepCount);

    static void GenerateMeshAlongLineUniform(FPMUMeshSectionRef OutSectionRef, const TArray<FVector2D>& Points, float GridSize = 1.f, float Offset = 1.f, float ExtrudeOffset = 0.f, int32 StepCount = 1, bool bClosedPoly = true);
    static void GenerateMeshAlongLineUniform(FPMUMeshSection& OutSection, const TArray<FVector2D>& Positions, const TArray<FVector2D>& Tangents, const TArray<float>& Distances, float GridSize, float Offset, float ExtrudeOffset, int32 StepCount, bool bClosedPoly);
};

FORCEINLINE_DEBUGGABLE void UPMUMeshGridUtility::K2_GenerateMeshAlongLineUniform(FPMUMeshSectionRef OutSectionRef, const TArray<FVector2D>& Points, float GridSize, float Offset, float ExtrudeOffset, int32 StepCount, bool bClosedPoly)
{
    GenerateMeshAlongLineUniform(OutSectionRef, Points, GridSize, Offset, ExtrudeOffset, StepCount, bClosedPoly);
}
