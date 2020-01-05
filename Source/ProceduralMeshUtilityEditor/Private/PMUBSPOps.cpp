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

#include "PMUBSPOps.h"

int32 FPMUBSPOps::GErrors = 0;
bool FPMUBSPOps::GFastRebuild = false;

int32   FPMUBSPOps::GDiscarded;
int32   FPMUBSPOps::GNode;
int32   FPMUBSPOps::GLastCoplanar;
int32   FPMUBSPOps::GNumNodes;
UModel* FPMUBSPOps::GModel;

FPMUBSPPointsGrid* FPMUBSPPointsGrid::GBSPPoints = nullptr;
FPMUBSPPointsGrid* FPMUBSPPointsGrid::GBSPVectors = nullptr;

void FPMUBSPOps::TagReferencedNodes(
    UModel* Model,
    int32* NodeRef,
    int32* PolyRef,
    int32 iNode
    )
{
    FBspNode &Node = Model->Nodes[iNode];

    NodeRef[iNode     ] = 0;
    PolyRef[Node.iSurf] = 0;

    if (Node.iFront != INDEX_NONE) TagReferencedNodes(Model, NodeRef, PolyRef, Node.iFront);
    if (Node.iBack  != INDEX_NONE) TagReferencedNodes(Model, NodeRef, PolyRef, Node.iBack );
    if (Node.iPlane != INDEX_NONE) TagReferencedNodes(Model, NodeRef, PolyRef, Node.iPlane);
}

void FPMUBSPOps::AddBrushToWorldFunc(UModel* Model, int32 iNode, FPoly* EdPoly, EPolyNodeFilter Filter, ENodePlace ENodePlace)
{
    switch (Filter)
    {
        case F_OUTSIDE:
        case F_COPLANAR_OUTSIDE:
            FPMUBSPOps::bspAddNode(Model, iNode, ENodePlace, NF_IsNew, EdPoly);
            break;
        case F_COSPATIAL_FACING_OUT:
            if (! (EdPoly->PolyFlags & PF_Semisolid))
                FPMUBSPOps::bspAddNode(Model, iNode, ENodePlace, NF_IsNew, EdPoly);
            break;
        case F_INSIDE:
        case F_COPLANAR_INSIDE:
        case F_COSPATIAL_FACING_IN:
            break;
    }
}

void FPMUBSPOps::SubtractBrushFromWorldFunc(UModel* Model, int32 iNode, FPoly* EdPoly, EPolyNodeFilter Filter, ENodePlace ENodePlace)
{
    switch (Filter)
    {
        case F_OUTSIDE:
        case F_COSPATIAL_FACING_OUT:
        case F_COSPATIAL_FACING_IN:
        case F_COPLANAR_OUTSIDE:
            break;
        case F_COPLANAR_INSIDE:
        case F_INSIDE:
            EdPoly->Reverse();
            // Add to Bsp back
            FPMUBSPOps::bspAddNode(Model, iNode, ENodePlace, NF_IsNew, EdPoly);
            EdPoly->Reverse();
            break;
    }
}

void FPMUBSPOps::AddWorldToBrushFunc(
    UModel* Model,
    int32 iNode,
    FPoly* EdPoly,
	EPolyNodeFilter Filter,
    ENodePlace ENodePlace
    )
{
	switch (Filter)
	{
		case F_OUTSIDE:
		case F_COPLANAR_OUTSIDE:
			// Only affect the world poly if it has been cut
			if (EdPoly->PolyFlags & PF_EdCut)
				FPMUBSPOps::bspAddNode(GModel, GLastCoplanar, NODE_Plane, NF_IsNew, EdPoly);
			break;
		case F_INSIDE:
		case F_COPLANAR_INSIDE:
		case F_COSPATIAL_FACING_IN:
		case F_COSPATIAL_FACING_OUT:
			// Discard original poly
			GDiscarded++;
			if (GModel->Nodes[GNode].NumVertices)
			{
				GModel->Nodes[GNode].NumVertices = 0;
			}
			break;
	}
}

void FPMUBSPOps::SubtractWorldToBrushFunc(
    UModel* Model,
    int32 iNode,
    FPoly* EdPoly,
    EPolyNodeFilter Filter,
    ENodePlace ENodePlace
    )
{
	switch (Filter)
	{
		case F_OUTSIDE:
		case F_COPLANAR_OUTSIDE:
		case F_COSPATIAL_FACING_IN:
			// Only affect the world poly if it has been cut
			if (EdPoly->PolyFlags & PF_EdCut)
				FPMUBSPOps::bspAddNode(GModel, GLastCoplanar, NODE_Plane, NF_IsNew, EdPoly);
			break;
		case F_INSIDE:
		case F_COPLANAR_INSIDE:
		case F_COSPATIAL_FACING_OUT:
			// Discard original poly
			GDiscarded++;
			if (GModel->Nodes[GNode].NumVertices)
			{
				GModel->Nodes[GNode].NumVertices = 0;
			}
			break;
	}
}

void FPMUBSPOps::FilterWorldThroughBrush(
    UModel*     Model,
    UModel*     Brush,
    EBrushType  BrushType,
    ECsgOper    CSGOper,
    int32       iNode,
    FSphere*    BrushSphere
    )
{
    // Loop through all coplanars
    while (iNode != INDEX_NONE)
    {
        // Get surface
        int32 iSurf = Model->Nodes[iNode].iSurf;

        // Skip new nodes and their children, which are guaranteed new
        if (Model->Nodes[iNode].NodeFlags & NF_IsNew)
        {
            return;
        }

        // Sphere reject
        int32 DoFront = 1, DoBack = 1;
        if (BrushSphere)
        {
            float Dist = Model->Nodes[iNode].Plane.PlaneDot(BrushSphere->Center);
            DoFront    = (Dist >= -BrushSphere->W);
            DoBack     = (Dist <= +BrushSphere->W);
        }

        // Process only polys that aren't empty
        FPoly TempEdPoly;
        //if (DoFront && DoBack && (GEditor->bspNodeToFPoly(Model,iNode,&TempEdPoly)>0))
        if (DoFront && DoBack && (bspNodeToFPoly(Model,iNode,&TempEdPoly) > 0))
        {
            TempEdPoly.Actor      = Model->Surfs[iSurf].Actor;
            TempEdPoly.iBrushPoly = Model->Surfs[iSurf].iBrushPoly;

            if ((BrushType == Brush_Add) || (BrushType == Brush_Subtract))
            {
                // Add and subtract work the same in this step
                GNode       = iNode;
                GModel      = Model;
                GDiscarded  = 0;
                GNumNodes   = Model->Nodes.Num();

                // Find last coplanar in chain
                GLastCoplanar = iNode;
                while (Model->Nodes[GLastCoplanar].iPlane != INDEX_NONE)
                    GLastCoplanar = Model->Nodes[GLastCoplanar].iPlane;

                // Do the filter operation
                bspFilterFPoly(
                    BrushType==Brush_Add
                        ? AddWorldToBrushFunc
                        : SubtractWorldToBrushFunc,
                    Brush,
                    &TempEdPoly
                    );
                
                if (GDiscarded == 0)
                {
                    // Get rid of all the fragments we added.
                    Model->Nodes[GLastCoplanar].iPlane = INDEX_NONE;
                    const bool bAllowShrinking = false;
                    Model->Nodes.RemoveAt(GNumNodes, Model->Nodes.Num()-GNumNodes, bAllowShrinking);
                }
                else
                {
                    // Tag original world poly for deletion;
                    // has been deleted or replaced by partial fragments
                    if (GModel->Nodes[GNode].NumVertices)
                    {
                        GModel->Nodes[GNode].NumVertices = 0;
                    }
                }
            }
            else
            if (CSGOper == CSG_Intersect)
            {
#if 0
                bspFilterFPoly(IntersectWorldWithBrushFunc, Brush, &TempEdPoly);
#else
                check(0);
#endif
            }
            else
            if (CSGOper == CSG_Deintersect)
            {
#if 0
                bspFilterFPoly(DeIntersectWorldWithBrushFunc, Brush, &TempEdPoly);
#else
                check(0);
#endif
            }
        }

        // Now recurse to filter all of the world's children nodes

        if (DoFront && (Model->Nodes[iNode].iFront != INDEX_NONE))
        {
            FilterWorldThroughBrush(
                Model,
                Brush,
                BrushType,
                CSGOper,
                Model->Nodes[iNode].iFront,
                BrushSphere
                );
        }

        if (DoBack && (Model->Nodes[iNode].iBack != INDEX_NONE))
        {
            FilterWorldThroughBrush(
                Model,
                Brush,
                BrushType,
                CSGOper,
                Model->Nodes[iNode].iBack,
                BrushSphere
                );
        }

        iNode = Model->Nodes[iNode].iPlane;
    }
}

void FPMUBSPOps::FilterLeaf(
    PMU_BSP_FILTER_FUNC FilterFunc, 
    UModel*             Model,
    int32               iNode, 
    FPoly*              EdPoly, 
    FCoplanarInfo       CoplanarInfo, 
    int32               LeafOutside, 
    ENodePlace          ENodePlace
    )
{
    EPolyNodeFilter FilterType;

    if (CoplanarInfo.iOriginalNode == INDEX_NONE)
    {
        // Processing regular, non-coplanar polygons.
        FilterType = LeafOutside ? F_OUTSIDE : F_INSIDE;
        FilterFunc( Model, iNode, EdPoly, FilterType, ENodePlace );
    }
    else
    if (CoplanarInfo.ProcessingBack)
    {
    DoneFilteringBack:

        // Finished filtering polygon through tree in back of parent coplanar.
        if      ((!LeafOutside) && (!CoplanarInfo.FrontLeafOutside)) FilterType = F_COPLANAR_INSIDE;
        else if (( LeafOutside) && ( CoplanarInfo.FrontLeafOutside)) FilterType = F_COPLANAR_OUTSIDE;
        else if ((!LeafOutside) && ( CoplanarInfo.FrontLeafOutside)) FilterType = F_COSPATIAL_FACING_OUT;
        else if (( LeafOutside) && (!CoplanarInfo.FrontLeafOutside)) FilterType = F_COSPATIAL_FACING_IN;
        else
        {
            UE_LOG(LogTemp, Fatal, TEXT("FilterLeaf: Bad Locs"));
            return;
        }

        FilterFunc(
            Model,
            CoplanarInfo.iOriginalNode,
            EdPoly,
            FilterType,
            FPMUBSPOps::NODE_Plane
            );
    }
    else
    {
        CoplanarInfo.FrontLeafOutside = LeafOutside;

        if (CoplanarInfo.iBackNode == INDEX_NONE)
        {
            // Back tree is empty.
            LeafOutside = CoplanarInfo.BackNodeOutside;
            goto DoneFilteringBack;
        }
        else
        {
            // Call FilterEdPoly to filter through the back.  This will result in
            // another call to FilterLeaf with iNode = leaf this falls into in the
            // back tree and EdPoly = the final EdPoly to insert.
            CoplanarInfo.ProcessingBack=1;
            FilterEdPoly(
                FilterFunc,
                Model,
                CoplanarInfo.iBackNode,
                EdPoly,CoplanarInfo,
                CoplanarInfo.BackNodeOutside
                );
        }
    }
}

