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

#include "March/PMUVoxelMap.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"

#include "ProceduralMeshUtility.h"
#include "PMUVoxelGrid.h"

#ifdef PMU_VOXEL_USE_OCL
#include "OCLBProgram.h"
#endif

static int32 GetNewIndexForOldVertIndex(
    int32 MeshVertIndex,
    TMap<int32, int32>& MeshToSectionVertMap,
    TSet<int32>& BorderVertSet,
    const FVector& Offset,
    const bool bCalculateBounds,
    const FPositionVertexBuffer* PosBuffer,
    const FStaticMeshVertexBuffer* VertBuffer,
    const FColorVertexBuffer* ColorBuffer,
    FPMUMeshSection& Section
    )
{
	int32* NewIndexPtr = MeshToSectionVertMap.Find(MeshVertIndex);
	if (NewIndexPtr != nullptr)
	{
		return *NewIndexPtr;
	}
	else
	{
        // Copy vertices

        TArray<FPMUMeshVertex>& Vertices(Section.VertexBuffer);
		int32 SectionVertIndex = Vertices.Emplace(
            Offset + PosBuffer->VertexPosition(MeshVertIndex),
            VertBuffer->VertexTangentZ(MeshVertIndex)
            );

		MeshToSectionVertMap.Add(MeshVertIndex, SectionVertIndex);

        // Find border vertices
        if (ColorBuffer && (uint32)MeshVertIndex < ColorBuffer->GetNumVertices())
        {
            // Find border vertices based on color mask threshold
            const FColor& Color(ColorBuffer->VertexColor(MeshVertIndex));

            if (Color.R >= 127)
            {
                BorderVertSet.Emplace(SectionVertIndex);
            }
        }

        // Calculate section bounds if required
        if (bCalculateBounds)
        {
            Section.LocalBox += Vertices[SectionVertIndex].Position;
        }

		return SectionVertIndex;
	}
}

bool FPMUVoxelMap::HasChunk(int32 ChunkIndex) const
{
    return chunks.IsValidIndex(ChunkIndex);
}

int32 FPMUVoxelMap::GetChunkCount() const
{
    return chunks.Num();
}

const FPMUVoxelGrid& FPMUVoxelMap::GetChunk(int32 ChunkIndex) const
{
    return *chunks[ChunkIndex];
}

void FPMUVoxelMap::RefreshAllChunks()
{
    for (FPMUVoxelGrid* Chunk : chunks)
    {
        Chunk->Refresh();
    }
}

void FPMUVoxelMap::RefreshAllChunksAsync(FGWTAsyncTaskRef& TaskRef)
{
    if (! TaskRef.IsValid())
    {
        FGWTAsyncTaskRef::Init(
            TaskRef,
            IProceduralMeshUtility::Get().GetThreadPool());
    }

    check(TaskRef.IsValid());

    FPSGWTAsyncTask& Task(TaskRef.Task);

    for (int32 i=0; i<chunks.Num(); ++i)
    {
        FPMUVoxelGrid& Chunk(*chunks[i]);
        Task->AddTask([&, i](){ Chunk.Refresh(); });
    }
}

void FPMUVoxelMap::ResetChunkStates(const TArray<int32>& ChunkIndices)
{
    for (int32 i : ChunkIndices)
    {
        if (chunks.IsValidIndex(i))
        {
            chunks[i]->ResetVoxels();
        }
    }
}

void FPMUVoxelMap::ResetAllChunkStates()
{
    for (FPMUVoxelGrid* Chunk : chunks)
    {
        Chunk->ResetVoxels();
    }
}

void FPMUVoxelMap::Clear()
{
    for (FPMUVoxelGrid* Chunk : chunks)
    {
        delete Chunk;
    }

    chunks.Empty();
}

TSharedPtr<FGWTAsyncThreadPool> FPMUVoxelMap::GetThreadPool()
{
    if (! ThreadPool.IsValid())
    {
        ThreadPool = IProceduralMeshUtility::Get().GetThreadPool();
    }
    return ThreadPool;
}

