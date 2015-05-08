// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NthIndexCache_h
#define NthIndexCache_h

#include "core/CoreExport.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class Document;

class CORE_EXPORT NthIndexCache final {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(NthIndexCache);
public:
    explicit NthIndexCache(Document&);
    ~NthIndexCache();

    inline unsigned nthChildIndex(Element&);
    inline unsigned nthChildIndexOfType(Element&, const QualifiedName&);
    inline unsigned nthLastChildIndex(Element&);
    inline unsigned nthLastChildIndexOfType(Element&, const QualifiedName&);

private:
    class NthIndexData final : public NoBaseWillBeGarbageCollected<NthIndexData> {
        WTF_MAKE_NONCOPYABLE(NthIndexData);
        DECLARE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(NthIndexData);
    public:
        NthIndexData() { }

        inline unsigned nthIndex(Element&);
        inline unsigned nthIndexOfType(Element&, const QualifiedName&);
        inline unsigned nthLastIndex(Element&);
        inline unsigned nthLastIndexOfType(Element&, const QualifiedName&);

        unsigned cacheNthIndices(Element&);
        unsigned cacheNthIndicesOfType(Element&, const QualifiedName&);

        WillBeHeapHashMap<RawPtrWillBeMember<Element>, unsigned> m_elementIndexMap;
        unsigned m_count = 0;

        DECLARE_TRACE();
    };

    using ParentMap = WillBeHeapHashMap<RefPtrWillBeMember<Node>, OwnPtrWillBeMember<NthIndexData>>;
    OwnPtrWillBeMember<ParentMap> m_parentMap;

    using IndexByType = WillBeHeapHashMap<String, OwnPtrWillBeMember<NthIndexData>>;

    using ParentMapForType = WillBeHeapHashMap<RefPtrWillBeMember<Node>, OwnPtrWillBeMember<IndexByType>>;
    OwnPtrWillBeMember<ParentMapForType> m_parentMapForType;


    NthIndexData& ensureNthIndexDataFor(Node&);
    NthIndexCache::IndexByType& ensureTypeIndexMap(Node&);

    NthIndexCache::NthIndexData& nthIndexDataWithTagName(Element&);

    RawPtrWillBeMember<Document> m_document;
    uint64_t m_domTreeVersion;
};

inline unsigned NthIndexCache::NthIndexData::nthIndex(Element& element)
{
    if (element.isPseudoElement())
        return 1;
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

inline unsigned NthIndexCache::NthIndexData::nthIndexOfType(Element& element, const QualifiedName& type)
{
    if (element.isPseudoElement())
        return 1;
    if (!m_count)
        return cacheNthIndicesOfType(element, type);
    unsigned index = 0;
    for (Element* sibling = &element; sibling; sibling = ElementTraversal::previousSibling(*sibling, HasTagName(type)), index++) {
        auto it = m_elementIndexMap.find(sibling);
        if (it != m_elementIndexMap.end())
            return it->value + index;
    }
    return index;
}

inline unsigned NthIndexCache::NthIndexData::nthLastIndex(Element& element)
{
    if (element.isPseudoElement())
        return 1;
    unsigned index = nthIndex(element);
    return m_count - index + 1;
}

inline unsigned NthIndexCache::NthIndexData::nthLastIndexOfType(Element& element, const QualifiedName& type)
{
    if (element.isPseudoElement())
        return 1;
    unsigned index = nthIndexOfType(element, type);
    return m_count - index + 1;
}

inline unsigned NthIndexCache::nthChildIndex(Element& element)
{
    ASSERT(element.parentNode());
    return ensureNthIndexDataFor(*element.parentNode()).nthIndex(element);
}

inline unsigned NthIndexCache::nthChildIndexOfType(Element& element, const QualifiedName& type)
{
    ASSERT(element.parentNode());
    return nthIndexDataWithTagName(element).nthIndexOfType(element, type);
}

inline unsigned NthIndexCache::nthLastChildIndex(Element& element)
{
    ASSERT(element.parentNode());
    return ensureNthIndexDataFor(*element.parentNode()).nthLastIndex(element);
}

inline unsigned NthIndexCache::nthLastChildIndexOfType(Element& element, const QualifiedName& type)
{
    ASSERT(element.parentNode());
    return nthIndexDataWithTagName(element).nthLastIndexOfType(element, type);
}


} // namespace blink

#endif // NthIndexCache_h
