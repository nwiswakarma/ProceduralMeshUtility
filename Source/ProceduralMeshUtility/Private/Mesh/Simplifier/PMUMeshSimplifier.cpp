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

#include "Mesh/Simplifier/PMUMeshSimplifier.h"

#define QEF_INCLUDE_IMPL
#include "qef_simd.h"
#undef QEF_INCLUDE_IMPL

#include <random>

void FPMUMeshSimplifierQEF::Simplify(
    TArray<FVector>& DstPositions,
    TArray<FVector>& DstNormals,
    TArray<uint32>& DstIndices,
    const TArray<FVector>& SrcPositions,
    const TArray<FVector>& SrcNormals,
    const TArray<uint32>& SrcIndices,
    const FPMUMeshSimplifierOptions& Options
    )
{
    if (SrcPositions.Num() < 16 || SrcIndices.Num() < (16*3))
    {
        return;
    }

    TArray<FVertex> vertices;
    TArray<FTri> triangles;

    vertices.SetNumUninitialized(SrcPositions.Num());

    for (int32 i=0; i<SrcPositions.Num(); ++i)
    {
        vertices[i] = { SrcPositions[i], SrcNormals[i] };
    }

    const int32 TriNum = SrcIndices.Num() / 3;
    triangles.SetNumUninitialized(TriNum);

    for (int32 i=0; i<TriNum; ++i)
    {
        const int32 idx = i*3;
        triangles[i].indices_[0] = SrcIndices[idx];
        triangles[i].indices_[1] = SrcIndices[idx+1];
        triangles[i].indices_[2] = SrcIndices[idx+2];
    }

    TArray<FEdge> edges;
    edges.Reserve(triangles.Num() * 3);

    BuildCandidateEdges(
        edges,
        vertices,
        triangles
        );

    TArray<FVector> collapsePosition;
    TArray<FVector> collapseNormal;
    TArray<int32> collapseValid;
    TArray<int32> collapseEdgeID;
    TArray<int32> collapseTarget;

    collapsePosition.SetNumUninitialized(edges.Num());
    collapseNormal.SetNumUninitialized(edges.Num());
    collapseValid.Reserve(edges.Num());
    collapseEdgeID.Reserve(vertices.Num());
    collapseTarget.Reserve(vertices.Num());

    TArray<FEdge> edgeBuffer;
    TArray<FTri> triBuffer;

    edgeBuffer.Reserve(edges.Num());
    triBuffer.Reserve(triangles.Num());

    // per vertex
    TArray<int32> vertexTriangleCounts;
    vertexTriangleCounts.SetNumZeroed(vertices.Num());

    for (int32 j=0; j<triangles.Num(); j++)
    {
        const uint32* indices = triangles[j].indices_;

        for (int32 index=0; index<3; index++)
        {
            ++vertexTriangleCounts[indices[index]];
        }
    }

    const int32 targetTriangleCount = triangles.Num() * Options.TargetPercentage;
    const int32 maxIterations = Options.MaxIterations;

    for (int32 i=0; i<maxIterations; ++i)
    {
        collapseEdgeID.Init(-1, vertices.Num());
        collapseTarget.Init(-1, vertices.Num());

        collapseValid.Reset();

        const int32 countValidCollapse = FindValidCollapses(
            collapseValid, 
            collapseEdgeID,
            collapsePosition,
            collapseNormal,
            edges,
            vertices,
            vertexTriangleCounts,
            Options
            );

        // No valid collapse, break
        if (countValidCollapse == 0)
        {
            break;
        }

        CollapseEdges(
            vertices,
            collapseTarget,
            collapseValid,
            edges,
            collapseEdgeID,
            collapsePosition,
            collapseNormal
            );

        RemoveTriangles(
            triangles,
            triBuffer,
            vertexTriangleCounts,
            vertices,
            collapseTarget
            );

        RemoveEdges(
            edges,
            edgeBuffer,
            collapseTarget
            );

        // Triangle result count exceed target triangle count, break
        if (triangles.Num() < targetTriangleCount)
        {
            break;
        }
    }

    CompactVertices(vertices, triangles);

    DstPositions.SetNumUninitialized(vertices.Num(), true);
    DstNormals.SetNumUninitialized(vertices.Num(), true);
    DstIndices.SetNumUninitialized(triangles.Num()*3, true);

    for (int32 vi=0; vi<vertices.Num(); vi++)
    {
        DstPositions[vi] = vertices[vi].Position;
        DstNormals[vi] = vertices[vi].Normal;
    }

    for (int32 ti=0; ti<triangles.Num(); ti++)
    {
        const int32 i = ti*3;
        DstIndices[i]   = triangles[ti].indices_[0];
        DstIndices[i+1] = triangles[ti].indices_[1];
        DstIndices[i+2] = triangles[ti].indices_[2];
    }
}