// Filter an EdPoly through the Bsp recursively, calling FilterFunc
// for all chunks that fall into leaves. FCoplanarInfo is used to
// handle the tricky case of double-recursion for polys that must be
// filtered through a node's front, then filtered through the node's back,
// in order to handle coplanar CSG properly.
void FPMUBSPOps::FilterEdPoly(
    PMU_BSP_FILTER_FUNC FilterFunc, 
    UModel*             Model,
    int32               iNode, 
    FPoly*              EdPoly, 
    FCoplanarInfo       CoplanarInfo, 
    int32               Outside
    )
{
    int32 SplitResult,iOurFront,iOurBack;
    int32 NewFrontOutside,NewBackOutside;

FilterLoop:

    // Split poly with node plane
    FPoly TempFrontEdPoly,TempBackEdPoly;
    SplitResult = EdPoly->SplitWithPlane(
        Model->Points [Model->Verts[Model->Nodes[iNode].iVertPool].pVertex],
        Model->Vectors[Model->Surfs[Model->Nodes[iNode].iSurf].vNormal],
        &TempFrontEdPoly,
        &TempBackEdPoly,
        0
        );

    // Process split results

    if (SplitResult == SP_Front)
    {
    Front:

        FBspNode *Node = &Model->Nodes[iNode];
        Outside        = Outside || Node->IsCsg();

        if (Node->iFront == INDEX_NONE)
        {
            FilterLeaf(
                FilterFunc,
                Model,
                iNode,
                EdPoly,
                CoplanarInfo,
                Outside,
                FPMUBSPOps::NODE_Front
                );
        }
        else
        {
            iNode = Node->iFront;
            goto FilterLoop;
        }
    }
    else
    if (SplitResult == SP_Back)
    {
        FBspNode *Node = &Model->Nodes[iNode];
        Outside        = Outside && !Node->IsCsg();

        if (Node->iBack == INDEX_NONE)
        {
            FilterLeaf(
                FilterFunc,
                Model,
                iNode,
                EdPoly,
                CoplanarInfo,
                Outside,
                FPMUBSPOps::NODE_Back
                );
        }
        else
        {
            iNode = Node->iBack;
            goto FilterLoop;
        }
    }
    else
    if (SplitResult == SP_Coplanar)
    {
        if (CoplanarInfo.iOriginalNode != INDEX_NONE)
        {
            // Will happen when a polygon is barely outside the
            // coplanar threshold and is split up into a new polygon that is
            // is barely inside the coplanar threshold.
            //
            // To handle this, classify it as front
            // and it will be handled properly
            GErrors++;

            //UE_LOG(LogEditorBsp, Warning, TEXT("FilterEdPoly: Encountered out-of-place coplanar") );

            goto Front;
        }

        CoplanarInfo.iOriginalNode        = iNode;
        CoplanarInfo.iBackNode            = INDEX_NONE;
        CoplanarInfo.ProcessingBack       = 0;
        CoplanarInfo.BackNodeOutside      = Outside;
        NewFrontOutside                   = Outside;

        // See whether Node's iFront or iBack points
        // to the side of the tree on the front of this polygon
        //
        // (will be as expected if this polygon is facing the same
        // way as first coplanar in link, otherwise opposite)
        if ((FVector(Model->Nodes[iNode].Plane) | EdPoly->Normal) >= 0.f)
        {
            iOurFront = Model->Nodes[iNode].iFront;
            iOurBack  = Model->Nodes[iNode].iBack;
            
            if (Model->Nodes[iNode].IsCsg())
            {
                CoplanarInfo.BackNodeOutside = 0;
                NewFrontOutside              = 1;
            }
        }
        else
        {
            iOurFront = Model->Nodes[iNode].iBack;
            iOurBack  = Model->Nodes[iNode].iFront;
            
            if (Model->Nodes[iNode].IsCsg())
            {
                CoplanarInfo.BackNodeOutside = 1; 
                NewFrontOutside              = 0;
            }
        }

        // Process front and back
        if ((iOurFront == INDEX_NONE) && (iOurBack == INDEX_NONE))
        {
            // No front or back.
            CoplanarInfo.ProcessingBack     = 1;
            CoplanarInfo.FrontLeafOutside   = NewFrontOutside;
            FilterLeaf(
                FilterFunc,
                Model,
                iNode,
                EdPoly,
                CoplanarInfo,
                CoplanarInfo.BackNodeOutside,
                FPMUBSPOps::NODE_Plane
                );
        }
        else
        if ((iOurFront == INDEX_NONE) && (iOurBack != INDEX_NONE))
        {
            // Back but no front
            CoplanarInfo.ProcessingBack     = 1;
            CoplanarInfo.iBackNode          = iOurBack;
            CoplanarInfo.FrontLeafOutside   = NewFrontOutside;

            iNode   = iOurBack;
            Outside = CoplanarInfo.BackNodeOutside;
            goto FilterLoop;
        }
        else
        {
            // Has a front and maybe a back
            //
            // Set iOurBack up to process back on next call to FilterLeaf,
            // and loop to process front
            //
            // Next call to FilterLeaf will set FrontLeafOutside
            CoplanarInfo.ProcessingBack = 0;

            // May be a node or may be INDEX_NONE
            CoplanarInfo.iBackNode = iOurBack;

            iNode   = iOurFront;
            Outside = NewFrontOutside;
            goto FilterLoop;
        }
    }
    else
    if (SplitResult == SP_Split)
    {
        // Front half of split
        if (Model->Nodes[iNode].IsCsg())
        {
            NewFrontOutside = 1; 
            NewBackOutside  = 0;
        }
        else
        {
            NewFrontOutside = Outside;
            NewBackOutside  = Outside;
        }

        if (Model->Nodes[iNode].iFront == INDEX_NONE)
        {
            FilterLeaf(
                FilterFunc,
                Model,
                iNode,
                &TempFrontEdPoly,
                CoplanarInfo,
                NewFrontOutside,
                FPMUBSPOps::NODE_Front
                );
        }
        else
        {
            FilterEdPoly(
                FilterFunc,
                Model,
                Model->Nodes[iNode].iFront,
                &TempFrontEdPoly,
                CoplanarInfo,
                NewFrontOutside
                );
        }

        // Back half of split
        if (Model->Nodes[iNode].iBack == INDEX_NONE)
        {
            FilterLeaf(
                FilterFunc,
                Model,
                iNode,
                &TempBackEdPoly,
                CoplanarInfo,
                NewBackOutside,
                FPMUBSPOps::NODE_Back
                );
        }
        else
        {
            FilterEdPoly(
                FilterFunc,
                Model,
                Model->Nodes[iNode].iBack,
                &TempBackEdPoly,
                CoplanarInfo,
                NewBackOutside
                );
        }
    }
}

// Update a bounding volume by expanding it to enclose a list of polys
void FPMUBSPOps::UpdateBoundWithPolys(
    FBox&   Bound,
    FPoly** PolyList,
    int32   nPolys
    )
{
    for (int32 i=0; i<nPolys; ++i)
        for (int32 j=0; j<PolyList[i]->Vertices.Num(); ++j)
            Bound += PolyList[i]->Vertices[j];
}

// Update a convolution hull with a list of polys
void FPMUBSPOps::UpdateConvolutionWithPolys(
    UModel* Model,
    int32   iNode,
    FPoly** PolyList,
    int32   nPolys
    )
{
    FBox Box(ForceInit);

    FBspNode &Node = Model->Nodes[iNode];
    Node.iCollisionBound = Model->LeafHulls.Num();

    for (int32 i=0; i<nPolys; ++i)
    {
        if (PolyList[i]->iBrushPoly != INDEX_NONE)
        {
            int32 j;
            for (j=0; j<i; j++)
                if (PolyList[j]->iBrushPoly == PolyList[i]->iBrushPoly)
                    break;

            if (j >= i)
            {
                Model->LeafHulls.Add(PolyList[i]->iBrushPoly);
            }
        }

        for (int32 j=0; j<PolyList[i]->Vertices.Num(); ++j)
        {
            Box += PolyList[i]->Vertices[j];
        }
    }

    Model->LeafHulls.Add(INDEX_NONE);

    // Add bounds
    Model->LeafHulls.Add(*(int32*)&Box.Min.X);
    Model->LeafHulls.Add(*(int32*)&Box.Min.Y);
    Model->LeafHulls.Add(*(int32*)&Box.Min.Z);
    Model->LeafHulls.Add(*(int32*)&Box.Max.X);
    Model->LeafHulls.Add(*(int32*)&Box.Max.Y);
    Model->LeafHulls.Add(*(int32*)&Box.Max.Z);
}

// Find the best splitting polygon within a pool of polygons,
// and return its index (into the PolyList array)
FPoly* FPMUBSPOps::FindBestSplit(
    int32 NumPolys,
    FPoly** PolyList,
    EBSPOptimization Opt,
    int32 Balance,
    int32 InPortalBias
    )
{
    check(NumPolys > 0);

    // No need to test if only one poly
    if (NumPolys == 1)
    {
        return PolyList[0];
    }

    FPoly *Poly, *Best = nullptr;
    float Score, BestScore;
    int32 i, Index, j, Inc;
    int32 Splits, Front, Back, Coplanar, AllSemiSolids;

    //PortalBias -- added by Legend on 4/12/2000
    float   PortalBias = InPortalBias / 100.0f;
    Balance &= 0xFF;                                // keep only the low byte to recover "Balance"
    //UE_LOG(LogBSPOps, Log, TEXT("Balance=%d PortalBias=%f"), Balance, PortalBias );

    if (Opt==FPMUBSPOps::BSP_Optimal)  Inc = 1;                   // Test lots of nodes.
    else
    if (Opt==FPMUBSPOps::BSP_Good)     Inc = FMath::Max(1,NumPolys/20);    // Test 20 nodes.
    else /* BSP_Lame */                Inc = FMath::Max(1,NumPolys/4); // Test 4 nodes.

    // See if there are any non-semisolid polygons here.
    for( i=0; i<NumPolys; i++ )
        if( !(PolyList[i]->PolyFlags & PF_AddLast) )
            break;
    AllSemiSolids = (i>=NumPolys);

    // Search through all polygons in the pool and find:
    // A. The number of splits each poly would make.
    // B. The number of front and back nodes the polygon would create.
    // C. Number of coplanars.
    BestScore = 0;
    for( i=0; i<NumPolys; i+=Inc )
    {
        Splits = Front = Back = Coplanar = 0;
        Index = i-1;
        do
        {
            Index++;
            Poly = PolyList[Index];
        } while( Index<(i+Inc) && Index<NumPolys 
            && ( (Poly->PolyFlags & PF_AddLast) && !(Poly->PolyFlags & PF_Portal) )
            && !AllSemiSolids );
        if( Index>=i+Inc || Index>=NumPolys )
            continue;

        for( j=0; j<NumPolys; j+=Inc ) if( j != Index )
        {
            FPoly *OtherPoly = PolyList[j];
            switch( OtherPoly->SplitWithPlaneFast( FPlane( Poly->Vertices[0], Poly->Normal), NULL, NULL ) )
            {
                case SP_Coplanar:
                    Coplanar++;
                    break;

                case SP_Front:
                    Front++;
                    break;

                case SP_Back:
                    Back++;
                    break;

                case SP_Split:
                    // Disfavor splitting polys that are zone portals.
                    if( !(OtherPoly->PolyFlags & PF_Portal) )
                        Splits++;
                    else
                        Splits += 16;
                    break;
            }
        }
        // added by Legend 1/31/1999
        // Score optimization: minimize cuts vs. balance tree (as specified in BSP Rebuilder dialog)
        Score = ( 100.0 - float(Balance) ) * Splits + float(Balance) * FMath::Abs( Front - Back );
        if( Poly->PolyFlags & PF_Portal )
        {
            // PortalBias -- added by Legend on 4/12/2000
            //
            // PortalBias enables level designers to control the effect of Portals on the BSP.
            // This effect can range from 0.0 (ignore portals), to 1.0 (portals cut everything).
            //
            // In builds prior to this (since the 221 build dating back to 1/31/1999) the bias
            // has been 1.0 causing the portals to cut the BSP in ways that will potentially
            // degrade level performance, and increase the BSP complexity.
            // 
            // By setting the bias to a value between 0.3 and 0.7 the positive effects of 
            // the portals are preserved without giving them unreasonable priority in the BSP.
            //
            // Portals should be weighted high enough in the BSP to separate major parts of the
            // level from each other (pushing entire rooms down the branches of the BSP), but
            // should not be so high that portals cut through adjacent geometry in a way that
            // increases complexity of the room being (typically, accidentally) cut.
            //
            Score -= ( 100.0 - float(Balance) ) * Splits * PortalBias; // ignore PortalBias of the split polys -- bias toward portal selection for cutting planes!
        }
        //UE_LOG(LogBSPOps, Log,  "  %4d: Score = %f (Front = %4d, Back = %4d, Splits = %4d, Flags = %08X)", Index, Score, Front, Back, Splits, Poly->PolyFlags ); //LEC

        if( Score<BestScore || !Best )
        {
            Best      = Poly;
            BestScore = Score;
        }
    }
    check(Best);
    return Best;
}

