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

#include "ProfilingDebugging/ScopedTimers.h"

#define QEF_INCLUDE_IMPL
#include "qef_simd.h"
#undef QEF_INCLUDE_IMPL

class FPMUScopeTimer : public FScopedDurationTimer
{
public:
    FPMUScopeTimer()
        : FScopedDurationTimer(AccumulatorValue)
        , AccumulatorValue(0)
    {
    }

    double GetTime()
    {
        return AccumulatorValue;
    }

private:
    double AccumulatorValue;
};

const int32 FPMUMeshSimplifierQEF::MAX_TRIANGLES_PER_VERTEX;

void FPMUMeshSimplifierQEF::SanitizeOptions(FPMUMeshSimplifierOptions& Options)
{
    Options.EdgeFraction = FMath::Max(0.f, Options.EdgeFraction);
    Options.MaxIteration = FMath::Max(0, Options.MaxIteration);
    Options.TargetPercentage = FMath::Max(0.f, Options.TargetPercentage);
}

void FPMUMeshSimplifierQEF::Simplify(
    TArray<FVector>& DstPositions,
    TArray<FVector>& DstNormals,
    TArray<uint32>& DstIndices,
    const TArray<FVector>& SrcPositions,
    const TArray<FVector>& SrcNormals,
    const TArray<uint32>& SrcIndices,
    FPMUMeshSimplifierOptions Options
    )
{
    if (SrcPositions.Num() < 16 || SrcIndices.Num() < 48)
    {
        return;
    }

    bool bHasNormals = SrcPositions.Num() == SrcNormals.Num();

    SanitizeOptions(Options);

    FPMUScopeTimer TIMER_BuildCandidateEdges;
    FPMUScopeTimer TIMER_FindValidCollapses;
    FPMUScopeTimer TIMER_CollapseEdges;
    FPMUScopeTimer TIMER_GatherCollapsedTriangles;
    FPMUScopeTimer TIMER_GatherCollapsedEdges;
    FPMUScopeTimer TIMER_CompactVertices;

    // Copy source geometry

    TArray<uint32> Triangles(SrcIndices);
    TArray<FVector> Positions(SrcPositions);
    TArray<FVector> Normals;

    TArray<FEdge> Edges;
    TArray<uint32> VertexTriangleCounts;

    if (bHasNormals)
    {
        Normals = SrcNormals;
    }

    // Build candidate edges

    TIMER_BuildCandidateEdges.Start();

    BuildCandidateEdges(
        Edges,
        VertexTriangleCounts,
        Triangles,
        Positions.Num()
        );

    TIMER_BuildCandidateEdges.Stop();

    // Collapse data containers

    TArray<uint32> CollapseCandidates;
    TArray<uint32> CollapseEdgeID;
    TArray<uint32> CollapseTarget;
    TArray<FVector> CollapsePositions;
    TArray<FVector> CollapseNormals;

    // Reserve fixed size containers

    CollapseCandidates.Reserve(Edges.Num());
    CollapsePositions.SetNumUninitialized(Edges.Num());

    if (bHasNormals)
    {
        CollapseNormals.SetNumUninitialized(Edges.Num());
    }

    const int32 TargetTriangleCount = Triangles.Num() * Options.TargetPercentage;
    const int32 MaxIteration = Options.MaxIteration;

    // Collapse Iteration

    for (int32 i=0; i<MaxIteration; ++i)
    {
        const uint32 PreCollapseVertexCount = Positions.Num();

        // Initialize iteration collapse data

        CollapseEdgeID.SetNumUninitialized(PreCollapseVertexCount);
        CollapseTarget.SetNumUninitialized(PreCollapseVertexCount);

        FMemory::Memset(CollapseEdgeID.GetData(), 0xFF, PreCollapseVertexCount*CollapseEdgeID.GetTypeSize());
        FMemory::Memset(CollapseTarget.GetData(), 0xFF, PreCollapseVertexCount*CollapseTarget.GetTypeSize());

        CollapseCandidates.Reset();

        // Find valid collapses

        TIMER_FindValidCollapses.Start();
        const int32 ValidCollapseCount = FindValidCollapses(
            CollapseCandidates, 
            CollapseEdgeID,
            CollapsePositions,
            CollapseNormals,
            Edges,
            Positions,
            Normals,
            VertexTriangleCounts,
            Options
            );
        TIMER_FindValidCollapses.Stop();

        // No valid collapses, break
        if (ValidCollapseCount == 0)
        {
            break;
        }

        // Collapse valid edge geometry

        TIMER_CollapseEdges.Start();
        CollapseEdges(
            Positions,
            Normals,
            CollapseTarget,
            Edges,
            CollapseCandidates,
            CollapseEdgeID,
            CollapsePositions,
            CollapseNormals
            );
        TIMER_CollapseEdges.Stop();

        // Gather collapsed geometry

        const int32 PostCollapseVertexCount = Positions.Num();

        TIMER_GatherCollapsedTriangles.Start();
        GatherCollapsedTriangles(
            Triangles,
            VertexTriangleCounts,
            CollapseTarget,
            PostCollapseVertexCount
            );
        TIMER_GatherCollapsedTriangles.Stop();

        TIMER_GatherCollapsedEdges.Start();
        GatherCollapsedEdges(
            Edges,
            CollapseTarget
            );
        TIMER_GatherCollapsedEdges.Stop();

        // Triangle result count reached target triangle count, break
        if (Triangles.Num() < TargetTriangleCount)
        {
            break;
        }
    }

    // Generate compact geometry and assign output

    TIMER_CompactVertices.Start();

    CompactVertices(Positions, Normals, Triangles);

    DstIndices = MoveTemp(Triangles);
    DstPositions = MoveTemp(Positions);

    if (bHasNormals)
    {
        DstNormals = MoveTemp(Normals);
    }

    TIMER_CompactVertices.Stop();

    UE_LOG(LogTemp,Warning, TEXT("TIMER_BuildCandidateEdges: %f"), TIMER_BuildCandidateEdges.GetTime());
    UE_LOG(LogTemp,Warning, TEXT("TIMER_FindValidCollapses: %f"), TIMER_FindValidCollapses.GetTime());
    UE_LOG(LogTemp,Warning, TEXT("TIMER_CollapseEdges: %f"), TIMER_CollapseEdges.GetTime());
    UE_LOG(LogTemp,Warning, TEXT("TIMER_GatherCollapsedTriangles: %f"), TIMER_GatherCollapsedTriangles.GetTime());
    UE_LOG(LogTemp,Warning, TEXT("TIMER_GatherCollapsedEdges: %f"), TIMER_GatherCollapsedEdges.GetTime());
    UE_LOG(LogTemp,Warning, TEXT("TIMER_CompactVertices: %f"), TIMER_CompactVertices.GetTime());
}

