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

#include "Line/PMUPolyLineExtrude.h"

void FPMUPolyLineExtrude::Reset(const int32 Reserve)
{
    bBuildStarted = false;
    LastFlip = -1;

    Positions.Empty(Reserve*2);
    Indices.Empty(Reserve*6);
}

void FPMUPolyLineExtrude::Build(const TArray<FVector2D>& Points)
{
    Reset(Points.Num());

    // Not enough points to extrude a line, return
    if (Points.Num() < 2)
    {
        return;
    }

    const int32 PointCount = Points.Num();

    // Construct segments
    for (int32 i=1, count=0; i<PointCount; i++)
    {
        bool bHasNext = i < (PointCount-1);

        FVector2D prev(Points[i-1]);
        FVector2D cur(Points[i]);
        FVector2D next;

        if (bHasNext)
        {
            next = Points[i+1];
        }

        //float thickness = this.mapThickness(cur, i, Points);
        float thickness = Thickness * .5f;

        //var amt = this._seg(complex, count, prev, cur, next, thickness/2)
        const int32 amt = CreateSegment(count, prev, cur, next, thickness, bHasNext);

        count += amt;
    }
}

int32 FPMUPolyLineExtrude::CreateSegment(const int32 index, FVector2D prev, FVector2D cur, const FVector2D& next, const float halfThick, const bool bHasNext)
{
    int32 count = 0;

    //var bCapSquared = this.cap === 'square';
    //var bJoinBevel = this.join === 'bevel';
    //bool bCapSquared = false;
    //bool bJoinBevel = false;

    // Get unit direction of line
    //direction(lineA, cur, prev);
    FVector2D lineA = (cur-prev).GetSafeNormal();

    // Haven't started yet, add the first two points
    if (! bBuildStarted)
    {
        bBuildStarted = true;

        // No normal from previous join, compute from line start - end

        //this._normal = vec.create()
        //normal(this._normal, lineA)
        Normal = GetPerpendicular(lineA);

        // If the end cap is type square, we can just push the verts out a bit

        if (bCapSquared)
        {
            //vec.scaleAndAdd(capEnd, prev, lineA, -halfThick);
            //prev = capEnd;

            prev += lineA * -halfThick;
        }

        AddExtrusion(prev, Normal, halfThick);
    }

    Indices.Emplace(index  );
    Indices.Emplace(index+1);
    Indices.Emplace(index+2);

    // Now determine the type of join with next segment

    // - Round (TODO)
    // - Bevel 
    // - Miter
    // - None (i.e. no next segment, use normal)
    
    // No next segment, simple extrusion
    if (! bHasNext)
    {
        //now reset normal to finish cap
        //normal(this._normal, lineA);
        Normal = GetPerpendicular(lineA);

        //push square end cap out a bit
        if (bCapSquared)
        {
            //vec.scaleAndAdd(capEnd, cur, lineA, halfThick)
            //cur = capEnd

            cur += lineA * -halfThick;
        }

        AddExtrusion(cur, Normal, halfThick);

        //cells.push(this._lastFlip===1 ? [index, index+2, index+3] : [index+2, index+1, index+3])
        if (LastFlip == 1)
        {
            Indices.Emplace(index  );
            Indices.Emplace(index+2);
            Indices.Emplace(index+3);
        }
        else
        {
            Indices.Emplace(index+2);
            Indices.Emplace(index+1);
            Indices.Emplace(index+3);
        }

        count += 2;
     }
     // We have a next segment, start with miter
     else
     {
        // Get unit dir of next line
        //direction(lineB, next, cur)
        FVector2D lineB = (next-cur).GetSafeNormal();

        // Stores tangent & miter
        //float miterLen = computeMiter(tangent, miter, lineA, lineB, halfThick);
        FVector2D tangent, miter;
        float miterLen = ComputeMiter(tangent, miter, lineA, lineB, halfThick);
        
        // Get orientation
        //var flip = (vec.dot(tangent, this._normal) < 0) ? -1 : 1;
        int32 flip = ((tangent | Normal) < 0.f) ? -1 : 1;

        //var bevel = bJoinBevel
        bool bUseBevel = bJoinBevel;

        // Use bevel join if miter length exceed miter limit
        if (! bUseBevel)
        {
            float limit = miterLen / halfThick;

            if (limit > MiterLimit)
            {
                bUseBevel = true;
            }
        }

        // Bevel join
        if (bUseBevel)
        {
            // Next two points in our first segment
            //vec.scaleAndAdd(tmp, cur, this._normal, -halfThick * flip)
            //positions.push(vec.clone(tmp))
            //vec.scaleAndAdd(tmp, cur, miter, miterLen * flip)
            //positions.push(vec.clone(tmp))
            Positions.Emplace(cur + (Normal * -halfThick * flip));
            Positions.Emplace(cur + (miter * miterLen * flip));

            //cells.push(this._lastFlip!==-flip
            //        ? [index, index+2, index+3] 
            //        : [index+2, index+1, index+3])

            if (LastFlip != -flip)
            {
                Indices.Emplace(index  );
                Indices.Emplace(index+2);
                Indices.Emplace(index+3);
            }
            else
            {
                Indices.Emplace(index+2);
                Indices.Emplace(index+1);
                Indices.Emplace(index+3);
            }

            // Now add the bevel triangle
            //cells.push([index+2, index+3, index+4]);
            Indices.Emplace(index+2);
            Indices.Emplace(index+3);
            Indices.Emplace(index+4);

            //normal(tmp, lineB)
            //vec.copy(this._normal, tmp) //store normal for next round
            Normal = GetPerpendicular(lineB);

            //vec.scaleAndAdd(tmp, cur, tmp, -halfThick*flip)
            //Positions.push(vec.clone(tmp))
            Positions.Emplace(cur + (Normal * -halfThick * flip));

            count += 3;
        }
        // Miter join
        else
        {
            // Next two points for our miter join
            AddExtrusion(cur, miter, miterLen);

            //cells.push(this._lastFlip===1
            //        ? [index, index+2, index+3] 
            //        : [index+2, index+1, index+3])

            if (LastFlip == 1)
            {
                Indices.Emplace(index  );
                Indices.Emplace(index+2);
                Indices.Emplace(index+3);
            }
            else
            {
                Indices.Emplace(index+2);
                Indices.Emplace(index+1);
                Indices.Emplace(index+3);
            }

            flip = -1;

            // The miter is now the normal for our next join
            //vec.copy(this._normal, miter)
            Normal = miter;

            count += 2;
        }

        LastFlip = flip;
    }

    return count;
}

