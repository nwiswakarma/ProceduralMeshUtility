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

// Special Heap to store vertex data as we process the input linestring.
// We need a min-heap with:
//    standard: top(), pop(), push(elem)
//    non-standard: remove(elem) anywhere in heap, 
//                  or alternatively, re-heapify a single element.
//
// This implementation uses a reheap() function and maintains a map to quickly
// lookup a heap index given an element T.
//
template <typename T, typename Comparator = std::less<T> >
class FPMULineSimplifyVisvalingamHeap
{
public:
    typedef TMap<T, SIZE_T> FNodeToHeapIndexMap;

    FPMULineSimplifyVisvalingamHeap(SIZE_T fixed_size)
        : m_data(new T[fixed_size])
        , m_capacity(fixed_size)
        , m_size(0)
        , m_comp(Comparator())
    {
        m_node_to_heap.Reserve(fixed_size);
    }

    ~FPMULineSimplifyVisvalingamHeap()
    {
        delete[] m_data;
    }

    void insert(T node)
    {
        check(m_size < m_capacity);

        // insert at left-most available leaf (i.e.: bottom)
        SIZE_T insert_idx = m_size;
        m_size++;
        m_data[insert_idx] = node;

        // bubble up until it's in the right place
        bubble_up(insert_idx);

#ifdef PMU_LINE_SIMPLIFY_VIS_CHECK_HEAP_CONSISTENCY
        validate_heap_state();
#endif
    }

    const T top() const
    {
        check(!empty());
        return m_data[NODE_TYPE_ROOT];
    }

    T pop()
    {
        T res = top();
        m_size--;
        clear_heap_index(res);
        if (!empty())
        {
            // take the bottom element, stick it at the root and let it
            // bubble down to its right place.
            m_data[NODE_TYPE_ROOT] = m_data[m_size];
            bubble_down(NODE_TYPE_ROOT);
        }
#ifdef PMU_LINE_SIMPLIFY_VIS_CHECK_HEAP_CONSISTENCY
        validate_heap_state();
#endif
        return res;
    }

    bool empty() const
    {
        return m_size == 0;
    }

    /** reheap re-enforces the heap property for a node that was modified
     *  externally. Logarithmic performance for a node anywhere in the tree.
     */
    void reheap(T node)
    {
        SIZE_T heap_idx = get_heap_index(node);
        bubble_down(bubble_up(heap_idx));
#ifdef PMU_LINE_SIMPLIFY_VIS_CHECK_HEAP_CONSISTENCY
        validate_heap_state();
#endif
    }

private:
    FPMULineSimplifyVisvalingamHeap(const FPMULineSimplifyVisvalingamHeap& other);
    FPMULineSimplifyVisvalingamHeap& operator=(const FPMULineSimplifyVisvalingamHeap& other);

    enum NodeType
    {
        NODE_TYPE_ROOT = 0,
        NODE_TYPE_INVALID = ~0
    };

    SIZE_T parent(SIZE_T n) const
    {
        if (n != NODE_TYPE_ROOT)
        {
            return (n-1)/2;
        }
        else
        {
            return NODE_TYPE_INVALID;
        }
    }

    SIZE_T left_child(SIZE_T n) const
    {
        return 2*n + 1;
    }

    SIZE_T bubble_up(SIZE_T n)
    {
        SIZE_T parent_idx = parent(n);
        while (n != NODE_TYPE_ROOT && parent_idx != NODE_TYPE_INVALID
                && m_comp(m_data[n], m_data[parent_idx]))
        {
            Swap(m_data[n], m_data[parent_idx]);
            // update parent which is now at heap index 'n'
            set_heap_index(m_data[n], n);

            n = parent_idx;
            parent_idx = parent(n);
        }

        set_heap_index(m_data[n], n);
        return n;
    }

    SIZE_T small_elem(SIZE_T n, SIZE_T a, SIZE_T b) const
    {
        SIZE_T smallest = n;
        if (a < m_size && m_comp(m_data[a], m_data[smallest]))
        {
            smallest = a;
        }
        if (b < m_size && m_comp(m_data[b], m_data[smallest]))
        {
            smallest = b;
        }
        return smallest;
    }

    SIZE_T bubble_down(SIZE_T n)
    {
        while (true)
        {
            SIZE_T left = left_child(n);
            SIZE_T right = 1+left;
            SIZE_T smallest = small_elem(n, left, right);
            if (smallest != n)
            {
                Swap(m_data[smallest], m_data[n]);
                // update 'smallest' which is now at 'n'
                set_heap_index(m_data[n], n);
                n = smallest;
            }
            else
            {
                set_heap_index(m_data[n], n);
                return n;
            }
        }
    }
    
    void set_heap_index(T node, SIZE_T n)
    {
        m_node_to_heap.Emplace(node, n);
    }

    void clear_heap_index(T node)
    {
        check(m_node_to_heap.Contains(node));
        m_node_to_heap.Remove(node);
    }

    SIZE_T get_heap_index(T node) const
    {
        check(m_node_to_heap.Contains(node));
        SIZE_T heap_idx = m_node_to_heap.FindChecked(node);
        check(m_data[heap_idx] == node);
        return heap_idx;
    }

#ifdef PMU_LINE_SIMPLIFY_VIS_CHECK_HEAP_CONSISTENCY
    void validate_heap_state() const
    {
        check(m_node_to_heap.Num() == m_size);

        for (SIZE_T i = 0; i < m_size; ++i)
        {
            // ensure we cached the node properly
            check(i == get_heap_index(m_data[i]));
            // make sure heap property is preserved
            SIZE_T parent_index = parent(i);
            if (parent_index != NODE_TYPE_INVALID)
            {
                check(m_comp(m_data[parent_index], m_data[i]));
            }
        }
    }
#endif

private:
    T*  m_data;
    SIZE_T m_capacity;
    SIZE_T m_size;
    const Comparator m_comp;
    FNodeToHeapIndexMap m_node_to_heap;
};
