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
#include "PMUMeshSimplifierOptions.generated.h"

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUMeshSimplifierOptions
{
    GENERATED_BODY()

	// Whether mesh simplification is enabled
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnabled = false;

	// Whether mesh simplification is enabled
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Seed = 1337;

	// Each iteration involves selecting a fraction of the edges at random as possible 
	// candidates for collapsing. There is likely a sweet spot here trading off against number 
	// of edges processed vs number of invalid collapses generated due to collisions 
	// (the more edges that are processed the higher the chance of collisions happening)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float EdgeFraction = 0.125f;

	// Stop simplfying after a given number of iterations
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	int32 MaxIteration = 10;

	// And/or stop simplifying when we've reached a percentage of the input triangles
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float TargetPercentage = 0.05f;

	// The maximum allowed error when collapsing an edge (error is calculated as 1.0/qef_error)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxError = 5.f;

	// Useful for controlling how uniform the mesh is (or isn't)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxEdgeSize = 2.5f;

	// If the mesh has sharp edges this can used to prevent collapses which would otherwise be used
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinAngleCosine = 0.8f;
};
