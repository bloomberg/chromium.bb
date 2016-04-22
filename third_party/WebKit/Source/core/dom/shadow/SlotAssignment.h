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

    DECLARE_TRACE();

private:
    SlotAssignment() { }

    void assign(Node&, HTMLSlotElement&);
    void distribute(Node&, HTMLSlotElement&);
    HeapHashMap<Member<Node>, Member<HTMLSlotElement>> m_assignment;
};

} // namespace blink

#endif // HTMLSlotAssignment_h