void FPMUVoxelMap::InitializeSettings()
{
    // Invalid resolution, abort
    if (mapSize < 1 || chunkResolution < 1 || voxelResolution < 1)
    {
        return;
    }

    // Initialize map settings

    check(mapSize > 0);
    check(chunkResolution > 0);
    check(voxelResolution > 0);

    chunkSize = mapSize / chunkResolution;
    voxelSize = chunkSize / voxelResolution;

    if (GridData)
    {
        const FIntPoint& GridDim(GridData->Dimension);
        bHasGridData = (GridDim.X >= mapSize) && (GridDim.Y >= mapSize);
    }
    else
    {
        bHasGridData = false;
    }

    // Create uninitialized chunks

    Clear();

    chunks.SetNumUninitialized(chunkResolution * chunkResolution);

    for (int32 i=0; i<chunks.Num(); ++i)
    {
        chunks[i] = new FPMUVoxelGrid;
    }
}

void FPMUVoxelMap::InitializeChunkSettings(int32 i, int32 x, int32 y, FPMUVoxelGridConfig& ChunkConfig)
{
    check(chunks.IsValidIndex(i));

    // Initialize chunk

    FVector2D chunkPosition(x * chunkSize, y * chunkSize);

    ChunkConfig.States = surfaceStates;
    ChunkConfig.Position = chunkPosition;
    ChunkConfig.ChunkSize = chunkSize;
    ChunkConfig.VoxelResolution = voxelResolution;
    ChunkConfig.MapSize = mapSize;
    ChunkConfig.MaxFeatureAngle = maxFeatureAngle;
    ChunkConfig.MaxParallelAngle = maxParallelAngle;
    ChunkConfig.ExtrusionHeight = extrusionHeight;
    ChunkConfig.GridData = bHasGridData ? GridData : nullptr;

#ifdef PMU_VOXEL_USE_OCL

    ChunkConfig.GPUProgram = GPUProgram.IsValid() ? GPUProgram.Get() : nullptr;
    ChunkConfig.GPUProgramKernelName = GPUProgramKernelName;

#endif

    // Link chunk neighbours

    FPMUVoxelGrid& Chunk(*chunks[i]);

    if (x > 0)
    {
        chunks[i - 1]->xNeighbor = &Chunk;
    }

    if (y > 0)
    {
        chunks[i - chunkResolution]->yNeighbor = &Chunk;

        if (x > 0)
        {
            chunks[i - chunkResolution - 1]->xyNeighbor = &Chunk;
        }
    }
}

void FPMUVoxelMap::InitializeChunk(int32 i, const FPMUVoxelGridConfig& ChunkConfig)
{
    check(chunks.IsValidIndex(i));
    chunks[i]->Initialize(ChunkConfig);
}

void FPMUVoxelMap::InitializeChunkAsync(int32 i, const FPMUVoxelGridConfig& ChunkConfig, FGWTAsyncTaskRef& TaskRef)
{
    check(chunks.IsValidIndex(i));

    FPMUVoxelGrid& chunk(*chunks[i]);
    TaskRef.AddTask([&chunk, ChunkConfig](){ chunk.Initialize(ChunkConfig); });
}

void FPMUVoxelMap::InitializeChunks()
{
    check(chunks.Num() == (chunkResolution * chunkResolution));

    for (int32 y=0, i=0; y<chunkResolution; y++)
    for (int32 x=0     ; x<chunkResolution; x++, i++)
    {
        FPMUVoxelGridConfig ChunkConfig;
        InitializeChunkSettings(i, x, y, ChunkConfig);
        InitializeChunk(i, ChunkConfig);
    }
}

void FPMUVoxelMap::InitializeChunksAsync(FGWTAsyncTaskRef& TaskRef)
{
    check(chunks.Num() == (chunkResolution * chunkResolution));

    for (int32 y=0, i=0; y<chunkResolution; y++)
    for (int32 x=0     ; x<chunkResolution; x++, i++)
    {
        FPMUVoxelGridConfig ChunkConfig;
        InitializeChunkSettings(i, x, y, ChunkConfig);
        InitializeChunkAsync(i, ChunkConfig, TaskRef);
    }
}

void FPMUVoxelMap::Initialize()
{
    InitializeSettings();
    InitializeChunks();
}

