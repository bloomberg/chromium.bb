// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/shadow/SlotAssignment.h"

#include "core/HTMLNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/Node.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLSlotElement.h"

namespace blink {

void SlotAssignment::DidAddSlot(HTMLSlotElement& slot) {
  // Relevant DOM Standard:
  // https://dom.spec.whatwg.org/#concept-node-insert
  // 6.4:  Run assign slotables for a tree with node's tree and a set containing
  // each inclusive descendant of node that is a slot.

  ++slot_count_;
  needs_collect_slots_ = true;

  if (!slot_map_->Contains(slot.GetName())) {
    slot_map_->Add(slot.GetName(), &slot);
    return;
  }

  HTMLSlotElement& old_active = *FindSlotByName(slot.GetName());
  DCHECK_NE(old_active, slot);
  slot_map_->Add(slot.GetName(), &slot);
  if (FindSlotByName(slot.GetName()) == old_active)
    return;
  // |oldActive| is no longer an active slot.
  if (old_active.FindHostChildWithSameSlotName())
    old_active.DidSlotChange(SlotChangeType::kInitial);
  // TODO(hayato): We should not enqeueue a slotchange event for |oldActive|
  // if |oldActive| was inserted together with |slot|.
  // This could happen if |oldActive| and |slot| are descendants of the inserted
  // node, and |oldActive| is preceding |slot|.
}

void SlotAssignment::SlotRemoved(HTMLSlotElement& slot) {
  DCHECK_GT(slot_count_, 0u);
  --slot_count_;
  needs_collect_slots_ = true;

  DCHECK(slot_map_->Contains(slot.GetName()));
  HTMLSlotElement* old_active = FindSlotByName(slot.GetName());
  slot_map_->Remove(slot.GetName(), &slot);
  HTMLSlotElement* new_active = FindSlotByName(slot.GetName());
  if (new_active && new_active != old_active) {
    // |newActive| slot becomes an active slot.
    if (new_active->FindHostChildWithSameSlotName())
      new_active->DidSlotChange(SlotChangeType::kInitial);
    // TODO(hayato): Prevent a false-positive slotchange.
    // This could happen if more than one slots which have the same name are
    // descendants of the removed node.
  }
}

bool SlotAssignment::FindHostChildBySlotName(
    const AtomicString& slot_name) const {
  // TODO(hayato): Avoid traversing children every time.
  for (Node& child : NodeTraversal::ChildrenOf(owner_->host())) {
    if (!child.IsSlotable())
      continue;
    if (child.SlotName() == slot_name)
      return true;
  }
  return false;
}

void SlotAssignment::SlotRenamed(const AtomicString& old_slot_name,
                                 HTMLSlotElement& slot) {
  // |slot| has already new name. Thus, we can not use
  // slot.hasAssignedNodesSynchronously.
  bool has_assigned_nodes_before = (FindSlotByName(old_slot_name) == &slot) &&
                                   FindHostChildBySlotName(old_slot_name);

  slot_map_->Remove(old_slot_name, &slot);
  slot_map_->Add(slot.GetName(), &slot);

  bool has_assigned_nodes_after = slot.HasAssignedNodesSlow();

  if (has_assigned_nodes_before || has_assigned_nodes_after)
    slot.DidSlotChange(SlotChangeType::kInitial);
}

void SlotAssignment::DidChangeHostChildSlotName(const AtomicString& old_value,
                                                const AtomicString& new_value) {
  if (HTMLSlotElement* slot =
          FindSlotByName(HTMLSlotElement::NormalizeSlotName(old_value))) {
    slot->DidSlotChange(SlotChangeType::kInitial);
    owner_->Owner()->SetNeedsDistributionRecalc();
  }
  if (HTMLSlotElement* slot =
          FindSlotByName(HTMLSlotElement::NormalizeSlotName(new_value))) {
    slot->DidSlotChange(SlotChangeType::kInitial);
    owner_->Owner()->SetNeedsDistributionRecalc();
  }
}

SlotAssignment::SlotAssignment(ShadowRoot& owner)
    : slot_map_(DocumentOrderedMap::Create()),
      owner_(&owner),
      needs_collect_slots_(false),
      slot_count_(0) {
  DCHECK(owner.IsV1());
}

void SlotAssignment::ResolveAssignment() {
  for (Member<HTMLSlotElement> slot : Slots())
    slot->SaveAndClearDistribution();

  for (Node& child : NodeTraversal::ChildrenOf(owner_->host())) {
    if (!child.IsSlotable()) {
      child.LazyReattachIfAttached();
      continue;
    }
    HTMLSlotElement* slot = FindSlotByName(child.SlotName());
    if (slot)
      slot->AppendAssignedNode(child);
    else
      child.LazyReattachIfAttached();
  }
}

void SlotAssignment::ResolveDistribution() {
  ResolveAssignment();
  const HeapVector<Member<HTMLSlotElement>>& slots = this->Slots();

  for (auto slot : slots)
    slot->ResolveDistributedNodes();

  // Update each slot's distribution in reverse tree order so that a child slot
  // is visited before its parent slot.
  for (auto slot = slots.rbegin(); slot != slots.rend(); ++slot) {
    (*slot)->UpdateDistributedNodesWithFallback();
    (*slot)->LazyReattachDistributedNodesIfNeeded();
  }
}

const HeapVector<Member<HTMLSlotElement>>& SlotAssignment::Slots() {
  if (needs_collect_slots_)
    CollectSlots();
  return slots_;
}

HTMLSlotElement* SlotAssignment::FindSlot(const Node& node) {
  return node.IsSlotable() ? FindSlotByName(node.SlotName()) : nullptr;
}

HTMLSlotElement* SlotAssignment::FindSlotByName(const AtomicString& slot_name) {
  return slot_map_->GetSlotByName(slot_name, owner_.Get());
}

void SlotAssignment::CollectSlots() {
  DCHECK(needs_collect_slots_);
  slots_.clear();

  slots_.ReserveCapacity(slot_count_);
  for (HTMLSlotElement& slot :
       Traversal<HTMLSlotElement>::DescendantsOf(*owner_)) {
    slots_.push_back(&slot);
  }
  needs_collect_slots_ = false;
  DCHECK_EQ(slots_.size(), slot_count_);
}

DEFINE_TRACE(SlotAssignment) {
  visitor->Trace(slots_);
  visitor->Trace(slot_map_);
  visitor->Trace(owner_);
}

}  // namespace blink
