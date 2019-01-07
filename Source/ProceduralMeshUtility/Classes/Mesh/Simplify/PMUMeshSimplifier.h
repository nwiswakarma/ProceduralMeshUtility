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
#include "PMUMeshSimplifier.generated.h"

struct FPMUMeshSection;

USTRUCT(BlueprintType)
struct FPMUMeshSimplifierOptions
{
    GENERATED_BODY();

	// Whether mesh simplification is enabled
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnabled = false;

	// Each iteration involves selecting a fraction of the edges at random as possible 
	// candidates for collapsing. There is likely a sweet spot here trading off against number 
	// of edges processed vs number of invalid collapses generated due to collisions 
	// (the more edges that are processed the higher the chance of collisions happening)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EdgeFraction = 0.125f;

	// Stop simplfying after a given number of iterations
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxIterations = 10;

	// And/or stop simplifying when we've reached a percentage of the input triangles
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
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

class FPMUMeshSimplifier
{
    typedef struct FPMUMeshVertex FVertex;

    struct FTri
    {
        FTri()
        {
            indices_[0] = indices_[1] = indices_[2] = 0;
        }

        FTri(const int32 i0, const int32 i1, const int32 i2)
        {
            indices_[0] = i0;
            indices_[1] = i1;
            indices_[2] = i2;
        }

        int32 indices_[3];
    };

    union FEdge
    {
        FEdge(uint32_t _minIndex, uint32_t _maxIndex)
            : min_(_minIndex)
            , max_(_maxIndex)
        {
        }

        uint64_t idx_ = 0;
        struct { uint32_t min_, max_; };
    };

    static const int32 COLLAPSE_MAX_DEGREE = 16;
    static const int32 MAX_TRIANGLES_PER_VERTEX = COLLAPSE_MAX_DEGREE;

    FPMUMeshSimplifierOptions Options;

    void BuildCandidateEdges(
        const TArray<FVertex>& vertices,
        const TArray<FTri>& triangles,
        TArray<FEdge>& edges
        );

    int32 FindValidCollapses(
        const TArray<FEdge>& edges,
        const TArray<FVertex>& vertices,
        const TArray<FTri>& tris,
        const TArray<int32>& vertexTriangleCounts,
        TArray<int32>& collapseValid, 
        TArray<int32>& collapseEdgeID, 
        TArray<FVector>& collapsePosition,
        TArray<FVector>& collapseNormal
        );

    void CollapseEdges(
        const TArray<int32>& collapseValid,
        const TArray<FEdge>& edges,
        const TArray<int32>& collapseEdgeID,
        const TArray<FVector>& collapsePositions,
        const TArray<FVector>& collapseNormal,
        TArray<FVertex>& vertices,
        TArray<int32>& collapseTarget
        );

    int32 RemoveTriangles(
        const TArray<FVertex>& vertices,
        const TArray<int32>& collapseTarget,
        TArray<FTri>& tris,
        TArray<FTri>& triBuffer,
        TArray<int32>& vertexTriangleCounts
        );

    void RemoveEdges(
        const TArray<int32>& collapseTarget,
        TArray<FEdge>& edges,
        TArray<FEdge>& edgeBuffer
        );

    void CompactVertices(
        TArray<FVertex>& vertices,
        TArray<FTri>& triangles
        );

public:

    void Simplify(
        FPMUMeshSection& mesh,
        const FVector& InWorldOffset,
        const FPMUMeshSimplifierOptions& InOptions
        );
};
