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

#include "core/rendering/RenderFlexibleBox.h"

namespace WebCore {

OrderIterator::OrderIterator(const RenderFlexibleBox* flexibleBox)
    : m_flexibleBox(flexibleBox)
    , m_currentChild(0)
    , m_orderValuesIterator(0)
{
}

void OrderIterator::setOrderValues(Vector<int>& orderValues)
{
    reset();
    m_orderValues.clear();

    if (orderValues.isEmpty())
        return;

    std::sort(orderValues.begin(), orderValues.end());


    // This is inefficient if there are many repeated values, but
    // saves a lot of allocations when the values are unique. By far,
    // the common case is that there's exactly one item in the list
    // (the default order value of 0).
    m_orderValues.reserveInitialCapacity(orderValues.size());

    int previous = orderValues[0];
    m_orderValues.append(previous);
    for (size_t i = 1; i < orderValues.size(); ++i) {
        int current = orderValues[i];
        if (current == previous)
            continue;
        m_orderValues.append(current);
        previous = current;
    }
    m_orderValues.shrinkToFit();
}

RenderBox* OrderIterator::first()
{
    reset();
    return next();
}

RenderBox* OrderIterator::next()
{
    do {
        if (!m_currentChild) {
            if (m_orderValuesIterator == m_orderValues.end())
                return 0;
            if (m_orderValuesIterator) {
                ++m_orderValuesIterator;
                if (m_orderValuesIterator == m_orderValues.end())
                    return 0;
            } else {
                m_orderValuesIterator = m_orderValues.begin();
            }

            m_currentChild = m_flexibleBox->firstChildBox();
        } else {
            m_currentChild = m_currentChild->nextSiblingBox();
        }
    } while (!m_currentChild || m_currentChild->style()->order() != *m_orderValuesIterator);

    return m_currentChild;
}

void OrderIterator::reset()
{
    m_currentChild = 0;
    m_orderValuesIterator = 0;
}

} // namespace WebCore
