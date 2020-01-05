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
#include "Engine/Polys.h"

class UModel;

class FPMUBSPOps
{

public:

    // Quality level for rebuilding BSP
    enum EBSPOptimization
    {
        BSP_Lame,
        BSP_Good,
        BSP_Optimal
    };

    // Possible positions of a child BSP node relative to its parent
    // (for BspAddToNode)
    enum ENodePlace 
    {
        NODE_Back       = 0, // Node is in back of parent              -> Bsp[iParent].iBack.
        NODE_Front      = 1, // Node is in front of parent             -> Bsp[iParent].iFront.
        NODE_Plane      = 2, // Node is coplanar with parent           -> Bsp[iParent].iPlane.
        NODE_Root       = 3, // Node is the Bsp root and has no parent -> Bsp[0].
    };

    // Status of filtered polygons
    enum EPolyNodeFilter
    {
        F_OUTSIDE               = 0, // Exterior leaf (visible to viewers)
        F_INSIDE                = 1, // Interior leaf (non-visible, hidden behind backface)
        F_COPLANAR_OUTSIDE      = 2, // Coplanar poly and in the exterior (visible to viewers)
        F_COPLANAR_INSIDE       = 3, // Coplanar poly and inside (invisible to viewers)
        F_COSPATIAL_FACING_IN   = 4, // Coplanar poly, cospatial, and facing in
        F_COSPATIAL_FACING_OUT  = 5, // Coplanar poly, cospatial, and facing out
    };

    // Information used by FilterEdPoly
    struct FCoplanarInfo
    {
        int32 iOriginalNode;
        int32 iBackNode;
        int32 BackNodeOutside;
        int32 FrontLeafOutside;
        int32 ProcessingBack;
    };

    // Generic filter function called by bspFilterFPoly
    //
    // A and B are pointers to any integers that your specific routine requires
    // (or NULL if not required)
    typedef void (*PMU_BSP_FILTER_FUNC)(
        UModel*         Model,
        int32           iNode,
        FPoly*          EdPoly,
        EPolyNodeFilter Leaf,
        ENodePlace      NodePlace
        );

    /**
     * Add a new vector to the model,
     * merging near-duplicates, and return its index
     */
    static int32 bspAddVector(UModel* Model, FVector* V, bool Exact);

    /**
     * Add a new point to the model,
     * merging near-duplicates and return its index
     */
    static int32 bspAddPoint(UModel* Model, FVector* V, bool Exact);

    static int32 bspAddNode(UModel* Model, int32 iParent, enum ENodePlace ENodePlace, uint32 NodeFlags, FPoly* EdPoly);

    static void bspRefresh(UModel* Model, bool NoRemapSurfs);
    static void bspValidateBrush( UModel* Brush, bool ForceValidate, bool DoStatusUpdate );
    static void bspUnlinkPolys( UModel* Brush );

    static void bspBuild(
        UModel* Model,
        EBSPOptimization Opt,
        int32 Balance,
        int32 PortalBias,
        int32 RebuildSimplePolys,
        int32 iNode
        );

    static void bspBuildBounds(UModel* Model);

    static int32 bspBrushCSG(
        UModel*    Model, 
        UModel*    Brush, 
        FTransform BrushTransform, 
        uint32     PolyFlags, 
        EBrushType BrushType,
        ECsgOper   CSGOper, 
        bool       bBuildBounds,
        bool       bMergePolys,
        bool       bReplaceNULLMaterialRefs,
        bool       bShowProgressBar = true
        );

    static void bspFilterFPoly(PMU_BSP_FILTER_FUNC FilterFunc, UModel *Model, FPoly *EdPoly);

    static void bspBuildFPolys(UModel* Model, bool SurfLinks, int32 iNode, TArray<FPoly>* DestArray = nullptr);

    static int32 bspNodeToFPoly(UModel* Model, int32 iNode, FPoly* EdPoly);

    static void bspCleanup(UModel *Model);

    static void TagReferencedNodes(
        UModel* Model,
        int32* NodeRef,
        int32* PolyRef,
        int32 iNode
        );

    static void FilterWorldThroughBrush(
        UModel*		Model,
        UModel*		Brush,
        EBrushType	BrushType,
        ECsgOper	CSGOper,
        int32		iNode,
        FSphere*	BrushSphere
        );

    // Handle a piece of a polygon that was filtered to a leaf
    static void FilterLeaf(
        PMU_BSP_FILTER_FUNC FilterFunc, 
        UModel*             Model,
        int32               iNode, 
        FPoly*              EdPoly, 
        FCoplanarInfo       CoplanarInfo, 
        int32               LeafOutside, 
        ENodePlace          ENodePlace
        );

    // Function to filter an EdPoly through the Bsp,
    // calling a callback function for all chunks that fall into leaves
    static void FilterEdPoly(
        PMU_BSP_FILTER_FUNC FilterFunc, 
        UModel*             Model,
        int32               iNode, 
        FPoly*              EdPoly, 
        FCoplanarInfo       CoplanarInfo, 
        int32               Outside
        );

    static void FilterBound(
        UModel* Model,
        FBox*   ParentBound,
        int32   iNode,
        FPoly** PolyList,
        int32   nPolys,
        int32   Outside
        );

    // Recursive worker function called by BspCleanup
    static void CleanupNodes(UModel *Model, int32 iNode, int32 iParent);

    static void UpdateBoundWithPolys(
        FBox&   Bound,
        FPoly** PolyList,
        int32   nPolys
        );