void FPMUVoxelMap::InitializeAsync(FGWTAsyncTaskRef& TaskRef)
{
    if (! TaskRef.IsValid())
    {
        FGWTAsyncTaskRef::Init(
            TaskRef,
            IProceduralMeshUtility::Get().GetThreadPool());
    }

    check(TaskRef.IsValid());

    InitializeSettings();
    InitializeChunksAsync(TaskRef);
}

void FPMUVoxelMap::CopyFrom(const FPMUVoxelMap& VoxelMap)
{
    chunkSize = VoxelMap.chunkSize;
    voxelSize = VoxelMap.voxelSize;
    bHasGridData = VoxelMap.bHasGridData;

#ifdef PMU_VOXEL_USE_OCL

    GPUProgram = VoxelMap.GPUProgram;
    GPUProgramKernelName = VoxelMap.GPUProgramKernelName;

#endif

    // Create uninitialized chunks

    Clear();

    chunks.SetNumUninitialized(chunkResolution * chunkResolution);

    for (int32 y=0, i=0; y<chunkResolution; y++)
    for (int32 x=0     ; x<chunkResolution; x++, i++)
    {
        chunks[i] = new FPMUVoxelGrid;

        FPMUVoxelGrid& Chunk(*chunks[i]);

        Chunk.CopyFrom(*VoxelMap.chunks[i]);

        // Link chunk neighbours

        if (x > 0)
        {
            chunks[i - 1]->xNeighbor = &Chunk;
        }

        if (y > 0)
        {
            chunks[i - chunkResolution]->yNeighbor = &Chunk;

            if (x > 0)
            {
                chunks[i - chunkResolution - 1]->xyNeighbor = &Chunk;
            }
        }
    }
}

bool FPMUVoxelMap::IsPrefabValid(int32 PrefabIndex, int32 LODIndex, int32 SectionIndex) const
{
    if (! HasPrefab(PrefabIndex))
    {
        return false;
    }

    const UStaticMesh* Mesh = meshPrefabs[PrefabIndex];

    if (Mesh->bAllowCPUAccess &&
        Mesh->RenderData      &&
        Mesh->RenderData->LODResources.IsValidIndex(LODIndex) && 
        Mesh->RenderData->LODResources[LODIndex].Sections.IsValidIndex(SectionIndex)
        )
    {
        return true;
    }

    return false;
}

bool FPMUVoxelMap::HasIntersectingBounds(const TArray<FBox2D>& Bounds) const
{
    if (Bounds.Num() > 0)
    {
        for (const FPrefabData& PrefabData : AppliedPrefabs)
        {
            const TArray<FBox2D>& AppliedBounds(PrefabData.Bounds);

            for (const FBox2D& Box0 : Bounds)
            for (const FBox2D& Box1 : AppliedBounds)
            {
                if (Box0.Intersect(Box1))
                {
                    // Skip exactly intersecting box

                    if (FMath::IsNearlyEqual(Box0.Min.X, Box1.Max.X, 1.e-3f) ||
                        FMath::IsNearlyEqual(Box1.Min.X, Box0.Max.X, 1.e-3f))
                    {
                        continue;
                    }

                    if (FMath::IsNearlyEqual(Box0.Min.Y, Box1.Max.Y, 1.e-3f) ||
                        FMath::IsNearlyEqual(Box1.Min.Y, Box0.Max.Y, 1.e-3f))
                    {
                        continue;
                    }

                    return true;
                }
            }
        }
    }

    return false;
}

bool FPMUVoxelMap::TryPlacePrefabAt(int32 PrefabIndex, const FVector2D& Center)
{
    TArray<FBox2D> Bounds;
    GetPrefabBounds(PrefabIndex, Bounds);

    // Offset prefab bounds towards the specified center location
    for (FBox2D& Box : Bounds)
    {
        Box = Box.ShiftBy(Center);
    }

    if (! HasIntersectingBounds(Bounds))
    {
        AppliedPrefabs.Emplace(Bounds);
        return true;
    }

    return false;
}