// Pick a splitter poly then split a pool of polygons
// into front and back polygons and recurse
//
// iParent = Parent Bsp node, or INDEX_NONE if this is the root node
//
// IsFront = 1 if this is the front node of iParent,
// 0 of back (undefined if iParent==INDEX_NONE)
void FPMUBSPOps::SplitPolyList(
    UModel*          Model,
    int32            iParent,
    ENodePlace       NodePlace,
    int32            NumPolys,
    FPoly**          PolyList,
    EBSPOptimization Opt,
    int32            Balance,
    int32            PortalBias,
    int32            RebuildSimplePolys
    )
{
    FMemMark Mark(FMemStack::Get());

    // Keeping track of allocated FPoly structures to delete later on
    TArray<FPoly*> AllocatedFPolys;

    // To account for big EdPolys split up
    int32 NumPolysToAlloc = NumPolys + 8 + NumPolys/4;
    int32 NumFront=0; FPoly **FrontList = new(FMemStack::Get(),NumPolysToAlloc)FPoly*;
    int32 NumBack =0; FPoly **BackList  = new(FMemStack::Get(),NumPolysToAlloc)FPoly*;

    FPoly* SplitPoly = FindBestSplit(NumPolys, PolyList, Opt, Balance, PortalBias);

    // Add the splitter poly to the Bsp with either
    // a new BspSurf or an existing one
    if (RebuildSimplePolys)
    {
        SplitPoly->iLinkSurf = Model->Surfs.Num();
    }

    int32 iOurNode = bspAddNode(Model, iParent, NodePlace, 0, SplitPoly);
    int32 iPlaneNode = iOurNode;

    // Now divide all polygons in the pool into (A) polygons that are
    // in front of Poly, and (B) polygons that are in back of Poly
    //
    // Coplanar polys are inserted immediately, before recursing

    // If any polygons are split by Poly, we ignrore the original poly,
    // split it into two polys, and add two new polys to the pool
    FPoly* FrontEdPoly = new FPoly;
    FPoly* BackEdPoly  = new FPoly;
    // Keep track of allocations
    AllocatedFPolys.Add(FrontEdPoly);
    AllocatedFPolys.Add(BackEdPoly);

    for (int32 i=0; i<NumPolys; ++i)
    {
        FPoly *EdPoly = PolyList[i];

        if (EdPoly == SplitPoly)
        {
            continue;
        }

        switch (EdPoly->SplitWithPlane(SplitPoly->Vertices[0], SplitPoly->Normal, FrontEdPoly, BackEdPoly, 0))
        {
            case SP_Coplanar:
                if (RebuildSimplePolys)
                {
                    EdPoly->iLinkSurf = Model->Surfs.Num()-1;
                }
                iPlaneNode = bspAddNode(Model, iPlaneNode, NODE_Plane, 0, EdPoly);
                break;
            
            case SP_Front:
                FrontList[NumFront++] = PolyList[i];
                break;
            
            case SP_Back:
                BackList[NumBack++] = PolyList[i];
                break;
            
            case SP_Split:

                // Create front & back nodes
                FrontList[NumFront++] = FrontEdPoly;
                BackList [NumBack ++] = BackEdPoly;

                FrontEdPoly = new FPoly;
                BackEdPoly  = new FPoly;

                // Keep track of allocations
                AllocatedFPolys.Add(FrontEdPoly);
                AllocatedFPolys.Add(BackEdPoly);

                break;
        }
    }

    // Recursively split the front and back pools
    if (NumFront > 0) SplitPolyList(Model, iOurNode, NODE_Front, NumFront, FrontList, Opt, Balance, PortalBias, RebuildSimplePolys);
    if (NumBack  > 0) SplitPolyList(Model, iOurNode, NODE_Back,  NumBack,  BackList,  Opt, Balance, PortalBias, RebuildSimplePolys);

    // Delete FPolys allocated above
    //
    // We cannot use FMemStack::Get() for FPoly as the array data
    // FPoly contains will be allocated in regular memory
    for (int32 i=0; i<AllocatedFPolys.Num(); ++i)
    {
        FPoly* AllocatedFPoly = AllocatedFPolys[i];
        delete AllocatedFPoly;
    }

    Mark.Pop();
}

// Cut a partitioning poly by a list of polys,
// and add the resulting inside pieces to the
// front list and back list
void FPMUBSPOps::SplitPartitioner(
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
    )
{
    FPoly FrontPoly,BackPoly;
    while (n < nPolys)
    {
        FPoly* Poly = PolyList[n];

        switch (InfiniteEdPoly.SplitWithPlane(Poly->Vertices[0],Poly->Normal,&FrontPoly,&BackPoly,0))
        {
            case SP_Coplanar:
                // May occasionally happen.
//              UE_LOG(LogBSPOps, Log,  TEXT("FilterBound: Got inficoplanar") );
                break;

            case SP_Front:
                // Shouldn't happen if hull is correct.
//              UE_LOG(LogBSPOps, Log,  TEXT("FilterBound: Got infifront") );
                return;

            case SP_Split:
                InfiniteEdPoly = BackPoly;
                break;

            case SP_Back:
                break;
        }
        n++;
    }

    FPoly* New = new FPoly;
    *New = InfiniteEdPoly;
    New->Reverse();
    New->iBrushPoly |= 0x40000000;
    FrontList[nFront++] = New;
    AllocatedFPolys.Add(New);
    
    New = new FPoly;
    *New = InfiniteEdPoly;
    BackList[nBack++] = New;
    AllocatedFPolys.Add(New);
}

// Build an FPoly representing an "infinite" plane
// (which exceeds the maximum dimensions of the world in all directions)
// for a particular Bsp node
FPoly FPMUBSPOps::BuildInfiniteFPoly(UModel* Model, int32 iNode)
{
    FBspNode &Node   = Model->Nodes  [iNode       ];
    FBspSurf &Poly   = Model->Surfs  [Node.iSurf  ];
    FVector  Base    = Poly.Plane * Poly.Plane.W;
    FVector  Normal  = Poly.Plane;
    FVector  Axis1,Axis2;

    // Find two non-problematic axis vectors.
    Normal.FindBestAxisVectors( Axis1, Axis2 );

    // Set up the FPoly.
    FPoly EdPoly;
    EdPoly.Init();
    EdPoly.Normal      = Normal;
    EdPoly.Base        = Base;
    new(EdPoly.Vertices) FVector(Base + Axis1*WORLD_MAX + Axis2*WORLD_MAX);
    new(EdPoly.Vertices) FVector(Base - Axis1*WORLD_MAX + Axis2*WORLD_MAX);
    new(EdPoly.Vertices) FVector(Base - Axis1*WORLD_MAX - Axis2*WORLD_MAX);
    new(EdPoly.Vertices) FVector(Base + Axis1*WORLD_MAX - Axis2*WORLD_MAX);

    return EdPoly;
}

// Convert a Bsp node's polygon to an EdPoly, add it to the list, and recurse
void FPMUBSPOps::MakeEdPolys(UModel* Model, int32 iNode, TArray<FPoly>* DestArray)
{
    FBspNode* Node = &Model->Nodes[iNode];

    FPoly Temp;

    //if (GEditor->bspNodeToFPoly(Model,iNode,&Temp) >= 3)
    if (bspNodeToFPoly(Model, iNode, &Temp) >= 3)
    {
        new(*DestArray)FPoly(Temp);
    }

    if (Node->iFront != INDEX_NONE)
    {
        MakeEdPolys(Model, Node->iFront, DestArray);
    }
    if (Node->iBack != INDEX_NONE) 
    {
        MakeEdPolys(Model, Node->iBack, DestArray);
    }
    if (Node->iPlane != INDEX_NONE)
    {
        MakeEdPolys(Model, Node->iPlane, DestArray);
    }
}

// Recursively filter a set of polys defining a convex hull down the Bsp,
// splitting it into two halves at each node and adding in the appropriate
// face polys at splits
void FPMUBSPOps::FilterBound(
    UModel* Model,
    FBox*   ParentBound,
    int32   iNode,
    FPoly** PolyList,
    int32   nPolys,
    int32   Outside
    )
{
    FMemMark Mark(FMemStack::Get());
    FBspNode&   Node    = Model->Nodes  [iNode];
    FBspSurf&   Surf    = Model->Surfs  [Node.iSurf];
    FVector     Base    = Surf.Plane * Surf.Plane.W;
    FVector&    Normal  = Model->Vectors[Surf.vNormal];
    FBox        Bound(ForceInit);

    Bound.Min.X = Bound.Min.Y = Bound.Min.Z = +WORLD_MAX;
    Bound.Max.X = Bound.Max.Y = Bound.Max.Z = -WORLD_MAX;

    // Split bound into front half and back half
    FPoly** FrontList = new(FMemStack::Get(),nPolys*2+16)FPoly*; int32 nFront=0;
    FPoly** BackList  = new(FMemStack::Get(),nPolys*2+16)FPoly*; int32 nBack=0;

    // Keeping track of allocated FPoly structures to delete later on
    TArray<FPoly*> AllocatedFPolys;

    FPoly* FrontPoly  = new FPoly;
    FPoly* BackPoly   = new FPoly;

    // Keep track of allocations
    AllocatedFPolys.Add(FrontPoly);
    AllocatedFPolys.Add(BackPoly);

    for (int32 i=0; i<nPolys; ++i)
    {
        FPoly *Poly = PolyList[i];
        switch (Poly->SplitWithPlane(Base, Normal, FrontPoly, BackPoly, 0))
        {
            case SP_Coplanar:
                //UE_LOG(LogBSPOps, Log, TEXT("FilterBound: Got coplanar"));
                FrontList[nFront++] = Poly;
                BackList[nBack++] = Poly;
                break;
            
            case SP_Front:
                FrontList[nFront++] = Poly;
                break;
            
            case SP_Back:
                BackList[nBack++] = Poly;
                break;
            
            case SP_Split:
                FrontList[nFront++] = FrontPoly;
                BackList [nBack++] = BackPoly;

                FrontPoly = new FPoly;
                BackPoly  = new FPoly;

                // Keep track of allocations
                AllocatedFPolys.Add( FrontPoly );
                AllocatedFPolys.Add( BackPoly );

                break;

            default:
                //UE_LOG(LogBSPOps, Fatal, TEXT("FZoneFilter::FilterToLeaf: Unknown split code") );
                UE_LOG(LogTemp, Fatal, TEXT("FZoneFilter::FilterToLeaf: Unknown split code") );
        }
    }

    if (nFront && nBack)
    {
        // Add partitioner plane to front and back
        FPoly InfiniteEdPoly = FPMUBSPOps::BuildInfiniteFPoly(Model, iNode);
        InfiniteEdPoly.iBrushPoly = iNode;

        SplitPartitioner(
            Model,
            PolyList,
            FrontList,
            BackList,
            0,
            nPolys,
            nFront,
            nBack,
            InfiniteEdPoly,
            AllocatedFPolys
            );
    }
    else
    {
        //if (!nFront) UE_LOG(LogBSPOps, Log, TEXT("FilterBound: Empty fronthull"));
        //if (!nBack ) UE_LOG(LogBSPOps, Log, TEXT("FilterBound: Empty backhull"));
    }

    // Recursively update all our childrens' bounding volumes
    if (nFront > 0)
    {
        if (Node.iFront != INDEX_NONE)
        {
            FilterBound(
                Model,
                &Bound,
                Node.iFront,
                FrontList,
                nFront,
                Outside || Node.IsCsg()
                );
        }
        else
        if (Outside || Node.IsCsg())
        {
            UpdateBoundWithPolys(Bound, FrontList, nFront);
        }
        else
        {
            UpdateConvolutionWithPolys(Model, iNode, FrontList, nFront);
        }
    }
    if (nBack > 0)
    {
        if (Node.iBack != INDEX_NONE)
        {
            FilterBound(
                Model,
                &Bound,
                Node.iBack,
                BackList,
                nBack,
                Outside && !Node.IsCsg()
                );
        }
        else
        if (Outside && !Node.IsCsg())
        {
            UpdateBoundWithPolys(Bound, BackList, nBack);
        }
        else
        {
            UpdateConvolutionWithPolys(Model, iNode, BackList, nBack);
        }
    }

    // Update parent bound to enclose this bound
    if (ParentBound)
    {
        *ParentBound += Bound;
    }

    // Delete FPolys allocated above. We cannot use FMemStack::Get() for FPoly as the array data FPoly contains will be allocated in regular memory
    for (int32 i=0; i<AllocatedFPolys.Num(); ++i)
    {
        FPoly* AllocatedFPoly = AllocatedFPolys[i];
        delete AllocatedFPoly;
    }

    Mark.Pop();
}

