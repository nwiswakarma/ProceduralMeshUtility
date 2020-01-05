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

#include "PMUCSGUtility.h"
#include "Model.h"

#include "PMUCSGBrush.h"
#include "PMUBSPOps.h"

bool UPMUCSGUtility::IsValidMeshSection(UStaticMesh* Mesh, int32 LODIndex, int32 SectionIndex)
{
    return
        Mesh &&
        Mesh->bAllowCPUAccess &&
        Mesh->RenderData &&
        Mesh->RenderData->LODResources.IsValidIndex(LODIndex) && 
        Mesh->RenderData->LODResources[LODIndex].Sections.IsValidIndex(SectionIndex);
}

void UPMUCSGUtility::ValidateBrush(UModel* Brush, bool ForceValidate, bool DoStatusUpdate)
{
    check(Brush != nullptr);
    Brush->Modify();
    if( ForceValidate || !Brush->Linked )
    {
        Brush->Linked = 1;
        for( int32 i=0; i<Brush->Polys->Element.Num(); i++ )
        {
            Brush->Polys->Element[i].iLink = i;
        }
        int32 n=0;
        for( int32 i=0; i<Brush->Polys->Element.Num(); i++ )
        {
            FPoly* EdPoly = &Brush->Polys->Element[i];
            if( EdPoly->iLink==i )
            {
                for( int32 j=i+1; j<Brush->Polys->Element.Num(); j++ )
                {
                    FPoly* OtherPoly = &Brush->Polys->Element[j];
                    if
                    (   OtherPoly->iLink == j
                    &&  OtherPoly->Material == EdPoly->Material
                    &&  OtherPoly->TextureU == EdPoly->TextureU
                    &&  OtherPoly->TextureV == EdPoly->TextureV
                    &&  OtherPoly->PolyFlags == EdPoly->PolyFlags
                    &&  (OtherPoly->Normal | EdPoly->Normal)>0.9999 )
                    {
                        float Dist = FVector::PointPlaneDist( OtherPoly->Vertices[0], EdPoly->Vertices[0], EdPoly->Normal );
                        if( Dist>-0.001 && Dist<0.001 )
                        {
                            OtherPoly->iLink = i;
                            n++;
                        }
                    }
                }
            }
        }
//      UE_LOG(LogBSPOps, Log,  TEXT("BspValidateBrush linked %i of %i polys"), n, Brush->Polys->Element.Num() );
    }

    // Build bounds.
    Brush->BuildBound();
}

UPMUCSGBrush* UPMUCSGUtility::ConvertStaticMeshToBrush(UStaticMesh* InMesh, UMaterialInterface* Material, int32 LODIndex, int32 SectionIndex)
{
    UPMUCSGBrush* Brush = nullptr;

    // Skip invalid prefabs
    if (! IsValidMeshSection(InMesh, LODIndex, SectionIndex))
    {
        UE_LOG(LogTemp,Error, TEXT("UPMUCSGUtility::ConvertStaticMeshToBrush() INVALID MESH TO CONVERT!"));
        return Brush;
    }

    const UStaticMesh& Mesh(*InMesh);

    const FStaticMeshVertexBuffers& VBs(Mesh.RenderData->LODResources[LODIndex].VertexBuffers);
    const FPositionVertexBuffer& PositionVB(VBs.PositionVertexBuffer);
    const FStaticMeshVertexBuffer& UVTangents(VBs.StaticMeshVertexBuffer);

    const FStaticMeshSection& Section(Mesh.RenderData->LODResources[LODIndex].Sections[SectionIndex]);
    FIndexArrayView IndexBuffer(Mesh.RenderData->LODResources[LODIndex].IndexBuffer.GetArrayView());

    const bool bHasUV = UVTangents.GetNumTexCoords() > 0;

    const int32 NumVertices = PositionVB.GetNumVertices();
    const int32 NumIndices = Section.NumTriangles * 3;
    const int32 OnePastLastIndex = Section.FirstIndex + NumIndices;

    // Generate brush model

    UModel* BrushModel = NewObject<UModel>();
    BrushModel->Initialize(nullptr, true);

    // Generate brush poly data

    for (uint32 ti=0; ti<Section.NumTriangles; ++ti)
    {
        const uint32 vi = Section.FirstIndex+ti*3;
        const uint32 i0 = vi+2;
        const uint32 i1 = vi+1;
        const uint32 i2 = vi  ;

        FPoly Poly;
        Poly.Init();
        Poly.Base = PositionVB.VertexPosition(IndexBuffer[vi]);
        Poly.PolyFlags = PF_DefaultFlags;

        new(Poly.Vertices)FVector(PositionVB.VertexPosition(IndexBuffer[i0]));
        new(Poly.Vertices)FVector(PositionVB.VertexPosition(IndexBuffer[i1]));
        new(Poly.Vertices)FVector(PositionVB.VertexPosition(IndexBuffer[i2]));

        check(Poly.Vertices.Num() == 3);

        if (Poly.Finalize(nullptr, 1) == 0)
        {
            new(BrushModel->Polys->Element)FPoly(Poly);
        }
    }

    // Optimize polys and assign poly material and name

	FPoly::OptimizeIntoConvexPolys(nullptr, BrushModel->Polys->Element);

    for (FPoly& Poly : BrushModel->Polys->Element)
    {
        Poly.ItemName = NAME_None;
        Poly.Material = Material;
    }

    // Finalize model

    BrushModel->Linked = true;
    ValidateBrush(BrushModel, false, true);
    BrushModel->BuildBound();

    // Generate brush

    Brush = NewObject<UPMUCSGBrush>();
    Brush->Model = BrushModel;

    return Brush;
}

