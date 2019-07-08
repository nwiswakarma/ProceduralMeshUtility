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

struct PROCEDURALMESHUTILITY_API FPMUMeshSimplifierOptions
{
	// Whether mesh simplification is enabled
    bool bEnabled = false;

	// Each iteration involves selecting a fraction of the edges at random as possible 
	// candidates for collapsing. There is likely a sweet spot here trading off against number 
	// of edges processed vs number of invalid collapses generated due to collisions 
	// (the more edges that are processed the higher the chance of collisions happening)
	float EdgeFraction = 0.125f;

	// Stop simplfying after a given number of iterations
	int32 MaxIterations = 10;

	// And/or stop simplifying when we've reached a percentage of the input triangles
	float TargetPercentage = 0.05f;

	// The maximum allowed error when collapsing an edge (error is calculated as 1.0/qef_error)
	float MaxError = 5.f;

	// Useful for controlling how uniform the mesh is (or isn't)
	float MaxEdgeSize = 2.5f;

	// If the mesh has sharp edges this can used to prevent collapses which would otherwise be used
	float MinAngleCosine = 0.8f;
};

class FPMUMeshSimplifierQEF
{
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

        uint32 indices_[3];
    };

    union FEdge
    {
        FEdge(uint32 _minIndex, uint32 _maxIndex)
            : min_(_minIndex)
            , max_(_maxIndex)
        {
        }

        uint64 idx_ = 0;
        struct { uint32 min_, max_; };
    };

    struct FVertex
    {
        FVector Position;
        FVector Normal;
    };

    static const int32 COLLAPSE_MAX_DEGREE = 16;
    static const int32 MAX_TRIANGLES_PER_VERTEX = COLLAPSE_MAX_DEGREE;

    FPMUMeshSimplifierOptions Options;

    static void BuildCandidateEdges(
        TArray<FEdge>& edges,
        const TArray<FVertex>& vertices,
        const TArray<FTri>& triangles
        );

    static int32 FindValidCollapses(
        TArray<int32>& collapseValid, 
        TArray<int32>& collapseEdgeID, 
        TArray<FVector>& collapsePosition,
        TArray<FVector>& collapseNormal,
        const TArray<FEdge>& edges,
        const TArray<FVertex>& vertices,
        const TArray<int32>& vertexTriangleCounts,
        const FPMUMeshSimplifierOptions& Options
        );

    static void CollapseEdges(
        TArray<FVertex>& vertices,
        TArray<int32>& collapseTarget,
        const TArray<int32>& collapseValid,
        const TArray<FEdge>& edges,
        const TArray<int32>& collapseEdgeID,
        const TArray<FVector>& collapsePositions,
        const TArray<FVector>& collapseNormal
        );

    static int32 RemoveTriangles(
        TArray<FTri>& tris,
        TArray<FTri>& triBuffer,
        TArray<int32>& vertexTriangleCounts,
        const TArray<FVertex>& vertices,
        const TArray<int32>& collapseTarget
        );

    static void RemoveEdges(
        TArray<FEdge>& edges,
        TArray<FEdge>& edgeBuffer,
        const TArray<int32>& collapseTarget
        );

    static void CompactVertices(
        TArray<FVertex>& vertices,
        TArray<FTri>& triangles
        );

public:

    static void Simplify(
        TArray<FVector>& DstPositions,
        TArray<FVector>& DstNormals,
        TArray<uint32>& DstIndices,
        const TArray<FVector>& SrcPositions,
        const TArray<FVector>& SrcNormals,
        const TArray<uint32>& SrcIndices,
        const FPMUMeshSimplifierOptions& Options
        );
};

class PROCEDURALMESHUTILITY_API FPMUMeshSimplifier
{
public:

    static void SimplifyMeshSection(FPMUMeshSectionRef SectionRef, const FPMUMeshSimplifierOptions& Options);
};