void FPMUBSPOps::CleanupNodes(UModel* Model, int32 iNode, int32 iParent)
{
    FBspNode *Node = &Model->Nodes[iNode];

    // Transactionally empty vertices of tag-for-empty nodes
    Node->NodeFlags &= ~(NF_IsNew | NF_IsFront | NF_IsBack);

    // Recursively clean up front, back, and plane nodes
    if (Node->iFront != INDEX_NONE) CleanupNodes(Model, Node->iFront, iNode);
    if (Node->iBack  != INDEX_NONE) CleanupNodes(Model, Node->iBack , iNode);
    if (Node->iPlane != INDEX_NONE) CleanupNodes(Model, Node->iPlane, iNode);

    // Reload Node since the recusive call aliases it
    Node = &Model->Nodes[iNode];

    // If this is an empty node with a coplanar, replace it with the coplanar
    if ((Node->NumVertices == 0) && (Node->iPlane != INDEX_NONE))
    {
        FBspNode* PlaneNode = &Model->Nodes[Node->iPlane];

        // Stick our front, back, and parent nodes on the coplanar
        if ((Node->Plane | PlaneNode->Plane) >= 0.0)
        {
            PlaneNode->iFront  = Node->iFront;
            PlaneNode->iBack   = Node->iBack;
        }
        else
        {
            PlaneNode->iFront  = Node->iBack;
            PlaneNode->iBack   = Node->iFront;
        }

        if (iParent == INDEX_NONE)
        {
            // This node is the root
            *Node                  = *PlaneNode;   // Replace root
            PlaneNode->NumVertices = 0;            // Mark as unused
        }
        else
        {
            // This is a child node
            FBspNode *ParentNode = &Model->Nodes[iParent];

            if      (ParentNode->iFront == iNode) ParentNode->iFront = Node->iPlane;
            else if (ParentNode->iBack  == iNode) ParentNode->iBack  = Node->iPlane;
            else if (ParentNode->iPlane == iNode) ParentNode->iPlane = Node->iPlane;
            else
            {
                UE_LOG(LogTemp, Fatal, TEXT("CleanupNodes: Parent and child are unlinked"));
            }
        }
    }
    else
    if ((Node->NumVertices == 0) && ((Node->iFront == INDEX_NONE) || (Node->iBack == INDEX_NONE)))
    {
        // Delete empty nodes with no fronts or backs
        // Replace empty nodes with only fronts
        // Replace empty nodes with only backs
        int32 iReplacementNode;
        if      (Node->iFront != INDEX_NONE) iReplacementNode = Node->iFront;
        else if (Node->iBack  != INDEX_NONE) iReplacementNode = Node->iBack;
        else                                 iReplacementNode = INDEX_NONE;

        if (iParent == INDEX_NONE)
        {
            // Root
            if (iReplacementNode == INDEX_NONE)
            {
                Model->Nodes.Empty();
            }
            else
            {
                *Node = Model->Nodes[iReplacementNode];
            }
        }
        else
        {
            // Regular node
            FBspNode* ParentNode = &Model->Nodes[iParent];

            if      (ParentNode->iFront == iNode) ParentNode->iFront = iReplacementNode;
            else if (ParentNode->iBack  == iNode) ParentNode->iBack  = iReplacementNode;
            else if (ParentNode->iPlane == iNode) ParentNode->iPlane = iReplacementNode;
            else
            {
                //UE_LOG(LogEditorBsp, Fatal, TEXT("CleanupNodes: Parent and child are unlinked"));
                UE_LOG(LogTemp, Fatal, TEXT("CleanupNodes: Parent and child are unlinked"));
            }
        }
    }
}

int32 FPMUBSPOps::bspAddVector(UModel* Model, FVector* V, bool Exact)
{
    const float Thresh = Exact ? THRESH_NORMALS_ARE_SAME : THRESH_VECTORS_ARE_NEAR;

    if (FPMUBSPPointsGrid::GBSPVectors)
    {
        // If a points grid has been built for quick vector lookup,
        // use that instead of doing a linear search
        const int32 NextIndex = Model->Vectors.Num();
        const int32 ReturnedIndex = FPMUBSPPointsGrid::GBSPVectors->FindOrAddPoint(*V, NextIndex, Thresh);

        if (ReturnedIndex == NextIndex)
        {
            Model->Vectors.Add(*V);
        }

        return ReturnedIndex;
    }

#if 0
    return AddThing(
        Model->Vectors,
        *V,
        Exact ? THRESH_NORMALS_ARE_SAME : THRESH_VECTORS_ARE_NEAR,
        1
        );
#else
    check(0);
    return -1;
#endif
}

int32 FPMUBSPOps::bspAddPoint(UModel* Model, FVector* V, bool Exact)
{
    const float Thresh = Exact ? THRESH_POINTS_ARE_SAME : THRESH_POINTS_ARE_NEAR;

    if (FPMUBSPPointsGrid::GBSPPoints)
    {
        // If a points grid has been built for quick point lookup,
        // use that instead of doing a linear search
        const int32 NextIndex = Model->Points.Num();

        // Always look for points with a low threshold,
        // a generous threshold can result in 'leaks'
        // in the BSP and unwanted polys being generated
        const int32 ReturnedIndex = FPMUBSPPointsGrid::GBSPPoints->FindOrAddPoint(*V, NextIndex, THRESH_POINTS_ARE_SAME);

        if (ReturnedIndex == NextIndex)
        {
            Model->Points.Add(*V);
        }

        return ReturnedIndex;
    }

#if 0
    // Try to find a match quickly from the Bsp. This finds all potential matches
    // except for any dissociated from nodes/surfaces during a rebuild.
    FVector Temp;
    int32 pVertex;
    float NearestDist = Model->FindNearestVertex(*V,Temp,Thresh,pVertex);
    if ((NearestDist >= 0.0) && (NearestDist <= Thresh))
    {
        // Found an existing point.
        return pVertex;
    }
    else
    {
        // No match found; add it slowly to find duplicates.
        return AddThing(Model->Points, *V, Thresh, !GFastRebuild);
    }
#else
    check(0);
    return -1;
#endif
}

int32 FPMUBSPOps::bspAddNode(UModel* Model, int32 iParent, ENodePlace NodePlace, uint32 NodeFlags, FPoly* EdPoly)
{
    if (NodePlace == NODE_Plane)
    {
        // Make sure coplanars are added at the end of the coplanar list so that 
        // we don't insert NF_IsNew nodes with non NF_IsNew coplanar children.
        while (Model->Nodes[iParent].iPlane != INDEX_NONE)
        {
            iParent = Model->Nodes[iParent].iPlane;
        }
    }

    FBspSurf* Surf = nullptr;

    if (EdPoly->iLinkSurf == Model->Surfs.Num())
    {
        int32 NewIndex = Model->Surfs.AddZeroed();
        Surf = &Model->Surfs[NewIndex];

        // This node has a new polygon being added by bspBrushCSG,
        // must set its properties here
        Surf->pBase         = bspAddPoint  (Model,&EdPoly->Base,1);
        Surf->vNormal       = bspAddVector (Model,&EdPoly->Normal,1);
        Surf->vTextureU     = bspAddVector (Model,&EdPoly->TextureU,0);
        Surf->vTextureV     = bspAddVector (Model,&EdPoly->TextureV,0);
        Surf->Material      = EdPoly->Material;
        Surf->Actor         = nullptr;

        Surf->PolyFlags     = EdPoly->PolyFlags & ~PF_NoAddToBSP;
        Surf->LightMapScale = EdPoly->LightMapScale;

        // Find the LightmassPrimitiveSettings in the UModel

        int32 FoundLightmassIndex = INDEX_NONE;

        if (Model->LightmassSettings.Find(EdPoly->LightmassSettings, FoundLightmassIndex) == false)
        {
            FoundLightmassIndex = Model->LightmassSettings.Add(EdPoly->LightmassSettings);
        }

        Surf->iLightmassIndex = FoundLightmassIndex;

        Surf->Actor      = EdPoly->Actor;
        Surf->iBrushPoly = EdPoly->iBrushPoly;
        
        if (EdPoly->Actor)
        {
            Surf->bHiddenEdTemporary = EdPoly->Actor->IsTemporarilyHiddenInEditor();
            Surf->bHiddenEdLevel = EdPoly->Actor->bHiddenEdLevel;
            Surf->bHiddenEdLayer = EdPoly->Actor->bHiddenEdLayer;
        }

        Surf->Plane = FPlane(EdPoly->Vertices[0],EdPoly->Normal);
    }
    else
    {
        check(EdPoly->iLinkSurf!=INDEX_NONE);
        check(EdPoly->iLinkSurf<Model->Surfs.Num());
        Surf = &Model->Surfs[EdPoly->iLinkSurf];
    }

    // Set NodeFlags.
    if (Surf->PolyFlags & PF_NotSolid             ) NodeFlags |= NF_NotCsg;
    if (Surf->PolyFlags & (PF_Invisible|PF_Portal)) NodeFlags |= NF_NotVisBlocking;

    if (EdPoly->Vertices.Num() > FBspNode::MAX_NODE_VERTICES)
    {
#if 0
        // Split up into two coplanar sub-polygons
        // (one with MAX_NODE_VERTICES vertices and
        // one with all the remaining vertices) and recursively add them.

        // EdPoly1 is just the first MAX_NODE_VERTICES from EdPoly.
        FMemMark Mark(FMemStack::Get());
        FPoly *EdPoly1 = new FPoly;
        *EdPoly1 = *EdPoly;
        EdPoly1->Vertices.RemoveAt(FBspNode::MAX_NODE_VERTICES,EdPoly->Vertices.Num() - FBspNode::MAX_NODE_VERTICES);

        // EdPoly2 is the first vertex from EdPoly,
        // and the last EdPoly->Vertices.Num() - MAX_NODE_VERTICES + 1.
        FPoly *EdPoly2 = new FPoly;
        *EdPoly2 = *EdPoly;
        EdPoly2->Vertices.RemoveAt(1,FBspNode::MAX_NODE_VERTICES - 2);

        int32 iNode = bspAddNode( Model, iParent, NodePlace, NodeFlags, EdPoly1 ); // Add this poly first.
        bspAddNode( Model, iNode,   NODE_Plane, NodeFlags, EdPoly2 ); // Then add other (may be bigger).

        delete EdPoly1;
        delete EdPoly2;

        Mark.Pop();
        return iNode; // Return coplanar "parent" node (not coplanar child)
#else
        check(0);
        return -1;
#endif
    }
    else
    {
        // Add node
        int32 iNode    = Model->Nodes.AddZeroed();
        FBspNode& Node = Model->Nodes[iNode];

        // Tell transaction tracking system that parent is about to be modified.

        FBspNode* Parent = nullptr;

        if (NodePlace != NODE_Root)
        {
            Parent = &Model->Nodes[iParent];
        }

        // Set node properties
        Node.iSurf           = EdPoly->iLinkSurf;
        Node.NodeFlags       = NodeFlags;
        Node.iCollisionBound = INDEX_NONE;
        Node.Plane           = FPlane(EdPoly->Vertices[0], EdPoly->Normal);
        Node.iVertPool       = Model->Verts.AddUninitialized(EdPoly->Vertices.Num());
        Node.iFront          = INDEX_NONE;
        Node.iBack           = INDEX_NONE;
        Node.iPlane          = INDEX_NONE;

        if (NodePlace == NODE_Root)
        {
            Node.iLeaf[0]    = INDEX_NONE;
            Node.iLeaf[1]    = INDEX_NONE;
            Node.iZone[0]    = 0;
            Node.iZone[1]    = 0;
        }
        else
        if (NodePlace == NODE_Front || NodePlace == NODE_Back)
        {
            int32 ZoneFront = (NodePlace == NODE_Front);
            Node.iLeaf[0] = Parent->iLeaf[ZoneFront];
            Node.iLeaf[1] = Parent->iLeaf[ZoneFront];
            Node.iZone[0] = Parent->iZone[ZoneFront];
            Node.iZone[1] = Parent->iZone[ZoneFront];
        }
        else
        {
            int32 IsFlipped = (Node.Plane|Parent->Plane) < 0.f;
            Node.iLeaf[0] = Parent->iLeaf[IsFlipped  ];
            Node.iLeaf[1] = Parent->iLeaf[1-IsFlipped];
            Node.iZone[0] = Parent->iZone[IsFlipped  ];
            Node.iZone[1] = Parent->iZone[1-IsFlipped];
        }

        // Link parent to current node
        if (NodePlace == NODE_Front)
        {
            Parent->iFront = iNode;
        }
        else
        if (NodePlace == NODE_Back)
        {
            Parent->iBack = iNode;
        }
        else
        if (NodePlace == NODE_Plane)
        {
            Parent->iPlane = iNode;
        }

        // Add all points to point table, merging nearly-overlapping
        // polygon points with other points in the poly
        // to prevent criscrossing vertices from being generated
        //
        // Must maintain Node->NumVertices on the fly so that
        // bspAddPoint is always called with the Bsp in a clean state.
        Node.NumVertices = 0;
        FVert* VertPool  = &Model->Verts[ Node.iVertPool ];

        for (uint8 i=0; i<EdPoly->Vertices.Num(); i++)
        {
            int32 pVertex = bspAddPoint(Model,&EdPoly->Vertices[i],0);

            if (Node.NumVertices == 0 || VertPool[Node.NumVertices-1].pVertex != pVertex)
            {
                VertPool[Node.NumVertices].iSide   = INDEX_NONE;
                VertPool[Node.NumVertices].pVertex = pVertex;
                Node.NumVertices++;
            }
        }

        if (Node.NumVertices >= 2 && VertPool[0].pVertex == VertPool[Node.NumVertices-1].pVertex)
        {
            Node.NumVertices--;
        }

        if (Node.NumVertices < 3)
        {
            GErrors++;
            //UE_LOG(LogBSPOps, Warning, TEXT("bspAddNode: Infinitesimal polygon %i (%i)"), Node.NumVertices, EdPoly->Vertices.Num());
            Node.NumVertices = 0;
        }

        return iNode;
    }
}

