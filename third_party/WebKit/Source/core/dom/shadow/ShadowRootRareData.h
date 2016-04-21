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
#include "core/html/HTMLSlotElement.h"
#include "wtf/Vector.h"

namespace blink {

class ShadowRootRareData : public GarbageCollected<ShadowRootRareData> {
public:
    ShadowRootRareData()
        : m_childShadowRootCount(0)
        , m_descendantSlotCount(0)
    {
    }

    bool containsShadowRoots() const { return m_childShadowRootCount; }

    void didAddChildShadowRoot() { ++m_childShadowRootCount; }
    void didRemoveChildShadowRoot() { DCHECK_GT(m_childShadowRootCount, 0u); --m_childShadowRootCount; }

    unsigned childShadowRootCount() const { return m_childShadowRootCount; }

    StyleSheetList* styleSheets() { return m_styleSheetList.get(); }
    void setStyleSheets(StyleSheetList* styleSheetList) { m_styleSheetList = styleSheetList; }

    void didAddSlot() { ++m_descendantSlotCount; }
    void didRemoveSlot() { DCHECK_GT(m_descendantSlotCount, 0u); --m_descendantSlotCount; }

    unsigned descendantSlotCount() const { return m_descendantSlotCount; }

    const HeapVector<Member<HTMLSlotElement>>& descendantSlots() const { return m_descendantSlots; }

    void setDescendantSlots(HeapVector<Member<HTMLSlotElement>>& slots) { m_descendantSlots.swap(slots); }
    void clearDescendantSlots() { m_descendantSlots.clear(); }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_styleSheetList);
        visitor->trace(m_descendantSlots);
    }

private:
    unsigned m_childShadowRootCount;
    Member<StyleSheetList> m_styleSheetList;
    unsigned m_descendantSlotCount;
    HeapVector<Member<HTMLSlotElement>> m_descendantSlots;
};

} // namespace blink

#endif
