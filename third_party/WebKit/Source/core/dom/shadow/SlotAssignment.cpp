// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/shadow/SlotAssignment.h"

#include "core/HTMLNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLSlotElement.h"

namespace blink {

HTMLSlotElement* SlotAssignment::assignedSlotFor(const Node& node) const
{
    return m_assignment.get(const_cast<Node*>(&node));
}

static void detachNotAssignedNode(Node& node)
{
    if (node.layoutObject())
        node.lazyReattachIfAttached();
}

void SlotAssignment::resolveAssignment(ShadowRoot& shadowRoot)
{
    m_assignment.clear();

    using Name2Slot = WillBeHeapHashMap<AtomicString, RefPtrWillBeMember<HTMLSlotElement>>;
    Name2Slot name2slot;
    HTMLSlotElement* defaultSlot = nullptr;

    const WillBeHeapVector<RefPtrWillBeMember<HTMLSlotElement>>& slots = shadowRoot.descendantSlots();

    for (RefPtrWillBeMember<HTMLSlotElement> slot : slots) {
        slot->clearDistribution();
        AtomicString name = slot->fastGetAttribute(HTMLNames::nameAttr);
        if (name.isNull() || name.isEmpty()) {
            if (!defaultSlot)
                defaultSlot = slot.get();
        } else {
            name2slot.add(name, slot.get());
        }
    }

    for (Node& child : NodeTraversal::childrenOf(*shadowRoot.host())) {
        if (child.isElementNode()) {
            if (isActiveInsertionPoint(child)) {
                // TODO(hayato): Support re-distribution across v0 and v1 shadow trees
                detachNotAssignedNode(child);
                continue;
            }
            AtomicString slotName = toElement(child).fastGetAttribute(HTMLNames::slotAttr);
            if (slotName.isNull() || slotName.isEmpty()) {
                if (defaultSlot)
                    assign(child, *defaultSlot);
                else
                    detachNotAssignedNode(child);
            } else {
                HTMLSlotElement* slot = name2slot.get(slotName);
                if (slot)
                    assign(child, *slot);
                else
                    detachNotAssignedNode(child);
            }
        } else if (defaultSlot && child.isTextNode()) {
            assign(child, *defaultSlot);
        } else {
            detachNotAssignedNode(child);
        }
    }

    // Update each slot's distribution in reverse tree order so that a child slot is visited before its parent slot.
    for (auto slot = slots.rbegin(); slot != slots.rend(); ++slot)
        (*slot)->updateDistributedNodesWithFallback();
}

void SlotAssignment::assign(Node& hostChild, HTMLSlotElement& slot)
{
    ASSERT(hostChild.isSlotAssignable());
    m_assignment.add(&hostChild, &slot);
    slot.appendAssignedNode(hostChild);
    if (isHTMLSlotElement(hostChild))
        slot.appendDistributedNodesFrom(toHTMLSlotElement(hostChild));
    else
        slot.appendDistributedNode(hostChild);
}

DEFINE_TRACE(SlotAssignment)
{
#if ENABLE(OILPAN)
    visitor->trace(m_assignment);
#endif
}

} // namespace blink