// If the Bsp's point and vector tables are nearly full,
// reorder them and delete unused ones
void FPMUBSPOps::bspRefresh(UModel* Model, bool NoRemapSurfs)
{
    FMemStack& MemStack = FMemStack::Get();

    FMemMark Mark(MemStack);

    int32 NumNodes = Model->Nodes.Num();
    int32 NumSurfs = Model->Surfs.Num();
    int32 NumVectors = Model->Vectors.Num();
    int32 NumPoints = Model->Points.Num();

    // Remove unreferenced Bsp surfs
    int32* PolyRef;

    if (NoRemapSurfs)
    {
        PolyRef = NewZeroed<int32>(MemStack, NumSurfs);
    }
    else
    {
        PolyRef = NewOned<int32>(MemStack, NumSurfs);
    }

    int32* NodeRef = NewOned<int32>(MemStack, NumNodes);

    if (NumNodes > 0)
    {
        TagReferencedNodes(Model, NodeRef, PolyRef, 0);
    }

    // Remap Bsp surfs
    {
        int32 n=0;

        for (int32 i=0; i<NumSurfs; ++i)
        {
            if (PolyRef[i]!=INDEX_NONE)
            {
                Model->Surfs[n] = Model->Surfs[i];
                PolyRef[i]=n++;
            }
        }

        //UE_LOG(LogBSPOps, Log,  TEXT("Polys: %i -> %i"), NumSurfs, n );
        Model->Surfs.RemoveAt(n, NumSurfs-n);
        NumSurfs = n;
    }

    // Remap Bsp nodes
    {
        int32 n=0;

        for (int32 i=0; i<NumNodes; ++i)
        {
            if (NodeRef[i]!=INDEX_NONE)
            {
                Model->Nodes[n] = Model->Nodes[i];
                NodeRef[i]=n++;
            }
        }

        //UE_LOG(LogBSPOps, Log,  TEXT("Nodes: %i -> %i"), NumNodes, n );
        Model->Nodes.RemoveAt(n, NumNodes-n);
        NumNodes = n;
    }

    // Update Bsp nodes
    for (int32 i=0; i<NumNodes; ++i)
    {
        FBspNode* Node = &Model->Nodes[i];
        Node->iSurf = PolyRef[Node->iSurf];
        if (Node->iFront != INDEX_NONE) Node->iFront = NodeRef[Node->iFront];
        if (Node->iBack  != INDEX_NONE) Node->iBack  = NodeRef[Node->iBack];
        if (Node->iPlane != INDEX_NONE) Node->iPlane = NodeRef[Node->iPlane];
    }

    // Remove unreferenced points and vectors
    int32* VectorRef = NewOned<int32>(MemStack, NumVectors);
    int32* PointRef  = NewOned<int32>(MemStack, NumPoints);

    // Check Bsp surfs
    TArray<int32*> VertexRef;
    for (int32 i=0; i<NumSurfs; ++i)
    {
        FBspSurf* Surf = &Model->Surfs[i];
        VectorRef [Surf->vNormal   ] = 0;
        VectorRef [Surf->vTextureU ] = 0;
        VectorRef [Surf->vTextureV ] = 0;
        PointRef  [Surf->pBase     ] = 0;
    }

    // Check Bsp nodes
    for (int32 i=0; i<NumNodes; i++)
    {
        // Tag all points used by nodes
        FBspNode*   Node        = &Model->Nodes[i];
        FVert*      VertPool    = &Model->Verts[Node->iVertPool];

        for (int32 B=0; B<Node->NumVertices; ++B)
        {
            PointRef[VertPool->pVertex] = 0;
            VertPool++;
        }
    }

    // Remap points
    {
        int32 n=0;

        for (int32 i=0; i<NumPoints; ++i)
        {
            if (PointRef[i] != INDEX_NONE)
            {
                Model->Points[n] = Model->Points[i];
                PointRef[i] = n++;
            }
        }

        //UE_LOG(LogBSPOps, Log,  TEXT("Points: %i -> %i"), NumPoints, n );
        Model->Points.RemoveAt(n, NumPoints-n);
        NumPoints = n;
    }

    // Remap vectors
    {
        int32 n=0;

        for (int32 i=0; i<NumVectors; ++i)
        {
            if (VectorRef[i] != INDEX_NONE)
            {
                Model->Vectors[n] = Model->Vectors[i];
                VectorRef[i] = n++;
            }
        }

        //UE_LOG(LogBSPOps, Log,  TEXT("Vectors: %i -> %i"), NumVectors, n );
        Model->Vectors.RemoveAt(n, NumVectors-n);
        NumVectors = n;
    }

    // Update Bsp surfs
    for (int32 i=0; i<NumSurfs; ++i)
    {
        FBspSurf *Surf  = &Model->Surfs[i];
        Surf->vNormal   = VectorRef [Surf->vNormal  ];
        Surf->vTextureU = VectorRef [Surf->vTextureU];
        Surf->vTextureV = VectorRef [Surf->vTextureV];
        Surf->pBase     = PointRef  [Surf->pBase    ];
    }

    // Update Bsp nodes
    for (int32 i=0; i<NumNodes; ++i)
    {
        FBspNode*   Node        = &Model->Nodes[i];
        FVert*      VertPool    = &Model->Verts[Node->iVertPool];

        for (int32 B=0; B<Node->NumVertices;  B++)
        {           
            VertPool->pVertex = PointRef [VertPool->pVertex];
            VertPool++;
        }
    }

    // Shrink the objects
    Model->ShrinkModel();

    Mark.Pop();
}

void FPMUBSPOps::bspBuild(
    UModel* Model,
    EBSPOptimization Opt,
    int32 Balance,
    int32 PortalBias,
    int32 RebuildSimplePolys,
    int32 iNode
    )
{
    int32 OriginalPolys = Model->Polys->Element.Num();

    // Empty the model's tables
    if (RebuildSimplePolys == 1)
    {
        // Empty everything but polys
        Model->EmptyModel(1, 0);
    }
    else
    if (RebuildSimplePolys == 0)
    {
        // Empty node vertices
        for (int32 i=0; i<Model->Nodes.Num(); ++i)
        {
            Model->Nodes[i].NumVertices = 0;
        }

        // Refresh the Bsp
        bspRefresh(Model,1);
        
        // Empty nodes
        Model->EmptyModel(0, 0);
    }

    if (Model->Polys->Element.Num())
    {
        // Allocate polygon pool
        FMemMark Mark(FMemStack::Get());
        FPoly** PolyList = new(FMemStack::Get(), Model->Polys->Element.Num())FPoly*;

        // Add all FPolys to active list
        for (int32 i=0; i<Model->Polys->Element.Num(); ++i)
            if (Model->Polys->Element[i].Vertices.Num())
                PolyList[i] = &Model->Polys->Element[i];

        // Now split the entire Bsp by splitting the list of all polygons
        SplitPolyList(
            Model,
            INDEX_NONE,
            NODE_Root,
            Model->Polys->Element.Num(),
            PolyList,
            Opt,
            Balance,
            PortalBias,
            RebuildSimplePolys
            );

        // Now build the bounding boxes for all nodes
        if (RebuildSimplePolys == 0)
        {
            // Remove unreferenced things
            bspRefresh(Model, 1);

            // Rebuild all bounding boxes
            bspBuildBounds(Model);
        }

        Mark.Pop();
    }

    //UE_LOG(LogBSPOps, Log,  TEXT("bspBuild built %i convex polys into %i nodes"), OriginalPolys, Model->Nodes.Num() );
}

