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

#include "Mesh/PMUMeshPrefabBuilder.h"

#include "ProceduralMeshUtility.h"

UPMUPrefabBuilder::UPMUPrefabBuilder()
    : bPrefabInitialized(false)
{
}

const FStaticMeshVertexBuffers& UPMUPrefabBuilder::GetVertexBuffers(const UStaticMesh& Mesh, int32 LODIndex) const
{
    return Mesh.RenderData->LODResources[LODIndex].VertexBuffers;
}

const FStaticMeshSection& UPMUPrefabBuilder::GetSection(const UStaticMesh& Mesh, int32 LODIndex, int32 SectionIndex) const
{
    return Mesh.RenderData->LODResources[LODIndex].Sections[SectionIndex];
}

FIndexArrayView UPMUPrefabBuilder::GetIndexBuffer(const UStaticMesh& Mesh, int32 LODIndex) const
{
    return Mesh.RenderData->LODResources[LODIndex].IndexBuffer.GetArrayView();
}

void UPMUPrefabBuilder::GetPrefabGeometryCount(const FPrefabData& Prefab, uint32& OutNumVertices, uint32& OutNumIndices) const
{
    check(IsValidPrefab(Prefab));

    const FStaticMeshVertexBuffers& VBs(GetVertexBuffers(Prefab));
    const FPositionVertexBuffer& PositionVB(VBs.PositionVertexBuffer);
    const FStaticMeshSection& Section(GetSection(Prefab));

    OutNumVertices = PositionVB.GetNumVertices();
    OutNumIndices = Section.NumTriangles * 3;
}

bool UPMUPrefabBuilder::IsValidPrefab(const UStaticMesh* Mesh, int32 LODIndex, int32 SectionIndex) const
{
    const int32 L = LODIndex;
    const int32 S = SectionIndex;
    return
        Mesh &&
        Mesh->bAllowCPUAccess &&
        Mesh->RenderData &&
        Mesh->RenderData->LODResources.IsValidIndex(L) && 
        Mesh->RenderData->LODResources[L].Sections.IsValidIndex(S);
}

void UPMUPrefabBuilder::ResetPrefabs()
{
    PreparedPrefabs.Reset();
    bPrefabInitialized = false;
}

void UPMUPrefabBuilder::InitializePrefabs()
{
    if (bPrefabInitialized)
    {
        return;
    }

    // Generate prepared prefab data
    for (const FPMUPrefabInputData& Input : MeshPrefabs)
    {
        // Skip invalid prefabs
        if (! IsValidPrefab(Input))
        {
            continue;
        }

        // Build prepared prefab data

        const UStaticMesh& Mesh(*Input.Mesh);

        const int32 LODIndex = Input.LODIndex;
        const int32 SectionIndex = Input.SectionIndex;

        const FStaticMeshVertexBuffers& VBs(GetVertexBuffers(Mesh, LODIndex));
        const FPositionVertexBuffer& PositionVB(VBs.PositionVertexBuffer);

        const int32 NumVertices = PositionVB.GetNumVertices();

        // Generate sorted index by distance to X zero

        TArray<uint32> SortedIndexAlongX;

        SortedIndexAlongX.Reserve(NumVertices);

        for (int32 i=0; i<NumVertices; ++i)
        {
            SortedIndexAlongX.Emplace(i);
        }

        SortedIndexAlongX.Sort([&PositionVB](const uint32& a, const uint32& b)
            {
                return PositionVB.VertexPosition(a).X < PositionVB.VertexPosition(b).X;
            } );

        // Generate new prefab data

        PreparedPrefabs.SetNum(PreparedPrefabs.Num()+1);
        FPrefabData& Prefab(PreparedPrefabs.Last());
        FBox Bounds(Mesh.GetBoundingBox());

        Prefab.Mesh = &Mesh;
        Prefab.LODIndex = LODIndex;
        Prefab.SectionIndex = SectionIndex;
        Prefab.Bounds.Min = FVector2D(Bounds.Min);
        Prefab.Bounds.Max = FVector2D(Bounds.Max);
        Prefab.Length = Prefab.Bounds.GetSize().X;
        Prefab.SortedIndexAlongX = MoveTemp(SortedIndexAlongX);

        check(Prefab.Length > 0.f);
    }

    // Generated sorted prefabs by size along X

    SortedPrefabs.Reserve(GetPreparedPrefabCount());

    for (int32 i=0; i<GetPreparedPrefabCount(); ++i)
    {
        SortedPrefabs.Emplace(i);
    }

    SortedPrefabs.Sort([this](const uint32& a, const uint32& b)
        {
            return PreparedPrefabs[a].Length < PreparedPrefabs[b].Length;
        } );
}

