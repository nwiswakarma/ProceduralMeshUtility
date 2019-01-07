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

#pragma once

#include "CoreMinimal.h"
#include "PMUVoxel.h"
#include "PMUVoxelCell.h"
#include "PMUVoxelFeaturePoint.h"
#include "PMUVoxelRenderer.h"
#include "PMUVoxelSurface.h"
#include "March/PMUVoxelTypes.h"
#include "Mesh/PMUMeshTypes.h"

struct FPMUGridData;

class FPMUVoxelGrid
{
private:

    friend class FPMUVoxelMap;
    friend class FPMUVoxelStencil;

    FPMUVoxelCell cell;

    TArray<FPMUVoxelRenderer> renderers;
    TArray<FPMUVoxel> voxels;

    float gridSize;
    float voxelSize;
    int32 voxelResolution;

    FPMUVoxel dummyX;
    FPMUVoxel dummyY;
    FPMUVoxel dummyT;

    void CreateRenderers(const FPMUVoxelGridConfig& Config);
    void ResetVoxels();
    void Refresh();
    void Triangulate();
    void SetStates(const FPMUVoxelStencil& stencil, int32 xStart, int32 xEnd, int32 yStart, int32 yEnd);
    void SetCrossings(const FPMUVoxelStencil& stencil, int32 xStart, int32 xEnd, int32 yStart, int32 yEnd);

    void FillFirstRowCache();
    void CacheFirstCorner(const FPMUVoxel& voxel);
    void CacheNextEdgeAndCorner(int32 i, const FPMUVoxel& xMin, const FPMUVoxel& xMax);
    void CacheNextMiddleEdge(const FPMUVoxel& yMin, const FPMUVoxel& yMax);
    void SwapRowCaches();
    
    void TriangulateCellRows();
    void TriangulateGapRow();
    void TriangulateGapCell(int32 i);

    // Triangulation Functions

    void TriangulateCell(int32 i, const FPMUVoxel& a, const FPMUVoxel& b, const FPMUVoxel& c, const FPMUVoxel& d);

public:

    FVector2D position;

    FPMUVoxelGrid* xNeighbor = nullptr;
    FPMUVoxelGrid* yNeighbor = nullptr;
    FPMUVoxelGrid* xyNeighbor = nullptr;

    void Initialize(const FPMUVoxelGridConfig& Config);
    void CopyFrom(const FPMUVoxelGrid& Chunk);

    FORCEINLINE bool HasRenderer(int32 RendererIndex) const
    {
        return renderers.IsValidIndex(RendererIndex);
    }

    FORCEINLINE int32 GetVertexCount(int32 StateIndex) const
    {
        return HasRenderer(StateIndex)
            ? renderers[StateIndex].GetSurface().GetVertexCount()
            : 0;
    }

    FORCEINLINE const FPMUMeshSection* GetSection(int32 StateIndex) const
    {
        if (HasRenderer(StateIndex))
        {
            return &renderers[StateIndex].GetSurface().Section;
        }

        return nullptr;
    }

private:

    FORCEINLINE void CreateVoxel(int32 i, int32 x, int32 y)
    {
        voxels[i].Set(x, y, voxelSize);
    }
    
    FORCEINLINE void FillA(const FPMUVoxelFeaturePoint& f)
    {
        if (cell.a.IsFilled())
        {
            renderers[cell.a.state].FillA(cell, f);
        }
    }

    FORCEINLINE void FillB(const FPMUVoxelFeaturePoint& f)
    {
        if (cell.b.IsFilled())
        {
            renderers[cell.b.state].FillB(cell, f);
        }
    }
    
    FORCEINLINE void FillC(const FPMUVoxelFeaturePoint& f)
    {
        if (cell.c.IsFilled())
        {
            renderers[cell.c.state].FillC(cell, f);
        }
    }
    
    FORCEINLINE void FillD(const FPMUVoxelFeaturePoint& f)
    {
        if (cell.d.IsFilled())
        {
            renderers[cell.d.state].FillD(cell, f);
        }
    }

