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

#include "PMUVoxelSurface.h"

#ifdef PMU_VOXEL_USE_OCL

#include "OCLBError.h"
#include "OCLBProgram.h"

namespace bc = boost::compute;

#pragma warning( push )
#pragma warning( disable : 4628 4668 )

#include "boost/compute/context.hpp"
#include "boost/compute/program.hpp"
#include "boost/compute/kernel.hpp"
#include "boost/compute/container/typed_buffer.hpp"
#include "boost/compute/image/image2d.hpp"

#pragma warning( pop )

struct BC_VPMVertex
{
    bc::float_ px;
    bc::float_ py;
    bc::float_ pz;

    bc::float_ nx;
    bc::float_ ny;
    bc::float_ nz;

    bc::uchar4_ c;
};

#endif // PMU_VOXEL_USE_OCL

void FPMUVoxelSurface::Initialize(const FPMUVoxelSurfaceConfig& Config)
{
    // Position & dimension configuration

    position = Config.Position;
    gridSize = Config.ChunkSize;
    mapSize  = Config.MapSize;
    voxelResolution = Config.VoxelResolution;
    voxelCount      = voxelResolution * voxelResolution;
    voxelSize       = gridSize / voxelResolution;
    voxelSizeHalf   = voxelSize / 2.f;
    voxelSizeInv    = 1.f / voxelSize;
    SimplifierOptions = Config.SimplifierOptions;

    // Extrusion configuration

    bGenerateExtrusion = Config.bGenerateExtrusion;
    bExtrusionSurface  = (! bGenerateExtrusion && Config.bExtrusionSurface);
    extrusionHeight = (FMath::Abs(Config.ExtrusionHeight) > 0.01f) ? -FMath::Abs(Config.ExtrusionHeight) : -1.f;

    // Grid data height map configuration

    if (Config.GridData)
    {
        GridData = Config.GridData;

        // Height map type
        ShapeHeightMapId   = GridData->GetNamedHeightMapId(SHAPE_HEIGHT_MAP_NAME);
        SurfaceHeightMapId = GridData->GetNamedHeightMapId(SURFACE_HEIGHT_MAP_NAME);
        ExtrudeHeightMapId = GridData->GetNamedHeightMapId(EXTRUDE_HEIGHT_MAP_NAME);
        const bool bHasShapeHeightMap   = ShapeHeightMapId   >= 0;
        const bool bHasSurfaceHeightMap = SurfaceHeightMapId >= 0;;
        const bool bHasExtrudeHeightMap = ExtrudeHeightMapId >= 0;;
        HeightMapType =  (bHasShapeHeightMap   ? 1 : 0)       |
                        ((bHasSurfaceHeightMap ? 1 : 0) << 1) |
                        ((bHasExtrudeHeightMap ? 1 : 0) << 2);
        bHasHeightMap = (HeightMapType != 0);

        // Range of grid dimension that is safe to do value interpolation with
        // (1, GridData.Dimension - 2)
        const FIntPoint& dim(GridData->Dimension);
        GridRangeMin = FVector2D(1.f, 1.f);
        GridRangeMax = FVector2D(dim.X-2, dim.Y-2);
    }
    else
    {
        bHasHeightMap = false;
        GridData = nullptr;
        HeightMapType = 0;
        ShapeHeightMapId   = -1;
        SurfaceHeightMapId = -1;
        ExtrudeHeightMapId = -1;
        GridRangeMin = FVector2D::ZeroVector;
        GridRangeMax = FVector2D::ZeroVector;
    }

    // Gradient data configuration

    const FPMUVoxelGradientConfig& GradConf(Config.GradientConfig);

    // Reset gradient data
    GradientPartitions.Empty();
    GradientMap = nullptr;
    bHasGradientData = false;

    if (IsValid(GradConf.GradientData) && IsValid(GradConf.DistanceFieldData))
    {
        GradientMap  = GradConf.GradientData->GradientData.GetNamedMap(GradConf.GradientMapName);
        GradientType = GradConf.GradientType;
        GradientExponent = FMath::Max(GradConf.GradientExponent, 1.f);

        const bool bHasValidGradientType = (GradientType == 0) || (GradientType == 1);

        if (GradientMap && bHasValidGradientType)
        {
            const FPMUDFGeometryData& GradientDF(GradConf.DistanceFieldData->GeometryData);
            TArray<FName> GradientDFNames  = GradConf.DistanceFieldNames;
            int32 GradientDFPartitionDepth = GradConf.DistanceFieldPartitionDepth;
            int32 GradientDFGroupCount     = (GradientType == 0) ? 1 : 3;
            bool bHasValidGroups = false;

            // Make sure gradient distance field have the required geometry groups
            if (GradientDFNames.Num() >= GradientDFGroupCount)
            {
                TArray<int32> GroupIds;

                // Resolve distance field geometry group names into indices

                for (int32 i=0; i<GradientDFGroupCount; ++i)
                {
                    int32 GroupId = GradientDF.GetGroupIdByName(GradientDFNames[i]);

                    if (GradientDF.HasGroup(GroupId))
                    {
                        GroupIds.Emplace(GroupId);
                    }
                }

                // Only create partition if geometry group have been resolved correctly

                if (GroupIds.Num() == GradientDFGroupCount)
                {
                    GradientPartitions.SetNum(GradientDFGroupCount);

                    for (int32 i=0; i<GroupIds.Num(); ++i)
                    {
                        GradientPartitions[i].Initialize(position, gridSize, GradientDFPartitionDepth);
                        bHasValidGroups |= GradientPartitions[i].Construct(GradientDF.GetGroupChecked(GroupIds[i]));
                    }
                }
            }

            if (bHasValidGroups)
            {
                bHasGradientData = true;
            }
            else
            {
                GradientPartitions.Empty();
                bHasGradientData = false;
            }
        }
    }

#ifdef PMU_VOXEL_USE_OCL
    // GPU program configuration

    if (Config.GPUProgram && bHasHeightMap)
    {
        check(GridData != nullptr);

        GPUProgram           = Config.GPUProgram;
        GPUProgramKernelName = Config.GPUProgramKernelName;
        bUseGPUProgram = true;
    }
    else
    {
        GPUProgram     = nullptr;
        bUseGPUProgram = false;
    }

#endif

    // Resize vertex cache containers
    
    cornersMin.SetNum(voxelResolution + 1);
    cornersMax.SetNum(voxelResolution + 1);

    xEdgesMin.SetNum(voxelResolution);
    xEdgesMax.SetNum(voxelResolution);

    // Reserves geometry container spaces

    Section.VertexBuffer.Reserve(voxelCount);
    Section.IndexBuffer.Reserve(voxelCount * 6);
}

