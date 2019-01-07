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
#include "PMUVoxelSurface.h"

class FPMUVoxelRenderer
{
private:

    friend class FPMUVoxelGrid;

	FPMUVoxelSurface surface;

public:

    FPMUVoxelRenderer() = default;

    FPMUVoxelRenderer(const FPMUVoxelSurfaceConfig& Config)
    {
        Initialize(Config);
    }

    void Initialize(const FPMUVoxelSurfaceConfig& Config)
    {
        surface.Initialize(Config);
    }

    void CopyFrom(const FPMUVoxelRenderer& Renderer)
    {
        surface.CopyFrom(Renderer.surface);
    }

	void Clear()
    {
		surface.Clear();
	}
	
	void Apply()
    {
		surface.Apply();
	}

    FORCEINLINE FPMUVoxelSurface& GetSurface()
    {
        return surface;
    }

    FORCEINLINE const FPMUVoxelSurface& GetSurface() const
    {
        return surface;
    }
	
	FORCEINLINE void PrepareCacheForNextCell()
    {
		surface.PrepareCacheForNextCell();
	}
	
	FORCEINLINE void PrepareCacheForNextRow()
    {
		surface.PrepareCacheForNextRow();
	}
	
	FORCEINLINE void CacheFirstCorner(const FPMUVoxel& voxel)
    {
		surface.CacheFirstCorner(voxel);
	}
	
	FORCEINLINE void CacheNextCorner(int32 i, const FPMUVoxel& voxel)
    {
		surface.CacheNextCorner(i, voxel);
	}
	
	FORCEINLINE void CacheXEdge(int32 i, const FPMUVoxel& voxel)
    {
		surface.CacheXEdge(i, voxel);
	}
	
	FORCEINLINE void CacheXEdgeWithWall(int32 i, const FPMUVoxel& voxel)
    {
		surface.CacheXEdge(i, voxel);
	}
	
	FORCEINLINE void CacheYEdge(const FPMUVoxel& voxel)
    {
		surface.CacheYEdge(voxel);
	}
	
	FORCEINLINE void CacheYEdgeWithWall(const FPMUVoxel& voxel)
    {
		surface.CacheYEdge(voxel);
	}

	void FillA(const FPMUVoxelCell& cell, const FPMUVoxelFeaturePoint& f)
    {
		if (f.exists)
        {
			surface.AddQuadA(cell.i, f.position, !cell.c.IsFilled(), !cell.b.IsFilled());
		}
        else
        {
			surface.AddTriangleA(cell.i, !cell.b.IsFilled());
		}
	}

	void FillB(const FPMUVoxelCell& cell, const FPMUVoxelFeaturePoint& f)
    {
		if (f.exists)
        {
			surface.AddQuadB(cell.i, f.position, !cell.a.IsFilled(), !cell.d.IsFilled());
		}
        else
        {
			surface.AddTriangleB(cell.i, !cell.a.IsFilled());
		}
	}
	
	void FillC(const FPMUVoxelCell& cell, const FPMUVoxelFeaturePoint& f)
    {
		if (f.exists)
        {
			surface.AddQuadC(cell.i, f.position, !cell.d.IsFilled(), !cell.a.IsFilled());
		}
        else
        {
			surface.AddTriangleC(cell.i, !cell.a.IsFilled());
		}
	}
	
	void FillD(const FPMUVoxelCell& cell, const FPMUVoxelFeaturePoint& f)
    {
		if (f.exists)
        {
			surface.AddQuadD(cell.i, f.position, !cell.b.IsFilled(), !cell.c.IsFilled());
		}
        else
        {
			surface.AddTriangleD(cell.i, !cell.b.IsFilled());
		}
	}

	void FillABC(const FPMUVoxelCell& cell, const FPMUVoxelFeaturePoint& f)
    {
		if (f.exists)
        {
			surface.AddHexagonABC(cell.i, f.position, !cell.d.IsFilled());
		}
        else
        {
			surface.AddPentagonABC(cell.i, !cell.d.IsFilled());
		}
	}
	
