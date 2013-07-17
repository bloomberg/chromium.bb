/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef OrderIterator_h
#define OrderIterator_h

#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"

namespace WebCore {

class RenderBox;

// Normally, -1 and 0 are not valid in a HashSet, but these are relatively likel y order: values. Instead,
// we make the two smallest int values invalid order: values (in the css parser code we clamp them to
// int min + 2).
struct OrdererValueMapKeyHashTraits : WTF::GenericHashTraits<int> {
    static const bool emptyValueIsZero = false;
    static int emptyValue() { return std::numeric_limits<int>::min(); }
    static void constructDeletedValue(int& slot) { slot = std::numeric_limits<int>::min() + 1; }
    static bool isDeletedValue(int value) { return value == std::numeric_limits<int>::min() + 1; }
};

class OrderIterator {
    WTF_MAKE_NONCOPYABLE(OrderIterator);
public:
    friend class OrderIteratorPopulator;

    OrderIterator(const RenderBox*);

    RenderBox* currentChild() const { return m_currentChild; }
    RenderBox* first();
    RenderBox* next();
    void reset();

private:
    const RenderBox* m_containerBox;

    // The inline capacity for a single item is used to cover the most
    // common case by far: if we only have the default 'order' value 0.
    typedef Vector<int, 1> OrderValues;
    OrderValues m_orderValues;

    RenderBox* m_currentChild;
    size_t m_currentOrderIndex;
    size_t m_currentChildIndex;

    // This HashMap is empty if there is only one value.
    typedef HashMap<int, Vector<RenderBox*>, DefaultHash<int>::Hash, OrdererValueMapKeyHashTraits > OrderedValuesMap;
    OrderedValuesMap m_orderedValues;
};

class OrderIteratorPopulator {
public:
    OrderIteratorPopulator(OrderIterator& iterator)
        : m_iterator(iterator)
    {
        // Note that we don't release the memory here, we only invalidate the size.
        // This avoids unneeded reallocation if the size ends up not changing.
        m_iterator.m_orderValues.shrink(0);
        m_iterator.m_orderedValues.clear();
    }

    ~OrderIteratorPopulator();

    void collectChild(RenderBox*);

private:
    OrderIterator& m_iterator;
};

} // namespace WebCore

#endif //  OrderIterator_h