void UPMUPrefabBuilder::BuildPrefabsAlongPoly(const TArray<FVector2D>& Points, bool bClosedPoly)
{
    InitializePrefabs();

    // Empty prepared prefab or not enough points, abort
    if (GetPreparedPrefabCount() < 1 || Points.Num() < 3)
    {
        return;
    }

    TArray<FVector2D> LineTangents;
    TArray<float> LineDistances;

    float Distance = 0.f;

    for (int32 i=1; i<Points.Num(); ++i)
    {
        const FVector2D& P0(Points[i-1]);
        const FVector2D& P1(Points[i]);
        const FVector2D P01(P1-P0);
        FVector2D Tangent;
        float Length;
        P01.ToDirectionAndLength(Tangent, Length);
        LineTangents.Emplace(Tangent);
        LineDistances.Emplace(Distance);
        Distance += Length;
    }
    if (bClosedPoly)
    {
        LineTangents.Emplace(LineTangents[0]);
        LineDistances.Emplace(Distance);
    }
    else
    {
        LineTangents.Emplace(LineTangents.Last());
        LineDistances.Emplace(Distance);
    }

    check(Points.Num() == LineTangents.Num());
    check(Points.Num() == LineDistances.Num());

    BuildPrefabsAlongPoly(Points, LineTangents, LineDistances, bClosedPoly);
}

