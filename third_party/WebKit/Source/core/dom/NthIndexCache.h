// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NthIndexCache_h
#define NthIndexCache_h

#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class Document;

class NthIndexCache final {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(NthIndexCache);
public:
    explicit NthIndexCache(Document&);
    ~NthIndexCache();

    inline unsigned nthChildIndex(Element&);
    inline unsigned nthLastChildIndex(Element&);

private:
    class NthIndexData final : public NoBaseWillBeGarbageCollected<NthIndexData> {
        WTF_MAKE_NONCOPYABLE(NthIndexData);
        DECLARE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(NthIndexData);
    public:
        NthIndexData() { }

        inline unsigned nthIndex(Element&);
        inline unsigned nthLastIndex(Element&);

        unsigned cacheNthIndices(Element&);

        WillBeHeapHashMap<RawPtrWillBeMember<Element>, unsigned> m_elementIndexMap;
        unsigned m_count = 0;

        DECLARE_TRACE();
    };

    NthIndexData& ensureNthIndexDataFor(Node&);
    inline unsigned nthIndex(Element&);

    using ParentMap = WillBeHeapHashMap<RefPtrWillBeMember<Node>, OwnPtrWillBeMember<NthIndexData>>;

    OwnPtrWillBeMember<ParentMap> m_parentMap;
    RawPtrWillBeMember<Document> m_document;
    uint64_t m_domTreeVersion;
};

inline unsigned NthIndexCache::NthIndexData::nthIndex(Element& element)
{
    if (!m_count)
        return cacheNthIndices(element);

    unsigned index = 0;
    for (Element* sibling = &element; sibling; sibling = ElementTraversal::previousSibling(*sibling), index++) {
        auto it = m_elementIndexMap.find(sibling);
        if (it != m_elementIndexMap.end())
            return it->value + index;
    }
    return index;
}

inline unsigned NthIndexCache::NthIndexData::nthLastIndex(Element& element)
{
    unsigned index = nthIndex(element);
    return m_count - index + 1;
}

inline unsigned NthIndexCache::nthChildIndex(Element& element)
{
    ASSERT(element.parentNode());
    return ensureNthIndexDataFor(*element.parentNode()).nthIndex(element);
}

inline unsigned NthIndexCache::nthLastChildIndex(Element& element)
{
    ASSERT(element.parentNode());
    return ensureNthIndexDataFor(*element.parentNode()).nthLastIndex(element);
}

} // namespace blink

#endif // NthIndexCache_h