// Build bounding volumes for all Bsp nodes.  The bounding volume of the node
// completely encloses the "outside" space occupied by the nodes.  Note that 
// this is not the same as representing the bounding volume of all of the 
// polygons within the node.
//
// We start with a practically-infinite cube and filter it down the Bsp,
// whittling it away until all of its convex volume fragments land in leaves.
void FPMUBSPOps::bspBuildBounds(UModel* Model)
{
    if (Model->Nodes.Num() == 0)
    {
        return;
    }

    FPoly Polys[6], *PolyList[6];
    for (int32 i=0; i<6; i++)
    {
        PolyList[i] = &Polys[i];
        PolyList[i]->Init();
        PolyList[i]->iBrushPoly = INDEX_NONE;
    }

    new(Polys[0].Vertices)FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX,HALF_WORLD_MAX);
    new(Polys[0].Vertices)FVector( HALF_WORLD_MAX,-HALF_WORLD_MAX,HALF_WORLD_MAX);
    new(Polys[0].Vertices)FVector( HALF_WORLD_MAX, HALF_WORLD_MAX,HALF_WORLD_MAX);
    new(Polys[0].Vertices)FVector(-HALF_WORLD_MAX, HALF_WORLD_MAX,HALF_WORLD_MAX);
    Polys[0].Normal   =FVector( 0.000000,  0.000000,  1.000000 );
    Polys[0].Base     =Polys[0].Vertices[0];

    new(Polys[1].Vertices)FVector(-HALF_WORLD_MAX, HALF_WORLD_MAX,-HALF_WORLD_MAX);
    new(Polys[1].Vertices)FVector( HALF_WORLD_MAX, HALF_WORLD_MAX,-HALF_WORLD_MAX);
    new(Polys[1].Vertices)FVector( HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX);
    new(Polys[1].Vertices)FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX);
    Polys[1].Normal   =FVector( 0.000000,  0.000000, -1.000000 );
    Polys[1].Base     =Polys[1].Vertices[0];

    new(Polys[2].Vertices)FVector(-HALF_WORLD_MAX,HALF_WORLD_MAX,-HALF_WORLD_MAX);
    new(Polys[2].Vertices)FVector(-HALF_WORLD_MAX,HALF_WORLD_MAX, HALF_WORLD_MAX);
    new(Polys[2].Vertices)FVector( HALF_WORLD_MAX,HALF_WORLD_MAX, HALF_WORLD_MAX);
    new(Polys[2].Vertices)FVector( HALF_WORLD_MAX,HALF_WORLD_MAX,-HALF_WORLD_MAX);
    Polys[2].Normal   =FVector( 0.000000,  1.000000,  0.000000 );
    Polys[2].Base     =Polys[2].Vertices[0];

    new(Polys[3].Vertices)FVector( HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX);
    new(Polys[3].Vertices)FVector( HALF_WORLD_MAX,-HALF_WORLD_MAX, HALF_WORLD_MAX);
    new(Polys[3].Vertices)FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX, HALF_WORLD_MAX);
    new(Polys[3].Vertices)FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX);
    Polys[3].Normal   =FVector( 0.000000, -1.000000,  0.000000 );
    Polys[3].Base     =Polys[3].Vertices[0];

    new(Polys[4].Vertices)FVector(HALF_WORLD_MAX, HALF_WORLD_MAX,-HALF_WORLD_MAX);
    new(Polys[4].Vertices)FVector(HALF_WORLD_MAX, HALF_WORLD_MAX, HALF_WORLD_MAX);
    new(Polys[4].Vertices)FVector(HALF_WORLD_MAX,-HALF_WORLD_MAX, HALF_WORLD_MAX);
    new(Polys[4].Vertices)FVector(HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX);
    Polys[4].Normal   =FVector( 1.000000,  0.000000,  0.000000 );
    Polys[4].Base     =Polys[4].Vertices[0];

    new(Polys[5].Vertices)FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX);
    new(Polys[5].Vertices)FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX, HALF_WORLD_MAX);
    new(Polys[5].Vertices)FVector(-HALF_WORLD_MAX, HALF_WORLD_MAX, HALF_WORLD_MAX);
    new(Polys[5].Vertices)FVector(-HALF_WORLD_MAX, HALF_WORLD_MAX,-HALF_WORLD_MAX);
    Polys[5].Normal   =FVector(-1.000000,  0.000000,  0.000000 );
    Polys[5].Base     =Polys[5].Vertices[0];

    // Empty hulls

    Model->LeafHulls.Empty();

    for (int32 i=0; i<Model->Nodes.Num(); ++i)
    {
        Model->Nodes[i].iCollisionBound  = INDEX_NONE;
    }

    FilterBound(Model, nullptr, 0, PolyList, 6, Model->RootOutside);

//  UE_LOG(LogBSPOps, Log,  TEXT("bspBuildBounds: Generated %i hulls"), Model->LeafHulls.Num() );
}

void FPMUBSPOps::bspFilterFPoly(PMU_BSP_FILTER_FUNC FilterFunc, UModel *Model, FPoly *EdPoly)
{
    FCoplanarInfo StartingCoplanarInfo;
    StartingCoplanarInfo.iOriginalNode = INDEX_NONE;

    if (Model->Nodes.Num() == 0)
    {
        // Bsp is empty, process at root
        FilterFunc(Model, 0, EdPoly, Model->RootOutside ? F_OUTSIDE : F_INSIDE, FPMUBSPOps::NODE_Root);
    }
    else
    {
        // Filter through BSP
        FilterEdPoly(FilterFunc, Model, 0, EdPoly, StartingCoplanarInfo, Model->RootOutside);
    }
}

#if 0
int32 FPMUBSPOps::bspBrushCSG(
    //ABrush*    Actor,
    UModel*    Model,
    UModel*    Brush,
    FTransform BrushTransform,
    uint32     PolyFlags,
    EBrushType BrushType,
    ECsgOper   CSGOper,
    bool       bBuildBounds,
    bool       bMergePolys,
    bool       bReplaceNULLMaterialRefs,
    bool       bShowProgressBar/*=true*/
    )
{
    uint32 NotPolyFlags = 0;
    int32 NumPolysFromBrush=0;
    int32 i; //,j,ReallyBig;
    //UModel* Brush = Actor->Brush;

    // Note no errors.
    GErrors = 0;

    // Make sure we're in an acceptable state.
    if (! Brush)
    {
        return 0;
    }

    // Non-solid and semisolid stuff can only be added.
    if (BrushType != Brush_Add)
    {
        NotPolyFlags |= (PF_Semisolid | PF_NotSolid);
    }

    //TempModel->EmptyModel(1,1);
    TArray<FPoly> BrushPolys;

#if 0
    // Update status.
    ReallyBig = (Brush->Polys->Element.Num() > 200) && bShowProgressBar;
    if (ReallyBig)
    {
        FText Description = NSLOCTEXT("UnrealEd", "PerformingCSGOperation", "Performing CSG operation");
        
        if (BrushType != Brush_MAX)
        {   
            if (BrushType == Brush_Add)
            {
                Description = NSLOCTEXT("UnrealEd", "AddingBrushToWorld", "Adding brush to world");
            }
            else if (BrushType == Brush_Subtract)
            {
                Description = NSLOCTEXT("UnrealEd", "SubtractingBrushFromWorld", "Subtracting brush from world");
            }
        }
        else if (CSGOper != CSG_None)
        {
            if (CSGOper == CSG_Intersect)
            {
                Description = NSLOCTEXT("UnrealEd", "IntersectingBrushWithWorld", "Intersecting brush with world");
            }
            else if (CSGOper == CSG_Deintersect)
            {
                Description = NSLOCTEXT("UnrealEd", "DeintersectingBrushWithWorld", "Deintersecting brush with world");
            }
        }

        GWarn->BeginSlowTask( Description, true );
        // Transform original brush poly into same coordinate system as world
        // so Bsp filtering operations make sense.
        GWarn->StatusUpdate(0, 0, NSLOCTEXT("UnrealEd", "Transforming", "Transforming"));
    }
#endif

    //UMaterialInterface* SelectedMaterialInstance = GetSelectedObjects()->GetTop<UMaterialInterface>();

    const FVector Location = BrushTransform.GetLocation(); //Actor->GetActorLocation();
    const FRotator Rotation = BrushTransform.Rotator(); //Actor->GetActorRotation();
    const FVector Scale = BrushTransform.GetScale3D(); //Actor->GetActorScale();

    const bool bIsMirrored = (Scale.X * Scale.Y * Scale.Z < 0.0f);

    // Cache actor transform which is used for the geometry being built
    Brush->OwnerLocationWhenLastBuilt = Location;
    Brush->OwnerRotationWhenLastBuilt = Rotation;
    Brush->OwnerScaleWhenLastBuilt = Scale;
    Brush->bCachedOwnerTransformValid = true;

    for (i=0; i<Brush->Polys->Element.Num(); ++i)
    {
        FPoly& SrcPoly = Brush->Polys->Element[i];

        // Replace null material if required
        //if (bReplaceNULLMaterialRefs)
        //{
        //    UMaterialInterface*& PolyMat = SrcPoly.Material;
        //    if (!PolyMat || PolyMat == UMaterial::GetDefaultMaterial(MD_Surface))
        //    {
        //        PolyMat = SelectedMaterialInstance;
        //    }
        //}

        // Get the brush poly.
        FPoly DstPoly(SrcPoly);

        check(SrcPoly.iLink < Brush->Polys->Element.Num());

        // Set its backward brush link id
        //DstPoly.Actor       = Actor;
        DstPoly.iBrushPoly  = i;

        // Update poly flags
        DstPoly.PolyFlags = (DstPoly.PolyFlags | PolyFlags) & ~NotPolyFlags;

        // Set its internal link.
        if (DstPoly.iLink == INDEX_NONE)
        {
            DstPoly.iLink = i;
        }

        // Transform poly
        DstPoly.Scale(Scale);
        DstPoly.Rotate(Rotation);
        DstPoly.Transform(Location);

        for (int32 vi=0; vi<DstPoly.Vertices.Num(); ++vi)
        {
            UE_LOG(LogTemp,Warning, TEXT("P[%d] V[%d] %s"),
                i, vi, *DstPoly.Vertices[vi].ToString());
        }

        // Reverse winding and normal if the parent brush is mirrored
        if (bIsMirrored)
        {
            DstPoly.Reverse();
            DstPoly.CalcNormal();
        }

        // Add poly to the temp model.
        //new (TempModel->Polys->Element)FPoly( DstPoly );
        new(BrushPolys)FPoly(DstPoly);
    }


    //if (ReallyBig) GWarn->StatusUpdate(0, 0, NSLOCTEXT("UnrealEd", "FilteringBrush", "Filtering brush"));

    // Perfrom brush CSG operation over target model

    // Intersect and deintersect brush operation
    if (CSGOper == CSG_Intersect || CSGOper == CSG_Deintersect)
    {
#if 0
        // Empty the brush.
        Brush->EmptyModel(1,1);

        // Intersect and deintersect.
        for( i=0; i<TempModel->Polys->Element.Num(); i++ )
        {
            FPoly EdPoly = TempModel->Polys->Element[i];
            GModel = Brush;
            // TODO: iLink / iLinkSurf in EdPoly / TempModel->Polys->Element[i] ?
            bspFilterFPoly( CSGOper==CSG_Intersect ? IntersectBrushWithWorldFunc : DeIntersectBrushWithWorldFunc, Model,  &EdPoly );
        }
        NumPolysFromBrush = Brush->Polys->Element.Num();
#else
        check(0);
#endif
    }
    // Add and subtract brush operation
    else
    {
        TMap<int32, int32> SurfaceIndexRemap;

        //for (i=0; i<TempModel->Polys->Element.Num(); ++i)
        for (i=0; i<BrushPolys.Num(); ++i)
        {
            //FPoly CsgPoly = TempModel->Polys->Element[i];
            FPoly CsgPoly = BrushPolys[i];

            // Mark the polygon as non-cut so that
            // it won't be harmed unless it must be split,
            // and set iLink so that BspAddNode will know to add its information
            // if a node is added based on this poly

            CsgPoly.PolyFlags &= ~(PF_EdCut);
            const int32* SurfaceIndexPtr = SurfaceIndexRemap.Find(CsgPoly.iLink);

            if (! SurfaceIndexPtr)
            {
                const int32 NewSurfaceIndex = Model->Surfs.Num();
                SurfaceIndexRemap.Emplace(CsgPoly.iLink, NewSurfaceIndex);
                //CsgPoly.iLinkSurf = TempModel->Polys->Element[i].iLinkSurf = NewSurfaceIndex;
                CsgPoly.iLinkSurf = BrushPolys[i].iLinkSurf = NewSurfaceIndex;
            }
            else
            {
                //CsgPoly.iLinkSurf = TempModel->Polys->Element[i].iLinkSurf = *SurfaceIndexPtr;
                CsgPoly.iLinkSurf = BrushPolys[i].iLinkSurf = *SurfaceIndexPtr;
            }

            // Filter brush through the target model
            bspFilterFPoly(
                (BrushType == Brush_Add)
                    ? AddBrushToWorldFunc
                    : SubtractBrushFromWorldFunc,
                Model,
                &CsgPoly
                );
        }
    }

#if 0
    if (Model->Nodes.Num() && !(PolyFlags & (PF_NotSolid | PF_Semisolid)))
    {
        // Quickly build a Bsp for the brush, tending to minimize splits rather than balance
        // the tree.  We only need the cutting planes, though the entire Bsp struct (polys and
        // all) is built.

        FBspPointsGrid* LevelModelPointsGrid = FBspPointsGrid::GBspPoints;
        FBspPointsGrid* LevelModelVectorsGrid = FBspPointsGrid::GBspVectors;

        // For the bspBuild call, temporarily create a new pair of BspPointsGrids for the TempModel.
        TUniquePtr<FBspPointsGrid> BspPoints = MakeUnique<FBspPointsGrid>(50.0f, THRESH_POINTS_ARE_SAME);
        TUniquePtr<FBspPointsGrid> BspVectors = MakeUnique<FBspPointsGrid>(1 / 16.0f, FMath::Max(THRESH_NORMALS_ARE_SAME, THRESH_VECTORS_ARE_NEAR));
        FBspPointsGrid::GBspPoints = BspPoints.Get();
        FBspPointsGrid::GBspVectors = BspVectors.Get();

        if (ReallyBig) GWarn->StatusUpdate( 0, 0, NSLOCTEXT("UnrealEd", "BuildingBSP", "Building BSP") );
        
        FPMUBSPOps::bspBuild( TempModel, FPMUBSPOps::BSP_Lame, 0, 70, 1, 0 );

        // Reinstate the original BspPointsGrids used for building the level Model.
        FBspPointsGrid::GBspPoints = LevelModelPointsGrid;
        FBspPointsGrid::GBspVectors = LevelModelVectorsGrid;

        if (ReallyBig) GWarn->StatusUpdate( 0, 0, NSLOCTEXT("UnrealEd", "FilteringWorld", "Filtering world") );
        GModel = Brush;
        TempModel->BuildBound();

        FSphere BrushSphere = TempModel->Bounds.GetSphere();
        FilterWorldThroughBrush( Model, TempModel, BrushType, CSGOper, 0, &BrushSphere );
    }
#endif

    if (CSGOper==CSG_Intersect || CSGOper==CSG_Deintersect)
    {
#if 0
        if (ReallyBig) GWarn->StatusUpdate( 0, 0, NSLOCTEXT("UnrealEd", "AdjustingBrush", "Adjusting brush") );

        // Link polys obtained from the original brush.
        for( i=NumPolysFromBrush-1; i>=0; i-- )
        {
            FPoly *DestEdPoly = &Brush->Polys->Element[i];
            for( j=0; j<i; j++ )
            {
                if (DestEdPoly->iLink == Brush->Polys->Element[j].iLink)
                {
                    DestEdPoly->iLink = j;
                    break;
                }
            }
            if (j >= i) DestEdPoly->iLink = i;
        }

        // Link polys obtained from the world.
        for( i=Brush->Polys->Element.Num()-1; i>=NumPolysFromBrush; i-- )
        {
            FPoly *DestEdPoly = &Brush->Polys->Element[i];
            for( j=NumPolysFromBrush; j<i; j++ )
            {
                if (DestEdPoly->iLink == Brush->Polys->Element[j].iLink)
                {
                    DestEdPoly->iLink = j;
                    break;
                }
            }
            if (j >= i) DestEdPoly->iLink = i;
        }
        Brush->Linked = 1;

        // Detransform the obtained brush back into its original coordinate system.
        for( i=0; i<Brush->Polys->Element.Num(); i++ )
        {
            FPoly *DestEdPoly = &Brush->Polys->Element[i];
            DestEdPoly->Transform(-Location);
            DestEdPoly->Rotate(Rotation.GetInverse());
            DestEdPoly->Scale(FVector(1.0f) / Scale);
            DestEdPoly->Fix();
            //DestEdPoly->Actor       = nullptr;
            DestEdPoly->iBrushPoly  = i;
        }
#else
        check(0)
#endif
    }

    if ((BrushType == Brush_Add) || (BrushType == Brush_Subtract))
    {
        // Clean up nodes, reset node flags
        bspCleanup(Model);

        // Rebuild bounding volumes.
        if (bBuildBounds)
        {
            FPMUBSPOps::bspBuildBounds(Model);
        }
    }

    //Brush->NumUniqueVertices = TempModel->Points.Num();

    // Release TempModel.
    //TempModel->EmptyModel(1,1);
    
    // Merge coplanars if needed.
    if (CSGOper==CSG_Intersect || CSGOper==CSG_Deintersect)
    {
#if 0
        if (ReallyBig)
        {
            GWarn->StatusUpdate( 0, 0, NSLOCTEXT("UnrealEd", "Merging", "Merging") );
        }
        if (bMergePolys)
        {
            bspMergeCoplanars( Brush, 1, 0 );
        }
#else
        check(0);
#endif
    }

    //if (ReallyBig)
    //{
    //    GWarn->EndSlowTask();
    //}

    return 1 + FPMUBSPOps::GErrors;
}
#endif