void UPMUPrefabBuilder::BuildPrefabsAlongPoly(const TArray<FVector2D>& Positions, const TArray<FVector2D>& Tangents, const TArray<float>& Distances, bool bClosedPoly)
{
    int32 PointCount = Positions.Num();

    // Point buffers must match
    if (Tangents.Num() != PointCount || Distances.Num() != PointCount)
    {
        return;
    }

    check(PointCount > 1);
    check(! bClosedPoly || PointCount > 3);

    const bool bHasDecorator = HasDecorator();
    const float EndDistance = Distances.Last();

    const FPrefabData& ShortestPrefab(GetSortedPrefab(0));
    const float ShortestPrefabLength = ShortestPrefab.Length;

    // Only build prefabs on edge list longer than the shortest prefab
    if (EndDistance < ShortestPrefabLength)
    {
        return;
    }

    FPrefabBufferMap PrefabBufferMap;

    // Reset section data
    GeneratedSection.Reset();

    // Generate required generated prefab list

    struct FPrefabAllocation
    {
        const FPrefabData* Prefab;
        float Scale;
    };

    TArray<FPrefabAllocation> PrefabList;
    PrefabList.Reserve(EndDistance/ShortestPrefabLength);

    uint32 NumVertices = 0;
    uint32 NumIndices = 0;

    // Generate prefab for each occupied distance

    {
        float OccupiedDistance = 0.f;

        enum { DEFAULT_PREFAB = 0 };
        static const float PREFAB_MINIMUM_SCALE = .4f;

        while (OccupiedDistance < EndDistance)
        {
            const FPrefabData& Prefab(GetSortedPrefab(DEFAULT_PREFAB));
            const float PrefabLength = Prefab.Length;
            float Scale = 1.f;

            OccupiedDistance += PrefabLength;

            // Prefab go past end distance, scale down
            if (OccupiedDistance >= EndDistance)
            {
                float InvLength = 1.f/PrefabLength;
                float DeltaDist = EndDistance-(OccupiedDistance-PrefabLength);
                Scale = DeltaDist*InvLength;
                Scale = FMath::Max(0.f, Scale);

                // Scale is below threshold, scale up last prefab and break
                if (Scale < PREFAB_MINIMUM_SCALE)
                {
                    check(PrefabList.Num() > 0);
                    FPrefabAllocation& LastPrefab(PrefabList.Last());
                    // Calculate scale up amount
                    InvLength = 1.f/LastPrefab.Prefab->Length;
                    Scale = 1.f+DeltaDist*InvLength;
                    LastPrefab.Scale = Scale;
                    break;
                }
            }

            // Assign new prefab allocation data

            FPrefabAllocation PrefabAlloc;
            PrefabAlloc.Prefab = &Prefab;
            PrefabAlloc.Scale = Scale;
            PrefabList.Emplace(PrefabAlloc);

            uint32 PrefabNumVertices;
            uint32 PrefabNumIndices;

            GetPrefabGeometryCount(Prefab, PrefabNumVertices, PrefabNumIndices);

            NumVertices += PrefabNumVertices;
            NumIndices += PrefabNumIndices;
        }
    }

    AllocateSection(GeneratedSection, NumVertices, NumIndices);

    // Transform and copy prefab buffers to output sections

    float CurrentDistance = Distances[0];
    int32 PointIndex = 0;

    for (int32 PrefabIndex=0; PrefabIndex<PrefabList.Num(); ++PrefabIndex)
    {
        const FPrefabAllocation& PrefabAlloc(PrefabList[PrefabIndex]);
        const FPrefabData& Prefab(*PrefabAlloc.Prefab);
        float PrefabScaleX(PrefabAlloc.Scale);

        // Copy prefab to section

        uint32 VertexOffsetIndex = CopyPrefabToSection(GeneratedSection, PrefabBufferMap, Prefab);

        // Transform positions along edges

        const FPrefabBuffers& PrefabBuffers(PrefabBufferMap.FindChecked(&Prefab));
        const TArray<FVector>& SrcPositions(PrefabBuffers.Positions);
        const TArray<uint32>& IndexAlongX(Prefab.SortedIndexAlongX);

        TArray<FVector>& DstPositions(GeneratedSection.Positions);

        float StartDistance = CurrentDistance;

        for (int32 vix=0; vix<IndexAlongX.Num(); ++vix)
        {
            const uint32 SrcIndex = IndexAlongX[vix];
            const uint32 DstIndex = VertexOffsetIndex+SrcIndex;
            const FVector SrcPos(SrcPositions[SrcIndex]);

            CurrentDistance = StartDistance+SrcPos.X*PrefabScaleX;
            const float DistanceThreshold = CurrentDistance+KINDA_SMALL_NUMBER;

            // Find current edge point along prefab mesh X

            for (int32 di=PointIndex; (di<PointCount && DistanceThreshold>Distances[di]); ++di)
            {
                PointIndex = di;
            }

            // Transform vertex

#if 1
            int32 pi0, pi1, pi2;
            pi1 = PointIndex;

            if (bClosedPoly)
            {
                pi0 = (pi1>0) ? (pi1-1) : PointCount-2;
                pi2 = (pi1+1) % PointCount;
            }
            else
            {
                pi0 = FMath::Max(0, pi1-1);
                pi2 = FMath::Min(pi1+1, PointCount-1);
            }

            const float Dist1 = Distances[pi1];
            const float Dist2 = Distances[pi2];
            const float Dist12 = Dist2-Dist1;
            const float Dist1P = CurrentDistance-Dist1;
            float DistAlpha = Dist12 > SMALL_NUMBER ? Dist1P/Dist12 : 0.f;

            //UE_LOG(LogTemp,Warning, TEXT("%d %d %d %d %d %f"), pi0, pi1, pi2, Dist12 > KINDA_SMALL_NUMBER, Dist12 > SMALL_NUMBER, Dist1P);

            FVector2D Tangent0(Tangents[pi0]);
            FVector2D Tangent1(Tangents[pi1]);
            FVector2D InterpTangent;
            InterpTangent = FMath::LerpStable(Tangent0, Tangent1, DistAlpha);
            InterpTangent.Normalize();

            FVector2D Offset(Positions[pi1]);
            FVector2D TransformedPoint(-InterpTangent.Y, InterpTangent.X);
            TransformedPoint *= SrcPos.Y;
            TransformedPoint += Offset+InterpTangent*Dist1P;

            DstPositions[DstIndex] = FVector(TransformedPoint, SrcPos.Z);
#else
            float DistanceDelta = CurrentDistance - Distances[PointIndex];
            FVector2D Tangent(Tangents[PointIndex]);
            FVector2D Offset(Positions[PointIndex]);
            FVector2D TransformedPoint(-Tangent.Y, Tangent.X);
            TransformedPoint *= SrcPos.Y;
            TransformedPoint += Offset+Tangent*DistanceDelta;
            DstPositions[DstIndex] = FVector(TransformedPoint, SrcPos.Z);
#endif

            // Expand bounding box
            GeneratedSection.SectionLocalBox += DstPositions[DstIndex];

            // Call decorator on position transform
            if (bHasDecorator)
            {
                Decorator->OnTransformPosition(GeneratedSection, DstIndex);
            }
        }
    }
}