    static void UpdateConvolutionWithPolys(
        UModel* Model,
        int32   iNode,
        FPoly** PolyList,
        int32   nPolys
        );

    static void SplitPartitioner(
        UModel* Model,
        FPoly** PolyList,
        FPoly** FrontList,
        FPoly** BackList,
        int32   n,
        int32   nPolys,
        int32&  nFront, 
        int32&  nBack, 
        FPoly   InfiniteEdPoly,
        TArray<FPoly*>& AllocatedFPolys
        );

    static FPoly BuildInfiniteFPoly(UModel* Model, int32 iNode);

    static void MakeEdPolys(UModel* Model, int32 iNode, TArray<FPoly>* DestArray);

    // Rebuild brush internals
    static void RebuildBrush(UModel* Brush);

    // Errors encountered in Csg operation
    static int32 GErrors;
    static bool GFastRebuild;

    // Global variables shared between bspBrushCSG and AddWorldToBrushFunc
    //
    // These are very tightly tied into the function AddWorldToBrush,
    // not general-purpose
    static int32   GDiscarded;    // Number of polys discarded and not added
    static int32   GNode;         // Node AddBrushToWorld is adding to
    static int32   GLastCoplanar; // Last coplanar beneath GNode at start of AddWorldToBrush
    static int32   GNumNodes;     // Number of Bsp nodes at start of AddWorldToBrush
    static UModel* GModel;       // Level map Model we're adding to

private:

    static void AddBrushToWorldFunc(UModel* Model, int32 iNode, FPoly* EdPoly, EPolyNodeFilter Filter, ENodePlace ENodePlace);
    static void SubtractBrushFromWorldFunc(UModel* Model, int32 iNode, FPoly* EdPoly, EPolyNodeFilter Filter, ENodePlace ENodePlace);

    static void AddWorldToBrushFunc(
        UModel* Model,
        int32 iNode,
        FPoly* EdPoly,
        EPolyNodeFilter Filter,
        ENodePlace ENodePlace
        );

    static void SubtractWorldToBrushFunc(
        UModel* Model,
        int32 iNode,
        FPoly* EdPoly,
        EPolyNodeFilter Filter,
        ENodePlace ENodePlace
        );

protected:

    static FPoly* FindBestSplit(
        int32 NumPolys,
        FPoly** PolyList,
        EBSPOptimization Opt,
        int32 Balance,
        int32 InPortalBias
        );

    static void SplitPolyList(
        UModel*          Model,
        int32            iParent,
        ENodePlace       NodePlace,
        int32            NumPolys,
        FPoly**          PolyList,
        EBSPOptimization Opt,
        int32            Balance,
        int32            PortalBias,
        int32            RebuildSimplePolys
        );
};

struct FPMUBSPPointsKey
{
    int32 X;
    int32 Y;
    int32 Z;

    FPMUBSPPointsKey(int32 InX, int32 InY, int32 InZ)
        : X(InX)
        , Y(InY)
        , Z(InZ)
    {}

    friend FORCEINLINE bool operator==(const FPMUBSPPointsKey& A, const FPMUBSPPointsKey& B)
    {
        return A.X == B.X && A.Y == B.Y && A.Z == B.Z;
    }

    friend FORCEINLINE uint32 GetTypeHash(const FPMUBSPPointsKey& Key)
    {
        return HashCombine(static_cast<uint32>(Key.X), HashCombine(static_cast<uint32>(Key.Y), static_cast<uint32>(Key.Z)));
    }
};

struct FPMUBSPIndexedPoint
{
    FPMUBSPIndexedPoint(const FVector& InPoint, int32 InIndex)
        : Point(InPoint)
        , Index(InIndex)
    {}

    FVector Point;
    int32 Index;
};

struct FPMUBSPPointsGridItem
{
    TArray<FPMUBSPIndexedPoint, TInlineAllocator<16>> IndexedPoints;
};

// Represents a sparse granular 3D grid into
// which points are added for quick (~O(1)) lookup.
//
// The 3D space is divided into a grid with a given granularity.
// Points are considered to have a given radius (threshold)
// and are added to the grid cube they fall in,
// and to up to seven neighbours if they overlap.
class FPMUBSPPointsGrid
{
public:
    FPMUBSPPointsGrid(float InGranularity, float InThreshold, int32 InitialSize = 0)
        : OneOverGranularity(1.0f / InGranularity)
        , Threshold(InThreshold)
    {
        check(InThreshold / InGranularity <= 0.5f);
        Clear(InitialSize);
    }

    void Clear(int32 InitialSize = 0);
    int32 FindOrAddPoint(const FVector& Point, int32 Index, float Threshold);

    FORCEINLINE static int32 GetAdjacentIndexIfOverlapping(int32 GridIndex, float GridPos, float GridThreshold);

    static FPMUBSPPointsGrid* GBSPPoints;
    static FPMUBSPPointsGrid* GBSPVectors;

private:

    float OneOverGranularity;
    float Threshold;

    typedef TMap<FPMUBSPPointsKey, FPMUBSPPointsGridItem> FGridMap;
    FGridMap GridMap;
};

FORCEINLINE int32 FPMUBSPPointsGrid::GetAdjacentIndexIfOverlapping(int32 GridIndex, float GridPos, float GridThreshold)
{
    if (GridPos - GridIndex < GridThreshold)
    {
        return GridIndex - 1;
    }
    else if (1.0f - (GridPos - GridIndex) < GridThreshold)
    {
        return GridIndex + 1;
    }
    else
    {
        return GridIndex;
    }
}