void FPMUMeshSimplifierQEF::BuildCandidateEdges(
    TArray<FEdge>& edges,
    const TArray<FVertex>& vertices,
    const TArray<FTri>& triangles
    )
{
    for (int32 i=0; i<triangles.Num(); i++)
    {
        const uint32* indices = triangles[i].indices_;
        edges.Emplace(FMath::Min(indices[0], indices[1]), FMath::Max(indices[0], indices[1]));
        edges.Emplace(FMath::Min(indices[1], indices[2]), FMath::Max(indices[1], indices[2]));
        edges.Emplace(FMath::Min(indices[0], indices[2]), FMath::Max(indices[0], indices[2]));
    }

    edges.Sort(
        [](const FEdge& lhs, const FEdge& rhs)
        {
            return lhs.idx_ < rhs.idx_;
        } );

    TArray<FEdge> filteredEdges;
    TArray<bool> boundaryVerts;

    filteredEdges.Reserve(edges.Num());
    boundaryVerts.Init(false, vertices.Num());

    FEdge prev = edges[0];

    for (int32 count=1, idx=1; idx<edges.Num(); idx++)
    {
        const FEdge curr = edges[idx];

        if (curr.idx_ != prev.idx_)
        {
            if (count == 1)
            {
                boundaryVerts[prev.min_] = true;
                boundaryVerts[prev.max_] = true;
            }
            else 
            {
                filteredEdges.Emplace(prev);
            }

            count = 1;
        }
        else
        {
            count++;
        }

        prev = curr;
    }

    edges.Reset();

    for (FEdge& edge: filteredEdges)
    {
        if (!boundaryVerts[edge.min_] && !boundaryVerts[edge.max_])
        {
            edges.Emplace(edge);
        }
    }
}