void FPMUVoxelMap::GetPrefabBounds(int32 PrefabIndex, TArray<FBox2D>& Bounds) const
{
    Bounds.Empty();

    if (! HasPrefab(PrefabIndex))
    {
        return;
    }

    const UStaticMesh& Mesh(*meshPrefabs[PrefabIndex]);
    const TArray<UStaticMeshSocket*>& Sockets(Mesh.Sockets);

    typedef TKeyValuePair<UStaticMeshSocket*, UStaticMeshSocket*> FBoundsPair;
    TArray<FBoundsPair> BoundSockets;

    const FString MIN_PREFIX(TEXT("Bounds_MIN_"));
    const FString MAX_PREFIX(TEXT("Bounds_MAX_"));
    const int32 PREFIX_LEN = MIN_PREFIX.Len();

    for (UStaticMeshSocket* Socket0 : Sockets)
    {
        // Invalid socket, skip
        if (! IsValid(Socket0))
        {
            continue;
        }

        FString MinSocketName(Socket0->SocketName.ToString());
        FString MaxSocketName(MAX_PREFIX);

        // Not a min bounds socket, skip
        if (! MinSocketName.StartsWith(*MIN_PREFIX, ESearchCase::IgnoreCase))
        {
            continue;
        }

        MaxSocketName += MinSocketName.RightChop(PREFIX_LEN);

        for (UStaticMeshSocket* Socket1 : Sockets)
        {
            if (IsValid(Socket1) && Socket1->SocketName.ToString() == MaxSocketName)
            {
                BoundSockets.Emplace(Socket0, Socket1);
                break;
            }
        }
    }

    for (FBoundsPair BoundsPair : BoundSockets)
    {
        FBox2D Box;
        const FVector& Min(BoundsPair.Key->RelativeLocation);
        const FVector& Max(BoundsPair.Value->RelativeLocation);
        const float MinX = FMath::RoundHalfFromZero(Min.X);
        const float MinY = FMath::RoundHalfFromZero(Min.Y);
        const float MaxX = FMath::RoundHalfFromZero(Max.X);
        const float MaxY = FMath::RoundHalfFromZero(Max.Y);
        Box += FVector2D(MinX, MinY);
        Box += FVector2D(MaxX, MaxY);
        Bounds.Emplace(Box);
    }
}

TArray<FBox2D> FPMUVoxelMap::GetPrefabBounds(int32 PrefabIndex) const
{
    TArray<FBox2D> Bounds;
    GetPrefabBounds(PrefabIndex, Bounds);
    return Bounds;
}

