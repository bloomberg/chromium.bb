/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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
#include "core/dom/shadow/ScopeContentDistribution.h"

#include "core/dom/NodeTraversal.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/shadow/HTMLContentElement.h"
#include "core/html/shadow/HTMLShadowElement.h"

namespace WebCore {

ScopeContentDistribution::ScopeContentDistribution()
    : m_insertionPointAssignedTo(0)
    , m_numberOfShadowElementChildren(0)
    , m_numberOfContentElementChildren(0)
    , m_numberOfElementShadowChildren(0)
    , m_insertionPointListIsValid(false)
{
}

void ScopeContentDistribution::setInsertionPointAssignedTo(PassRefPtr<InsertionPoint> insertionPoint)
{
    m_insertionPointAssignedTo = insertionPoint;
}

void ScopeContentDistribution::invalidateInsertionPointList()
{
    m_insertionPointListIsValid = false;
    m_insertionPointList.clear();
}

const Vector<RefPtr<InsertionPoint> >& ScopeContentDistribution::ensureInsertionPointList(ShadowRoot* shadowRoot)
{
    if (m_insertionPointListIsValid)
        return m_insertionPointList;

    m_insertionPointListIsValid = true;
    ASSERT(m_insertionPointList.isEmpty());

    if (!shadowRoot->containsInsertionPoints())
        return m_insertionPointList;

    for (Element* element = ElementTraversal::firstWithin(shadowRoot); element; element = ElementTraversal::next(element, shadowRoot)) {
        if (element->isInsertionPoint())
            m_insertionPointList.append(toInsertionPoint(element));
    }

    return m_insertionPointList;
}

void ScopeContentDistribution::registerInsertionPoint(InsertionPoint* point)
{
    if (isHTMLShadowElement(point))
        ++m_numberOfShadowElementChildren;
    else if (isHTMLContentElement(point))
        ++m_numberOfContentElementChildren;
    else
        ASSERT_NOT_REACHED();

    invalidateInsertionPointList();
}

void ScopeContentDistribution::unregisterInsertionPoint(InsertionPoint* point)
{
    if (isHTMLShadowElement(point))
        --m_numberOfShadowElementChildren;
    else if (isHTMLContentElement(point))
        --m_numberOfContentElementChildren;
    else
        ASSERT_NOT_REACHED();

    ASSERT(m_numberOfContentElementChildren >= 0);
    ASSERT(m_numberOfShadowElementChildren >= 0);

    invalidateInsertionPointList();
}

} // namespace WebCore