int32 FPMUMeshSimplifierQEF::FindValidCollapses(
    TArray<int32>& collapseValid, 
    TArray<int32>& collapseEdgeID, 
    TArray<FVector>& collapsePosition,
    TArray<FVector>& collapseNormal,
    const TArray<FEdge>& edges,
    const TArray<FVertex>& vertices,
    const TArray<int32>& vertexTriangleCounts,
    const FPMUMeshSimplifierOptions& Options
    )
{
    int32 validCollapses = 0;

    std::mt19937 prng;
    prng.seed(42);

    const int32 numRandomEdges = edges.Num() * Options.EdgeFraction;
    std::uniform_int_distribution<int32> distribution(0, (int32)(edges.Num() - 1));

    TArray<int32> randomEdges;
    randomEdges.Reserve(numRandomEdges);

    for (int32 i=0; i<numRandomEdges; i++)
    {
        randomEdges.Emplace(distribution(prng));
    }

    // Sort the edges to improve locality
    randomEdges.Sort();

    TArray<float> minEdgeCost;
    minEdgeCost.Init(BIG_NUMBER, vertices.Num());

    for (int32 i : randomEdges)
    {
        const FEdge& edge(edges[i]);
        const FVertex& vMin(vertices[edge.min_]);
        const FVertex& vMax(vertices[edge.max_]);

        // Prevent collapses below angle threshold
        const float cosAngle = vMin.Normal | vMax.Normal;
        if (cosAngle < Options.MinAngleCosine)
        {
            continue;
        }

        // Prevent collapses above maximum edge size
        const float edgeSize = (vMax.Position - vMin.Position).SizeSquared();
        if (edgeSize > (Options.MaxEdgeSize * Options.MaxEdgeSize))
        {
            continue;
        }

        const int32 degree = vertexTriangleCounts[edge.min_] + vertexTriangleCounts[edge.max_];
        if (degree > COLLAPSE_MAX_DEGREE)
        {
            continue;
        }

        //__declspec(align(16)) float pos[4];
        FVector pos;
        //FVertex data[2] = { vMin, vMax };
        FVector posData[2] = { vMin.Position, vMax.Position };
        FVector nrmData[2] = { vMin.Normal, vMax.Normal };

        // Solve quadratic error function for points
        float error = qef_solve_from_points_3d(&posData[0].X, &nrmData[0].X, 2, &pos.X);
        if (error > 0.f)
        {
            error = 1.f / error;
        }

        // Avoid vertices becoming a 'hub' for lots of edges by penalising collapses
        // which will lead to a vertex with degree > 10
        const int32 penalty = FMath::Max(0, degree-10);
        error += penalty * (Options.MaxError * 0.1f);

        if (error > Options.MaxError)
        {
            continue;
        }

        collapseValid.Emplace(i);

        collapseNormal[i] = (vMin.Normal+vMax.Normal) * 0.5f;
        collapsePosition[i] = pos;

        if (error < minEdgeCost[edge.min_])
        {
            minEdgeCost[edge.min_] = error;
            collapseEdgeID[edge.min_] = i;
        }

        if (error < minEdgeCost[edge.max_])
        {
            minEdgeCost[edge.max_] = error;
            collapseEdgeID[edge.max_] = i;
        }

        validCollapses++;
    }

    return validCollapses;
}

void FPMUMeshSimplifierQEF::CollapseEdges(
    TArray<FVertex>& vertices,
    TArray<int32>& collapseTarget,
    const TArray<int32>& collapseValid,
    const TArray<FEdge>& edges,
    const TArray<int32>& collapseEdgeID,
    const TArray<FVector>& collapsePositions,
    const TArray<FVector>& collapseNormal
    )
{
    int32 countCollapsed = 0;
    int32 countCandidates = 0;

    for (int32 i: collapseValid)
    {
        const FEdge& edge(edges[i]);

        countCandidates++;

        if (collapseEdgeID[edge.min_] == i && collapseEdgeID[edge.max_] == i)
        {
            countCollapsed++;

            collapseTarget[edge.max_] = edge.min_;
            vertices[edge.min_].Position = collapsePositions[i];
            vertices[edge.min_].Normal = collapseNormal[i];
        }
    }
}

int32 FPMUMeshSimplifierQEF::RemoveTriangles(
    TArray<FTri>& tris,
    TArray<FTri>& triBuffer,
    TArray<int32>& vertexTriangleCounts,
    const TArray<FVertex>& vertices,
    const TArray<int32>& collapseTarget
    )
{
    int32 removedCount = 0;

    vertexTriangleCounts.Reset();
    vertexTriangleCounts.SetNumZeroed(vertices.Num());

    triBuffer.Reset();

    for (FTri& tri: tris)
    {
        for (int32 j=0; j<3; j++)
        {
            const int32 t = collapseTarget[tri.indices_[j]];
            if (t != -1)
            {
                tri.indices_[j] = t;
            }
        }

        // Skip triangles with duplicate indices
        if (tri.indices_[0] == tri.indices_[1] ||
            tri.indices_[0] == tri.indices_[2] ||
            tri.indices_[1] == tri.indices_[2]
            )
        {
            removedCount++;
            continue;
        }

        const uint32* indices = tri.indices_;
        for (int32 index=0; index<3; index++)
        {
            vertexTriangleCounts[indices[index]] += 1;
        }

        triBuffer.Emplace(tri);
    }

    Swap(tris, triBuffer);

    return removedCount;
}