bool FPMUVoxelMap::ApplyPrefab(
    int32 PrefabIndex,
    int32 SectionIndex,
    FVector Center,
    bool bApplyHeightMap,
    bool bInverseHeightMap,
    FPMUMeshSection& OutSection
    )
{
    int32 LODIndex = 0;

    // Check prefab validity
    if (! IsPrefabValid(PrefabIndex, LODIndex, SectionIndex))
    {
        return false;
    }

    UStaticMesh* InMesh = meshPrefabs[PrefabIndex];
    const FStaticMeshLODResources& LOD(InMesh->RenderData->LODResources[LODIndex]);

    // Place prefab only if the prefab would not intersect with applied prefabs
    if (! TryPlacePrefabAt(PrefabIndex, FVector2D(Center)))
    {
        return false;
    }

    bool bApplySuccess = false;

    // Map from vert buffer for whole mesh to vert buffer for section of interest
    TMap<int32, int32> MeshToSectionVertMap;
    TSet<int32> BorderVertSet;

    const FStaticMeshSection& Section = LOD.Sections[SectionIndex];
    const uint32 OnePastLastIndex = Section.FirstIndex + Section.NumTriangles * 3;

    // Empty output buffers
    FIndexArrayView Indices = LOD.IndexBuffer.GetArrayView();
    const int32 VertexCountOffset = OutSection.GetVertexCount();

    // Iterate over section index buffer, copying verts as needed
    for (uint32 i = Section.FirstIndex; i < OnePastLastIndex; i++)
    {
        uint32 MeshVertIndex = Indices[i];

        // See if we have this vert already in our section vert buffer,
        // copy vert in if not 
        int32 SectionVertIndex = GetNewIndexForOldVertIndex(
            MeshVertIndex,
            MeshToSectionVertMap,
            BorderVertSet,
            Center,
            ! bApplyHeightMap,
            &LOD.PositionVertexBuffer,
            &LOD.VertexBuffer,
            &LOD.ColorVertexBuffer,
            OutSection
            );

        // Add to index buffer
        OutSection.IndexBuffer.Emplace(SectionVertIndex);
    }

    // Mark success
    bApplySuccess = true;

    if (! bApplyHeightMap || ! GridData)
    {
        return bApplySuccess;
    }

    const FPMUGridData* GD = GridData;
    int32 ShapeHeightMapId   = -1;
    int32 SurfaceHeightMapId = -1;
    int32 ExtrudeHeightMapId = -1;
    uint8 HeightMapType = 0;

    if (GD)
    {
        ShapeHeightMapId   = GridData->GetNamedHeightMapId(TEXT("PMU_VOXEL_SHAPE_HEIGHT_MAP"));
        SurfaceHeightMapId = GridData->GetNamedHeightMapId(TEXT("PMU_VOXEL_SURFACE_HEIGHT_MAP"));
        ExtrudeHeightMapId = GridData->GetNamedHeightMapId(TEXT("PMU_VOXEL_EXTRUDE_HEIGHT_MAP"));
        const bool bHasShapeHeightMap   = ShapeHeightMapId   >= 0;
        const bool bHasSurfaceHeightMap = SurfaceHeightMapId >= 0;
        const bool bHasExtrudeHeightMap = ExtrudeHeightMapId >= 0;
        HeightMapType = (bHasShapeHeightMap ? 1 : 0) | ((bHasSurfaceHeightMap ? 1 : 0) << 1) | ((bHasExtrudeHeightMap ? 1 : 0) << 2);
    }

    float voxelSize     = 1.0f;
    float voxelSizeInv  = 1.0f;
    float voxelSizeHalf = voxelSize / 2.f;
    const int32 VertexCount = OutSection.GetVertexCount();

    const FIntPoint& Dim(GridData->Dimension);
    FVector2D GridRangeMin = FVector2D(1.f, 1.f);
    FVector2D GridRangeMax = FVector2D(Dim.X-2, Dim.Y-2);

    for (int32 i=VertexCountOffset; i<VertexCount; ++i)
    {
        FVector& Position(OutSection.VertexBuffer[i].Position);
        FVector& VNormal(OutSection.VertexBuffer[i].Normal);

        const float PX = Position.X + voxelSizeHalf;
        const float PY = Position.Y + voxelSizeHalf;
        const float GridX = FMath::Clamp(PX, GridRangeMin.X, GridRangeMax.X);
        const float GridY = FMath::Clamp(PY, GridRangeMin.Y, GridRangeMax.Y);
        const int32 IX = GridX;
        const int32 IY = GridY;

        const float Z = Position.Z;
        float Height = Z;
        FVector Normal(0.f, 0.f, bInverseHeightMap ? -1.f : 1.f);

        if (HeightMapType != 0)
        {
            check(GD != nullptr);

            if (bInverseHeightMap)
            {
                switch (HeightMapType & 5)
                {
                    case 1:
                        UPMUGridHeightMapUtility::GetHeightNormal(GridX, GridY, IX, IY, *GD, ShapeHeightMapId, Height, Normal);
                        break;

                    case 4:
                    case 5:
                        UPMUGridHeightMapUtility::GetHeightNormal(GridX, GridY, IX, IY, *GD, ExtrudeHeightMapId, Height, Normal);
                        break;
                }

                Height += Z;
                Normal = -Normal;
            }
            else
            {
                switch (HeightMapType & 3)
                {
                    case 1:
                        UPMUGridHeightMapUtility::GetHeightNormal(GridX, GridY, IX, IY, *GD, ShapeHeightMapId, Height, Normal);
                        break;

                    case 2:
                    case 3:
                        UPMUGridHeightMapUtility::GetHeightNormal(GridX, GridY, IX, IY, *GD, SurfaceHeightMapId, Height, Normal);
                        break;
                }
            }
        }

        Position.Z += Height;
        VNormal = BorderVertSet.Contains(i)
            ? Normal
            : (VNormal+Normal).GetSafeNormal();

        OutSection.LocalBox += Position;
    }

    return bApplySuccess;
}

// ----------------------------------------------------------------------------

// MAP SETTINGS FUNCTIONS

