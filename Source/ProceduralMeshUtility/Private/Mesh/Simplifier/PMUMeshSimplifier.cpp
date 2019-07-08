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

#include "Mesh/Simplifier/pcg_basic.h"

#define QEF_INCLUDE_IMPL
#include "qef_simd.h"
#undef QEF_INCLUDE_IMPL

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
    if (SrcPositions.Num() < 16 || SrcIndices.Num() < 48)
    {
        return;
    }

    TArray<FVector> Positions(SrcPositions);
    TArray<FVector> Normals(SrcNormals);
    TArray<uint32> Triangles(SrcIndices);

    TArray<FEdge> Edges;
    Edges.Reserve(Triangles.Num());

    BuildCandidateEdges(
        Edges,
        Triangles,
        Positions.Num()
        );

    TArray<FEdge> EdgeBuffer;
    TArray<uint32> TriBuffer;

    EdgeBuffer.Reserve(Edges.Num());
    TriBuffer.Reserve(Triangles.Num());

    // Per vertex triangle count
    TArray<int32> VertexTriangleCounts;
    VertexTriangleCounts.SetNumZeroed(Positions.Num());

    for (int32 i=0; i<Triangles.Num(); i++)
    {
        ++VertexTriangleCounts[Triangles[i]];
    }

    const int32 targetTriangleCount = Triangles.Num() * Options.TargetPercentage;
    const int32 maxIterations = Options.MaxIterations;

    TArray<FVector> CollapsePositions;
    TArray<FVector> CollapseNormals;
    TArray<uint32> CollapseValid;
    TArray<uint32> CollapseEdgeID;
    TArray<uint32> CollapseTarget;

    CollapsePositions.SetNumUninitialized(Edges.Num());
    CollapseNormals.SetNumUninitialized(Edges.Num());
    CollapseValid.Reserve(Edges.Num());
    CollapseEdgeID.Reserve(Positions.Num());
    CollapseTarget.Reserve(Positions.Num());

    for (int32 i=0; i<maxIterations; ++i)
    {
        const uint32 VertexCount = Positions.Num();

        CollapseEdgeID.SetNumUninitialized(VertexCount);
        CollapseTarget.SetNumUninitialized(VertexCount);

        FMemory::Memset(CollapseEdgeID.GetData(), 0xFF, VertexCount*CollapseEdgeID.GetTypeSize());
        FMemory::Memset(CollapseTarget.GetData(), 0xFF, VertexCount*CollapseTarget.GetTypeSize());

        CollapseValid.Reset();

        const int32 countValidCollapse = FindValidCollapses(
            CollapseValid, 
            CollapseEdgeID,
            CollapsePositions,
            CollapseNormals,
            Edges,
            Positions,
            Normals,
            VertexTriangleCounts,
            Options
            );

        // No valid collapses, break
        if (countValidCollapse == 0)
        {
            break;
        }

        CollapseEdges(
            Positions,
            Normals,
            CollapseTarget,
            Edges,
            CollapseValid,
            CollapseEdgeID,
            CollapsePositions,
            CollapseNormals
            );

        RemoveTriangles(
            Triangles,
            TriBuffer,
            VertexTriangleCounts,
            CollapseTarget,
            Positions.Num()
            );

        RemoveEdges(
            Edges,
            EdgeBuffer,
            CollapseTarget
            );

        // Triangle result count reached target triangle count, break
        if (Triangles.Num() < targetTriangleCount)
        {
            break;
        }
    }

    CompactVertices(Positions, Normals, Triangles);

    DstPositions = Positions;
    DstNormals = Normals;
    DstIndices = Triangles;
}

