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

#include "Mesh/PMUMeshComponent.h"

#include "PositionVertexBuffer.h"
#include "Materials/Material.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Stats/Stats.h"
#include "Mesh/PMUMeshSceneProxy.h"

//////////////////////////////////////////////////////////////////////////

UPMUMeshComponent::UPMUMeshComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bUseComplexAsSimpleCollision = true;
}

void UPMUMeshComponent::PostLoad()
{
    Super::PostLoad();

    if (MeshBodySetup && IsTemplate())
    {
        MeshBodySetup->SetFlags(RF_Public);
    }
}

FPrimitiveSceneProxy* UPMUMeshComponent::CreateSceneProxy()
{
    return new FPMUMeshSceneProxy(this);
}

UBodySetup* UPMUMeshComponent::GetBodySetup()
{
    CreateMeshBodySetup();
    return MeshBodySetup;
}

UMaterialInterface* UPMUMeshComponent::GetMaterial(int32 ElementIndex) const
{
    return UMeshComponent::GetMaterial(bUseSingleMaterial ? 0 : ElementIndex);
}

UMaterialInterface* UPMUMeshComponent::GetMaterialFromCollisionFaceIndex(int32 FaceIndex, int32& SectionIndex) const
{
    UMaterialInterface* Result = nullptr;
    SectionIndex = 0;

    // Single material is used for all sections, return the material
    if (bUseSingleMaterial)
    {
        Result = GetMaterial(SectionIndex);
    }
    // Otherwise, look for element that corresponds to the supplied face
    else
    {
        int32 TotalFaceCount = 0;

        for (int32 i=0; i<Sections.Num(); ++i)
        {
            const FPMUMeshSection& Section = Sections[i];

            int32 NumFaces = Section.Indices.Num() / 3;
            TotalFaceCount += NumFaces;

            if (FaceIndex < TotalFaceCount)
            {
                // Grab the material
                Result = GetMaterial(i);
                SectionIndex = i;
                break;
            }
        }
    }

    return Result;
}

int32 UPMUMeshComponent::GetNumMaterials() const
{
    return bUseSingleMaterial ? 1 : Sections.Num();
}

bool UPMUMeshComponent::GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
{
    // Base vertex index for current section
    int32 VertexBase = 0;

    // Copy UVs if required
    bool bCopyUVs = UPhysicsSettings::Get()->bSupportUVFromHitResults;
    if (bCopyUVs)
    {
        // Only one UV channel
        CollisionData->UVs.AddZeroed(1);
    }

    // Construct collision data
    for (int32 SectionIdx=0; SectionIdx < Sections.Num(); SectionIdx++)
    {
        FPMUMeshSection& Section = Sections[SectionIdx];

        // Construct collision data if required
        if (Section.bEnableCollision)
        {
            // Copy vert data
            for (int32 VertIdx = 0; VertIdx < Section.Positions.Num(); VertIdx++)
            {
                CollisionData->Vertices.Emplace(Section.Positions[VertIdx]);

                // Copy UV if desired
                if (bCopyUVs)
                {
                    CollisionData->UVs[0].Emplace(Section.UVs[VertIdx]);
                }
            }

            // Copy triangle data
            const int32 NumTriangles = Section.Indices.Num() / 3;
            for (int32 TriIdx = 0; TriIdx < NumTriangles; TriIdx++)
            {
                // Need to add base offset for indices
                FTriIndices Triangle;
                Triangle.v0 = Section.Indices[(TriIdx * 3) + 0] + VertexBase;
                Triangle.v1 = Section.Indices[(TriIdx * 3) + 1] + VertexBase;
                Triangle.v2 = Section.Indices[(TriIdx * 3) + 2] + VertexBase;
                CollisionData->Indices.Emplace(Triangle);

                // Also store material info
                CollisionData->MaterialIndices.Emplace(SectionIdx);
            }

            // Remember the base index that new verts will be added from in next section
            VertexBase = CollisionData->Vertices.Num();
        }
    }

    CollisionData->bFlipNormals = true;
    CollisionData->bDeformableMesh = true;
    CollisionData->bFastCook = true;

    return true;
}