int32 FPMUBSPOps::bspBrushCSG(
    UModel*    Model,
    UModel*    Brush,
    FTransform BrushTransform,
    uint32     PolyFlags,
    EBrushType BrushType,
    ECsgOper   CSGOper,
    bool       bBuildBounds,
    bool       bMergePolys,
    bool       bReplaceNULLMaterialRefs,
    bool       bShowProgressBar/*=true*/
    )
{
    uint32 NotPolyFlags = 0;
    int32 NumPolysFromBrush=0;
    int32 i; //,j,ReallyBig;

    // Note no errors
    GErrors = 0;

    // Make sure we're in an acceptable state
    if (! Brush)
    {
        return 0;
    }

    // Non-solid and semisolid stuff can only be added
    if (BrushType != Brush_Add)
    {
        NotPolyFlags |= (PF_Semisolid | PF_NotSolid);
    }

    //TempModel->EmptyModel(1,1);
    UModel* TempModel = NewObject<UModel>();
    TempModel->Initialize(nullptr, true);

    // Update status
#if 0
    ReallyBig = (Brush->Polys->Element.Num() > 200) && bShowProgressBar;

    if (ReallyBig)
    {
        FText Description = NSLOCTEXT("UnrealEd", "PerformingCSGOperation", "Performing CSG operation");
        
        if (BrushType != Brush_MAX)
        {   
            if (BrushType == Brush_Add)
            {
                Description = NSLOCTEXT("UnrealEd", "AddingBrushToWorld", "Adding brush to world");
            }
            else if (BrushType == Brush_Subtract)
            {
                Description = NSLOCTEXT("UnrealEd", "SubtractingBrushFromWorld", "Subtracting brush from world");
            }
        }
        else if (CSGOper != CSG_None)
        {
            if (CSGOper == CSG_Intersect)
            {
                Description = NSLOCTEXT("UnrealEd", "IntersectingBrushWithWorld", "Intersecting brush with world");
            }
            else if (CSGOper == CSG_Deintersect)
            {
                Description = NSLOCTEXT("UnrealEd", "DeintersectingBrushWithWorld", "Deintersecting brush with world");
            }
        }

        GWarn->BeginSlowTask( Description, true );
        // Transform original brush poly into same coordinate system as world
        // so Bsp filtering operations make sense.
        GWarn->StatusUpdate(0, 0, NSLOCTEXT("UnrealEd", "Transforming", "Transforming"));
    }
#endif

    //UMaterialInterface* SelectedMaterialInstance = GetSelectedObjects()->GetTop<UMaterialInterface>();

    const FVector Location = BrushTransform.GetLocation();
    const FRotator Rotation = BrushTransform.Rotator();
    const FVector Scale = BrushTransform.GetScale3D();

    const bool bIsMirrored = (Scale.X * Scale.Y * Scale.Z < 0.0f);

    // Cache actor transform which is used for the geometry being built
    Brush->OwnerLocationWhenLastBuilt = Location;
    Brush->OwnerRotationWhenLastBuilt = Rotation;
    Brush->OwnerScaleWhenLastBuilt = Scale;
    Brush->bCachedOwnerTransformValid = true;

    for (i=0; i<Brush->Polys->Element.Num(); i++)
    {
        FPoly& CurrentPoly = Brush->Polys->Element[i];

        // Set texture the first time
        //if (bReplaceNULLMaterialRefs)
        //{
        //    UMaterialInterface*& PolyMat = CurrentPoly.Material;

        //    if (!PolyMat || PolyMat == UMaterial::GetDefaultMaterial(MD_Surface))
        //    {
        //        PolyMat = SelectedMaterialInstance;
        //    }
        //}

        // Get the brush poly
        FPoly DestEdPoly = CurrentPoly;

        check(CurrentPoly.iLink<Brush->Polys->Element.Num());

        // Set its backward brush link
        //DestEdPoly.Actor = Actor;
        DestEdPoly.iBrushPoly = i;

        // Update its flags.
        DestEdPoly.PolyFlags = (DestEdPoly.PolyFlags | PolyFlags) & ~NotPolyFlags;

        // Set its internal link.
        if (DestEdPoly.iLink == INDEX_NONE)
        {
            DestEdPoly.iLink = i;
        }

        // Transform it.
        DestEdPoly.Scale(Scale);
        DestEdPoly.Rotate(Rotation);
        DestEdPoly.Transform(Location);

        // Reverse winding and normal if the parent brush is mirrored
        if (bIsMirrored)
        {
            DestEdPoly.Reverse();
            DestEdPoly.CalcNormal();
        }

        // Add poly to the temp model.
        new(TempModel->Polys->Element)FPoly(DestEdPoly);
    }

    //if (ReallyBig)
    //{
    //    GWarn->StatusUpdate(0, 0, NSLOCTEXT("UnrealEd", "FilteringBrush", "Filtering brush"));
    //}

    // Pass the brush polys through the world Bsp
    if (CSGOper==CSG_Intersect || CSGOper==CSG_Deintersect)
    {
#if 0
        // Empty the brush.
        Brush->EmptyModel(1,1);

        // Intersect and deintersect.
        for( i=0; i<TempModel->Polys->Element.Num(); i++ )
        {
            FPoly EdPoly = TempModel->Polys->Element[i];
            GModel = Brush;
            // TODO: iLink / iLinkSurf in EdPoly / TempModel->Polys->Element[i] ?
            bspFilterFPoly( CSGOper==CSG_Intersect ? IntersectBrushWithWorldFunc : DeIntersectBrushWithWorldFunc, Model,  &EdPoly );
        }
        NumPolysFromBrush = Brush->Polys->Element.Num();
#else
        check(0);
#endif
    }
    // Add and subtract
    else
    {
        TMap<int32, int32> SurfaceIndexRemap;

        for (i=0; i<TempModel->Polys->Element.Num(); i++)
        {
            FPoly EdPoly = TempModel->Polys->Element[i];

            // Mark the polygon as non-cut so that
            // it won't be harmed unless it must be split,
            // and set iLink so that BspAddNode will know to add
            // its information if a node is added based on this poly
            EdPoly.PolyFlags &= ~(PF_EdCut);
            const int32* SurfaceIndexPtr = SurfaceIndexRemap.Find(EdPoly.iLink);

            if (SurfaceIndexPtr == nullptr)
            {
                const int32 NewSurfaceIndex = Model->Surfs.Num();
                SurfaceIndexRemap.Add(EdPoly.iLink, NewSurfaceIndex);
                EdPoly.iLinkSurf = TempModel->Polys->Element[i].iLinkSurf = NewSurfaceIndex;
            }
            else
            {
                EdPoly.iLinkSurf = TempModel->Polys->Element[i].iLinkSurf = *SurfaceIndexPtr;
            }

            // Filter brush through the world
            bspFilterFPoly(
                (BrushType == Brush_Add)
                    ? AddBrushToWorldFunc
                    : SubtractBrushFromWorldFunc,
                Model,
                &EdPoly
                );
        }
    }

    if (Model->Nodes.Num() && !(PolyFlags & (PF_NotSolid | PF_Semisolid)))
    {
        // Quickly build a Bsp for the brush,
        // tending to minimize splits rather than balance the tree
        //
        // We only need the cutting planes,
        // though the entire Bsp struct (polys and all) is built

        FPMUBSPPointsGrid* LevelModelPointsGrid = FPMUBSPPointsGrid::GBSPPoints;
        FPMUBSPPointsGrid* LevelModelVectorsGrid = FPMUBSPPointsGrid::GBSPVectors;

        // For the bspBuild call, temporarily create a new pair
        // of BspPointsGrids for the TempModel
        TUniquePtr<FPMUBSPPointsGrid> BspPoints = MakeUnique<FPMUBSPPointsGrid>(50.0f, THRESH_POINTS_ARE_SAME);
        TUniquePtr<FPMUBSPPointsGrid> BspVectors = MakeUnique<FPMUBSPPointsGrid>(1 / 16.0f, FMath::Max(THRESH_NORMALS_ARE_SAME, THRESH_VECTORS_ARE_NEAR));
        FPMUBSPPointsGrid::GBSPPoints = BspPoints.Get();
        FPMUBSPPointsGrid::GBSPVectors = BspVectors.Get();

        //if (ReallyBig)
        //{
        //    GWarn->StatusUpdate(0, 0, NSLOCTEXT("UnrealEd", "BuildingBSP", "Building BSP"));
        //}
        
        FPMUBSPOps::bspBuild(TempModel, FPMUBSPOps::BSP_Lame, 0, 70, 1, 0);

        // Reinstate the original BspPointsGrids used for building the level Model.
        FPMUBSPPointsGrid::GBSPPoints = LevelModelPointsGrid;
        FPMUBSPPointsGrid::GBSPVectors = LevelModelVectorsGrid;

        //if (ReallyBig)
        //{
        //    GWarn->StatusUpdate(0, 0, NSLOCTEXT("UnrealEd", "FilteringWorld", "Filtering world"));
        //}

        GModel = Brush;
        TempModel->BuildBound();

        FSphere BrushSphere = TempModel->Bounds.GetSphere();
        FilterWorldThroughBrush(Model, TempModel, BrushType, CSGOper, 0, &BrushSphere);
    }

    if (CSGOper==CSG_Intersect || CSGOper==CSG_Deintersect)
    {
#if 0
        if (ReallyBig)
        {
            GWarn->StatusUpdate(0, 0, NSLOCTEXT("UnrealEd", "AdjustingBrush", "Adjusting brush"));
        }

        // Link polys obtained from the original brush.
        for (i=NumPolysFromBrush-1; i>=0; i--)
        {
            FPoly *DestEdPoly = &Brush->Polys->Element[i];

            for( j=0; j<i; j++ )
            {
                if (DestEdPoly->iLink == Brush->Polys->Element[j].iLink)
                {
                    DestEdPoly->iLink = j;
                    break;
                }
            }

            if (j >= i)
            {
                DestEdPoly->iLink = i;
            }
        }

        // Link polys obtained from the world.
        for (i=Brush->Polys->Element.Num()-1; i>=NumPolysFromBrush; i--)
        {
            FPoly *DestEdPoly = &Brush->Polys->Element[i];

            for (j=NumPolysFromBrush; j<i; j++)
            {
                if (DestEdPoly->iLink == Brush->Polys->Element[j].iLink)
                {
                    DestEdPoly->iLink = j;
                    break;
                }
            }

            if (j >= i)
            {
                DestEdPoly->iLink = i;
            }
        }
        Brush->Linked = 1;

        // Detransform the obtained brush back into its original coordinate system.
        for (i=0; i<Brush->Polys->Element.Num(); i++)
        {
            FPoly *DestEdPoly = &Brush->Polys->Element[i];
            DestEdPoly->Transform(-Location);
            DestEdPoly->Rotate(Rotation.GetInverse());
            DestEdPoly->Scale(FVector(1.0f) / Scale);
            DestEdPoly->Fix();
            DestEdPoly->Actor       = NULL;
            DestEdPoly->iBrushPoly  = i;
        }
#else
        check(0);
#endif
    }

    if (BrushType==Brush_Add || BrushType==Brush_Subtract)
    {
        // Clean up nodes, reset node flags
        bspCleanup(Model);

        // Rebuild bounding volumes
        if (bBuildBounds)
        {
            FPMUBSPOps::bspBuildBounds(Model);
        }
    }

    Brush->NumUniqueVertices = TempModel->Points.Num();

    // Release TempModel
    TempModel->EmptyModel(1,1);
    
    // Merge coplanars if needed
    if (CSGOper==CSG_Intersect || CSGOper==CSG_Deintersect)
    {
#if 0
        if (ReallyBig)
        {
            GWarn->StatusUpdate( 0, 0, NSLOCTEXT("UnrealEd", "Merging", "Merging") );
        }

        if (bMergePolys)
        {
            bspMergeCoplanars(Brush, 1, 0);
        }
#else
        check(0);
#endif
    }

    //if (ReallyBig)
    //{
    //    GWarn->EndSlowTask();
    //}

    return 1 + GErrors;
}

