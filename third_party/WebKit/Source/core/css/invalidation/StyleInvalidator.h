// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleInvalidator_h
#define StyleInvalidator_h

#include "core/css/invalidation/PendingInvalidations.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class Document;
class Element;
class InvalidationSet;


class StyleInvalidator {
    DISALLOW_NEW();
    WTF_MAKE_NONCOPYABLE(StyleInvalidator);
public:
    StyleInvalidator();
    ~StyleInvalidator();
    void invalidate(Document&);
    void scheduleInvalidationSetsForElement(const InvalidationLists&, Element&);
    void clearInvalidation(Element&);

    DECLARE_TRACE();

private:
    struct RecursionData {
        RecursionData()
            : m_invalidateCustomPseudo(false)
            , m_wholeSubtreeInvalid(false)
            , m_treeBoundaryCrossing(false)
            , m_insertionPointCrossing(false)
        { }

        void pushInvalidationSet(const DescendantInvalidationSet&);
        bool matchesCurrentInvalidationSets(Element&) const;
        bool hasInvalidationSets() const { return !wholeSubtreeInvalid() && m_invalidationSets.size(); }

        bool wholeSubtreeInvalid() const { return m_wholeSubtreeInvalid; }
        void setWholeSubtreeInvalid() { m_wholeSubtreeInvalid = true; }

        bool treeBoundaryCrossing() const { return m_treeBoundaryCrossing; }
        bool insertionPointCrossing() const { return m_insertionPointCrossing; }

        using DescendantInvalidationSets = Vector<const DescendantInvalidationSet*, 16>;
        DescendantInvalidationSets m_invalidationSets;
        bool m_invalidateCustomPseudo;
        bool m_wholeSubtreeInvalid;
        bool m_treeBoundaryCrossing;
        bool m_insertionPointCrossing;
    };

    class SiblingData {
        STACK_ALLOCATED();
    public:
        SiblingData()
            : m_elementIndex(0)
        { }

        void pushInvalidationSet(const SiblingInvalidationSet&);
        bool matchCurrentInvalidationSets(Element&, RecursionData&) const;

        bool isEmpty() const { return m_invalidationEntries.isEmpty(); }
        void advance() { m_elementIndex++; }

    private:
        struct Entry {
            DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
            Entry(const SiblingInvalidationSet* invalidationSet, unsigned invalidationLimit)
                : m_invalidationSet(invalidationSet)
                , m_invalidationLimit(invalidationLimit)
            {}

            const SiblingInvalidationSet* m_invalidationSet;
            unsigned m_invalidationLimit;
        };

        mutable Vector<Entry, 16> m_invalidationEntries;
        unsigned m_elementIndex;
    };

    bool invalidate(Element&, RecursionData&, SiblingData&);
    bool invalidateShadowRootChildren(Element&, RecursionData&);
    bool invalidateChildren(Element&, RecursionData&);
    bool checkInvalidationSetsAgainstElement(Element&, RecursionData&, SiblingData&);
    void pushInvalidationSetsForElement(Element&, RecursionData&, SiblingData&);

    class RecursionCheckpoint {
    public:
        RecursionCheckpoint(RecursionData* data)
            : m_prevInvalidationSetsSize(data->m_invalidationSets.size())
            , m_prevInvalidateCustomPseudo(data->m_invalidateCustomPseudo)
            , m_prevWholeSubtreeInvalid(data->m_wholeSubtreeInvalid)
            , m_treeBoundaryCrossing(data->m_treeBoundaryCrossing)
            , m_insertionPointCrossing(data->m_insertionPointCrossing)
            , m_data(data)
        { }
        ~RecursionCheckpoint()
        {
            m_data->m_invalidationSets.remove(m_prevInvalidationSetsSize, m_data->m_invalidationSets.size() - m_prevInvalidationSetsSize);
            m_data->m_invalidateCustomPseudo = m_prevInvalidateCustomPseudo;
            m_data->m_wholeSubtreeInvalid = m_prevWholeSubtreeInvalid;
            m_data->m_treeBoundaryCrossing = m_treeBoundaryCrossing;
            m_data->m_insertionPointCrossing = m_insertionPointCrossing;
        }

    private:
        int m_prevInvalidationSetsSize;
        bool m_prevInvalidateCustomPseudo;
        bool m_prevWholeSubtreeInvalid;
        bool m_treeBoundaryCrossing;
        bool m_insertionPointCrossing;
        RecursionData* m_data;
    };

    using PendingInvalidationMap = WillBeHeapHashMap<RawPtrWillBeMember<Element>, OwnPtr<PendingInvalidations>>;

    PendingInvalidations& ensurePendingInvalidations(Element&);

    PendingInvalidationMap m_pendingInvalidationMap;
};

} // namespace blink

#endif // StyleInvalidator_h