void FPMUVoxelSurface::CopyFrom(const FPMUVoxelSurface& Surface)
{
    position = Surface.position;
    gridSize = Surface.gridSize;
    mapSize  = Surface.mapSize;

    voxelResolution = Surface.voxelResolution;
    voxelCount      = Surface.voxelCount;
    voxelSize       = Surface.voxelSize;
    voxelSizeHalf   = Surface.voxelSizeHalf;
    voxelSizeInv    = Surface.voxelSizeInv;

    SimplifierOptions = Surface.SimplifierOptions;

    // Extrusion configuration

    bGenerateExtrusion = Surface.bGenerateExtrusion;
    bExtrusionSurface  = Surface.bExtrusionSurface;
    extrusionHeight    = Surface.extrusionHeight;

    // Grid data height map configuration

    GridData           = Surface.GridData;
    bHasHeightMap      = Surface.bHasHeightMap;
    ShapeHeightMapId   = Surface.ShapeHeightMapId;
    SurfaceHeightMapId = Surface.SurfaceHeightMapId;
    ExtrudeHeightMapId = Surface.ExtrudeHeightMapId;
    HeightMapType      = Surface.HeightMapType;
    GridRangeMin       = Surface.GridRangeMin;
    GridRangeMax       = Surface.GridRangeMax;

    // Gradient data configuration

    // Reset gradient data
    GradientPartitions.Empty();

    GradientMap      = Surface.GradientMap;
    bHasGradientData = Surface.bHasGradientData;

    if (bHasGradientData)
    {
        GradientMap  = Surface.GradientMap;
        GradientType = Surface.GradientType;
        GradientExponent = Surface.GradientExponent;

        GradientPartitions.SetNum(Surface.GradientPartitions.Num());

        for (int32 i=0; i<GradientPartitions.Num(); ++i)
        {
            GradientPartitions[i].CopyFrom(Surface.GradientPartitions[i]);
        }
    }

#ifdef PMU_VOXEL_USE_OCL
    // GPU program configuration
    GPUProgram     = Surface.GPUProgram;
    bUseGPUProgram = Surface.bUseGPUProgram;
#endif

    // Resize vertex cache containers
    
    cornersMin.SetNum(voxelResolution + 1);
    cornersMax.SetNum(voxelResolution + 1);

    xEdgesMin.SetNum(voxelResolution);
    xEdgesMax.SetNum(voxelResolution);

    // Reserves geometry container spaces

    Section.VertexBuffer.Reserve(voxelCount);
    Section.IndexBuffer.Reserve(voxelCount * 6);
}

