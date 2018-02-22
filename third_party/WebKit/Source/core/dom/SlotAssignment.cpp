// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/SlotAssignment.h"

#include "core/dom/ElementShadow.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/Node.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/V0InsertionPoint.h"
#include "core/html/HTMLDetailsElement.h"
#include "core/html/HTMLSlotElement.h"
#include "core/html/forms/HTMLOptGroupElement.h"
#include "core/html/forms/HTMLSelectElement.h"
#include "core/html_names.h"

namespace blink {

namespace {
bool ShouldAssignToCustomSlot(const Node& node) {
  if (IsHTMLDetailsElement(node.parentElement()))
    return HTMLDetailsElement::IsFirstSummary(node);
  if (IsHTMLSelectElement(node.parentElement()))
    return HTMLSelectElement::CanAssignToSelectSlot(node);
  if (IsHTMLOptGroupElement(node.parentElement()))
    return HTMLOptGroupElement::CanAssignToOptGroupSlot(node);
  return false;
}
}  // anonymous namespace

void SlotAssignment::DidAddSlot(HTMLSlotElement& slot) {
  // Relevant DOM Standard:
  // https://dom.spec.whatwg.org/#concept-node-insert

  // |slot| was already connected to the tree, however, |slot_map_| doesn't
  // reflect the insertion yet.

  ++slot_count_;
  needs_collect_slots_ = true;

  DCHECK(!slot_map_->Contains(slot.GetName()) ||
         GetCachedFirstSlotWithoutAccessingNodeTree(slot.GetName()));
  DidAddSlotInternal(slot);
  // Ensures that TreeOrderedMap has a cache if there is a slot for the name.
  DCHECK(GetCachedFirstSlotWithoutAccessingNodeTree(slot.GetName()));
}

void SlotAssignment::DidRemoveSlot(HTMLSlotElement& slot) {
  // Relevant DOM Standard:
  // https://dom.spec.whatwg.org/#concept-node-remove

  // |slot| was already removed from the tree, however, |slot_map_| doesn't
  // reflect the removal yet.

  DCHECK_GT(slot_count_, 0u);
  --slot_count_;
  needs_collect_slots_ = true;

  DCHECK(GetCachedFirstSlotWithoutAccessingNodeTree(slot.GetName()));
  DidRemoveSlotInternal(slot, slot.GetName(), SlotMutationType::kRemoved);
  // Ensures that TreeOrderedMap has a cache if there is a slot for the name.
  DCHECK(!slot_map_->Contains(slot.GetName()) ||
         GetCachedFirstSlotWithoutAccessingNodeTree(slot.GetName()));
}

void SlotAssignment::DidAddSlotInternal(HTMLSlotElement& slot) {
  // There are the following 3 cases for addition:
  //         Before:              After:
  // case 1: []                -> [*slot*]
  // case 2: [old_active, ...] -> [*slot*, old_active, ...]
  // case 3: [old_active, ...] -> [old_active, ..., *slot*, ...]

  // TODO(hayato): Explain the details in README.md file.

  const AtomicString& slot_name = slot.GetName();

  // At this timing, we can't use FindSlotByName because what we are interested
  // in is the first slot *before* |slot| was inserted. Here, |slot| was already
  // disconnected from the tree. Thus, we can't use on FindBySlotName because
  // it might scan the current tree and return a wrong result.
  HTMLSlotElement* old_active =
      GetCachedFirstSlotWithoutAccessingNodeTree(slot_name);
  DCHECK(!old_active || old_active != slot);

  // This might invalidate the slot_map's cache.
  slot_map_->Add(slot_name, &slot);

  // This also ensures that TreeOrderedMap has a cache for the first element.
  HTMLSlotElement* new_active = FindSlotByName(slot_name);
  DCHECK(new_active);
  DCHECK(new_active == slot || new_active == old_active);

  if (new_active == slot) {
    // case 1 or 2
    if (FindHostChildBySlotName(slot_name)) {
      // |slot| got assigned nodes
      slot.DidSlotChange(SlotChangeType::kSignalSlotChangeEvent);
      if (old_active) {
        // case 2
        //  |old_active| lost assigned nodes.
        old_active->DidSlotChange(SlotChangeType::kSignalSlotChangeEvent);
      }
    } else {
      // |slot| is active, but it doesn't have assigned nodes.
      // Fallback might matter.
      slot.CheckFallbackAfterInsertedIntoShadowTree();
    }
  } else {
    // case 3
    slot.CheckFallbackAfterInsertedIntoShadowTree();
  }
}

void SlotAssignment::DidRemoveSlotInternal(
    HTMLSlotElement& slot,
    const AtomicString& slot_name,
    SlotMutationType slot_mutation_type) {
  // There are the following 3 cases for removal:
  //         Before:                            After:
  // case 1: [*slot*]                        -> []
  // case 2: [*slot*, new_active, ...]       -> [new_active, ...]
  // case 3: [new_active, ..., *slot*, ...]  -> [new_active, ...]

  // TODO(hayato): Explain the details in README.md file.

  // At this timing, we can't use FindSlotByName because what we are interested
  // in is the first slot *before* |slot| was removed. Here, |slot| was already
  // connected to the tree. Thus, we can't use FindBySlotName because
  // it might scan the current tree and return a wrong result.
  HTMLSlotElement* old_active =
      GetCachedFirstSlotWithoutAccessingNodeTree(slot_name);
  DCHECK(old_active);
  slot_map_->Remove(slot_name, &slot);
  // This also ensures that TreeOrderedMap has a cache for the first element.
  HTMLSlotElement* new_active = FindSlotByName(slot_name);
  DCHECK(!new_active || new_active != slot);

  if (old_active == slot) {
    // case 1 or 2
    if (FindHostChildBySlotName(slot_name)) {
      // |slot| lost assigned nodes
      if (slot_mutation_type == SlotMutationType::kRemoved) {
        slot.DidSlotChangeAfterRemovedFromShadowTree();
      } else {
        slot.DidSlotChangeAfterRenaming();
      }
      if (new_active) {
        // case 2
        // |new_active| got assigned nodes
        new_active->DidSlotChange(SlotChangeType::kSignalSlotChangeEvent);
      }
    } else {
      // |slot| was active, but it didn't have assigned nodes.
      // Fallback might matter.
      slot.CheckFallbackAfterRemovedFromShadowTree();
    }
  } else {
    // case 3
    slot.CheckFallbackAfterRemovedFromShadowTree();
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

void SlotAssignment::DidRenameSlot(const AtomicString& old_slot_name,
                                   HTMLSlotElement& slot) {
  // Rename can be thought as "Remove and then Add", except that
  // we don't need to set needs_collect_slots_.
  DCHECK(GetCachedFirstSlotWithoutAccessingNodeTree(old_slot_name));
  DidRemoveSlotInternal(slot, old_slot_name, SlotMutationType::kRenamed);
  DidAddSlotInternal(slot);
  DCHECK(GetCachedFirstSlotWithoutAccessingNodeTree(slot.GetName()));
}

void SlotAssignment::DidChangeHostChildSlotName(const AtomicString& old_value,
                                                const AtomicString& new_value) {
  if (HTMLSlotElement* slot =
          FindSlotByName(HTMLSlotElement::NormalizeSlotName(old_value))) {
    slot->DidSlotChange(SlotChangeType::kSignalSlotChangeEvent);
  }
  if (HTMLSlotElement* slot =
          FindSlotByName(HTMLSlotElement::NormalizeSlotName(new_value))) {
    slot->DidSlotChange(SlotChangeType::kSignalSlotChangeEvent);
  }
}

SlotAssignment::SlotAssignment(ShadowRoot& owner)
    : slot_map_(TreeOrderedMap::Create()),
      owner_(&owner),
      needs_collect_slots_(false),
      slot_count_(0) {
  DCHECK(owner.IsV1());
}

void SlotAssignment::RecalcAssignmentNg() {
  DCHECK(RuntimeEnabledFeatures::IncrementalShadowDOMEnabled());

  if (!needs_assignment_recalc_)
    return;
  needs_assignment_recalc_ = false;

  for (Member<HTMLSlotElement> slot : Slots())
    slot->ClearAssignedNodes();

  const bool is_user_agent = owner_->IsUserAgent();

  HTMLSlotElement* user_agent_default_slot = nullptr;
  HTMLSlotElement* user_agent_custom_assign_slot = nullptr;
  if (is_user_agent) {
    user_agent_default_slot =
        FindSlotByName(HTMLSlotElement::UserAgentDefaultSlotName());
    user_agent_custom_assign_slot =
        FindSlotByName(HTMLSlotElement::UserAgentCustomAssignSlotName());
  }

  for (Node& child : NodeTraversal::ChildrenOf(owner_->host())) {
    if (!child.IsSlotable())
      continue;

    HTMLSlotElement* slot = nullptr;
    if (!is_user_agent) {
      slot = FindSlotByName(child.SlotName());
    } else {
      if (user_agent_custom_assign_slot && ShouldAssignToCustomSlot(child)) {
        slot = user_agent_custom_assign_slot;
      } else {
        slot = user_agent_default_slot;
      }
    }

    if (slot)
      slot->AppendAssignedNode(child);
  }
}

void SlotAssignment::RecalcAssignment() {
  DCHECK(!RuntimeEnabledFeatures::IncrementalShadowDOMEnabled());

  for (Member<HTMLSlotElement> slot : Slots())
    slot->SaveAndClearDistribution();

  const bool is_user_agent = owner_->IsUserAgent();

  HTMLSlotElement* user_agent_default_slot = nullptr;
  HTMLSlotElement* user_agent_custom_assign_slot = nullptr;
  if (is_user_agent) {
    user_agent_default_slot =
        FindSlotByName(HTMLSlotElement::UserAgentDefaultSlotName());
    user_agent_custom_assign_slot =
        FindSlotByName(HTMLSlotElement::UserAgentCustomAssignSlotName());
  }

  for (Node& child : NodeTraversal::ChildrenOf(owner_->host())) {
    if (!child.IsSlotable()) {
      child.LazyReattachIfAttached();
      continue;
    }

    HTMLSlotElement* slot = nullptr;
    if (!is_user_agent) {
      slot = FindSlotByName(child.SlotName());
    } else {
      if (user_agent_custom_assign_slot && ShouldAssignToCustomSlot(child)) {
        slot = user_agent_custom_assign_slot;
      } else {
        slot = user_agent_default_slot;
      }
    }

    if (slot)
      slot->AppendAssignedNode(child);
    else
      child.LazyReattachIfAttached();
  }
}

void SlotAssignment::RecalcDistribution() {
  DCHECK(!RuntimeEnabledFeatures::IncrementalShadowDOMEnabled());

  RecalcAssignment();
  const HeapVector<Member<HTMLSlotElement>>& slots = Slots();

  for (auto slot : slots)
    slot->RecalcDistributedNodes();

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

HTMLSlotElement* SlotAssignment::FindSlot(const Node& node) const {
  if (!node.IsSlotable())
    return nullptr;
  if (owner_->IsUserAgent())
    return FindSlotInUserAgentShadow(node);
  return FindSlotByName(node.SlotName());
}

HTMLSlotElement* SlotAssignment::FindSlotByName(
    const AtomicString& slot_name) const {
  return slot_map_->GetSlotByName(slot_name, *owner_);
}

HTMLSlotElement* SlotAssignment::FindSlotInUserAgentShadow(
    const Node& node) const {
  HTMLSlotElement* user_agent_custom_assign_slot =
      FindSlotByName(HTMLSlotElement::UserAgentCustomAssignSlotName());
  if (user_agent_custom_assign_slot && ShouldAssignToCustomSlot(node))
    return user_agent_custom_assign_slot;
  HTMLSlotElement* user_agent_default_slot =
      FindSlotByName(HTMLSlotElement::UserAgentDefaultSlotName());
  return user_agent_default_slot;
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

HTMLSlotElement* SlotAssignment::GetCachedFirstSlotWithoutAccessingNodeTree(
    const AtomicString& slot_name) {
  if (Element* slot =
          slot_map_->GetCachedFirstElementWithoutAccessingNodeTree(slot_name)) {
    return ToHTMLSlotElement(slot);
  }
  return nullptr;
}

void SlotAssignment::Trace(blink::Visitor* visitor) {
  visitor->Trace(slots_);
  visitor->Trace(slot_map_);
  visitor->Trace(owner_);
}

}  // namespace blink