float FPMUPolyLineExtrude::ComputeMiter(FVector2D& tangent, FVector2D& miter, const FVector2D& lineA, const FVector2D& lineB, const float halfThick)
{
    // Get tangent line
    //add(tangent, lineA, lineB)
    //normalize(tangent, tangent)
    tangent = (lineA + lineB).GetSafeNormal();

    // Get miter as a unit vector
    //set(miter, -tangent[1], tangent[0])
    //set(tmp, -lineA[1], lineA[0])
    miter = GetPerpendicular(tangent);

    // Get the necessary length of our miter
    //return halfThick / dot(miter, tmp)
    return halfThick / (miter | GetPerpendicular(lineA));
}

void FPMUPolyLineExtrude::AddExtrusion(const FVector2D& point, const FVector2D& normal, const float scale)
{
    // Next two points to end our segment
    //vec.scaleAndAdd(tmp, point, normal, -scale)
    //positions.push(vec.clone(tmp))

    //vec.scaleAndAdd(tmp, point, normal, scale)
    //positions.push(vec.clone(tmp))

    Positions.Emplace(point + (normal*-scale));
    Positions.Emplace(point + (normal* scale));
}

FPMUMeshSection UPMUPolyLineExtrude::CreateLineExtrudeSection(const TArray<FVector2D>& Points)
{
    FPMUMeshSection Section;

    ExtrudeUtility.Thickness   = Thickness;
    ExtrudeUtility.MiterLimit  = MiterLimit;
    ExtrudeUtility.bCapSquared = bCapSquared;
    ExtrudeUtility.bJoinBevel  = bJoinBevel;

    ExtrudeUtility.Build(Points);

    const TArray<FVector2D>& LinePositions(ExtrudeUtility.GetPositions());
    const int32 VertexCount = LinePositions.Num();

	TArray<FPMUMeshVertex>& Vertices(Section.VertexBuffer);
	FBox& Bounds(Section.LocalBox);

    Vertices.Reserve(VertexCount);

    for (int32 i=0; i<VertexCount; ++i)
    {
        FVector Position(LinePositions[i], 0.f);
        Vertices.Emplace(Position, FVector(0.f, 0.f, 1.f), FColor(255 * (i%2), 0, 0, 255));
        Bounds += Position;
    }

    Section.IndexBuffer = ExtrudeUtility.GetIndices();
    Section.bSectionVisible = true;

    return Section;
}
