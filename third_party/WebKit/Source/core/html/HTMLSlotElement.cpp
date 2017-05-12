/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
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

#include "core/html/HTMLSlotElement.h"

#include "core/HTMLNames.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/SlotAssignment.h"
#include "core/events/Event.h"
#include "core/frame/UseCounter.h"
#include "core/html/AssignedNodesOptions.h"
#include "core/probe/CoreProbes.h"
#include "platform/bindings/Microtask.h"

namespace blink {

using namespace HTMLNames;

inline HTMLSlotElement::HTMLSlotElement(Document& document)
    : HTMLElement(slotTag, document) {
  UseCounter::Count(document, UseCounter::kHTMLSlotElement);
  SetHasCustomStyleCallbacks();
}

DEFINE_NODE_FACTORY(HTMLSlotElement);

// static
AtomicString HTMLSlotElement::NormalizeSlotName(const AtomicString& name) {
  return (name.IsNull() || name.IsEmpty()) ? g_empty_atom : name;
}

const HeapVector<Member<Node>>& HTMLSlotElement::AssignedNodes() {
  DCHECK(!NeedsDistributionRecalc());
  DCHECK(IsInShadowTree() || assigned_nodes_.IsEmpty());
  return assigned_nodes_;
}

const HeapVector<Member<Node>> HTMLSlotElement::assignedNodesForBinding(
    const AssignedNodesOptions& options) {
  UpdateDistribution();
  if (options.hasFlatten() && options.flatten())
    return GetDistributedNodesForBinding();
  return assigned_nodes_;
}

const HeapVector<Member<Node>>
HTMLSlotElement::GetDistributedNodesForBinding() {
  DCHECK(!NeedsDistributionRecalc());
  if (SupportsDistribution())
    return distributed_nodes_;

  // If a slot does not support distribution, its m_distributedNodes should not
  // be used.  Instead, calculate distribution manually here. This happens only
  // in a slot in non-shadow trees, so its assigned nodes are always empty.
  HeapVector<Member<Node>> distributed_nodes;
  Node* child = NodeTraversal::FirstChild(*this);
  while (child) {
    if (!child->IsSlotable()) {
      child = NodeTraversal::NextSkippingChildren(*child, this);
      continue;
    }
    if (isHTMLSlotElement(child)) {
      child = NodeTraversal::Next(*child, this);
    } else {
      distributed_nodes.push_back(child);
      child = NodeTraversal::NextSkippingChildren(*child, this);
    }
  }
  return distributed_nodes;
}

const HeapVector<Member<Node>>& HTMLSlotElement::GetDistributedNodes() {
  DCHECK(!NeedsDistributionRecalc());
  DCHECK(SupportsDistribution() || distributed_nodes_.IsEmpty());
  return distributed_nodes_;
}

void HTMLSlotElement::AppendAssignedNode(Node& host_child) {
  DCHECK(host_child.IsSlotable());
  assigned_nodes_.push_back(&host_child);
}

void HTMLSlotElement::ResolveDistributedNodes() {
  for (auto& node : assigned_nodes_) {
    DCHECK(node->IsSlotable());
    if (isHTMLSlotElement(*node))
      AppendDistributedNodesFrom(toHTMLSlotElement(*node));
    else
      AppendDistributedNode(*node);

    if (IsChildOfV1ShadowHost())
      ParentElementShadow()->SetNeedsDistributionRecalc();
  }
}

void HTMLSlotElement::AppendDistributedNode(Node& node) {
  size_t size = distributed_nodes_.size();
  distributed_nodes_.push_back(&node);
  distributed_indices_.Set(&node, size);
}

void HTMLSlotElement::AppendDistributedNodesFrom(const HTMLSlotElement& other) {
  size_t index = distributed_nodes_.size();
  distributed_nodes_.AppendVector(other.distributed_nodes_);
  for (const auto& node : other.distributed_nodes_)
    distributed_indices_.Set(node.Get(), index++);
}

void HTMLSlotElement::ClearDistribution() {
  // TODO(hayato): Figure out when to call
  // lazyReattachDistributedNodesIfNeeded()
  assigned_nodes_.clear();
  distributed_nodes_.clear();
  distributed_indices_.clear();
}

void HTMLSlotElement::SaveAndClearDistribution() {
  old_distributed_nodes_.swap(distributed_nodes_);
  ClearDistribution();
}

void HTMLSlotElement::DispatchSlotChangeEvent() {
  Event* event = Event::CreateBubble(EventTypeNames::slotchange);
  event->SetTarget(this);
  DispatchScopedEvent(event);
}

Node* HTMLSlotElement::DistributedNodeNextTo(const Node& node) const {
  DCHECK(SupportsDistribution());
  const auto& it = distributed_indices_.find(&node);
  if (it == distributed_indices_.end())
    return nullptr;
  size_t index = it->value;
  if (index + 1 == distributed_nodes_.size())
    return nullptr;
  return distributed_nodes_[index + 1].Get();
}

Node* HTMLSlotElement::DistributedNodePreviousTo(const Node& node) const {
  DCHECK(SupportsDistribution());
  const auto& it = distributed_indices_.find(&node);
  if (it == distributed_indices_.end())
    return nullptr;
  size_t index = it->value;
  if (index == 0)
    return nullptr;
  return distributed_nodes_[index - 1].Get();
}

AtomicString HTMLSlotElement::GetName() const {
  return NormalizeSlotName(FastGetAttribute(HTMLNames::nameAttr));
}

void HTMLSlotElement::AttachLayoutTree(const AttachContext& context) {
  if (SupportsDistribution()) {
    for (auto& node : distributed_nodes_) {
      if (node->NeedsAttach())
        node->AttachLayoutTree(context);
    }
  }
  HTMLElement::AttachLayoutTree(context);
}

void HTMLSlotElement::DetachLayoutTree(const AttachContext& context) {
  if (SupportsDistribution()) {
    for (auto& node : distributed_nodes_)
      node->LazyReattachIfAttached();
  }
  HTMLElement::DetachLayoutTree(context);
}

void HTMLSlotElement::RebuildDistributedChildrenLayoutTrees() {
  if (!SupportsDistribution())
    return;
  Text* next_text_sibling = nullptr;
  // This loop traverses the nodes from right to left for the same reason as the
  // one described in ContainerNode::RebuildChildrenLayoutTrees().
  for (auto it = distributed_nodes_.rbegin(); it != distributed_nodes_.rend();
       ++it) {
    RebuildLayoutTreeForChild(*it, next_text_sibling);
  }
}

void HTMLSlotElement::AttributeChanged(
    const AttributeModificationParams& params) {
  if (params.name == nameAttr) {
    if (ShadowRoot* root = ContainingShadowRoot()) {
      if (root->IsV1() && params.old_value != params.new_value) {
        root->GetSlotAssignment().SlotRenamed(
            NormalizeSlotName(params.old_value), *this);
      }
    }
  }
  HTMLElement::AttributeChanged(params);
}

static bool WasInDifferentShadowTreeBeforeInserted(
    HTMLSlotElement& slot,
    ContainerNode& insertion_point) {
  ShadowRoot* root1 = slot.ContainingShadowRoot();
  ShadowRoot* root2 = insertion_point.ContainingShadowRoot();
  if (root1 && root2 && root1 == root2)
    return false;
  return root1;
}

Node::InsertionNotificationRequest HTMLSlotElement::InsertedInto(
    ContainerNode* insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  ShadowRoot* root = ContainingShadowRoot();
  if (root) {
    DCHECK(root->Owner());
    root->Owner()->SetNeedsDistributionRecalc();
    // Relevant DOM Standard: https://dom.spec.whatwg.org/#concept-node-insert
    // - 6.4:  Run assign slotables for a tree with node's tree and a set
    // containing each inclusive descendant of node that is a slot.
    if (root->IsV1() &&
        !WasInDifferentShadowTreeBeforeInserted(*this, *insertion_point))
      root->DidAddSlot(*this);
  }

  // We could have been distributed into in a detached subtree, make sure to
  // clear the distribution when inserted again to avoid cycles.
  ClearDistribution();

  return kInsertionDone;
}

static ShadowRoot* ContainingShadowRootBeforeRemoved(
    Node& removed_descendant,
    ContainerNode& insertion_point) {
  if (ShadowRoot* root = removed_descendant.ContainingShadowRoot())
    return root;
  return insertion_point.ContainingShadowRoot();
}

void HTMLSlotElement::RemovedFrom(ContainerNode* insertion_point) {
  // `removedFrom` is called after the node is removed from the tree.
  // That means:
  // 1. If this slot is still in a tree scope, it means the slot has been in a
  // shadow tree. An inclusive shadow-including ancestor of the shadow host was
  // originally removed from its parent.
  // 2. Or (this slot is now not in a tree scope), this slot's inclusive
  // ancestor was orginally removed from its parent (== insertion point). This
  // slot and the originally removed node was in the same tree.

  ShadowRoot* root = ContainingShadowRootBeforeRemoved(*this, *insertion_point);
  if (root) {
    if (ElementShadow* root_owner = root->Owner())
      root_owner->SetNeedsDistributionRecalc();
  }

  // Since this insertion point is no longer visible from the shadow subtree, it
  // need to clean itself up.
  ClearDistribution();

  if (root && root->IsV1() &&
      root == insertion_point->GetTreeScope().RootNode()) {
    // This slot was in a shadow tree and got disconnected from the shadow root.
    root->GetSlotAssignment().SlotRemoved(*this);
  }

  HTMLElement::RemovedFrom(insertion_point);
}

void HTMLSlotElement::WillRecalcStyle(StyleRecalcChange change) {
  if (change < kIndependentInherit &&
      GetStyleChangeType() < kSubtreeStyleChange)
    return;

  for (auto& node : distributed_nodes_)
    node->SetNeedsStyleRecalc(
        kLocalStyleChange,
        StyleChangeReasonForTracing::Create(
            StyleChangeReason::kPropagateInheritChangeToDistributedNodes));
}

void HTMLSlotElement::UpdateDistributedNodesWithFallback() {
  if (!distributed_nodes_.IsEmpty())
    return;
  for (auto& child : NodeTraversal::ChildrenOf(*this)) {
    if (!child.IsSlotable())
      continue;
    if (isHTMLSlotElement(child))
      AppendDistributedNodesFrom(toHTMLSlotElement(child));
    else
      AppendDistributedNode(child);
  }
}

void HTMLSlotElement::LazyReattachDistributedNodesIfNeeded() {
  // TODO(hayato): Figure out an exact condition where reattach is required
  if (old_distributed_nodes_ != distributed_nodes_) {
    for (auto& node : old_distributed_nodes_)
      node->LazyReattachIfAttached();
    for (auto& node : distributed_nodes_)
      node->LazyReattachIfAttached();
    probe::didPerformSlotDistribution(this);
  }
  old_distributed_nodes_.clear();
}

void HTMLSlotElement::DidSlotChange(SlotChangeType slot_change_type) {
  if (slot_change_type == SlotChangeType::kInitial)
    EnqueueSlotChangeEvent();
  ShadowRoot* root = ContainingShadowRoot();
  // TODO(hayato): Relax this check if slots in non-shadow trees are well
  // supported.
  DCHECK(root);
  DCHECK(root->IsV1());
  root->Owner()->SetNeedsDistributionRecalc();
  // Check slotchange recursively since this slotchange may cause another
  // slotchange.
  CheckSlotChange(SlotChangeType::kChained);
}

void HTMLSlotElement::EnqueueSlotChangeEvent() {
  if (slotchange_event_enqueued_)
    return;
  MutationObserver::EnqueueSlotChange(*this);
  slotchange_event_enqueued_ = true;
}

bool HTMLSlotElement::HasAssignedNodesSlow() const {
  ShadowRoot* root = ContainingShadowRoot();
  DCHECK(root);
  DCHECK(root->IsV1());
  SlotAssignment& assignment = root->GetSlotAssignment();
  if (assignment.FindSlotByName(GetName()) != this)
    return false;
  return assignment.FindHostChildBySlotName(GetName());
}

bool HTMLSlotElement::FindHostChildWithSameSlotName() const {
  ShadowRoot* root = ContainingShadowRoot();
  DCHECK(root);
  DCHECK(root->IsV1());
  SlotAssignment& assignment = root->GetSlotAssignment();
  return assignment.FindHostChildBySlotName(GetName());
}

int HTMLSlotElement::tabIndex() const {
  return Element::tabIndex();
}

DEFINE_TRACE(HTMLSlotElement) {
  visitor->Trace(assigned_nodes_);
  visitor->Trace(distributed_nodes_);
  visitor->Trace(old_distributed_nodes_);
  visitor->Trace(distributed_indices_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