void FPMUBSPOps::bspBuildFPolys(UModel* Model, bool SurfLinks, int32 iNode, TArray<FPoly>* DestArray)
{
    if (DestArray == nullptr)
    {
        DestArray = &Model->Polys->Element;
    }

    DestArray->Reset();

    if (Model->Nodes.Num())
    {
        MakeEdPolys(Model, iNode, DestArray);
    }

    if (! SurfLinks)
    {
        const int32 ElementsCount = DestArray->Num();

        for (int32 i=0; i<ElementsCount; ++i)
        {
            (*DestArray)[i].iLink=i;
        }
    }
}

int32 FPMUBSPOps::bspNodeToFPoly(UModel* Model, int32 iNode, FPoly* EdPoly)
{
    FPoly MasterEdPoly;

    FBspNode& Node      = Model->Nodes[iNode];
    FBspSurf& Poly      = Model->Surfs[Node.iSurf];
    FVert*    VertPool  = &Model->Verts[Node.iVertPool];

    EdPoly->Base        = Model->Points [Poly.pBase];
    EdPoly->Normal      = Model->Vectors[Poly.vNormal];

    EdPoly->PolyFlags   = Poly.PolyFlags & ~(PF_EdCut | PF_EdProcessed | PF_Selected | PF_Memorized);
    EdPoly->iLinkSurf   = Node.iSurf;
    EdPoly->Material    = Poly.Material;

    EdPoly->Actor       = Poly.Actor;
    EdPoly->iBrushPoly  = Poly.iBrushPoly;
    
    //if (polyFindMaster(Model,Node.iSurf,MasterEdPoly))
    //{
    //    EdPoly->ItemName  = MasterEdPoly.ItemName;
    //}
    //else
    {
        EdPoly->ItemName  = NAME_None;
    }

    EdPoly->TextureU = Model->Vectors[Poly.vTextureU];
    EdPoly->TextureV = Model->Vectors[Poly.vTextureV];

    EdPoly->LightMapScale = Poly.LightMapScale;

    EdPoly->LightmassSettings = Model->LightmassSettings[Poly.iLightmassIndex];

    EdPoly->Vertices.Empty();

    for (int32 VertexIndex=0; VertexIndex<Node.NumVertices; ++VertexIndex)
    {
        new(EdPoly->Vertices)FVector(Model->Points[VertPool[VertexIndex].pVertex]);
    }

    if (EdPoly->Vertices.Num() < 3)
    {
        EdPoly->Vertices.Empty();
    }
    else
    {
        // Remove colinear points and identical points (which will appear
        // if T-joints were eliminated).
        EdPoly->RemoveColinears();
    }

    return EdPoly->Vertices.Num();
}

void FPMUBSPOps::bspCleanup(UModel* Model)
{
    if (Model->Nodes.Num() > 0)
    {
        CleanupNodes(Model, 0, INDEX_NONE);
    }
}

// --- FPMUBSPPointsGrid

void FPMUBSPPointsGrid::Clear(int32 InitialSize)
{
    GridMap.Empty(InitialSize);
}

int32 FPMUBSPPointsGrid::FindOrAddPoint(const FVector& Point, int32 Index, float PointThreshold)
{
    // Offset applied to the grid coordinates so aligned vertices (the normal case) don't overlap several grid items (taking into account the threshold)
    const float GridOffset = 0.12345f;

    const float AdjustedPointX = Point.X - GridOffset;
    const float AdjustedPointY = Point.Y - GridOffset;
    const float AdjustedPointZ = Point.Z - GridOffset;

    const float GridX = AdjustedPointX * OneOverGranularity;
    const float GridY = AdjustedPointY * OneOverGranularity;
    const float GridZ = AdjustedPointZ * OneOverGranularity;

    // Get the grid indices corresponding to the point coordinates
    const int32 GridIndexX = FMath::FloorToInt(GridX);
    const int32 GridIndexY = FMath::FloorToInt(GridY);
    const int32 GridIndexZ = FMath::FloorToInt(GridZ);

    // Find grid item in map
    FPMUBSPPointsGridItem& GridItem = GridMap.FindOrAdd(FPMUBSPPointsKey(GridIndexX, GridIndexY, GridIndexZ));

    // Iterate through grid item points and return a point
    // if it's close to the threshold
    const float PointThresholdSquared = PointThreshold * PointThreshold;
    for (const FPMUBSPIndexedPoint& IndexedPoint : GridItem.IndexedPoints)
    {
        if (FVector::DistSquared(IndexedPoint.Point, Point) <= PointThresholdSquared)
        {
            return IndexedPoint.Index;
        }
    }

    // Otherwise, the point is new: add it to the grid item.
    GridItem.IndexedPoints.Emplace(Point, Index);

    // The grid has a maximum threshold of a certain radius.
    // If the point is near the edge of a grid cube,
    // it may overlap into other items.
    //
    // Add it to all grid items it can be seen from.

    const float GridThreshold = Threshold * OneOverGranularity;
    const int32 NeighbourX = GetAdjacentIndexIfOverlapping(GridIndexX, GridX, GridThreshold);
    const int32 NeighbourY = GetAdjacentIndexIfOverlapping(GridIndexY, GridY, GridThreshold);
    const int32 NeighbourZ = GetAdjacentIndexIfOverlapping(GridIndexZ, GridZ, GridThreshold);

    const bool bOverlapsInX = (NeighbourX != GridIndexX);
    const bool bOverlapsInY = (NeighbourY != GridIndexY);
    const bool bOverlapsInZ = (NeighbourZ != GridIndexZ);

    if (bOverlapsInX)
    {
        GridMap.FindOrAdd(FPMUBSPPointsKey(NeighbourX, GridIndexY, GridIndexZ)).IndexedPoints.Emplace(Point, Index);

        if (bOverlapsInY)
        {
            GridMap.FindOrAdd(FPMUBSPPointsKey(NeighbourX, NeighbourY, GridIndexZ)).IndexedPoints.Emplace(Point, Index);

            if (bOverlapsInZ)
            {
                GridMap.FindOrAdd(FPMUBSPPointsKey(NeighbourX, NeighbourY, NeighbourZ)).IndexedPoints.Emplace(Point, Index);
            }
        }
        else if (bOverlapsInZ)
        {
            GridMap.FindOrAdd(FPMUBSPPointsKey(NeighbourX, GridIndexY, NeighbourZ)).IndexedPoints.Emplace(Point, Index);
        }
    }
    else
    {
        if (bOverlapsInY)
        {
            GridMap.FindOrAdd(FPMUBSPPointsKey(GridIndexX, NeighbourY, GridIndexZ)).IndexedPoints.Emplace(Point, Index);

            if (bOverlapsInZ)
            {
                GridMap.FindOrAdd(FPMUBSPPointsKey(GridIndexX, NeighbourY, NeighbourZ)).IndexedPoints.Emplace(Point, Index);
            }
        }
        else if (bOverlapsInZ)
        {
            GridMap.FindOrAdd(FPMUBSPPointsKey(GridIndexX, GridIndexY, NeighbourZ)).IndexedPoints.Emplace(Point, Index);
        }
    }

    return Index;
}
