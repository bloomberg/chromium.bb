// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleInvalidator_h
#define StyleInvalidator_h

#include "core/css/RuleFeature.h"
#include "heap/Heap.h"

namespace WebCore {

class StyleInvalidator {
    STACK_ALLOCATED();
public:
    explicit StyleInvalidator(Document&);
    void invalidate();

private:
    bool invalidate(Element&);
    bool invalidateChildren(Element&);

    bool checkInvalidationSetsAgainstElement(Element&);

    struct RecursionData {
        RecursionData() : m_foundInvalidationSet(false) { }
        void pushInvalidationSet(const DescendantInvalidationSet&);
        bool matchesCurrentInvalidationSets(Element&);
        bool foundInvalidationSet() { return m_foundInvalidationSet; }

        Vector<AtomicString> m_invalidationClasses;
        Vector<AtomicString> m_invalidationAttributes;
        bool m_foundInvalidationSet;
    };

    class RecursionCheckpoint {
    public:
        RecursionCheckpoint(RecursionData* data)
            : m_prevClassLength(data->m_invalidationClasses.size()),
            m_prevAttributeLength(data->m_invalidationAttributes.size()),
            m_prevFoundInvalidationSet(data->m_foundInvalidationSet),
            m_data(data)
        { }
        ~RecursionCheckpoint()
        {
            m_data->m_invalidationClasses.remove(m_prevClassLength, m_data->m_invalidationClasses.size() - m_prevClassLength);
            m_data->m_invalidationAttributes.remove(m_prevAttributeLength, m_data->m_invalidationAttributes.size() - m_prevAttributeLength);
            m_data->m_foundInvalidationSet = m_prevFoundInvalidationSet;
        }

    private:
        int m_prevClassLength;
        int m_prevAttributeLength;
        bool m_prevFoundInvalidationSet;
        RecursionData* m_data;
    };

    Document& m_document;
    RuleFeatureSet::PendingInvalidationMap& m_pendingInvalidationMap;
    RecursionData m_recursionData;
};

} // namespace WebCore

#endif // StyleInvalidator_h