void UPMUPrefabBuilder::AllocateSection(FPMUMeshSection& OutSection, uint32 NumVertices, uint32 NumIndices)
{
    OutSection.Positions.Reserve(NumVertices);
    OutSection.UVs.Reserve(NumVertices);
    OutSection.Tangents.Reserve(NumVertices*2);
    OutSection.Indices.Reserve(NumIndices);
}

uint32 UPMUPrefabBuilder::CopyPrefabToSection(FPMUMeshSection& OutSection, FPrefabBufferMap& PrefabBufferMap, const FPrefabData& Prefab)
{
    check(IsValidPrefab(Prefab));

    FPrefabBuffers* PrefabBuffers = PrefabBufferMap.Find(&Prefab);

    // Prefab buffers have not been mapped, create new one
    if (! PrefabBuffers)
    {
        PrefabBufferMap.Emplace(&Prefab, FPrefabBuffers());
        PrefabBuffers = &PrefabBufferMap.FindChecked(&Prefab);

        // Copy vertex buffer

        const FStaticMeshVertexBuffers& VBs(GetVertexBuffers(Prefab));
        const FPositionVertexBuffer& SrcPositions(VBs.PositionVertexBuffer);
        const FStaticMeshVertexBuffer& SrcUVTans(VBs.StaticMeshVertexBuffer);
        const FColorVertexBuffer& SrcColors(VBs.ColorVertexBuffer);
        const uint32 NumVertices = SrcPositions.GetNumVertices();
        const bool bHasUV = SrcUVTans.GetNumTexCoords() > 0;
        const bool bHasColor = SrcColors.GetNumVertices() == NumVertices;

        check(SrcUVTans.GetNumVertices() == NumVertices);

        TArray<FVector>& DstPositions(PrefabBuffers->Positions);
        TArray<uint32>& DstTangents(PrefabBuffers->Tangents);
        TArray<FVector2D>& DstUVs(PrefabBuffers->UVs);
        TArray<FColor>& DstColors(PrefabBuffers->Colors);

        DstPositions.SetNumUninitialized(NumVertices);
        DstTangents.SetNumUninitialized(NumVertices*2);

        if (bHasUV)
        {
            DstUVs.SetNumUninitialized(NumVertices);
        }

        if (bHasColor)
        {
            DstColors.SetNumUninitialized(NumVertices);
        }

        for (uint32 i=0; i<NumVertices; ++i)
        {
            DstPositions[i] = SrcPositions.VertexPosition(i);

            FPackedNormal TangentX(SrcUVTans.VertexTangentX(i));
            FPackedNormal TangentZ(SrcUVTans.VertexTangentZ(i));
            DstTangents[i*2  ] = TangentX.Vector.Packed;
            DstTangents[i*2+1] = TangentZ.Vector.Packed;

            if (bHasUV)
            {
                DstUVs[i] = SrcUVTans.GetVertexUV(i, 0);
            }

            if (bHasColor)
            {
                DstColors[i] = SrcColors.VertexColor(i);
            }
        }

        check(DstPositions.Num() == (DstTangents.Num()/2));

        // Copy index buffer

        const FStaticMeshSection& Section(GetSection(Prefab));

        FIndexArrayView SrcIndexBuffer = GetIndexBuffer(Prefab);
        const uint32 NumIndices = Section.NumTriangles * 3;
        const uint32 OnePastLastIndex = Section.FirstIndex + NumIndices;

        TArray<uint32>& DstIndices(PrefabBuffers->Indices);
        DstIndices.Reserve(NumIndices);

        for (uint32 i=Section.FirstIndex; i<OnePastLastIndex; ++i)
        {
            DstIndices.Emplace(SrcIndexBuffer[i]);
        }
    }

    check(PrefabBuffers != nullptr);

    // Copy prefab from mapped buffers
    uint32 VertexOffsetIndex = CopyPrefabToSection(OutSection, *PrefabBuffers);

    // Call decorator on copy prefab
    if (HasDecorator())
    {
        Decorator->OnCopyPrefabToSection(OutSection, Prefab, VertexOffsetIndex);
    }

    return VertexOffsetIndex;
}

