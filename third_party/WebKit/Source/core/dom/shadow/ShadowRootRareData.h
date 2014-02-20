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
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class ShadowRootRareData {
public:
    ShadowRootRareData()
        : m_descendantShadowElementCount(0)
        , m_descendantContentElementCount(0)
        , m_childShadowRootCount(0)
        , m_childrenAffectedByDirectAdjacentRules(false)
        , m_childrenAffectedByForwardPositionalRules(false)
        , m_childrenAffectedByBackwardPositionalRules(false)
        , m_childrenAffectedByFirstChildRules(false)
        , m_childrenAffectedByLastChildRules(false)
    {
    }

    HTMLShadowElement* shadowInsertionPointOfYoungerShadowRoot() const { return m_shadowInsertionPointOfYoungerShadowRoot.get(); }
    void setShadowInsertionPointOfYoungerShadowRoot(PassRefPtr<HTMLShadowElement> shadowInsertionPoint) { m_shadowInsertionPointOfYoungerShadowRoot = shadowInsertionPoint; }

    void didAddInsertionPoint(InsertionPoint*);
    void didRemoveInsertionPoint(InsertionPoint*);

    bool containsShadowElements() const { return m_descendantShadowElementCount; }
    bool containsContentElements() const { return m_descendantContentElementCount; }
    bool containsShadowRoots() const { return m_childShadowRootCount; }

    unsigned descendantShadowElementCount() const { return m_descendantShadowElementCount; }

    void didAddChildShadowRoot() { ++m_childShadowRootCount; }
    void didRemoveChildShadowRoot() { ASSERT(m_childShadowRootCount > 0); --m_childShadowRootCount; }

    unsigned childShadowRootCount() const { return m_childShadowRootCount; }

    const Vector<RefPtr<InsertionPoint> >& descendantInsertionPoints() { return m_descendantInsertionPoints; }
    void setDescendantInsertionPoints(Vector<RefPtr<InsertionPoint> >& list) { m_descendantInsertionPoints.swap(list); }
    void clearDescendantInsertionPoints() { m_descendantInsertionPoints.clear(); }

    StyleSheetList* styleSheets() { return m_styleSheetList.get(); }
    void setStyleSheets(PassRefPtrWillBeRawPtr<StyleSheetList> styleSheetList) { m_styleSheetList = styleSheetList; }

    bool childrenAffectedByDirectAdjacentRules() const { return m_childrenAffectedByDirectAdjacentRules; }
    void setChildrenAffectedByDirectAdjacentRules(bool value) { m_childrenAffectedByDirectAdjacentRules = value; }
    bool childrenAffectedByForwardPositionalRules() const { return m_childrenAffectedByForwardPositionalRules; }
    void setChildrenAffectedByForwardPositionalRules(bool value) { m_childrenAffectedByForwardPositionalRules = value; }

    bool childrenAffectedByBackwardPositionalRules() const { return m_childrenAffectedByBackwardPositionalRules; }
    void setChildrenAffectedByBackwardPositionalRules(bool value) { m_childrenAffectedByBackwardPositionalRules = value; }

    bool childrenAffectedByFirstChildRules() const { return m_childrenAffectedByFirstChildRules; }
    void setChildrenAffectedByFirstChildRules(bool value) { m_childrenAffectedByFirstChildRules = value; }

    bool childrenAffectedByLastChildRules() const { return m_childrenAffectedByLastChildRules; }
    void setChildrenAffectedByLastChildRules(bool value) { m_childrenAffectedByLastChildRules = value; }

private:
    RefPtr<HTMLShadowElement> m_shadowInsertionPointOfYoungerShadowRoot;
    unsigned m_descendantShadowElementCount;
    unsigned m_descendantContentElementCount;
    unsigned m_childShadowRootCount;
    Vector<RefPtr<InsertionPoint> > m_descendantInsertionPoints;
    RefPtrWillBePersistent<StyleSheetList> m_styleSheetList;

    unsigned m_childrenAffectedByDirectAdjacentRules : 1;
    unsigned m_childrenAffectedByForwardPositionalRules : 1;
    unsigned m_childrenAffectedByBackwardPositionalRules : 1;
    unsigned m_childrenAffectedByFirstChildRules : 1;
    unsigned m_childrenAffectedByLastChildRules : 1;
};

inline void ShadowRootRareData::didAddInsertionPoint(InsertionPoint* point)
{
    if (point->hasTagName(HTMLNames::shadowTag))
        ++m_descendantShadowElementCount;
    else if (point->hasTagName(HTMLNames::contentTag))
        ++m_descendantContentElementCount;
    else
        ASSERT_NOT_REACHED();
}

inline void ShadowRootRareData::didRemoveInsertionPoint(InsertionPoint* point)
{
    if (point->hasTagName(HTMLNames::shadowTag))
        --m_descendantShadowElementCount;
    else if (point->hasTagName(HTMLNames::contentTag))
        --m_descendantContentElementCount;
    else
        ASSERT_NOT_REACHED();

    ASSERT(m_descendantContentElementCount >= 0);
    ASSERT(m_descendantShadowElementCount >= 0);
}

} // namespace WebCore

#endif
