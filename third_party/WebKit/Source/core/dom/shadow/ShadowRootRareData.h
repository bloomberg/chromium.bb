/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef ShadowRootRareData_h
#define ShadowRootRareData_h

#include "core/dom/shadow/InsertionPoint.h"
#include "core/html/shadow/HTMLContentElement.h"
#include "core/html/shadow/HTMLShadowElement.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class ShadowRootRareData {
public:
    ShadowRootRareData()
        : m_childShadowElementCount(0)
        , m_childContentElementCount(0)
        , m_childShadowRootCount(0)
    {
    }

    InsertionPoint* insertionPoint() const { return m_insertionPoint.get(); }
    void setInsertionPoint(PassRefPtr<InsertionPoint> insertionPoint) { m_insertionPoint = insertionPoint; }

    void addInsertionPoint(InsertionPoint*);
    void removeInsertionPoint(InsertionPoint*);

    bool hasShadowElementChildren() const { return m_childShadowElementCount; }
    bool hasContentElementChildren() const { return m_childContentElementCount; }
    bool hasShadowRootChildren() const { return m_childShadowRootCount; }

    void addChildShadowRoot() { ++m_childShadowRootCount; }
    void removeChildShadowRoot() { ASSERT(m_childShadowRootCount > 0); --m_childShadowRootCount; }

    unsigned childShadowRootCount() const { return m_childShadowRootCount; }

    const Vector<RefPtr<InsertionPoint> >& childInsertionPoints() { return m_childInsertionPoints; }
    void setChildInsertionPoints(Vector<RefPtr<InsertionPoint> >& list) { m_childInsertionPoints.swap(list); }
    void clearChildInsertionPoints() { m_childInsertionPoints.clear(); }

    StyleSheetList* styleSheets() { return m_styleSheetList.get(); }
    void setStyleSheets(PassRefPtr<StyleSheetList> styleSheetList) { m_styleSheetList = styleSheetList; }

private:
    RefPtr<InsertionPoint> m_insertionPoint;
    unsigned m_childShadowElementCount;
    unsigned m_childContentElementCount;
    unsigned m_childShadowRootCount;
    Vector<RefPtr<InsertionPoint> > m_childInsertionPoints;
    RefPtr<StyleSheetList> m_styleSheetList;
};

inline void ShadowRootRareData::addInsertionPoint(InsertionPoint* point)
{
    if (isHTMLShadowElement(point))
        ++m_childShadowElementCount;
    else if (isHTMLContentElement(point))
        ++m_childContentElementCount;
    else
        ASSERT_NOT_REACHED();
}

inline void ShadowRootRareData::removeInsertionPoint(InsertionPoint* point)
{
    if (isHTMLShadowElement(point))
        --m_childShadowElementCount;
    else if (isHTMLContentElement(point))
        --m_childContentElementCount;
    else
        ASSERT_NOT_REACHED();

    ASSERT(m_childContentElementCount >= 0);
    ASSERT(m_childShadowElementCount >= 0);
}

} // namespace WebCore

#endif
