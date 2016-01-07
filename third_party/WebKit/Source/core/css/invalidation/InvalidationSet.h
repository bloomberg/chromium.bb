/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef InvalidationSet_h
#define InvalidationSet_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/StringHash.h"

namespace blink {

class DescendantInvalidationSet;
class Element;
class TracedValue;

enum InvalidationType {
    InvalidateDescendants,
    InvalidateSiblings
};

// Tracks data to determine which descendants in a DOM subtree, or
// siblings and their descendants, need to have style recalculated.
class CORE_EXPORT InvalidationSet : public RefCounted<InvalidationSet> {
    WTF_MAKE_NONCOPYABLE(InvalidationSet);
    USING_FAST_MALLOC_WITH_TYPE_NAME(blink::InvalidationSet);
public:
    bool isDescendantInvalidationSet() const { return m_type == InvalidateDescendants; }
    bool isSiblingInvalidationSet() const { return m_type == InvalidateSiblings; }

    static void cacheTracingFlag();

    bool invalidatesElement(Element&) const;

    void addClass(const AtomicString& className);
    void addId(const AtomicString& id);
    void addTagName(const AtomicString& tagName);
    void addAttribute(const AtomicString& attributeLocalName);

    void setWholeSubtreeInvalid();
    bool wholeSubtreeInvalid() const { return m_allDescendantsMightBeInvalid; }

    void setInvalidatesSelf() { m_invalidatesSelf = true; }
    bool invalidatesSelf() const { return m_invalidatesSelf; }

    void setTreeBoundaryCrossing() { m_treeBoundaryCrossing = true; }
    bool treeBoundaryCrossing() const { return m_treeBoundaryCrossing; }

    void setInsertionPointCrossing() { m_insertionPointCrossing = true; }
    bool insertionPointCrossing() const { return m_insertionPointCrossing; }

    void setCustomPseudoInvalid() { m_customPseudoInvalid = true; }
    bool customPseudoInvalid() const { return m_customPseudoInvalid; }

    bool isEmpty() const { return !m_classes && !m_ids && !m_tagNames && !m_attributes && !m_customPseudoInvalid && !m_insertionPointCrossing; }

    void toTracedValue(TracedValue*) const;

#ifndef NDEBUG
    void show() const;
#endif

    const HashSet<AtomicString>& classSetForTesting() const { ASSERT(m_classes); return *m_classes; }
    const HashSet<AtomicString>& idSetForTesting() const { ASSERT(m_ids); return *m_ids; }
    const HashSet<AtomicString>& tagNameSetForTesting() const { ASSERT(m_tagNames); return *m_tagNames; }
    const HashSet<AtomicString>& attributeSetForTesting() const { ASSERT(m_attributes); return *m_attributes; }

    void deref()
    {
        if (!derefBase())
            return;
        destroy();
    }

protected:
    InvalidationSet(InvalidationType);

    void combine(const InvalidationSet& other);

private:
    void destroy();

    HashSet<AtomicString>& ensureClassSet();
    HashSet<AtomicString>& ensureIdSet();
    HashSet<AtomicString>& ensureTagNameSet();
    HashSet<AtomicString>& ensureAttributeSet();

    // FIXME: optimize this if it becomes a memory issue.
    OwnPtr<HashSet<AtomicString>> m_classes;
    OwnPtr<HashSet<AtomicString>> m_ids;
    OwnPtr<HashSet<AtomicString>> m_tagNames;
    OwnPtr<HashSet<AtomicString>> m_attributes;

    unsigned m_type : 1;

    // If true, all descendants might be invalidated, so a full subtree recalc is required.
    unsigned m_allDescendantsMightBeInvalid : 1;

    // If true, the element itself is invalid.
    unsigned m_invalidatesSelf : 1;

    // If true, all descendants which are custom pseudo elements must be invalidated.
    unsigned m_customPseudoInvalid : 1;

    // If true, the invalidation must traverse into ShadowRoots with this set.
    unsigned m_treeBoundaryCrossing : 1;

    // If true, insertion point descendants must be invalidated.
    unsigned m_insertionPointCrossing : 1;
};

class CORE_EXPORT DescendantInvalidationSet final : public InvalidationSet {
public:
    static PassRefPtr<DescendantInvalidationSet> create()
    {
        return adoptRef(new DescendantInvalidationSet);
    }

    void combine(const DescendantInvalidationSet& other)
    {
        InvalidationSet::combine(other);
    }

private:
    DescendantInvalidationSet()
        : InvalidationSet(InvalidateDescendants) {}
};

class CORE_EXPORT SiblingInvalidationSet final : public InvalidationSet {
public:
    static PassRefPtr<SiblingInvalidationSet> create()
    {
        return adoptRef(new SiblingInvalidationSet);
    }

    void combine(const SiblingInvalidationSet& other);

    DescendantInvalidationSet& descendants() { return *m_descendantInvalidationSet; }
    const DescendantInvalidationSet& descendants() const { return *m_descendantInvalidationSet; }

    unsigned maxDirectAdjacentSelectors() const { return m_maxDirectAdjacentSelectors; }
    void updateMaxDirectAdjacentSelectors(unsigned value) { m_maxDirectAdjacentSelectors = std::max(value, m_maxDirectAdjacentSelectors); }

private:
    SiblingInvalidationSet();

    // Indicates the maximum possible number of siblings affected.
    unsigned m_maxDirectAdjacentSelectors;

    // Indicates the descendants of siblings.
    RefPtr<DescendantInvalidationSet> m_descendantInvalidationSet;
};

using InvalidationSetVector = Vector<RefPtr<InvalidationSet>>;

struct InvalidationLists {
    InvalidationSetVector descendants;
    InvalidationSetVector siblings;
};

DEFINE_TYPE_CASTS(DescendantInvalidationSet, InvalidationSet, value, value->isDescendantInvalidationSet(), value.isDescendantInvalidationSet());
DEFINE_TYPE_CASTS(SiblingInvalidationSet, InvalidationSet, value, value->isSiblingInvalidationSet(), value.isSiblingInvalidationSet());

} // namespace blink

#endif // InvalidationSet_h