void FPMUMeshSimplifierQEF::RemoveEdges(
    TArray<FEdge>& edges,
    TArray<FEdge>& edgeBuffer,
    const TArray<int32>& collapseTarget
    )
{
    edgeBuffer.Reset();
    for (FEdge& edge: edges)
    {
        int32 t = collapseTarget[edge.min_];
        if (t != -1)
        {
            edge.min_ = t;
        }

        t = collapseTarget[edge.max_];
        if (t != -1)
        {
            edge.max_ = t;
        }

        if (edge.min_ != edge.max_)
        {
            edgeBuffer.Emplace(edge);
        }
    }

    Swap(edges, edgeBuffer);
}

void FPMUMeshSimplifierQEF::CompactVertices(
	TArray<FVertex>& vertices,
    TArray<FTri>& triangles
    )
{
	TArray<bool> vertexUsed;
	vertexUsed.Init(false, vertices.Num());

	for (int32 i=0; i<triangles.Num(); i++)
	{
		const FTri& tri(triangles[i]);

		vertexUsed[tri.indices_[0]] = true;
		vertexUsed[tri.indices_[1]] = true;
		vertexUsed[tri.indices_[2]] = true;
	}

	TArray<FVertex> compactVertices;
	TArray<uint32> remappedVertexIndices;

    compactVertices.Reserve(vertices.Num());
	remappedVertexIndices.Init(~0U, vertices.Num());

	for (int32 i=0; i<vertices.Num(); i++)
	{
		if (vertexUsed[i])
		{
			remappedVertexIndices[i] = compactVertices.Num();
			compactVertices.Emplace(vertices[i]);
		}
	}

	for (int32 i=0; i<triangles.Num(); i++)
	{
		FTri& tri(triangles[i]);
		
		for (int32 j=0; j<3; j++)
		{
			const uint32 updatedIndex = remappedVertexIndices[tri.indices_[j]];
			tri.indices_[j] = updatedIndex;
		}
	}

    vertices = MoveTemp(compactVertices);
}

void FPMUMeshSimplifier::SimplifyMeshSection(FPMUMeshSectionRef SectionRef, const FPMUMeshSimplifierOptions& Options)
{
    // Empty section, abort
    if (! SectionRef.HasGeometry())
    {
        return;
    }

    FPMUMeshSection& Section(*SectionRef.SectionPtr);
    const TArray<FVector>& Positions(Section.Positions);
    const TArray<uint32>& Indices(Section.Indices);

    TArray<FVector> Normals;
    Normals.SetNumUninitialized(Section.Tangents.Num()/2);

    for (int32 i=0; i<Normals.Num(); ++i)
    {
        FPackedNormal PackedNormal;
        PackedNormal.Vector.Packed = Section.Tangents[i*2+1];

        Normals[i] = PackedNormal.ToFVector();
    }

    TArray<FVector> DstPositions;
    TArray<FVector> DstNormals;
    TArray<uint32> DstIndices;

    FPMUMeshSimplifierQEF::Simplify(
        DstPositions,
        DstNormals,
        DstIndices,
        Positions,
        Normals,
        Indices,
        Options
        );

    Section.Positions = DstPositions;
    Section.Tangents.SetNum(DstNormals.Num()*2, true);
    Section.Indices = DstIndices;

    for (int32 i=0; i<DstNormals.Num(); ++i)
    {
        FPackedNormal PackedNormal(FVector4(DstNormals[i], 1.f));
        Section.Tangents[i*2+1] = PackedNormal.Vector.Packed;
    }
}