uint32 UPMUPrefabBuilder::CopyPrefabToSection(FPMUMeshSection& OutSection, const FPrefabBuffers& PrefabBuffers)
{
    // Copy buffers

    const TArray<FVector>& SrcPositions(PrefabBuffers.Positions);
    const TArray<FVector2D>& SrcUVs(PrefabBuffers.UVs);
    const TArray<uint32>& SrcTangents(PrefabBuffers.Tangents);
    const TArray<FColor>& SrcColors(PrefabBuffers.Colors);
    const TArray<uint32>& SrcIndices(PrefabBuffers.Indices);

    TArray<FVector>& DstPositions(OutSection.Positions);
    TArray<FVector2D>& DstUVs(OutSection.UVs);
    TArray<uint32>& DstTangents(OutSection.Tangents);
    TArray<FColor>& DstColors(OutSection.Colors);
    TArray<uint32>& DstIndices(OutSection.Indices);

    const uint32 OffsetIndex = DstPositions.Num();
    const uint32 TargetNumVertices = OffsetIndex + SrcPositions.Num();

    // Copy vertex buffers

    DstPositions.Append(SrcPositions);
    DstTangents.Append(SrcTangents);

    check(TargetNumVertices == DstPositions.Num());

    // Copy UV buffer
    if (PrefabBuffers.HasUVs())
    {
        DstUVs.Append(SrcUVs);
    }
    // Fill zeroed UV if prefab mesh have no valid UV buffer
    else
    {
        DstUVs.SetNumZeroed(TargetNumVertices, false);
    }

    // Copy color buffer
    if (PrefabBuffers.HasColors())
    {
        DstColors.Append(SrcColors);
    }
    // Fill zeroed color if prefab mesh have no valid color buffer
    else
    {
        DstColors.SetNumZeroed(TargetNumVertices, false);
    }

    // Ensure all vertex buffers size match
    check(TargetNumVertices == DstUVs.Num());
    check(TargetNumVertices == (DstTangents.Num()/2));
    check(TargetNumVertices == DstColors.Num());

    // Copy index buffer with offset

    const uint32 NumIndices = SrcIndices.Num();

    for (uint32 i=0; i<NumIndices; ++i)
    {
        DstIndices.Emplace(OffsetIndex+SrcIndices[i]);
    }

    return OffsetIndex;
}