void FPMUVoxelSurface::GenerateEdgeNormals()
{
    if (bGenerateExtrusion)
    {
        for (const TPair<int32, int32>& EdgePair : EdgePairs)
        {
            const int32& a(EdgePair.Key);
            const int32& b(EdgePair.Value);

            const FVector& v0(Section.VertexBuffer[a].Position);
            const FVector& v1(Section.VertexBuffer[b].Position);

            const FVector EdgeDirection = (v0-v1).GetSafeNormal();
            const FVector EdgeCross = EdgeDirection ^ FVector::UpVector;

            Section.VertexBuffer[a  ].Normal += EdgeCross;
            Section.VertexBuffer[b  ].Normal += EdgeCross;
            Section.VertexBuffer[a+1].Normal += EdgeCross;
            Section.VertexBuffer[b+1].Normal += EdgeCross;
        }

        for (int32 i : EdgeIndexSet)
        {
            Section.VertexBuffer[i  ].Normal.Normalize();
            Section.VertexBuffer[i+1].Normal.Normalize();
        }
    }
}

void FPMUVoxelSurface::ApplyVertex()
{
    if (bGenerateExtrusion && SimplifierOptions.bEnabled)
    {
        // Generate vertex height normal
        if (bHasHeightMap)
        {
            for (FPMUMeshVertex& Vertex : Section.VertexBuffer)
            {
                FVector& Position(Vertex.Position);
                const float PX = Position.X + voxelSizeHalf;
                const float PY = Position.Y + voxelSizeHalf;

                // Map height
                GetVertexHeight(PX, PY, (Vertex.Normal.Z < 0.f), Vertex.Normal, Position.Z);

                Section.LocalBox += Position;
            }
        }

        GenerateEdgeNormals();

        // Simplify mesh
        FPMUMeshSimplifier Simplifier;
        Simplifier.Simplify(
            Section,
            FVector::ZeroVector,
            SimplifierOptions
            );

        // Generate vertex color gradient
        if (bHasGradientData)
        {
            for (FPMUMeshVertex& Vertex : Section.VertexBuffer)
            {
                FVector& Position(Vertex.Position);
                const float PX = Position.X + voxelSizeHalf;
                const float PY = Position.Y + voxelSizeHalf;

                // Map color gradient
                GetVertexGradient(PX, PY, Vertex.Color);
            }
        }
    }
    else
    {
        if (bHasHeightMap || bHasGradientData)
        {
            for (FPMUMeshVertex& Vertex : Section.VertexBuffer)
            {
                FVector& Position(Vertex.Position);
                const float PX = Position.X + voxelSizeHalf;
                const float PY = Position.Y + voxelSizeHalf;

                // Map height
                if (bHasHeightMap)
                {
                    GetVertexHeight(PX, PY, (Vertex.Normal.Z < 0.f), Vertex.Normal, Position.Z);
                }

                // Map color gradient
                if (bHasGradientData)
                {
                    GetVertexGradient(PX, PY, Vertex.Color);
                }

                Section.LocalBox += Position;
            }
        }

        // Simplify mesh if required
        if (SimplifierOptions.bEnabled)
        {
            FPMUMeshSimplifier Simplifier;
            Simplifier.Simplify(
                Section,
                FVector::ZeroVector,
                SimplifierOptions
                );
        }
        else if (bGenerateExtrusion)
        {
            GenerateEdgeNormals();
        }
    }
}