void UPMUCSGUtility::GenerateSectionsFromBSPBrushes(TArray<FPMUMeshSection>& Sections, const TArray<FPMUCSGBrushInput>& BrushInputs)
{
    Sections.Reset();

    if (BrushInputs.Num() < 1)
    {
        return;
    }

    UModel* MeshModel = NewObject<UModel>();
    MeshModel->Initialize(nullptr, true);

    TUniquePtr<FPMUBSPPointsGrid> BSPPoints = MakeUnique<FPMUBSPPointsGrid>(50.0f, THRESH_POINTS_ARE_SAME);
    TUniquePtr<FPMUBSPPointsGrid> BSPVectors = MakeUnique<FPMUBSPPointsGrid>(1/16.0f, FMath::Max(THRESH_NORMALS_ARE_SAME, THRESH_VECTORS_ARE_NEAR));
    FPMUBSPPointsGrid::GBSPPoints = BSPPoints.Get();
    FPMUBSPPointsGrid::GBSPVectors = BSPVectors.Get();

    // Empty the mesh model out

    const int32 NumPoints = MeshModel->Points.Num();
    const int32 NumNodes = MeshModel->Nodes.Num();
    const int32 NumVerts = MeshModel->Verts.Num();
    const int32 NumVectors = MeshModel->Vectors.Num();
    const int32 NumSurfs = MeshModel->Surfs.Num();

    MeshModel->EmptyModel(1, 1);

    // Reserve mesh geometry an eighth bigger than the previous allocation

    MeshModel->Points.Empty(NumPoints + NumPoints / 8);
    MeshModel->Nodes.Empty(NumNodes + NumNodes / 8);
    MeshModel->Verts.Empty(NumVerts + NumVerts / 8);
    MeshModel->Vectors.Empty(NumVectors + NumVectors / 8);
    MeshModel->Surfs.Empty(NumSurfs + NumSurfs / 8);

    // Compose csg model from brushes

    for (const FPMUCSGBrushInput& BrushInput : BrushInputs)
    {
        if (IsValid(BrushInput.Brush) && BrushInput.Brush->Model)
        {
            UModel* Brush = BrushInput.Brush->Model;

            FPMUBSPOps::bspBrushCSG(
                MeshModel,
                Brush,
                BrushInput.Transform,
                PF_DefaultFlags, //Brush->PolyFlags,
                EBrushType::Brush_Add, //Brush->BrushType,
                ECsgOper::CSG_None,
                false,
                true,
                false,
                false
                );
        }
    }

    FPMUBSPPointsGrid::GBSPPoints = nullptr;
    FPMUBSPPointsGrid::GBSPVectors = nullptr;

    // Generate poly from brushes

    FPMUBSPOps::bspBuildFPolys(MeshModel, true, 0);

    // Convert polys to mesh section

    CreateStaticMeshFromBrush(Sections, MeshModel);
}

