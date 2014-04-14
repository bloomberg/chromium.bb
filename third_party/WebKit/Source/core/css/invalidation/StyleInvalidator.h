// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleInvalidator_h
#define StyleInvalidator_h

#include "heap/Handle.h"

namespace WebCore {

class DescendantInvalidationSet;
class Document;
class Element;

class StyleInvalidator {
public:
    StyleInvalidator();
    ~StyleInvalidator();
    void invalidate(Document&);
    void scheduleInvalidation(PassRefPtr<DescendantInvalidationSet>, Element&);

    // Clears all style invalidation state for the passed node.
    void clearInvalidation(Node&);

    void clearPendingInvalidations();

private:
    bool invalidate(Element&);
    bool invalidateChildren(Element&);

    bool checkInvalidationSetsAgainstElement(Element&);

    struct RecursionData {
        RecursionData()
            : m_invalidateCustomPseudo(false)
            , m_wholeSubtreeInvalid(false)
        { }
        void pushInvalidationSet(const DescendantInvalidationSet&);
        bool matchesCurrentInvalidationSets(Element&);
        bool hasInvalidationSets() const { return m_invalidationSets.size(); }
        bool wholeSubtreeInvalid() const { return m_wholeSubtreeInvalid; }

        typedef Vector<const DescendantInvalidationSet*, 16> InvalidationSets;
        InvalidationSets m_invalidationSets;
        bool m_invalidateCustomPseudo;
        bool m_wholeSubtreeInvalid;
    };

    class RecursionCheckpoint {
    public:
        RecursionCheckpoint(RecursionData* data)
            : m_prevInvalidationSetsSize(data->m_invalidationSets.size())
            , m_prevInvalidateCustomPseudo(data->m_invalidateCustomPseudo)
            , m_prevWholeSubtreeInvalid(data->m_wholeSubtreeInvalid)
            , m_data(data)
        { }
        ~RecursionCheckpoint()
        {
            m_data->m_invalidationSets.remove(m_prevInvalidationSetsSize, m_data->m_invalidationSets.size() - m_prevInvalidationSetsSize);
            m_data->m_invalidateCustomPseudo = m_prevInvalidateCustomPseudo;
            m_data->m_wholeSubtreeInvalid = m_prevWholeSubtreeInvalid;
        }

    private:
        int m_prevInvalidationSetsSize;
        bool m_prevInvalidateCustomPseudo;
        bool m_prevWholeSubtreeInvalid;
        RecursionData* m_data;
    };

    typedef Vector<RefPtr<DescendantInvalidationSet> > InvalidationList;
    typedef HashMap<Element*, OwnPtr<InvalidationList> > PendingInvalidationMap;

    InvalidationList& ensurePendingInvalidationList(Element&);

    PendingInvalidationMap m_pendingInvalidationMap;
    RecursionData m_recursionData;
};

} // namespace WebCore

#endif // StyleInvalidator_h
