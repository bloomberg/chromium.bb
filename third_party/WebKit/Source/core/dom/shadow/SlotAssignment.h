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

class SlotAssignment final : public NoBaseWillBeGarbageCollected<SlotAssignment> {
public:
    static PassOwnPtrWillBeRawPtr<SlotAssignment> create()
    {
        return adoptPtrWillBeNoop(new SlotAssignment);
    }

    HTMLSlotElement* assignedSlotFor(const Node&) const;
    void resolveAssignment(const ShadowRoot&);

    DECLARE_TRACE();

private:
    SlotAssignment() { }

    void assign(Node&, HTMLSlotElement&);
    WillBeHeapHashMap<RawPtrWillBeMember<Node>, RawPtrWillBeMember<HTMLSlotElement>> m_assignment;
};

} // namespace blink

#endif // HTMLSlotAssignment_h
