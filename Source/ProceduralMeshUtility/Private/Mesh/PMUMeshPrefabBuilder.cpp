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

void UPMUPrefabBuilder::ResetDecorator()
{
    Decorator = NewObject<UPMUPrefabBuilderDecorator>(this);
    Decorator->SetBuilder(this);
}

void UPMUPrefabBuilder::InitializeDecorator()
{
    if (! IsValid(Decorator))
    {
        ResetDecorator();
    }
}

void UPMUPrefabBuilder::GetPrefabGeometryCount(uint32& OutNumVertices, uint32& OutNumIndices, const FPrefabData& Prefab) const
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
        Prefab.AngleThreshold = Input.AngleThreshold;
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
    const int32 PrefabCount = GetPreparedPrefabCount();
    const int32 PointCount = Positions.Num();

    // Point buffers must match
    if (Tangents.Num() != PointCount || Distances.Num() != PointCount)
    {
        return;
    }

    InitializeDecorator();

    check(PrefabCount > 0);
    check(PointCount > 1);
    check(! bClosedPoly || PointCount > 3);
    check(IsValid(Decorator));

    const float EndDistance = Distances.Last();

    const FPrefabData& ShortestPrefab(GetSortedPrefab(0));
    const float ShortestPrefabLength = ShortestPrefab.Length;

    // Only build prefabs on edge list longer than the shortest prefab
    if (EndDistance < ShortestPrefabLength)
    {
        return;
    }

    // Allocate sections

    Decorator->OnAllocateSections();

    struct FSectionAllocation
    {
        uint32 NumVertices;
        uint32 NumIndices;
    };

    TArray<FSectionAllocation> SectionList;

    // Make sure there is at least single section
    if (GeneratedSections.Num() < 1)
    {
        GeneratedSections.SetNum(1);
    }

    // Generate section allocation data and reset section geometry data
    for (FPMUMeshSection& Section : GeneratedSections)
    {
        FSectionAllocation AllocationData = { 0, 0 };
        SectionList.Emplace(AllocationData);

        Section.Reset();
    }

    // Generate required generated prefab list

    struct FPrefabAllocation
    {
        const FPrefabData* Prefab;
        float Scale;
        int32 SectionIndex;
    };

    TArray<FPrefabAllocation> PrefabList;
    PrefabList.Reserve(EndDistance/ShortestPrefabLength);

    // Generate prefab for each occupied distance

    {
        int32 LastOccupiedPointIndex = 0;
        float OccupiedDistance = 0.f;

        static const float PREFAB_MINIMUM_SCALE = .4f;

        Decorator->OnPreDistributePrefab();

        while (OccupiedDistance < EndDistance)
        {
            uint32 PrefabIndex = 0;
            float RemainingDistance = OccupiedDistance-EndDistance;

            // Find prefab to occupy along line, look from the longest
            for (int32 pi=(PrefabCount-1); pi>=0; --pi)
            {
                const FPrefabData& Prefab(GetSortedPrefab(pi));
                const float PrefabLength = Prefab.Length;

                // Make sure to not go past remaining line distance
                if (PrefabLength > RemainingDistance)
                {
                    const float AngleThreshold = Prefab.AngleThreshold;
                    bool bValidPrefab = true;

                    // Find prefab by angle threshold to the current line point origin
                    if (AngleThreshold > -1.f)
                    {
                        const FVector2D& LineTangentOrigin(Tangents[LastOccupiedPointIndex]);
                        const float DistanceThreshold = OccupiedDistance + PrefabLength + KINDA_SMALL_NUMBER;

                        // Iterate over next line point within prefab distance
                        for (int32 di=LastOccupiedPointIndex+1; (di<PointCount && DistanceThreshold>Distances[di]); ++di)
                        {
                            const FVector2D& NextPoint(Tangents[di]);
                            const float Dot = LineTangentOrigin | NextPoint;

                            // Filter points within origin line point angle threshold
                            if (Dot < AngleThreshold)
                            {
                                bValidPrefab = false;
                                break;
                            }
                        }
                    }

                    // Matching prefab found, break
                    if (bValidPrefab)
                    {
                        PrefabIndex = pi;
                        break;
                    }
                }
            }

            const FPrefabData& Prefab(GetSortedPrefab(PrefabIndex));
            const float PrefabLength = Prefab.Length;
            float Scale = 1.f;

            OccupiedDistance += PrefabLength;

            // Find last occupied index

            const int32 PointIndex = LastOccupiedPointIndex; 
            const float DistanceThreshold = OccupiedDistance + KINDA_SMALL_NUMBER;

            for (int32 di=LastOccupiedPointIndex; (di<PointCount && DistanceThreshold>Distances[di]); ++di)
            {
                LastOccupiedPointIndex = di;
            }

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

            // Assign new prefab and section allocation data

            const FVector PrefabOrigin(Positions[PointIndex], 0.f);
            const int32 SectionIndex = Decorator->GetSectionIndexByOrigin(PrefabOrigin);

            FPrefabAllocation PrefabAllocation;
            PrefabAllocation.Prefab = &Prefab;
            PrefabAllocation.Scale = Scale;
            PrefabAllocation.SectionIndex = SectionIndex;
            PrefabList.Emplace(PrefabAllocation);

            uint32 PrefabNumVertices;
            uint32 PrefabNumIndices;

            GetPrefabGeometryCount(PrefabNumVertices, PrefabNumIndices, Prefab);

            FSectionAllocation& SectionAllocation(SectionList[SectionIndex]);
            SectionAllocation.NumVertices += PrefabNumVertices;
            SectionAllocation.NumIndices += PrefabNumIndices;
        }

        Decorator->OnPostDistributePrefab(SectionList.Num(), PrefabList.Num());
    }

    // Allocate section geometry container
    for (int32 i=0; i<SectionList.Num(); ++i)
    {
        const FSectionAllocation& SectionAllocation(SectionList[i]);
        uint32 NumVertices = SectionAllocation.NumVertices;
        uint32 NumIndices = SectionAllocation.NumIndices;
        AllocateSection(GetSection(i), NumVertices, NumIndices);
    }

    // Transform and copy prefab buffers to output sections

    FPrefabBufferMap PrefabBufferMap;
    float CurrentDistance = Distances[0];
    int32 PointIndex = 0;

    for (int32 PrefabIndex=0; PrefabIndex<PrefabList.Num(); ++PrefabIndex)
    {
        const FPrefabAllocation& PrefabAllocation(PrefabList[PrefabIndex]);
        const FPrefabData& Prefab(*PrefabAllocation.Prefab);
        const int32 SectionIndex = PrefabAllocation.SectionIndex;
        const float PrefabScaleX = PrefabAllocation.Scale;

        // Copy prefab to section

        FPMUMeshSection& Section(GetSection(SectionIndex));
        uint32 VertexOffsetIndex = CopyPrefabToSection(Section, PrefabBufferMap, Prefab);

        Decorator->OnPrePrefabTransform(SectionIndex, PrefabIndex, Prefab, VertexOffsetIndex);

        // Transform positions along edges

        const FPrefabBuffers& PrefabBuffers(PrefabBufferMap.FindChecked(&Prefab));
        const TArray<FVector>& SrcPositions(PrefabBuffers.Positions);
        const TArray<FVector>& SrcTangentX(PrefabBuffers.TangentX);
        const TArray<FVector4>& SrcTangentZ(PrefabBuffers.TangentZ);
        const TArray<uint32>& IndexAlongX(Prefab.SortedIndexAlongX);

        TArray<FVector>& DstPositions(Section.Positions);
        TArray<uint32>& DstTangents(Section.Tangents);

        float StartDistance = CurrentDistance;

        for (int32 vix=0; vix<IndexAlongX.Num(); ++vix)
        {
            const uint32 SrcIndex = IndexAlongX[vix];
            const uint32 DstIndex = VertexOffsetIndex+SrcIndex;
            const FVector SrcPos(SrcPositions[SrcIndex]);
            const FVector SrcTanX(SrcTangentX[SrcIndex]);
            const FVector4 SrcTanZ(SrcTangentZ[SrcIndex]);

            CurrentDistance = StartDistance+SrcPos.X*PrefabScaleX;

            // Find current line point index along transformed prefab mesh X

            const float DistanceThreshold = CurrentDistance + KINDA_SMALL_NUMBER;
            for (int32 di=PointIndex; (di<PointCount && DistanceThreshold>Distances[di]); ++di)
            {
                PointIndex = di;
            }

            // Calculate tangent along line

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
            const float DistAlpha = Dist12 > SMALL_NUMBER ? Dist1P/Dist12 : 0.f;

            const FVector2D& Offset(Positions[pi1]);
            const FVector2D& Tangent0(Tangents[pi0]);
            const FVector2D& Tangent1(Tangents[pi1]);

            FVector2D InterpTangent;
            InterpTangent = FMath::LerpStable(Tangent0, Tangent1, DistAlpha);
            InterpTangent.Normalize();

            // Transform position

            FVector2D TransformedPosition(-InterpTangent.Y, InterpTangent.X);
            TransformedPosition *= SrcPos.Y;
            TransformedPosition += Offset+InterpTangent*Dist1P;

            DstPositions[DstIndex] = FVector(TransformedPosition, SrcPos.Z);

            // Transform tangents

            FVector TransformedTangent(SrcTanX);
            FVector4 TransformedNormal(SrcTanZ);

            {
                float Angle = FMath::Acos(InterpTangent.X);

                if (InterpTangent.Y < 0.f)
                {
                    Angle *= -1.f;
                }

                float S, C;
                FMath::SinCos(&S, &C, Angle);

                TransformedTangent.X = C*SrcTanX.X - S*SrcTanX.Y;
                TransformedTangent.Y = S*SrcTanX.X + C*SrcTanX.Y;

                TransformedNormal.X = C*SrcTanZ.X - S*SrcTanZ.Y;
                TransformedNormal.Y = S*SrcTanZ.X + C*SrcTanZ.Y;
            }

            DstTangents[DstIndex*2  ] = FPackedNormal(TransformedTangent).Vector.Packed;
            DstTangents[DstIndex*2+1] = FPackedNormal(TransformedNormal).Vector.Packed;

            // Expand bounding box
            Section.SectionLocalBox += DstPositions[DstIndex];

            // Call decorator on position transform
            Decorator->OnTransformPosition(SectionIndex, PrefabIndex, DstIndex, Offset, InterpTangent);
        }

        Decorator->OnPostPrefabTransform(SectionIndex, PrefabIndex, Prefab, VertexOffsetIndex);
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
        TArray<FVector>& DstTangentX(PrefabBuffers->TangentX);
        TArray<FVector4>& DstTangentZ(PrefabBuffers->TangentZ);
        TArray<FVector2D>& DstUVs(PrefabBuffers->UVs);
        TArray<FColor>& DstColors(PrefabBuffers->Colors);

        DstPositions.SetNumUninitialized(NumVertices);
        DstTangentX.SetNumUninitialized(NumVertices);
        DstTangentZ.SetNumUninitialized(NumVertices);

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
            DstTangentX[i] = SrcUVTans.VertexTangentX(i);
            DstTangentZ[i] = SrcUVTans.VertexTangentZ(i);

            if (bHasUV)
            {
                DstUVs[i] = SrcUVTans.GetVertexUV(i, 0);
            }

            if (bHasColor)
            {
                DstColors[i] = SrcColors.VertexColor(i);
            }
        }

        check(DstPositions.Num() == DstTangentX.Num());
        check(DstPositions.Num() == DstTangentZ.Num());

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
    Decorator->OnCopyPrefabToSection(OutSection, Prefab, VertexOffsetIndex);

    return VertexOffsetIndex;
}