void FPMUMeshSimplifierQEF::BuildCandidateEdges(
    TArray<FEdge>& Edges,
    const TArray<uint32>& Triangles,
    const int32 VertexCount
    )
{
    for (int32 i=0; i<Triangles.Num(); i+=3)
    {
        const uint32 vi0 = Triangles[i  ];
        const uint32 vi1 = Triangles[i+1];
        const uint32 vi2 = Triangles[i+2];
        Edges.Emplace(FMath::Min(vi0, vi1), FMath::Max(vi0, vi1));
        Edges.Emplace(FMath::Min(vi0, vi2), FMath::Max(vi0, vi2));
        Edges.Emplace(FMath::Min(vi1, vi2), FMath::Max(vi1, vi2));
    }

    Edges.Sort(
        [](const FEdge& lhs, const FEdge& rhs)
        {
            return lhs.idx_ < rhs.idx_;
        } );

    TArray<FEdge> filteredEdges;
    TArray<uint8> boundaryVerts;

    filteredEdges.Reserve(Edges.Num());
    boundaryVerts.SetNumZeroed(VertexCount);

    FEdge prev = Edges[0];

    for (int32 count=1, idx=1; idx<Edges.Num(); idx++)
    {
        const FEdge curr = Edges[idx];

        if (curr.idx_ != prev.idx_)
        {
            if (count == 1)
            {
                boundaryVerts[prev.min_] = 1;
                boundaryVerts[prev.max_] = 1;
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

    Edges.Reset();

    for (FEdge& Edge: filteredEdges)
    {
        if (!boundaryVerts[Edge.min_] && !boundaryVerts[Edge.max_])
        {
            Edges.Emplace(Edge);
        }
    }
}

int32 FPMUMeshSimplifierQEF::FindValidCollapses(
    TArray<uint32>& CollapseValid, 
    TArray<uint32>& CollapseEdgeID, 
    TArray<FVector>& CollapsePositions,
    TArray<FVector>& CollapseNormals,
    const TArray<FEdge>& Edges,
    const TArray<FVector>& Positions,
    const TArray<FVector>& Normals,
    const TArray<int32>& VertexTriangleCounts,
    const FPMUMeshSimplifierOptions& Options
    )
{
    const int32 EdgeCount = Edges.Num();
    const int32 numRandomEdges = EdgeCount * Options.EdgeFraction;

    FPMUPCG::FRNG Rand;
    FPMUPCG::pcg32_srandom_r(&Rand, Options.Seed, 1337U);

    TArray<uint32> randomEdges;
    randomEdges.SetNumZeroed(numRandomEdges);

    for (int32 i=0; i<numRandomEdges; i++)
    {
        randomEdges[i] = FPMUPCG::pcg32_boundedrand_r(&Rand, EdgeCount);
    }

    // Sort the edges to improve locality
    randomEdges.Sort();

    TArray<float> minEdgeCost;
    minEdgeCost.Init(BIG_NUMBER, Positions.Num());

    int32 validCollapses = 0;

    for (uint32 EdgeId : randomEdges)
    {
        const FEdge& Edge(Edges[EdgeId]);
        const FVector& vMin(Positions[Edge.min_]);
        const FVector& vMax(Positions[Edge.max_]);
        const FVector& nMin(Normals[Edge.min_]);
        const FVector& nMax(Normals[Edge.max_]);

        // Prevent collapses below angle threshold
        const float cosAngle = nMin | nMax;
        if (cosAngle < Options.MinAngleCosine)
        {
            continue;
        }

        // Prevent collapses above maximum edge size
        const float edgeSizeSq = (vMax - vMin).SizeSquared();
        if (edgeSizeSq > (Options.MaxEdgeSize * Options.MaxEdgeSize))
        {
            continue;
        }

        const int32 degree = VertexTriangleCounts[Edge.min_] + VertexTriangleCounts[Edge.max_];
        if (degree > COLLAPSE_MAX_DEGREE)
        {
            continue;
        }

        //__declspec(align(16)) float pos[4];
        FVector pos;
        //FVertex data[2] = { vMin, vMax };
        FVector posData[2] = { vMin, vMax };
        FVector nrmData[2] = { nMin, nMax };

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

        CollapseValid.Emplace(EdgeId);

        CollapseNormals[EdgeId] = (nMin+nMax) * 0.5f;
        CollapsePositions[EdgeId] = pos;

        if (error < minEdgeCost[Edge.min_])
        {
            minEdgeCost[Edge.min_] = error;
            CollapseEdgeID[Edge.min_] = EdgeId;
        }

        if (error < minEdgeCost[Edge.max_])
        {
            minEdgeCost[Edge.max_] = error;
            CollapseEdgeID[Edge.max_] = EdgeId;
        }

        ++validCollapses;
    }

    return validCollapses;
}

void FPMUMeshSimplifierQEF::CollapseEdges(
    TArray<FVector>& Positions,
    TArray<FVector>& Normals,
    TArray<uint32>& CollapseTarget,
    const TArray<FEdge>& Edges,
    const TArray<uint32>& CollapseValid,
    const TArray<uint32>& CollapseEdgeID,
    const TArray<FVector>& CollapsePositions,
    const TArray<FVector>& CollapseNormals
    )
{
    for (uint32 EdgeId: CollapseValid)
    {
        const FEdge& Edge(Edges[EdgeId]);

        if (CollapseEdgeID[Edge.min_] == EdgeId && CollapseEdgeID[Edge.max_] == EdgeId)
        {
            CollapseTarget[Edge.max_] = Edge.min_;
            Positions[Edge.min_] = CollapsePositions[EdgeId];
            Normals[Edge.min_] = CollapseNormals[EdgeId];
        }
    }
}

int32 FPMUMeshSimplifierQEF::RemoveTriangles(
    TArray<uint32>& Tris,
    TArray<uint32>& TriBuffer,
    TArray<int32>& VertexTriangleCounts,
    const TArray<uint32>& CollapseTarget,
    const int32 VertexCount
    )
{
    int32 removedCount = 0;

    VertexTriangleCounts.Reset();
    VertexTriangleCounts.SetNumZeroed(VertexCount);

    TriBuffer.Reset();

    for (int32 i0=0; i0<Tris.Num(); i0+=3)
    {
        int32 i1 = i0+1;
        int32 i2 = i0+2;

        const uint32 t0 = CollapseTarget[Tris[i0]];
        const uint32 t1 = CollapseTarget[Tris[i1]];
        const uint32 t2 = CollapseTarget[Tris[i2]];

        if (t0 != -1) Tris[i0] = t0;
        if (t1 != -1) Tris[i1] = t1;
        if (t2 != -1) Tris[i2] = t2;

        uint32 vi0 = Tris[i0];
        uint32 vi1 = Tris[i1];
        uint32 vi2 = Tris[i2];

        // Skip triangles with duplicate indices
        if (vi0 == vi1 ||
            vi0 == vi2 ||
            vi1 == vi2
            )
        {
            removedCount++;
            continue;
        }

        ++VertexTriangleCounts[vi0];
        ++VertexTriangleCounts[vi1];
        ++VertexTriangleCounts[vi2];

        TriBuffer.Emplace(vi0);
        TriBuffer.Emplace(vi1);
        TriBuffer.Emplace(vi2);
    }

    Swap(Tris, TriBuffer);

    return removedCount;
}

void FPMUMeshSimplifierQEF::RemoveEdges(
    TArray<FEdge>& Edges,
    TArray<FEdge>& EdgeBuffer,
    const TArray<uint32>& CollapseTarget
    )
{
    EdgeBuffer.Reset();

    for (FEdge& Edge: Edges)
    {
        uint32 t;

        t = CollapseTarget[Edge.min_];
        if (t != ~0U)
        {
            Edge.min_ = t;
        }

        t = CollapseTarget[Edge.max_];
        if (t != ~0U)
        {
            Edge.max_ = t;
        }

        if (Edge.min_ != Edge.max_)
        {
            EdgeBuffer.Emplace(Edge);
        }
    }

    Swap(Edges, EdgeBuffer);
}

void FPMUMeshSimplifierQEF::CompactVertices(
	TArray<FVector>& Positions,
	TArray<FVector>& Normals,
    TArray<uint32>& Triangles
    )
{
    const int32 VertexCount = Positions.Num();

	TArray<uint8> vertexUsed;
	vertexUsed.SetNumZeroed(VertexCount);

	for (int32 i=0; i<Triangles.Num(); ++i)
	{
		vertexUsed[Triangles[i]] = 1;
	}

	TArray<FVector> compactPositions;
	TArray<FVector> compactNormals;
	TArray<uint32> remappedVertexIndices;

    compactPositions.Reserve(VertexCount);
    compactNormals.Reserve(VertexCount);
	remappedVertexIndices.SetNumUninitialized(VertexCount);

	for (int32 i=0; i<VertexCount; i++)
	{
		if (vertexUsed[i])
		{
			remappedVertexIndices[i] = compactPositions.Num();
			compactPositions.Emplace(Positions[i]);
			compactNormals.Emplace(Normals[i]);
		}
	}

	for (int32 i=0; i<Triangles.Num(); ++i)
	{
        Triangles[i] = remappedVertexIndices[Triangles[i]];
	}

    Positions = MoveTemp(compactPositions);
    Normals = MoveTemp(compactNormals);
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
