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
            : m_foundInvalidationSet(false)
            , m_invalidateCustomPseudo(false)
        { }
        void pushInvalidationSet(const DescendantInvalidationSet&);
        bool matchesCurrentInvalidationSets(Element&);
        bool foundInvalidationSet() { return m_foundInvalidationSet; }

        Vector<AtomicString> m_invalidationClasses;
        Vector<AtomicString> m_invalidationAttributes;
        Vector<AtomicString> m_invalidationIds;
        Vector<AtomicString> m_invalidationTagNames;
        bool m_foundInvalidationSet;
        bool m_invalidateCustomPseudo;
    };

    class RecursionCheckpoint {
    public:
        RecursionCheckpoint(RecursionData* data)
            : m_prevClassLength(data->m_invalidationClasses.size()),
            m_prevAttributeLength(data->m_invalidationAttributes.size()),
            m_prevIdLength(data->m_invalidationIds.size()),
            m_prevTagNameLength(data->m_invalidationTagNames.size()),
            m_prevFoundInvalidationSet(data->m_foundInvalidationSet),
            m_prevInvalidateCustomPseudo(data->m_invalidateCustomPseudo),
            m_data(data)
        { }
        ~RecursionCheckpoint()
        {
            m_data->m_invalidationClasses.remove(m_prevClassLength, m_data->m_invalidationClasses.size() - m_prevClassLength);
            m_data->m_invalidationAttributes.remove(m_prevAttributeLength, m_data->m_invalidationAttributes.size() - m_prevAttributeLength);
            m_data->m_invalidationIds.remove(m_prevIdLength, m_data->m_invalidationIds.size() - m_prevIdLength);
            m_data->m_invalidationTagNames.remove(m_prevTagNameLength, m_data->m_invalidationTagNames.size() - m_prevTagNameLength);
            m_data->m_foundInvalidationSet = m_prevFoundInvalidationSet;
            m_data->m_invalidateCustomPseudo = m_prevInvalidateCustomPseudo;
        }

    private:
        int m_prevClassLength;
        int m_prevAttributeLength;
        int m_prevIdLength;
        int m_prevTagNameLength;
        bool m_prevFoundInvalidationSet;
        bool m_prevInvalidateCustomPseudo;
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
