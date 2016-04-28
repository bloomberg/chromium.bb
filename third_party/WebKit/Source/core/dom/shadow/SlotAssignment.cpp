// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/shadow/SlotAssignment.h"

#include "core/HTMLNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/shadow/ElementShadow.h"
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

    using Name2Slot = HeapHashMap<AtomicString, Member<HTMLSlotElement>>;
    Name2Slot name2slot;

    const HeapVector<Member<HTMLSlotElement>>& slots = shadowRoot.descendantSlots();

    for (Member<HTMLSlotElement> slot : slots) {
        slot->willUpdateAssignment();
        slot->willUpdateFallback();
        name2slot.add(slot->name(), slot.get());
    }

    for (Node& child : NodeTraversal::childrenOf(*shadowRoot.host())) {
        if (child.isInsertionPoint()) {
            // A re-distribution across v0 and v1 shadow trees is not supported.
            detachNotAssignedNode(child);
            continue;
        }
        if (!child.slottable()) {
            detachNotAssignedNode(child);
            continue;
        }
        AtomicString slotName = child.slotName();
        HTMLSlotElement* slot = name2slot.get(slotName);
        if (slot)
            assign(child, *slot);
        else
            detachNotAssignedNode(child);
    }

    for (auto slot = slots.rbegin(); slot != slots.rend(); ++slot)
        (*slot)->updateFallbackNodes();

    // For each slot, check if assigned nodes have changed
    // If so, call fireSlotchange function
    for (const auto& slot : slots)
        slot->didUpdateAssignment();
}

void SlotAssignment::resolveDistribution(ShadowRoot& shadowRoot)
{
    const HeapVector<Member<HTMLSlotElement>>& slots = shadowRoot.descendantSlots();
    for (Member<HTMLSlotElement> slot : slots) {
        slot->willUpdateDistribution();
    }

    for (auto slot : slots) {
        for (auto node : slot->assignedNodes())
            distribute(*node, *slot);
    }

    // Update each slot's distribution in reverse tree order so that a child slot is visited before its parent slot.
    for (auto slot = slots.rbegin(); slot != slots.rend(); ++slot)
        (*slot)->updateDistributedNodesWithFallback();
    for (const auto& slot : slots)
        slot->didUpdateDistribution();
}

void SlotAssignment::assign(Node& hostChild, HTMLSlotElement& slot)
{
    DCHECK(hostChild.isSlotAssignable());
    m_assignment.add(&hostChild, &slot);
    slot.appendAssignedNode(hostChild);
}

void SlotAssignment::distribute(Node& hostChild, HTMLSlotElement& slot)
{
    DCHECK(hostChild.isSlotAssignable());
    if (isHTMLSlotElement(hostChild))
        slot.appendDistributedNodesFrom(toHTMLSlotElement(hostChild));
    else
        slot.appendDistributedNode(hostChild);

    if (slot.isChildOfV1ShadowHost())
        slot.parentElementShadow()->setNeedsDistributionRecalc();
}

DEFINE_TRACE(SlotAssignment)
{
    visitor->trace(m_descendantSlots);
    visitor->trace(m_assignment);
}

} // namespace blink