void UPMUVoxelMapRef::ApplyMapSettings()
{
    VoxelMap.mapSize = MapSize;
    VoxelMap.voxelResolution = VoxelResolution;
    VoxelMap.chunkResolution = ChunkResolution;
    VoxelMap.extrusionHeight = ExtrusionHeight;

    VoxelMap.maxFeatureAngle = MaxFeatureAngle;
    VoxelMap.maxParallelAngle = MaxParallelAngle;

    VoxelMap.surfaceStates = SurfaceStates;
    VoxelMap.meshPrefabs = MeshPrefabs;

    if (GridData)
    {
        VoxelMap.GridData = &GridData->GridData;
    }

#ifdef PMU_VOXEL_USE_OCL

    if (bUseGPUProgram)
    {
        VoxelMap.GPUProgram = BuildGPUProgram();
        VoxelMap.GPUProgramKernelName = GPUProgramKernelName;
    }

#endif
}

#ifdef PMU_VOXEL_USE_OCL

TSharedPtr<FOCLBProgram> UPMUVoxelMapRef::BuildGPUProgram()
{
    if (GPUProgramName.IsNone())
    {
        return nullptr;
    }

    IProceduralMeshUtility& PMU(IProceduralMeshUtility::Get());
    TSharedPtr<FOCLBProgram> Program = PMU.GetGPUProgramWeak(GPUProgramName).Pin();

    if (! Program.IsValid() || ! Program->HasValidProgram())
    {
        Program = MakeShareable(new FOCLBProgram(
            GPUProgramSourcePaths,
            GPUProgramBuildOptions
            ) );
        Program->Build();

        if (Program->HasValidProgram())
        {
            PMU.SetGPUProgramWeak(GPUProgramName, Program.ToSharedRef(), true);
        }
        else
        {
            Program = nullptr;
        }
    }

    return Program;
}

#endif // PMU_VOXEL_USE_OCL

// TRIANGULATION FUNCTIONS

void UPMUVoxelMapRef::EditMapAsync(FGWTAsyncTaskRef& TaskRef, const TArray<UPMUVoxelStencilRef*>& Stencils)
{
    if (! TaskRef.IsValid())
    {
        FGWTAsyncTaskRef::Init(
            TaskRef,
            IProceduralMeshUtility::Get().GetThreadPool());
    }

    check(TaskRef.IsValid());

    FPSGWTAsyncTask& Task(TaskRef.Task);

    TArray<UPMUVoxelStencilRef*> ValidStencils(
        Stencils.FilterByPredicate(
            [&](UPMUVoxelStencilRef* const & Stencil) { return IsValid(Stencil); }
        ) );

    for (int32 i=0; i<ValidStencils.Num(); ++i)
    {
        UPMUVoxelStencilRef* Stencil(ValidStencils[i]);

        check(IsValid(Stencil));

        Stencil->EditMapAsync(Task, VoxelMap);
    }
}

void UPMUVoxelMapRef::EditStatesAsync(FGWTAsyncTaskRef& TaskRef, const TArray<UPMUVoxelStencilRef*>& Stencils)
{
    if (! TaskRef.IsValid())
    {
        FGWTAsyncTaskRef::Init(
            TaskRef,
            IProceduralMeshUtility::Get().GetThreadPool());
    }

    check(TaskRef.IsValid());

    FPSGWTAsyncTask& Task(TaskRef.Task);

    TArray<UPMUVoxelStencilRef*> ValidStencils(
        Stencils.FilterByPredicate(
            [&](UPMUVoxelStencilRef* const & Stencil) { return IsValid(Stencil); }
        ) );

    for (int32 i=0; i<ValidStencils.Num(); ++i)
    {
        UPMUVoxelStencilRef* Stencil(ValidStencils[i]);

        check(IsValid(Stencil));

        //Stencil->EditStatesAsync(Task, VoxelMap);

        Task->AddTask(
            [this, Stencil]()
            {
                Stencil->EditStates(VoxelMap);
            } );
    }
}

