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
#include "Mesh/Simplifier/PMUMeshSimplifierOptions.h"

class FPMUMeshSimplifierQEF
{
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

    static const int32 COLLAPSE_MAX_DEGREE = 16;
    static const int32 MAX_TRIANGLES_PER_VERTEX = COLLAPSE_MAX_DEGREE;

    FPMUMeshSimplifierOptions Options;

    static void BuildCandidateEdges(
        TArray<FEdge>& Edges,
        const TArray<uint32>& Triangles,
        const int32 VertexCount
        );

    static int32 FindValidCollapses(
        TArray<int32>& CollapseValid, 
        TArray<int32>& CollapseEdgeID, 
        TArray<FVector>& CollapsePositions,
        TArray<FVector>& CollapseNormals,
        const TArray<FEdge>& Edges,
        const TArray<FVector>& Positions,
        const TArray<FVector>& Normals,
        const TArray<int32>& VertexTriangleCounts,
        const FPMUMeshSimplifierOptions& Options
        );

    static void CollapseEdges(
        TArray<FVector>& Positions,
        TArray<FVector>& Normals,
        TArray<int32>& CollapseTarget,
        const TArray<int32>& CollapseValid,
        const TArray<FEdge>& Edges,
        const TArray<int32>& CollapseEdgeID,
        const TArray<FVector>& CollapsePositions,
        const TArray<FVector>& CollapseNormals
        );

    static int32 RemoveTriangles(
        TArray<uint32>& Tris,
        TArray<uint32>& TriBuffer,
        TArray<int32>& VertexTriangleCounts,
        const TArray<int32>& CollapseTarget,
        const int32 VertexCount
        );

    static void RemoveEdges(
        TArray<FEdge>& Edges,
        TArray<FEdge>& EdgeBuffer,
        const TArray<int32>& CollapseTarget
        );

    static void CompactVertices(
        TArray<FVector>& Positions,
        TArray<FVector>& Normals,
        TArray<uint32>& Triangles
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