#ifdef PMU_VOXEL_USE_OCL

void FPMUVoxelSurface::ApplyVertex_GPU()
{
    bool bGPUSuccess = ApplyVertex_GPU_Impl();

    // GPU operation failed, fallback to CPU implemention
    if (! bGPUSuccess)
    {
        ApplyVertex();
    }
}

bool FPMUVoxelSurface::ApplyVertex_GPU_Impl()
{
    const int32 VertexNum  = Section.VertexBuffer.Num();

    // GPU program is not enabled or not enough vertex count, abort
    if (! bUseGPUProgram || VertexNum < 3)
    {
        return false;
    }

    const FIntPoint MapDim = GridData->Dimension;
    bool bGPUSuccess = false;

    check(GridData != nullptr);
    check(GPUProgram->HasValidProgram());

    bc::program& Program(GPUProgram->GetProgram());

    OCL_TRY(PMU_VOXEL_ApplyVertex)

    bc::context Context(Program.get_context());
    bc::command_queue Queue(Context, Context.get_device());

    OCL_TRY(K_PMU_VOXEL_GenerateVertexHeightNormal)

    bc::kernel K_GenerateVertexHeight;
    bc::typed_buffer<BC_VPMVertex> d_Vertices(Context, VertexNum);
    bc::image2d t_Map0;
    bc::image2d t_Map1;

    // Construct surface height map

    if (bGenerateExtrusion || !bExtrusionSurface)
    {
        bc::image2d Image;
        float* HeightMap = nullptr;

        switch (HeightMapType & 3)
        {
            case 1:
                HeightMap = GridData->GetHeightMapChecked(ShapeHeightMapId).GetData();
                break;

            case 2:
            case 3:
                HeightMap = GridData->GetHeightMapChecked(SurfaceHeightMapId).GetData();
                break;
        }

        if (HeightMap != nullptr)
        {
            Image = bc::image2d(
                Context,
                MapDim.X,
                MapDim.Y,
                bc::image_format(
                    bc::image_format::r,
                    bc::image_format::float32
                    ),
                bc::image2d::read_only
                );

            Queue.enqueue_write_image(
                Image,
                Image.origin(),
                Image.size(),
                HeightMap
                ).wait();

            t_Map0 = MoveTemp(Image);
        }
    }

    // Construct extrude height map

    if (bGenerateExtrusion || bExtrusionSurface)
    {
        bc::image2d Image;
        float* HeightMap = nullptr;

        switch (HeightMapType & 5)
        {
            case 1:
                HeightMap = GridData->GetHeightMapChecked(ShapeHeightMapId).GetData();
                break;

            case 4:
            case 5:
                HeightMap = GridData->GetHeightMapChecked(ExtrudeHeightMapId).GetData();
                break;
        }

        if (HeightMap != nullptr)
        {
            Image = bc::image2d(
                Context,
                MapDim.X,
                MapDim.Y,
                bc::image_format(
                    bc::image_format::r,
                    bc::image_format::float32
                    ),
                bc::image2d::read_only
                );

            Queue.enqueue_write_image(
                Image,
                Image.origin(),
                Image.size(),
                HeightMap
                ).wait();

            t_Map1 = MoveTemp(Image);
        }
    }

    const bool bHasSurfaceMap = (t_Map0 != bc::image2d());
    const bool bHasExtrudeMap = (t_Map1 != bc::image2d());
    bool bHasValidKernel = false;

    if (bGenerateExtrusion)
    {
        if (bHasSurfaceMap && bHasExtrudeMap)
        {
            K_GenerateVertexHeight = bc::kernel(Program, "PMU_VOXEL_GenerateVertexHeightNormal_Dual");
            K_GenerateVertexHeight.set_args(
                t_Map0,
                t_Map1,
                d_Vertices
                );
        }
        else if (bHasSurfaceMap || bHasExtrudeMap)
        {
            const bool bIsSurface = (t_Map0 != bc::image2d());
            K_GenerateVertexHeight = bc::kernel(Program, "PMU_VOXEL_GenerateVertexHeightNormal");
            K_GenerateVertexHeight.set_args(
                bHasSurfaceMap ? t_Map0 : t_Map1,
                bHasSurfaceMap ? 1.f : -1.f,
                d_Vertices
                );
        }

        bHasValidKernel = (K_GenerateVertexHeight != bc::kernel());
    }
    else
    {
        if (bExtrusionSurface)
        {
            if (bHasExtrudeMap)
            {
                K_GenerateVertexHeight = bc::kernel(Program, "PMU_VOXEL_GenerateVertexHeightNormal_Extrude");
            }
        }
        else
        {
            if (bHasSurfaceMap)
            {
                K_GenerateVertexHeight = bc::kernel(Program, "PMU_VOXEL_GenerateVertexHeightNormal_Surface");
            }
        }

        bHasValidKernel = (K_GenerateVertexHeight != bc::kernel());

        if (bHasValidKernel)
        {
            K_GenerateVertexHeight.set_args(
                bHasSurfaceMap ? t_Map0 : t_Map1,
                d_Vertices
                );
        }
    }

    if (bHasValidKernel)
    {
        // Execute kernel
        Queue.enqueue_read_buffer(
            d_Vertices.get_buffer(),
            0,
            Section.VertexBuffer.Num() * Section.VertexBuffer.GetTypeSize(),
            Section.VertexBuffer.GetData(),
            Queue.enqueue_1d_range_kernel(
                K_GenerateVertexHeight,
                0,
                VertexNum,
                0,
                Queue.enqueue_write_buffer_async(
                    d_Vertices.get_buffer(),
                    0,
                    d_Vertices.get_buffer().size(),
                    Section.VertexBuffer.GetData()
                    )
                )
            );

        bGPUSuccess = true;

        // Generate extrusion edge normals if required
        if (bGenerateExtrusion)
        {
            GenerateEdgeNormals();
        }

        // Simplify mesh if required
        if (SimplifierOptions.bEnabled)
        {
            FPMUMeshSimplifier Simplifier;
            Simplifier.Simplify(
                Section,
                FVector::ZeroVector,
                SimplifierOptions
                );
        }

        // Expand local bounds and generate gradient data
        if (bHasGradientData)
        {
            for (FPMUMeshVertex& Vertex : Section.VertexBuffer)
            {
                FVector& Position(Vertex.Position);
                const float PX = Position.X + voxelSizeHalf;
                const float PY = Position.Y + voxelSizeHalf;
                GetVertexGradient(PX, PY, Vertex.Color);

                Section.LocalBox += Position;
            }
        }
        else
        {
            for (FPMUMeshVertex& Vertex : Section.VertexBuffer)
            {
                Section.LocalBox += Vertex.Position;
            }
        }
    }

    OCL_RTL(K_PMU_VOXEL_GenerateVertexHeightNormal, LogPMU)

    OCL_CTL(PMU_VOXEL_ApplyVertex, LogPMU)

    return bGPUSuccess;
}

#endif // PMU_VOXEL_USE_OCL
