// Copyright (c) 2013, Mathieu Courtemanche
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met: 
// 
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer. 
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution. 
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// The views and conclusions contained in the software and documentation are those
// of the authors and should not be interpreted as representing official policies, 
// either expressed or implied, of the FreeBSD Project.
//
// Modified 2018 Nuraga Wiswakarma

#pragma once

#include "CoreMinimal.h"

class FPMULineSimplifyVisvalingam
{
private:

    typedef TArray<FVector2D> FLinestring;

    // Represents 3 vertices from the input line
    // and its associated effective area.
    struct FVertexNode
    {
#ifdef PMU_LINE_SIMPLIFY_VIS_CHECK_INSTANCE_MEMORY
        static int32 Instance;
#endif

        FVertexNode(SIZE_T vertex_, SIZE_T prev_vertex_, SIZE_T next_vertex_, float area_)
            : vertex(vertex_)
            , prev_vertex(prev_vertex_)
            , next_vertex(next_vertex_)
            , area(area_)
        {
#ifdef PMU_LINE_SIMPLIFY_VIS_CHECK_INSTANCE_MEMORY
            Instance++;
#endif
        }

#ifdef PMU_LINE_SIMPLIFY_VIS_CHECK_INSTANCE_MEMORY
        ~FVertexNode()
        {
            Instance--;
        }
#endif

        FORCEINLINE void Set(SIZE_T vertex_, SIZE_T prev_vertex_, SIZE_T next_vertex_, float area_)
        {
            vertex      = vertex_;
            prev_vertex = prev_vertex_;
            next_vertex = next_vertex_;
            area        = area_;
        }

        // ie: a triangle
        SIZE_T vertex;
        SIZE_T prev_vertex;
        SIZE_T next_vertex;
        // effective area
        float area;
    };

    struct FVertexNodeCompare
    {
        bool operator()(const FVertexNode* lhs, const FVertexNode* rhs) const
        {
            return lhs->area < rhs->area;
        }
    };

    FORCEINLINE static float effective_area(
        SIZE_T current,
        SIZE_T previous,
        SIZE_T next,
        const FLinestring& input_line
        )
    {
        const FVector2D& c = input_line[current];
        const FVector2D& p = input_line[previous];
        const FVector2D& n = input_line[next];
        const FVector2D c_n = n-c;
        const FVector2D c_p = p-c;
        const float det = c_n ^ c_p;
        return 0.5f * FMath::Abs(det);
    }

    FORCEINLINE static float effective_area(const FVertexNode& node, const FLinestring& input_line)
    {
        return effective_area(
            node.vertex,
            node.prev_vertex,
            node.next_vertex,
            input_line
            );
    }

public:

    FPMULineSimplifyVisvalingam(const FLinestring& input);

    void simplify(float area_threshold, FLinestring& result) const;

private:

    FORCEINLINE bool contains_vertex(SIZE_T vertex_index, float area_threshold) const
    {
        check(vertex_index < m_effective_areas.Num());
        check(m_effective_areas.Num() != 0);

        if (vertex_index == 0 || vertex_index == m_effective_areas.Num()-1)
        {
            // end points always kept since
            // we don't evaluate their effective areas
            return true;
        }

        return m_effective_areas[vertex_index] > area_threshold;
    }

    TArray<float> m_effective_areas;
    const FLinestring& m_input_line;
};
