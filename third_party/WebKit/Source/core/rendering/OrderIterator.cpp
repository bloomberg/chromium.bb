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
    , m_orderValuesIterator(0)
{
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

            m_currentChild = firstChildBox();
        } else {
            m_currentChild = nextSiblingBox();
        }
    } while (!m_currentChild || m_currentChild->style()->order() != *m_orderValuesIterator);

    return m_currentChild;
}

void OrderIterator::reset()
{
    m_currentChild = 0;
    m_orderValuesIterator = 0;
}

RenderBox* OrderIterator::firstChildBox()
{
    if (m_children.isEmpty())
        return m_containerBox->firstChildBox();

    m_childIndex = 0;
    return m_children[0];
}

RenderBox* OrderIterator::nextSiblingBox()
{
    if (m_children.isEmpty())
        return m_currentChild->nextSiblingBox();

    if (m_childIndex >= m_children.size() - 1)
        return 0;

    return m_children[++m_childIndex];
}

OrderIteratorPopulator::~OrderIteratorPopulator()
{
    m_iterator.reset();

    if (m_anyChildHasDefaultOrderValue)
        m_iterator.m_orderValues.append(0);

    if (m_iterator.m_orderValues.size() > 1)
        removeDuplicatedOrderValues();

    // Ensure that we release any extra memory we hold onto.
    m_iterator.m_orderValues.shrinkToFit();
}

void OrderIteratorPopulator::removeDuplicatedOrderValues()
{
    OrderIterator::OrderValues& orderValues = m_iterator.m_orderValues;

    std::sort(orderValues.begin(), orderValues.end());

    int previous = orderValues[0];
    size_t uniqueItemIndex = 0;
    for (size_t i = 1; i < orderValues.size(); ++i) {
        int current = orderValues[i];
        if (current == previous)
            continue;
        ++uniqueItemIndex;
        std::swap(orderValues[i], orderValues[uniqueItemIndex]);
        previous = current;
    }
    orderValues.shrink(uniqueItemIndex + 1);
}

void OrderIteratorPopulator::storeChild(RenderBox* child)
{
    m_iterator.m_children.append(child);

    collectChild(child);
}

void OrderIteratorPopulator::collectChild(const RenderBox* child)
{
    // Avoid growing the vector for the common-case default value of 0.
    if (int order = child->style()->order())
        m_iterator.m_orderValues.append(order);
    else
        m_anyChildHasDefaultOrderValue = true;
}

} // namespace WebCore