void UPMUCSGUtility::CreateStaticMeshFromBrush(TArray<FPMUMeshSection>& Sections, UModel* Model)
{
    TMap<UMaterialInterface*, int32> MaterialSectionMap;

    int32 NumPolys = Model->Polys->Element.Num();

    // Generate polygon geometry
    for (int32 PolyIndex = 0; PolyIndex < NumPolys; ++PolyIndex)
    {
        FPoly& Poly(Model->Polys->Element[PolyIndex]);

        // Get poly material, revert to default if none is set
        UMaterialInterface* Material = Poly.Material;

        if (Material == nullptr)
        {
            Material = UMaterial::GetDefaultMaterial(MD_Surface);
        }

        // Find or generate mesh section for material

        int32* SectionIndexPtr = MaterialSectionMap.Find(Material);
        int32 SectionIndex;

        if (SectionIndexPtr)
        {
            SectionIndex = *SectionIndexPtr;
        }
        else
        {
            SectionIndex = Sections.Num();
            Sections.AddDefaulted();
            MaterialSectionMap.Emplace(Material, SectionIndex);
        }

        UE_LOG(LogTemp,Warning, TEXT("MAT: %s, BASE: %s, NORMAL: %s, ID: %d, FLAGS: %u"),
            *Material->GetName(),
            *Poly.Base.ToString(),
            *Poly.Normal.ToString(),
            Poly.iBrushPoly,
            Poly.PolyFlags);

        // Generate mesh section geometry

        FPMUMeshSection& Section(Sections[SectionIndex]);
        TArray<int32> VertexIds;

        auto FindVertex = [&Section](const FVector& V) {
            return [V,&Section](int32 Elem) {
                return FVerticesEqual(V, Section.Positions[Elem]);
            };
        };

        for (int32 vi=2; vi<Poly.Vertices.Num(); ++vi)
        {
            FPackedNormal TangentX(FVector4(1,0,0,0));
            FPackedNormal TangentZ(FVector4(Poly.Normal, 1.f));

            int32 i0 = vi;
            int32 i1 = vi-1;
            int32 i2 = 0;

            const FVector& Pos0(Poly.Vertices[i0]);
            const FVector& Pos1(Poly.Vertices[i1]);
            const FVector& Pos2(Poly.Vertices[i2]);

            int32* pvi0 = VertexIds.FindByPredicate(FindVertex(Pos0));
            int32* pvi1 = VertexIds.FindByPredicate(FindVertex(Pos1));
            int32* pvi2 = VertexIds.FindByPredicate(FindVertex(Pos2));

            int32 vi0, vi1, vi2;

            if (pvi0)
            {
                vi0 = *pvi0;
            }
            else
            {
                vi0 = Section.Positions.Emplace(Pos0);
                Section.Tangents.Emplace(TangentX.Vector.Packed);
                Section.Tangents.Emplace(TangentZ.Vector.Packed);
                VertexIds.Emplace(vi0);
            }

            if (pvi1)
            {
                vi1 = *pvi1;
            }
            else
            {
                vi1 = Section.Positions.Emplace(Pos1);
                Section.Tangents.Emplace(TangentX.Vector.Packed);
                Section.Tangents.Emplace(TangentZ.Vector.Packed);
                VertexIds.Emplace(vi1);
            }

            if (pvi2)
            {
                vi2 = *pvi2;
            }
            else
            {
                vi2 = Section.Positions.Emplace(Pos2);
                Section.Tangents.Emplace(TangentX.Vector.Packed);
                Section.Tangents.Emplace(TangentZ.Vector.Packed);
                VertexIds.Emplace(vi2);
            }

            Section.Indices.Emplace(vi0);
            Section.Indices.Emplace(vi1);
            Section.Indices.Emplace(vi2);
        }
    }
}