bool UPMUMeshComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
    // True if any of there is any non-empty section with collision enabled
    for (const FPMUMeshSection& Section : Sections)
    {
        if (Section.Indices.Num() >= 3 && Section.bEnableCollision)
        {
            return true;
        }
    }

    return false;
}

void UPMUMeshComponent::UpdateRenderState()
{
    UpdateLocalBounds();
    UpdateCollision();
    MarkRenderStateDirty();
}

void UPMUMeshComponent::ClearSection(int32 SectionIndex)
{
	if (SectionIndex < Sections.Num())
	{
		Sections[SectionIndex].Reset();
	}
}

void UPMUMeshComponent::ClearAllSections()
{
	Sections.Empty();
}

bool UPMUMeshComponent::IsValidSection(int32 SectionIndex) const
{
    return Sections.IsValidIndex(SectionIndex);
}

bool UPMUMeshComponent::IsValidNonEmptySection(int32 SectionIndex) const
{
    return IsValidSection(SectionIndex) && (Sections[SectionIndex].Positions.Num() > 0);
}

bool UPMUMeshComponent::IsSectionVisible(int32 SectionIndex) const
{
	return Sections.IsValidIndex(SectionIndex) ? Sections[SectionIndex].bSectionVisible : false;
}

int32 UPMUMeshComponent::GetNumSections() const
{
	return Sections.Num();
}

void UPMUMeshComponent::GetSectionRefs(TArray<FPMUMeshSectionRef>& SectionRefs, const TArray<int32>& SectionIndices)
{
    SectionRefs.Reset(SectionIndices.Num());

    for (int32 i : SectionIndices)
    {
        if (Sections.IsValidIndex(i))
        {
            SectionRefs.Emplace(Sections[i]);
        }
    }
}

FPMUMeshSectionRef UPMUMeshComponent::GetSectionRef(int32 SectionIndex)
{
    FPMUMeshSectionRef SectionRef;

    if (Sections.IsValidIndex(SectionIndex))
    {
        SectionRef = FPMUMeshSectionRef(Sections[SectionIndex]);
    }

    return SectionRef;
}

void UPMUMeshComponent::GetAllNonEmptySectionIndices(TArray<int32>& SectionIndices)
{
    SectionIndices.Reset();

    for (int32 i=0; i<Sections.Num(); ++i)
    {
        if (Sections[i].HasGeometry())
        {
            SectionIndices.Emplace(i);
        }
    }
}

FPMUMeshSceneProxy* UPMUMeshComponent::GetSceneProxy()
{
    return (FPMUMeshSceneProxy*) SceneProxy;
}

int32 UPMUMeshComponent::CreateNewSection(const FPMUMeshSectionRef& Section, int32 GroupIndex)
{
    // Invalid section resource, abort
    if (! Section.HasValidSection())
    {
        return -1;
    }

    int32 SectionIndex = Sections.Num();

    CreateSection(SectionIndex, Section);

    if (GroupIndex >= 0)
    {
        AssignSectionGroup(SectionIndex, GroupIndex);
    }

    return SectionIndex;
}

void UPMUMeshComponent::AssignSectionGroup(int32 SectionIndex, int32 GroupIndex)
{
    // Invalid section or section group, abort
    if (! IsValidSection(SectionIndex) || GroupIndex < 0)
    {
        return;
    }

    FSectionIdGroup& SectionGroup(SectionGroupMap.FindOrAdd(GroupIndex));
    SectionGroup.AddUnique(SectionIndex);
}

void UPMUMeshComponent::AllocateMeshSection(int32 SectionIndex)
{
	if (SectionIndex >= Sections.Num())
	{
		Sections.SetNum(SectionIndex+1, false);
	}
}

