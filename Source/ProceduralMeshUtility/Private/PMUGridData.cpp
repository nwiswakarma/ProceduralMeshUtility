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

#include "PMUGridData.h"
#include "PMUUtilityLibrary.h"
#include "Mesh/PMUMeshTypes.h"
#include "Grid/PMUGridUtility.h"
#include "Grid/HeightMap/PMUGridHeightMapUtility.h"
#include "Grid/Tasks/PMUGridGenerationTask.h"

#include "AGGTypes.h"
#include "AGGTypedContext.h"
#include "AGGRenderer.h"

#include "GWTAsyncTypes.h"

#include "Engine/Texture2D.h"

void FPMUGridData::Empty()
{
    PointMask.Empty();
    HeightMaps.Empty();
    NamedHeightMap.Empty();

    PointSet.Empty();
    BorderSet.Empty();
}

FPMUMeshSection UPMUGridInstance::CreateMeshSection(int32 HeightMapId, bool bWinding)
{
    FPMUMeshSection Section;

    const FIntPoint Dimension(GetDimension());
    const int32 DimX(Dimension.X);
    const int32 DimY(Dimension.Y);

    // Invalid dimension, abort
    if (DimX < 2 || DimY < 2)
    {
        return Section;
    }

    // Construct vertex buffer

    TArray<FPMUMeshVertex>& Vertices(Section.VertexBuffer);
    const int32 VertexCount = DimX * DimY;

    Vertices.SetNumUninitialized(VertexCount);

    const FPMUGridData::FHeightMap* HeightMapPtr = GridData.GetHeightMap(HeightMapId);

    if (HeightMapPtr)
    {
        const FPMUGridData::FHeightMap& HeightMap(*HeightMapPtr);

        for (int32 y=0,i=0; y<DimY; ++y)
        for (int32 x=0;     x<DimX; ++x, ++i)
        {
            if (x > 1 && x < (DimX-2) && y > 1 && y < (DimY-2))
            {
                FVector Position(x, y, 0);
                FVector Normal;
                UPMUGridHeightMapUtility::GetHeightNormal(0.f, 0.f, x, y, GridData, HeightMapId, Position.Z, Normal);
                Vertices[i] = FPMUMeshVertex(Position, Normal);
            }
            else
            {
                Vertices[i] = FPMUMeshVertex(FVector(x, y, 0), FVector(0,0,1));
            }
        }
    }
    else
    {
        for (int32 y=0,i=0; y<DimY; ++y)
        for (int32 x=0;     x<DimX; ++x, ++i)
        {
            Vertices[i] = FPMUMeshVertex(FVector(x, y, 0), FVector(0,0,1));
        }
    }

    // Construct index buffer

    enum { INDEX_COUNT_PER_QUAD = 6 };

    TArray<int32>& Indices(Section.IndexBuffer);

    const int32 QuadCountX = DimX-1;
    const int32 QuadCountY = DimY-1;
    const int32 QuadCount = QuadCountX * QuadCountY;

    Indices.Reserve(QuadCount * INDEX_COUNT_PER_QUAD);

    for (int32 XIdx=0; XIdx<QuadCountX; ++XIdx)
    for (int32 YIdx=0; YIdx<QuadCountY; ++YIdx)
    {
        const int32 I0 = (XIdx+0) + (YIdx+0)*DimX;
        const int32 I1 = (XIdx+1) + (YIdx+0)*DimX;
        const int32 I2 = (XIdx+1) + (YIdx+1)*DimX;
        const int32 I3 = (XIdx+0) + (YIdx+1)*DimX;

        if (bWinding)
        {
            Indices.Emplace(I0);
            Indices.Emplace(I1);
            Indices.Emplace(I3);

            Indices.Emplace(I1);
            Indices.Emplace(I2);
            Indices.Emplace(I3);
        }
        else
        {
            Indices.Emplace(I0);
            Indices.Emplace(I3);
            Indices.Emplace(I1);

            Indices.Emplace(I3);
            Indices.Emplace(I2);
            Indices.Emplace(I1);
        }
    }

    // Calculate section bounds

    Section.LocalBox = FBox(FVector(0,0,-1), FVector(DimX,DimY,1));

    return Section;
}

void UPMUGridInstance::CreateGPUMeshSection(
    UObject* WorldContextObject,
    FPMUMeshSectionResourceRef Section,
    FPMUShaderTextureParameterInput TextureInput,
    float HeightScale,
    float BoundsHeightMin,
    float BoundsHeightMax,
    bool bReverseWinding,
    UGWTTickEvent* CallbackEvent
    )
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

    if (! IsValid(World))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUGridInstance::CreateGPUMeshSection() ABORTED, INVALID WORLD CONTEXT OBJECT"));
        return;
    }

    if (! World->Scene)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUGridInstance::CreateGPUMeshSection() ABORTED, INVALID WORLD SCENE"));
        return;
    }

    if (! Section.IsValid())
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUGridInstance::CreateGPUMeshSection() ABORTED, INVALID SECTION"));
        return;
    }

    UPMUGridUtility::CreateGPUMeshSection(
        World->Scene->GetFeatureLevel(),
        GridData,
        *Section.SectionPtr,
        TextureInput.GetResource_GT(),
        HeightScale,
        BoundsHeightMin,
        BoundsHeightMax,
        bReverseWinding,
        CallbackEvent
        );
}