void UPMUVoxelMapRef::EditCrossingsAsync(FGWTAsyncTaskRef& TaskRef, const TArray<UPMUVoxelStencilRef*>& Stencils)
{
    if (! TaskRef.IsValid())
    {
        FGWTAsyncTaskRef::Init(
            TaskRef,
            IProceduralMeshUtility::Get().GetThreadPool());
    }

    check(TaskRef.IsValid());

    FPSGWTAsyncTask& Task(TaskRef.Task);

    TArray<UPMUVoxelStencilRef*> ValidStencils(
        Stencils.FilterByPredicate(
            [&](UPMUVoxelStencilRef* const & Stencil) { return IsValid(Stencil); }
        ) );

    for (int32 i=0; i<ValidStencils.Num(); ++i)
    {
        UPMUVoxelStencilRef* Stencil(ValidStencils[i]);

        check(IsValid(Stencil));

        //Stencil->EditCrossingsAsync(Task, VoxelMap);

        Task->AddTask(
            [this, Stencil]()
            {
                Stencil->EditCrossings(VoxelMap);
            } );
    }
}

UPMUVoxelMapRef* UPMUVoxelMapRef::Copy(UObject* Outer) const
{
    UPMUVoxelMapRef* MapCopy = NewObject<UPMUVoxelMapRef>(Outer);

    MapCopy->GridData = GridData;
    MapCopy->MapSize = MapSize;
	MapCopy->VoxelResolution = VoxelResolution;
	MapCopy->ChunkResolution = ChunkResolution;
    MapCopy->SurfaceStates = SurfaceStates;
	MapCopy->ExtrusionHeight = ExtrusionHeight;
	MapCopy->MaxFeatureAngle = MaxFeatureAngle;
	MapCopy->MaxParallelAngle = MaxParallelAngle;
    MapCopy->MeshPrefabs = MeshPrefabs;

#ifdef PMU_VOXEL_USE_OCL

    MapCopy->bUseGPUProgram         = bUseGPUProgram;
    MapCopy->GPUProgramName         = GPUProgramName;
    MapCopy->GPUProgramKernelName   = GPUProgramKernelName;
    MapCopy->GPUProgramSourcePaths  = GPUProgramSourcePaths;
    MapCopy->GPUProgramBuildOptions = GPUProgramBuildOptions;

#endif

    if (IsInitialized())
    {
        MapCopy->ApplyMapSettings();
        MapCopy->VoxelMap.CopyFrom(VoxelMap);
    }

    return MapCopy;
}

void UPMUVoxelMapRef::ResetChunkStates(const TArray<int32>& ChunkIndices)
{
    if (IsInitialized())
    {
        VoxelMap.ResetChunkStates(ChunkIndices);
    }
}

void UPMUVoxelMapRef::ResetAllChunkStates()
{
    if (IsInitialized())
    {
        VoxelMap.ResetAllChunkStates();
    }
}

// CHUNK & SECTION FUNCTIONS

FVector UPMUVoxelMapRef::GetChunkPosition(int32 ChunkIndex) const
{
    return HasChunk(ChunkIndex)
        ? FVector(VoxelMap.GetChunk(ChunkIndex).position, 0.f)
        : FVector();
}

FPMUMeshSection UPMUVoxelMapRef::GetSection(int32 ChunkIndex, int32 StateIndex) const
{
    if (HasChunk(ChunkIndex))
    {
        const FPMUMeshSection* Section = VoxelMap.GetChunk(ChunkIndex).GetSection(StateIndex);
        return Section ? *Section : FPMUMeshSection();
    }

    return FPMUMeshSection();
}

// PREFAB FUNCTIONS

TArray<FBox2D> UPMUVoxelMapRef::GetPrefabBounds(int32 PrefabIndex) const
{
    return IsInitialized()
        ? VoxelMap.GetPrefabBounds(PrefabIndex)
        : TArray<FBox2D>();
}

bool UPMUVoxelMapRef::HasPrefab(int32 PrefabIndex) const
{
    return IsInitialized() ? VoxelMap.HasPrefab(PrefabIndex) : false;
}

bool UPMUVoxelMapRef::ApplyPrefab(
    int32 PrefabIndex,
    int32 SectionIndex,
    FVector Location,
    bool bApplyHeightMap,
    bool bInverseHeightMap,
    FPMUMeshSection& OutSection
    )
{
    if (IsInitialized())
    {
        return VoxelMap.ApplyPrefab(
            PrefabIndex,
            SectionIndex,
            Location,
            bApplyHeightMap,
            bInverseHeightMap,
            OutSection
            );
    }

    return false;
}