void UPMUMeshComponent::CreateSection(int32 SectionIndex, const FPMUMeshSectionRef& Section)
{
    // Invalid section resource, abort
    if (! Section.HasValidSection())
    {
        return;
    }

	if (SectionIndex >= Sections.Num())
	{
		Sections.SetNum(SectionIndex + 1, false);
	}

    const FPMUMeshSection& SrcSection(*Section.SectionPtr);
	FPMUMeshSection& DstSection(Sections[SectionIndex]);

	DstSection.Reset();
    DstSection.Positions = SrcSection.Positions;
    DstSection.Indices = SrcSection.Indices;

    const int32 NumVerts = DstSection.Positions.Num();

    // Copy tangent data

    if (SrcSection.Tangents.Num() == (NumVerts*2))
    {
        DstSection.Tangents = SrcSection.Tangents;
    }
    else
    if (SrcSection.bInitializeInvalidVertexData)
    {
        DstSection.Tangents.SetNumZeroed(NumVerts*2);
    }

    // Copy uv data

    if (SrcSection.UVs.Num() == NumVerts)
    {
        DstSection.UVs = SrcSection.UVs;
    }
    else
    if (SrcSection.bInitializeInvalidVertexData)
    {
        DstSection.UVs.SetNumZeroed(NumVerts);
    }

    // Copy color data

    if (SrcSection.Colors.Num() == NumVerts)
    {
        DstSection.Colors = SrcSection.Colors;
    }
    else
    if (SrcSection.bInitializeInvalidVertexData)
    {
        DstSection.Colors.SetNumZeroed(NumVerts);
    }

    DstSection.SectionLocalBox = SrcSection.SectionLocalBox;
    DstSection.bSectionVisible = SrcSection.bSectionVisible;
    DstSection.bEnableCollision = SrcSection.bEnableCollision;
    DstSection.bEnableFastUVCopy = SrcSection.bEnableFastUVCopy;
    DstSection.bEnableFastTangentsCopy = SrcSection.bEnableFastTangentsCopy;
    DstSection.bInitializeInvalidVertexData = SrcSection.bInitializeInvalidVertexData;
}

void UPMUMeshComponent::CreateMeshSection_LinearColor(
    int32 SectionIndex,
    const TArray<FVector>& Positions,
    const TArray<int32>& Indices,
    const TArray<FVector>& Normals,
    const TArray<FVector>& Tangents,
    const TArray<FVector2D>& UVs,
    const TArray<FLinearColor>& LinearColors,
    FBox SectionBounds,
    bool bCreateCollision,
    bool bCalculateBounds
    )
{
    // Invalid section index, abort
    if (SectionIndex < 0)
    {
        return;
    }

    // Empty position buffer, abort

	const int32 NumVerts = Positions.Num();

    if (NumVerts < 1)
    {
        return;
    }

	// Convert FLinearColors to FColors

	TArray<FColor> Colors;

	if (LinearColors.Num() == NumVerts)
	{
		Colors.SetNumUninitialized(LinearColors.Num());

		for (int32 ColorIdx = 0; ColorIdx < LinearColors.Num(); ColorIdx++)
		{
			Colors[ColorIdx] = LinearColors[ColorIdx].ToFColor(false);
		}
	}

	// Ensure sections array is long enough

	if (SectionIndex >= Sections.Num())
	{
		Sections.SetNum(SectionIndex + 1, false);
	}

	// Reset section (in case it already existed)
	FPMUMeshSection& NewSection = Sections[SectionIndex];
	NewSection.Reset();

	// Copy data to vertex buffer

    NewSection.Positions = Positions;

    // Copy tangents data

    const bool bHasValidTangentsX = Tangents.Num() == NumVerts;
    const bool bHasValidTangentsZ = Normals.Num() == NumVerts;

    if (NewSection.bEnableFastTangentsCopy)
    {
        NewSection.Tangents.SetNumZeroed(NumVerts*2);

        if (bHasValidTangentsX && bHasValidTangentsZ)
        {
            for (int32 i=0; i<NumVerts; ++i)
            {
                NewSection.Tangents[2*i  ] = FPackedNormal(Tangents[i]).Vector.Packed;
                NewSection.Tangents[2*i+1] = FPackedNormal(FVector4(Normals[i],1)).Vector.Packed;
            }
        }
        else
        if (bHasValidTangentsX)
        {
            for (int32 i=0; i<NumVerts; ++i)
            {
                NewSection.Tangents[2*i  ] = FPackedNormal(Tangents[i]).Vector.Packed;
                NewSection.Tangents[2*i+1] = FPackedNormal(FVector4(0,0,1,1)).Vector.Packed;
            }
        }
        else
        if (bHasValidTangentsZ)
        {
            for (int32 i=0; i<NumVerts; ++i)
            {
                NewSection.Tangents[2*i  ] = FPackedNormal(FVector(1,0,0)).Vector.Packed;
                NewSection.Tangents[2*i+1] = FPackedNormal(FVector4(Normals[i],1)).Vector.Packed;
            }
        }
        else
        {
            for (int32 i=0; i<NumVerts; ++i)
            {
                NewSection.Tangents[2*i  ] = FPackedNormal(FVector(1,0,0)).Vector.Packed;
                NewSection.Tangents[2*i+1] = FPackedNormal(FVector4(0,0,1,1)).Vector.Packed;
            }
        }
    }
    else
    {
        if (bHasValidTangentsZ)
        {
            NewSection.TangentsZ = Normals;
        }
        else
        {
            NewSection.TangentsZ.Init(FVector(0,0,1), NumVerts);
        }

        if (bHasValidTangentsX)
        {
            NewSection.TangentsX = Tangents;
        }
        else
        {
            NewSection.TangentsX.Init(FVector(1,0,0), NumVerts);
        }
    }

    // Copy color data

    if (Colors.Num() == NumVerts)
    {
        NewSection.Colors = Colors;
    }
    else
    {
        NewSection.Colors.Init(FColor(255,255,255), NumVerts);
    }

    // Copy uv data

    if (UVs.Num() == NumVerts)
    {
        NewSection.UVs = UVs;
    }
    else
    {
        NewSection.UVs.SetNumZeroed(NumVerts);
    }

    // Calculate section local bounds if required

    if (bCalculateBounds)
    {
        for (int32 i=0; i<NumVerts; i++)
        {
            NewSection.SectionLocalBox += Positions[i];
        }
    }
    else
    {
		NewSection.SectionLocalBox = SectionBounds;
    }

	// Copy index buffer

	int32 NumIndices = Indices.Num();
	// Ensure we have exact number of triangles (array is multiple of 3 long)
    NumIndices = (NumIndices/3) * 3;

    NewSection.Indices.SetNumUninitialized(NumIndices, true);

    for (int32 i=0; i<NumIndices; ++i)
    {
        NewSection.Indices[i] = Indices[i];
    }

    // Set collision mode

	NewSection.bEnableCollision = bCreateCollision;
}