TArray<int32> UPMUGridInstance::FilterHeightMapPoints(const TArray<FVector2D>& Points, const int32 MapId, const float Threshold)
{
    TArray<int32> Indices;

    if (HasHeightMap(MapId))
    {
        const FPMUGridData::FHeightMap& HeightMap(GridData.GetHeightMapChecked(MapId));
        const FIntPoint Dimension(GridData.Dimension);

        for (int32 i=0; i<Points.Num(); ++i)
        {
            const int32 X = Points[i].X;
            const int32 Y = Points[i].Y;

            if (X >= 0 && X < Dimension.X && Y >= 0 && Y < Dimension.Y)
            {
                if (HeightMap[X + Y*Dimension.X] > Threshold)
                {
                    Indices.Emplace(i);
                }
            }
        }
    }

    return Indices;
}

void UPMUGridInstance::DrawPointMask(const TArray<FVector2D>& Points)
{
    if (Points.Num() < 3 || GridData.CalcSize() <= 0)
    {
        return;
    }

    const FIntPoint& Dim(GridData.Dimension);

    FAGGBufferG8 Buffer;
    FAGGRendererScanlineG8 Renderer;
    FAGGPathController Path;

    Buffer.Init(Dim.X, Dim.Y, 0, true);
    Renderer.Attach(Buffer.GetAGGBuffer());

    // Draw vector paths

    Path.Clear();

    Path.MoveTo(Points[0].X, Points[0].Y);
    for (int32 i=1; i<Points.Num(); ++i)
    {
        const FVector2D& V(Points[i]);
        Path.LineTo(V.X, V.Y);
    }
    Path.ClosePolygon();

    const FColor MaskColor(255,255,255,255);
    Renderer.Render(Path.GetAGGPath(), MaskColor, EAGGScanline::SL_Bin);

    // Copy buffer as point mask

    GridData.PointMask = Buffer.GetByteBuffer();

    check(GridData.HasValidPointMask());
}

UTexture2D* UPMUGridInstance::CreatePointMaskTexture(bool bFilterNearest)
{
    if (! GridData.HasValidPointMask())
    {
        return nullptr;
    }

    const FIntPoint Dim(GetDimension());
    const TArray<uint8>& PointMask(GridData.PointMask);

    // Generates transient texture
    UTexture2D*	Texture = UTexture2D::CreateTransient(Dim.X, Dim.Y, EPixelFormat::PF_G8);
    Texture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
    Texture->SRGB = 0;
    Texture->Filter = bFilterNearest ? TF_Nearest : TF_Bilinear;
    Texture->AddressX = TA_Clamp;
    Texture->AddressY = TA_Clamp;

    // Copy buffer data
    FTexture2DMipMap& Mip(Texture->PlatformData->Mips[0]);
    void* MipData = Mip.BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(MipData, PointMask.GetData(), PointMask.Num() * PointMask.GetTypeSize());
    Mip.BulkData.Unlock();

    // Update texture resource
    Texture->UpdateResource();

    return Texture;
}

void UPMUGridInstance::ExecuteTasks()
{
    for (UPMUGridGenerationTask* Task : GenerationTasks)
    {
        if (IsValid(Task))
        {
            // Only execute task if setup succeed
            if (Task->SetupTask(GridData))
            {
                Task->ExecuteTask(GridData);
            }
            // Otherwise, abort the rest of task execution
            else
            {
                break;
            }
        }
    }
}

bool UPMUGridInstance::ExecuteTasksAsync(FGWTAsyncTaskRef& TaskRef)
{
    if (! TaskRef.IsValid())
    {
        FGWTAsyncTaskRef::Init(
            TaskRef,
            IProceduralMeshUtility::Get().GetThreadPool());
    }

    check(TaskRef.IsValid());

    bool bInvalidTaskChain = false;

    for (UPMUGridGenerationTask* Task : GenerationTasks)
    {
        if (IsValid(Task))
        {
            // Only execute task if setup succeed
            if (Task->SetupTask(GridData))
            {
                TFunction<void()> TaskCallback(
                    [Task, this]
                    {
                        Task->ExecuteTask(GridData);
                    } );

                Task->AsyncSetupTask(GridData);

                if (Task->bAsyncSimultaneous)
                {
                    TaskRef.AddTask(MoveTemp(TaskCallback));
                }
                else
                {
                    TaskRef.AddTaskChain(MoveTemp(TaskCallback));
                }
            }
            // Otherwise, abort the rest of task execution
            else
            {
                bInvalidTaskChain = true;
                break;
            }
        }
    }

    return !bInvalidTaskChain;
}