    FORCEINLINE void FillABC(const FPMUVoxelFeaturePoint& f)
    {
        if (cell.a.IsFilled())
        {
            renderers[cell.a.state].FillABC(cell, f);
        }
    }
    
    FORCEINLINE void FillABD(const FPMUVoxelFeaturePoint& f)
    {
        if (cell.a.IsFilled())
        {
            renderers[cell.a.state].FillABD(cell, f);
        }
    }
    
    FORCEINLINE void FillACD(const FPMUVoxelFeaturePoint& f)
    {
        if (cell.a.IsFilled())
        {
            renderers[cell.a.state].FillACD(cell, f);
        }
    }
    
    FORCEINLINE void FillBCD(const FPMUVoxelFeaturePoint& f)
    {
        if (cell.b.IsFilled())
        {
            renderers[cell.b.state].FillBCD(cell, f);
        }
    }

    FORCEINLINE void FillAB(const FPMUVoxelFeaturePoint& f)
    {
        if (cell.a.IsFilled())
        {
            renderers[cell.a.state].FillAB(cell, f);
        }
    }
    
    FORCEINLINE void FillAC(const FPMUVoxelFeaturePoint& f)
    {
        if (cell.a.IsFilled())
        {
            renderers[cell.a.state].FillAC(cell, f);
        }
    }
    
    FORCEINLINE void FillBD(const FPMUVoxelFeaturePoint& f)
    {
        if (cell.b.IsFilled())
        {
            renderers[cell.b.state].FillBD(cell, f);
        }
    }
    
    FORCEINLINE void FillCD(const FPMUVoxelFeaturePoint& f)
    {
        if (cell.c.IsFilled())
        {
            renderers[cell.c.state].FillCD(cell, f);
        }
    }

    FORCEINLINE void FillADToB(const FPMUVoxelFeaturePoint& f)
    {
        if (cell.a.IsFilled())
        {
            renderers[cell.a.state].FillADToB(cell, f);
        }
    }
    
    FORCEINLINE void FillADToC(const FPMUVoxelFeaturePoint& f)
    {
        if (cell.a.IsFilled())
        {
            renderers[cell.a.state].FillADToC(cell, f);
        }
    }
    
    FORCEINLINE void FillBCToA(const FPMUVoxelFeaturePoint& f)
    {
        if (cell.b.IsFilled())
        {
            renderers[cell.b.state].FillBCToA(cell, f);
        }
    }
    
    FORCEINLINE void FillBCToD(const FPMUVoxelFeaturePoint& f)
    {
        if (cell.b.IsFilled())
        {
            renderers[cell.b.state].FillBCToD(cell, f);
        }
    }

    FORCEINLINE void FillABCD()
    {
        if (cell.a.IsFilled())
        {
            renderers[cell.a.state].FillABCD(cell);
        }
    }

    FORCEINLINE void FillJoinedCorners(const FPMUVoxelFeaturePoint& fA, const FPMUVoxelFeaturePoint& fB, const FPMUVoxelFeaturePoint& fC, const FPMUVoxelFeaturePoint& fD)
    {
        
        FPMUVoxelFeaturePoint point = FPMUVoxelFeaturePoint::Average(fA, fB, fC, fD);
        if (!point.exists)
        {
            point.position = cell.GetAverageNESW();
            point.exists = true;
        }
        FillA(point);
        FillB(point);
        FillC(point);
        FillD(point);
    }

    // TRIANGULATION FUNCTIONS

    void Triangulate0000();
    void Triangulate0001();
    void Triangulate0010();
    void Triangulate0100();
    void Triangulate0111();
    void Triangulate0011();
    void Triangulate0101();
    void Triangulate0012();
    void Triangulate0102();
    void Triangulate0121();
    void Triangulate0122();
    void Triangulate0110();
    void Triangulate0112();
    void Triangulate0120();
    void Triangulate0123();
};
