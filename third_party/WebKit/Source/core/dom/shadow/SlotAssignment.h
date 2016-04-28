// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SlotAssignment_h
#define SlotAssignment_h

#include "platform/heap/Handle.h"

namespace blink {

class HTMLSlotElement;
class Node;
class ShadowRoot;

class SlotAssignment final : public GarbageCollected<SlotAssignment> {
public:
    static SlotAssignment* create()
    {
        return new SlotAssignment;
    }

    HTMLSlotElement* assignedSlotFor(const Node&) const;
    void resolveAssignment(ShadowRoot&);
    void resolveDistribution(ShadowRoot&);

    void didAddSlot() { ++m_descendantSlotCount; }
    void didRemoveSlot() { DCHECK_GT(m_descendantSlotCount, 0u); --m_descendantSlotCount; }
    unsigned descendantSlotCount() const { return m_descendantSlotCount; }

    const HeapVector<Member<HTMLSlotElement>>& descendantSlots() const { return m_descendantSlots; }

    void setDescendantSlots(HeapVector<Member<HTMLSlotElement>>& slots) { m_descendantSlots.swap(slots); }
    void clearDescendantSlots() { m_descendantSlots.clear(); }

    DECLARE_TRACE();

private:
    SlotAssignment() { };

    void assign(Node&, HTMLSlotElement&);
    void distribute(Node&, HTMLSlotElement&);

    unsigned m_descendantSlotCount = 0;
    HeapVector<Member<HTMLSlotElement>> m_descendantSlots;
    HeapHashMap<Member<Node>, Member<HTMLSlotElement>> m_assignment;
};

} // namespace blink

#endif // HTMLSlotAssignment_h