void UPMUMeshComponent::CopyMeshSection(
    int32 SectionIndex,
    const FPMUMeshSection& SourceSection,
    bool bCalculateBounds
    )
{
    // Invalid section index, abort
    if (SectionIndex < 0)
    {
        return;
    }

	// Ensure sections array is long enough
	if (SectionIndex >= Sections.Num())
	{
		Sections.SetNum(SectionIndex + 1, false);
	}

	// Reset section (in case it already existed)
	FPMUMeshSection& NewSection = Sections[SectionIndex];
	NewSection.Reset();

	// Copy data
    NewSection = SourceSection;

    // Calculate section local bounds if required
    if (bCalculateBounds)
    {
        for (const FVector& Position : NewSection.Positions)
        {
            NewSection.SectionLocalBox += Position;
        }
    }
}

void UPMUMeshComponent::ExpandSectionBounds(int32 SectionIndex, const FVector& NegativeExpand, const FVector& PositiveExpand)
{
    // Invalid section index, abort
    if (! IsValidSection(SectionIndex))
    {
        return;
    }

	FPMUMeshSection& Section(Sections[SectionIndex]);
    Section.SectionLocalBox = Section.SectionLocalBox.ExpandBy(NegativeExpand, PositiveExpand);
}

void UPMUMeshComponent::UpdateLocalBounds()
{
    FBox LocalBox(ForceInit);

    for (const FPMUMeshSection& Section : Sections)
    {
        LocalBox += Section.SectionLocalBox;
    }

    LocalBounds = LocalBox.IsValid ? FBoxSphereBounds(LocalBox) : FBoxSphereBounds(FVector(0, 0, 0), FVector(0, 0, 0), 0);

    UpdateBounds();
    MarkRenderTransformDirty();
}

FBoxSphereBounds UPMUMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
    FBoxSphereBounds Ret(LocalBounds.TransformBy(LocalToWorld));

    Ret.BoxExtent *= BoundsScale;
    Ret.SphereRadius *= BoundsScale;

    return Ret;
}

