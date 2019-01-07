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

#include "PMULineSimplifyVisvalingam.h"
#include "PMULineSimplifyVisvalingamHeap.h"

#ifdef PMU_LINE_SIMPLIFY_VIS_CHECK_INSTANCE_MEMORY
int32 FPMULineSimplifyVisvalingam::FVertexNode::Instance = 0;
#endif

FPMULineSimplifyVisvalingam::FPMULineSimplifyVisvalingam(const FLinestring& input)
    : m_input_line(input)
{
    m_effective_areas.SetNumZeroed(input.Num());

    typedef FPMULineSimplifyVisvalingamHeap<FVertexNode*, FVertexNodeCompare> FHeap;

    // Compute effective area for each point in the input (except endpoints)

    TArray<FVertexNode*> node_list;
    FHeap min_heap(input.Num());

    node_list.Init(nullptr, input.Num());

    for (SIZE_T i=1; i < input.Num()-1; ++i)
    {
        float area = effective_area(i, i-1, i+1, input);
        if (area > KINDA_SMALL_NUMBER)
        {
            node_list[i] = new FVertexNode(i, i-1, i+1, area);
            min_heap.insert(node_list[i]);
        }
    }

    float min_area = TNumericLimits<float>::Lowest();

    while (!min_heap.empty())
    {
        FVertexNode* curr_node = min_heap.pop();
        check(curr_node == node_list[curr_node->vertex]);

        // If the current point's calculated area is less than that of the last
        // point to be eliminated, use the latter's area instead. (This ensures
        // that the current point cannot be eliminated without eliminating
        // previously eliminated points.)
        min_area = FMath::Max(min_area, curr_node->area);

        FVertexNode* prev_node = node_list[curr_node->prev_vertex];
        if (prev_node != nullptr)
        {
            prev_node->next_vertex = curr_node->next_vertex;
            prev_node->area = effective_area(*prev_node, input);
            min_heap.reheap(prev_node);
        }
        
        FVertexNode* next_node = node_list[curr_node->next_vertex];
        if (next_node != nullptr)
        {
            next_node->prev_vertex = curr_node->prev_vertex;
            next_node->area = effective_area(*next_node, input);
            min_heap.reheap(next_node);
        }

        // store the final value for this vertex and delete the node.
        m_effective_areas[curr_node->vertex] = min_area;
        node_list[curr_node->vertex] = nullptr;
        delete curr_node;
    }

    // Delete all remaining nodes
    for (FVertexNode*& Node : node_list)
    {
        if (Node)
        {
            delete Node;
            Node = nullptr;
        }
    }

#ifdef PMU_LINE_SIMPLIFY_VIS_CHECK_INSTANCE_MEMORY
    check(FVertexNode::Instance == 0);
#endif
}

void FPMULineSimplifyVisvalingam::simplify(float area_threshold, FLinestring& result) const
{
    result.Reserve(m_input_line.Num());

    for (SIZE_T i=0; i < m_input_line.Num(); ++i)
    {
        if (contains_vertex(i, area_threshold))
        {
            result.Emplace(m_input_line[i]);
        }
    }

    if (result.Num() < 3)
    {
        result.Empty();
    }

    result.Shrink();
}