	void FillABD(const FPMUVoxelCell& cell, const FPMUVoxelFeaturePoint& f)
    {
		if (f.exists)
        {
			surface.AddHexagonABD(cell.i, f.position, !cell.c.IsFilled());
		}
        else
        {
			surface.AddPentagonABD(cell.i, !cell.c.IsFilled());
		}
	}
	
	void FillACD(const FPMUVoxelCell& cell, const FPMUVoxelFeaturePoint& f)
    {
		if (f.exists)
        {
			surface.AddHexagonACD(cell.i, f.position, !cell.b.IsFilled());
		}
        else
        {
			surface.AddPentagonACD(cell.i, !cell.b.IsFilled());
		}
	}
	
	void FillBCD(const FPMUVoxelCell& cell, const FPMUVoxelFeaturePoint& f)
    {
		if (f.exists)
        {
			surface.AddHexagonBCD(cell.i, f.position, !cell.a.IsFilled());
		}
        else
        {
			surface.AddPentagonBCD(cell.i, !cell.a.IsFilled());
		}
	}

	void FillAB(const FPMUVoxelCell& cell, const FPMUVoxelFeaturePoint& f)
    {
		if (f.exists)
        {
			surface.AddPentagonAB(cell.i, f.position, !cell.c.IsFilled(), !cell.d.IsFilled());
		}
        else
        {
			surface.AddQuadAB(cell.i, !cell.c.IsFilled());
		}
	}
	
	void FillAC(const FPMUVoxelCell& cell, const FPMUVoxelFeaturePoint& f)
    {
		if (f.exists)
        {
			surface.AddPentagonAC(cell.i, f.position, !cell.d.IsFilled(), !cell.b.IsFilled());
		}
        else
        {
			surface.AddQuadAC(cell.i, !cell.b.IsFilled());
		}
	}
	
	void FillBD(const FPMUVoxelCell& cell, const FPMUVoxelFeaturePoint& f)
    {
		if (f.exists)
        {
			surface.AddPentagonBD(cell.i, f.position, !cell.a.IsFilled(), !cell.c.IsFilled());
		}
        else
        {
			surface.AddQuadBD(cell.i, !cell.a.IsFilled());
		}
	}
	
	void FillCD(const FPMUVoxelCell& cell, const FPMUVoxelFeaturePoint& f)
    {
		if (f.exists)
        {
			surface.AddPentagonCD(cell.i, f.position, !cell.b.IsFilled(), !cell.a.IsFilled());
		}
        else
        {
			surface.AddQuadCD(cell.i, !cell.a.IsFilled());
		}
	}

	void FillADToB(const FPMUVoxelCell& cell, const FPMUVoxelFeaturePoint& f)
    {
		if (f.exists)
        {
			surface.AddPentagonADToB(cell.i, f.position, !cell.b.IsFilled());
		}
        else
        {
			surface.AddQuadADToB(cell.i, !cell.b.IsFilled());
		}
	}
	
	void FillADToC(const FPMUVoxelCell& cell, const FPMUVoxelFeaturePoint& f)
    {
		if (f.exists)
        {
			surface.AddPentagonADToC(cell.i, f.position, !cell.c.IsFilled());
		}
        else
        {
			surface.AddQuadADToC(cell.i, !cell.c.IsFilled());
		}
	}
	
	void FillBCToA(const FPMUVoxelCell& cell, const FPMUVoxelFeaturePoint& f)
    {
		if (f.exists)
        {
			surface.AddPentagonBCToA(cell.i, f.position, !cell.a.IsFilled());
		}
        else
        {
			surface.AddQuadBCToA(cell.i, !cell.a.IsFilled());
		}
	}
	
	void FillBCToD(const FPMUVoxelCell& cell, const FPMUVoxelFeaturePoint& f)
    {
		if (f.exists)
        {
			surface.AddPentagonBCToD(cell.i, f.position, !cell.d.IsFilled());
		}
        else
        {
			surface.AddQuadBCToD(cell.i, !cell.d.IsFilled());
		}
	}

	void FillABCD(const FPMUVoxelCell& cell)
    {
		surface.AddQuadABCD(cell.i);
	}
};