uint32 UPMUPrefabBuilder::CopyPrefabToSection(FPMUMeshSection& OutSection, const FPrefabBuffers& PrefabBuffers)
{
    // Copy buffers

    const TArray<FVector>& SrcPositions(PrefabBuffers.Positions);
    const TArray<FVector2D>& SrcUVs(PrefabBuffers.UVs);
    const TArray<FColor>& SrcColors(PrefabBuffers.Colors);
    const TArray<uint32>& SrcIndices(PrefabBuffers.Indices);

    TArray<FVector>& DstPositions(OutSection.Positions);
    TArray<FVector2D>& DstUVs(OutSection.UVs);
    TArray<uint32>& DstTangents(OutSection.Tangents);
    TArray<FColor>& DstColors(OutSection.Colors);
    TArray<uint32>& DstIndices(OutSection.Indices);

    const uint32 OffsetIndex = DstPositions.Num();
    const uint32 NumAddVertices = SrcPositions.Num();
    const uint32 TargetNumVertices = OffsetIndex + NumAddVertices;

    // Copy vertex buffers

    DstPositions.Append(SrcPositions);
    DstTangents.AddUninitialized(NumAddVertices*2);

    check(TargetNumVertices == DstPositions.Num());
    check(TargetNumVertices == (DstTangents.Num()/2));

    // Copy UV buffer
    if (PrefabBuffers.HasUVs())
    {
        DstUVs.Append(SrcUVs);
    }
    // Fill zeroed UV if prefab mesh have no valid UV buffer
    else
    {
        DstUVs.AddZeroed(NumAddVertices);
    }

    // Copy color buffer
    if (PrefabBuffers.HasColors())
    {
        DstColors.Append(SrcColors);
    }
    // Fill zeroed color if prefab mesh have no valid color buffer
    else
    {
        DstColors.AddZeroed(NumAddVertices);
    }

    // Ensure all vertex buffers size match
    check(TargetNumVertices == DstUVs.Num());
    check(TargetNumVertices == DstColors.Num());

    // Copy index buffer with offset

    const uint32 NumIndices = SrcIndices.Num();

    for (uint32 i=0; i<NumIndices; ++i)
    {
        DstIndices.Emplace(OffsetIndex+SrcIndices[i]);
    }

    return OffsetIndex;
}
