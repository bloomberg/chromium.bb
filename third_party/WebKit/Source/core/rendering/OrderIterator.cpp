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

#include "config.h"
#include "core/rendering/OrderIterator.h"

#include "core/rendering/RenderBox.h"

namespace WebCore {

OrderIterator::OrderIterator(const RenderBox* containerBox)
    : m_containerBox(containerBox)
    , m_currentChild(0)
    , m_currentOrderIndex(0)
    , m_currentChildIndex(0)
{
}

RenderBox* OrderIterator::first()
{
    reset();
    return next();
}

RenderBox* OrderIterator::next()
{
    for (; m_currentOrderIndex < m_orderValues.size(); ++m_currentOrderIndex) {
        const Vector<RenderBox*>& currentOrderChildren = m_orderedValues.get(m_orderValues[m_currentOrderIndex]);
        ASSERT(!currentOrderChildren.isEmpty());
        for (; m_currentChildIndex < currentOrderChildren.size(); ++m_currentChildIndex) {
            m_currentChild = currentOrderChildren[m_currentChildIndex];
            ++m_currentChildIndex;
            return m_currentChild;
        }

        m_currentChildIndex = 0;
    }

    m_currentChild = 0;
    return m_currentChild;
}

void OrderIterator::reset()
{
    m_currentOrderIndex = 0;
    m_currentChildIndex = 0;
    m_currentChild = 0;
}

void OrderIterator::invalidate()
{
    // Note that we don't release the memory here, we only invalidate the size.
    // This avoids unneeded reallocation if the size ends up not changing.
    m_orderValues.shrink(0);
    m_orderedValues.clear();

    reset();
}

OrderIteratorPopulator::~OrderIteratorPopulator()
{
    m_iterator.reset();

    std::sort(m_iterator.m_orderValues.begin(), m_iterator.m_orderValues.end());

    // Ensure that we release any extra memory we hold onto.
    m_iterator.m_orderValues.shrinkToFit();
}

void OrderIteratorPopulator::collectChild(RenderBox* child)
{
    int order = child->style()->order();

    // FIXME: Ideally we would want to avoid inserting into the HashMap for the common case where there are only items
    // with the default 'order' 0. The current API is designed to blend into a single iteration which makes having a
    // slower fallback difficult without having to store the children (grid items may not be contiguous in DOM order).
    OrderIterator::OrderedValuesMap::AddResult result = m_iterator.m_orderedValues.add(order, Vector<RenderBox*>());
    result.iterator->value.append(child);
    if (result.isNewEntry)
        m_iterator.m_orderValues.append(order);
}

} // namespace WebCore