void FPMUMeshSimplifierQEF::BuildCandidateEdges(
    TArray<FEdge>& Edges,
    TArray<uint32>& VertexTriangleCounts,
    const TArray<uint32>& Triangles,
    const int32 VertexCount
    )
{
    Edges.Reserve(Triangles.Num());
    VertexTriangleCounts.SetNumZeroed(VertexCount);

    for (int32 i=0; i<Triangles.Num(); i+=3)
    {
        const uint32 vi0 = Triangles[i  ];
        const uint32 vi1 = Triangles[i+1];
        const uint32 vi2 = Triangles[i+2];

        Edges.Emplace(FMath::Min(vi0, vi1), FMath::Max(vi0, vi1));
        Edges.Emplace(FMath::Min(vi0, vi2), FMath::Max(vi0, vi2));
        Edges.Emplace(FMath::Min(vi1, vi2), FMath::Max(vi1, vi2));

        ++VertexTriangleCounts[vi0];
        ++VertexTriangleCounts[vi1];
        ++VertexTriangleCounts[vi2];
    }

    Edges.Sort(
        [](const FEdge& lhs, const FEdge& rhs)
        {
            return lhs.idx_ < rhs.idx_;
        } );

    TArray<FEdge> FilteredEdges;
    TArray<uint8> BoundaryVerts;

    FilteredEdges.Reserve(Edges.Num());
    BoundaryVerts.SetNumZeroed(VertexCount);

    FEdge prev = Edges[0];
    FEdge curr;

    for (int32 count=1, idx=1; idx<Edges.Num(); idx++)
    {
        curr = Edges[idx];

        if (curr.idx_ != prev.idx_)
        {
            if (count == 1)
            {
                BoundaryVerts[prev.min_] = 1;
                BoundaryVerts[prev.max_] = 1;
            }
            else 
            {
                FilteredEdges.Emplace(prev);
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

    for (FEdge& Edge : FilteredEdges)
    {
        if (!BoundaryVerts[Edge.min_] && !BoundaryVerts[Edge.max_])
        {
            Edges.Emplace(Edge);
        }
    }
}

int32 FPMUMeshSimplifierQEF::FindValidCollapses(
    TArray<uint32>& CollapseCandidates, 
    TArray<uint32>& CollapseEdgeID, 
    TArray<FVector>& CollapsePositions,
    TArray<FVector>& CollapseNormals,
    const TArray<FEdge>& Edges,
    const TArray<FVector>& Positions,
    const TArray<FVector>& Normals,
    const TArray<uint32>& VertexTriangleCounts,
    const FPMUMeshSimplifierOptions& Options
    )
{
    const int32 VertexCount = Positions.Num();
    const int32 EdgeCount = Edges.Num();
    const int32 numRandomEdges = EdgeCount * Options.EdgeFraction;
    const bool bHasNormals = Positions.Num() == Normals.Num();

    FPMUScopeTimer TIMER_RandomEdge;
    FPMUScopeTimer TIMER_EdgeCostInit;

    FPMUScopeTimer TIMER_IterateEdge;
    FPMUScopeTimer TIMER_QEF;
    FPMUScopeTimer TIMER_EdgeCost;

    TIMER_RandomEdge.Start();

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

    TIMER_RandomEdge.Stop();

    TIMER_EdgeCostInit.Start();

#if 0
    TMap<uint32, float> EdgeCostMap;
    EdgeCostMap.Reserve(VertexCount);
#else
    TArray<float> EdgeCostMap;
    EdgeCostMap.Init(BIG_NUMBER, VertexCount);
#endif

    TIMER_EdgeCostInit.Stop();

    const float MaxEdgeSizeSq = Options.MaxEdgeSize * Options.MaxEdgeSize;
    int32 ValidCollapseCount = 0;

    TIMER_IterateEdge.Start();

    for (uint32 EdgeId : randomEdges)
    {
        const FEdge& Edge(Edges[EdgeId]);
        const FVector& vMin(Positions[Edge.min_]);
        const FVector& vMax(Positions[Edge.max_]);
        FVector nMin;
        FVector nMax;

        if (bHasNormals)
        {
            nMin = Normals[Edge.min_];
            nMax = Normals[Edge.max_];
        }
        else
        {
            nMin = FVector::UpVector;
            nMax = FVector::UpVector;
        }

        // Skip edge below angle threshold
        const float EdgeNormalDot = nMin | nMax;
        if (EdgeNormalDot < Options.MinAngleCosine)
        {
            continue;
        }

        // Skip edge above maximum size
        const float EdgeSizeSq = (vMax - vMin).SizeSquared();
        if (EdgeSizeSq > MaxEdgeSizeSq)
        {
            continue;
        }

        // Skip edge with too many end points connection
        const int32 TriCountPerEdge = VertexTriangleCounts[Edge.min_] + VertexTriangleCounts[Edge.max_];
        if (TriCountPerEdge > MAX_TRIANGLES_PER_VERTEX)
        {
            continue;
        }

        FVector CollapsePosition;
        FVector posData[2] = { vMin, vMax };
        FVector nrmData[2] = { nMin, nMax };

        TIMER_QEF.Start();

        // Solve quadratic error function for points
        float EdgeCost;
        EdgeCost = qef_solve_from_points_3d(&posData[0].X, &nrmData[0].X, 2, &CollapsePosition.X);
        EdgeCost = (EdgeCost > 0.f) ? (1.f/EdgeCost) : 0.f;

        TIMER_QEF.Stop();

        // Avoid vertices becoming a 'hub' for lots of edges by penalising collapses
        // which will lead to a vertex with TriCountPerEdge > 10
        const int32 penalty = FMath::Max(0, TriCountPerEdge-10);
        EdgeCost += penalty * (Options.MaxError * .1f);

        if (EdgeCost > Options.MaxError)
        {
            continue;
        }

        // Record edge end points minimum edge cost

        TIMER_EdgeCost.Start();

#if 0
        float* PrevEdgeMinCost = EdgeCostMap.Find(Edge.min_);
        float* PrevEdgeMaxCost = EdgeCostMap.Find(Edge.max_);

        if (!PrevEdgeMinCost)
        {
            EdgeCostMap.Emplace(Edge.min_, EdgeCost);
            CollapseEdgeID[Edge.min_] = EdgeId;
        }
        else
        if (EdgeCost < *PrevEdgeMinCost)
        {
            *PrevEdgeMinCost = EdgeCost;
            CollapseEdgeID[Edge.min_] = EdgeId;
        }

        if (!PrevEdgeMaxCost)
        {
            EdgeCostMap.Emplace(Edge.max_, EdgeCost);
            CollapseEdgeID[Edge.max_] = EdgeId;
        }
        else
        if (EdgeCost < *PrevEdgeMaxCost)
        {
            *PrevEdgeMaxCost = EdgeCost;
            CollapseEdgeID[Edge.max_] = EdgeId;
        }
#else
        if (EdgeCost < EdgeCostMap[Edge.min_])
        {
            EdgeCostMap[Edge.min_] = EdgeCost;
            CollapseEdgeID[Edge.min_] = EdgeId;
        }

        if (EdgeCost < EdgeCostMap[Edge.max_])
        {
            EdgeCostMap[Edge.max_] = EdgeCost;
            CollapseEdgeID[Edge.max_] = EdgeId;
        }
#endif

        TIMER_EdgeCost.Stop();

        // Assign collapse result

        CollapseCandidates.Emplace(EdgeId);
        CollapsePositions[EdgeId] = CollapsePosition;

        if (bHasNormals)
        {
            CollapseNormals[EdgeId] = (nMin+nMax) * .5f;
        }

        ++ValidCollapseCount;
    }

    TIMER_IterateEdge.Stop();

    //UE_LOG(LogTemp,Warning, TEXT("TIMER_RandomEdge: %f"), TIMER_RandomEdge.GetTime());
    //UE_LOG(LogTemp,Warning, TEXT("TIMER_EdgeCostInit: %f"), TIMER_EdgeCostInit.GetTime());

    //UE_LOG(LogTemp,Warning, TEXT("TIMER_QEF: %f"), TIMER_QEF.GetTime());
    //UE_LOG(LogTemp,Warning, TEXT("TIMER_EdgeCost: %f"), TIMER_EdgeCost.GetTime());
    //UE_LOG(LogTemp,Warning, TEXT("TIMER_IterateEdge: %f"), TIMER_IterateEdge.GetTime());

    return ValidCollapseCount;
}

void FPMUMeshSimplifierQEF::CollapseEdges(
    TArray<FVector>& Positions,
    TArray<FVector>& Normals,
    TArray<uint32>& CollapseTarget,
    const TArray<FEdge>& Edges,
    const TArray<uint32>& CollapseCandidates,
    const TArray<uint32>& CollapseEdgeID,
    const TArray<FVector>& CollapsePositions,
    const TArray<FVector>& CollapseNormals
    )
{
    const bool bHasNormals = CollapsePositions.Num() == CollapseNormals.Num();

    for (uint32 EdgeId: CollapseCandidates)
    {
        const FEdge& Edge(Edges[EdgeId]);

        if (EdgeId == CollapseEdgeID[Edge.min_] && EdgeId == CollapseEdgeID[Edge.max_])
        {
            CollapseTarget[Edge.max_] = Edge.min_;
            Positions[Edge.min_] = CollapsePositions[EdgeId];

            if (bHasNormals)
            {
                Normals[Edge.min_] = CollapseNormals[EdgeId];
            }
        }
    }
}

int32 FPMUMeshSimplifierQEF::GatherCollapsedTriangles(
    TArray<uint32>& Triangles,
    TArray<uint32>& VertexTriangleCounts,
    const TArray<uint32>& CollapseTarget,
    const int32 VertexCount
    )
{
    int32 RemoveCount = 0;

    VertexTriangleCounts.Reset();
    VertexTriangleCounts.SetNumZeroed(VertexCount);

    TArray<uint32> TriBuffer;
    TriBuffer.Reserve(Triangles.Num());

    for (int32 i0=0; i0<Triangles.Num(); i0+=3)
    {
        int32 i1 = i0+1;
        int32 i2 = i0+2;

        const uint32 t0 = CollapseTarget[Triangles[i0]];
        const uint32 t1 = CollapseTarget[Triangles[i1]];
        const uint32 t2 = CollapseTarget[Triangles[i2]];

        if (t0 != -1) Triangles[i0] = t0;
        if (t1 != -1) Triangles[i1] = t1;
        if (t2 != -1) Triangles[i2] = t2;

        uint32 vi0 = Triangles[i0];
        uint32 vi1 = Triangles[i1];
        uint32 vi2 = Triangles[i2];

        // Skip triangles with duplicate indices
        if (vi0 == vi1 ||
            vi0 == vi2 ||
            vi1 == vi2
            )
        {
            RemoveCount++;
            continue;
        }

        ++VertexTriangleCounts[vi0];
        ++VertexTriangleCounts[vi1];
        ++VertexTriangleCounts[vi2];

        TriBuffer.Emplace(vi0);
        TriBuffer.Emplace(vi1);
        TriBuffer.Emplace(vi2);
    }

    Triangles = MoveTemp(TriBuffer);

    return RemoveCount;
}

void FPMUMeshSimplifierQEF::GatherCollapsedEdges(
    TArray<FEdge>& Edges,
    const TArray<uint32>& CollapseTarget
    )
{
    TArray<FEdge> EdgeBuffer;
    EdgeBuffer.Reserve(Edges.Num());

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

    Edges = MoveTemp(EdgeBuffer);
}

void FPMUMeshSimplifierQEF::CompactVertices(
	TArray<FVector>& Positions,
	TArray<FVector>& Normals,
    TArray<uint32>& Triangles
    )
{
    const int32 VertexCount = Positions.Num();
    const bool bHasNormals = Positions.Num() == Normals.Num();

    // Gather used vertex list

	TArray<uint8> VertexUsed;
	VertexUsed.SetNumZeroed(VertexCount);

	for (int32 i=0; i<Triangles.Num(); ++i)
	{
		VertexUsed[Triangles[i]] = 1;
	}

    // Create compact containers

	TArray<FVector> CompactPositions;
	TArray<FVector> CompactNormals;
	TArray<uint32> VertexIndexRemapTable;

    CompactPositions.Reserve(VertexCount);
	VertexIndexRemapTable.SetNumUninitialized(VertexCount);

    if (bHasNormals)
    {
        CompactNormals.Reserve(VertexCount);
    }

    // Generate compact geometry

	for (int32 i=0; i<VertexCount; i++)
	{
		if (VertexUsed[i])
		{
			VertexIndexRemapTable[i] = CompactPositions.Num();
			CompactPositions.Emplace(Positions[i]);

            if (bHasNormals)
            {
                CompactNormals.Emplace(Normals[i]);
            }
		}
	}

    // Remap triangle indices

	for (int32 i=0; i<Triangles.Num(); ++i)
	{
        Triangles[i] = VertexIndexRemapTable[Triangles[i]];
	}

    // Move output

    Positions = MoveTemp(CompactPositions);

    if (bHasNormals)
    {
        Normals = MoveTemp(CompactNormals);
    }
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

    {
        FScopedDurationTimeLogger Timer(TEXT("FPMUMeshSimplifier::SimplifyMeshSection()"));

        FPMUMeshSimplifierQEF::Simplify(
            DstPositions,
            DstNormals,
            DstIndices,
            Positions,
            Normals,
            Indices,
            Options
            );
    }

    Section.Positions = DstPositions;
    Section.Tangents.SetNum(DstNormals.Num()*2, true);
    Section.Indices = DstIndices;

    for (int32 i=0; i<DstNormals.Num(); ++i)
    {
        FPackedNormal PackedNormal(FVector4(DstNormals[i], 1.f));
        Section.Tangents[i*2+1] = PackedNormal.Vector.Packed;
    }
}