void UPMUMeshComponent::AddCollisionConvexMesh(TArray<FVector> ConvexVerts)
{
	if (ConvexVerts.Num() >= 4)
	{
		// New element
		FKConvexElem NewConvexElem;
		// Copy in vertex info
		NewConvexElem.VertexData = ConvexVerts;
		// Update bounding box
		NewConvexElem.ElemBox = FBox(NewConvexElem.VertexData);
		// Add to array of convex elements
		CollisionConvexElems.Add(NewConvexElem);
		// Refresh collision
		UpdateCollision();
	}
}

void UPMUMeshComponent::ClearCollisionConvexMeshes()
{
	// Empty simple collision info
	CollisionConvexElems.Empty();
	// Refresh collision
	UpdateCollision();
}

UBodySetup* UPMUMeshComponent::CreateBodySetupHelper()
{
    // The body setup in a template needs to be public since the property is instanced
    // and thus is the archetype of the instance meaning there is a direct reference
    UBodySetup* NewBodySetup = NewObject<UBodySetup>(this, NAME_None, (IsTemplate() ? RF_Public : RF_NoFlags));
    NewBodySetup->BodySetupGuid = FGuid::NewGuid();

    NewBodySetup->bGenerateMirroredCollision = false;
    NewBodySetup->bDoubleSidedGeometry = true;
    NewBodySetup->CollisionTraceFlag = bUseComplexAsSimpleCollision ? CTF_UseComplexAsSimple : CTF_UseDefault;

    return NewBodySetup;
}

void UPMUMeshComponent::CreateMeshBodySetup()
{
    if (MeshBodySetup == nullptr)
    {
        MeshBodySetup = CreateBodySetupHelper();
    }
}

void UPMUMeshComponent::UpdateCollision()
{
    //SCOPE_CYCLE_COUNTER(STAT_VoxelMesh_UpdateCollision);

    UWorld* World = GetWorld();
    const bool bUseAsyncCook = World && World->IsGameWorld() && bUseAsyncCooking;

    if (bUseAsyncCook)
    {
        AsyncBodySetupQueue.Emplace(CreateBodySetupHelper());
    }
    else
    {
        // If for some reason we modified the async at runtime,
        // just clear any pending async body setups
        AsyncBodySetupQueue.Empty();
        CreateMeshBodySetup();
    }

    UBodySetup* UseBodySetup = bUseAsyncCook ? AsyncBodySetupQueue.Last() : MeshBodySetup;

    // Fill in simple collision convex elements
    UseBodySetup->AggGeom.ConvexElems = CollisionConvexElems;

    // Set trace flag
    UseBodySetup->CollisionTraceFlag = bUseComplexAsSimpleCollision ? CTF_UseComplexAsSimple : CTF_UseDefault;

    if (bUseAsyncCook)
    {
        UseBodySetup->CreatePhysicsMeshesAsync(FOnAsyncPhysicsCookFinished::CreateUObject(this, &UPMUMeshComponent::FinishPhysicsAsyncCook, UseBodySetup));
    }
    else
    {
        // New GUID as collision has changed
        UseBodySetup->BodySetupGuid = FGuid::NewGuid();
        // Also we want cooked data for this
        UseBodySetup->bHasCookedCollisionData = true;
        UseBodySetup->InvalidatePhysicsData();
        UseBodySetup->CreatePhysicsMeshes();
        RecreatePhysicsState();
    }
}

void UPMUMeshComponent::FinishPhysicsAsyncCook(bool bSuccess, UBodySetup* FinishedBodySetup)
{
	TArray<UBodySetup*> NewQueue;
	NewQueue.Reserve(AsyncBodySetupQueue.Num());

	int32 FoundIdx;
	if (AsyncBodySetupQueue.Find(FinishedBodySetup, FoundIdx))
	{
		if (bSuccess)
		{
			//The new body was found in the array meaning it's newer so use it
			MeshBodySetup = FinishedBodySetup;
			RecreatePhysicsState();

			//remove any async body setups that were requested before this one
			for (int32 AsyncIdx = FoundIdx + 1; AsyncIdx < AsyncBodySetupQueue.Num(); ++AsyncIdx)
			{
				NewQueue.Add(AsyncBodySetupQueue[AsyncIdx]);
			}

			AsyncBodySetupQueue = NewQueue;
		}
		else
		{
			AsyncBodySetupQueue.RemoveAt(FoundIdx);
		}
	}
}
