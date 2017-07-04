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

#include <array>
#include "core/HTMLNames.h"
#include "core/dom/ElementShadow.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/SlotAssignment.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/V0InsertionPoint.h"
#include "core/dom/WhitespaceAttacher.h"
#include "core/events/Event.h"
#include "core/frame/UseCounter.h"
#include "core/html/AssignedNodesOptions.h"
#include "core/probe/CoreProbes.h"
#include "platform/bindings/Microtask.h"

namespace blink {

using namespace HTMLNames;

namespace {
constexpr size_t kLCSTableSizeLimit = 16;
}

inline HTMLSlotElement::HTMLSlotElement(Document& document)
    : HTMLElement(slotTag, document) {
  UseCounter::Count(document, WebFeature::kHTMLSlotElement);
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
    return GetDistributedNodes();
  return assigned_nodes_;
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
    if (isHTMLSlotElement(*node) &&
        toHTMLSlotElement(*node).SupportsDistribution())
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

void HTMLSlotElement::AttachLayoutTree(AttachContext& context) {
  if (SupportsDistribution()) {
    AttachContext children_context(context);
    children_context.resolved_style = nullptr;

    for (auto& node : distributed_nodes_) {
      if (node->NeedsAttach())
        node->AttachLayoutTree(children_context);
    }
    if (children_context.previous_in_flow)
      context.previous_in_flow = children_context.previous_in_flow;
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

void HTMLSlotElement::RebuildDistributedChildrenLayoutTrees(
    WhitespaceAttacher& whitespace_attacher) {
  if (!SupportsDistribution())
    return;
  // This loop traverses the nodes from right to left for the same reason as the
  // one described in ContainerNode::RebuildChildrenLayoutTrees().
  for (auto it = distributed_nodes_.rbegin(); it != distributed_nodes_.rend();
       ++it) {
    RebuildLayoutTreeForChild(*it, whitespace_attacher);
  }
}

void HTMLSlotElement::AttributeChanged(
    const AttributeModificationParams& params) {
  if (params.name == nameAttr) {
    if (ShadowRoot* root = ContainingShadowRoot()) {
      if (root->IsV1() && params.old_value != params.new_value) {
        root->GetSlotAssignment().DidRenameSlot(
            NormalizeSlotName(params.old_value), *this);
      }
    }
  }
  HTMLElement::AttributeChanged(params);
}

Node::InsertionNotificationRequest HTMLSlotElement::InsertedInto(
    ContainerNode* insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  if (SupportsDistribution()) {
    ShadowRoot* root = ContainingShadowRoot();
    DCHECK(root);
    DCHECK(root->IsV1());
    DCHECK(root->Owner());
    if (root == insertion_point->ContainingShadowRoot()) {
      // This slot is inserted into the same tree of |insertion_point|
      root->DidAddSlot(*this);
    }
  }
  return kInsertionDone;
}

void HTMLSlotElement::RemovedFrom(ContainerNode* insertion_point) {
  // `removedFrom` is called after the node is removed from the tree.
  // That means:
  // 1. If this slot is still in a tree scope, it means the slot has been in a
  // shadow tree. An inclusive shadow-including ancestor of the shadow host was
  // originally removed from its parent.
  // 2. Or (this slot is not in a tree scope), this slot's inclusive
  // ancestor was orginally removed from its parent (== insertion point). This
  // slot and the originally removed node was in the same tree before removal.

  // For exmaple, given the following trees, (srN: = shadow root, sN: = slot)
  // a
  // |- b --sr1
  // |- c   |--d
  //           |- e-----sr2
  //              |- s1 |--f
  //                    |--s2

  // If we call 'e.remove()', then:
  // - For slot s1, s1.removedFrom(d) is called.
  // - For slot s2, s2.removedFrom(d) is called.

  // ContainingShadowRoot() is okay to use here because 1) It doesn't use
  // kIsInShadowTreeFlag flag, and 2) TreeScope has been alreay updated for the
  // slot.
  if (insertion_point->IsInV1ShadowTree() && !ContainingShadowRoot()) {
    // This slot was in a shadow tree and got disconnected from the shadow tree
    insertion_point->ContainingShadowRoot()->GetSlotAssignment().DidRemoveSlot(
        *this);
    ClearDistribution();
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

void HTMLSlotElement::LazyReattachDistributedNodesByDynamicProgramming(
    const HeapVector<Member<Node>>& nodes1,
    const HeapVector<Member<Node>>& nodes2) {
  // Use dynamic programming to minimize the number of nodes being reattached.
  using LCSTable =
      std::array<std::array<size_t, kLCSTableSizeLimit>, kLCSTableSizeLimit>;
  using Backtrack = std::pair<size_t, size_t>;
  using BacktrackTable =
      std::array<std::array<Backtrack, kLCSTableSizeLimit>, kLCSTableSizeLimit>;

  DEFINE_STATIC_LOCAL(LCSTable*, lcs_table, (new LCSTable));
  DEFINE_STATIC_LOCAL(BacktrackTable*, backtrack_table, (new BacktrackTable));

  FillLongestCommonSubsequenceDynamicProgrammingTable(
      nodes1, nodes2, *lcs_table, *backtrack_table);

  size_t r = nodes1.size();
  size_t c = nodes2.size();
  while (r > 0 && c > 0) {
    Backtrack backtrack = (*backtrack_table)[r][c];
    if (backtrack == std::make_pair(r - 1, c - 1)) {
      DCHECK_EQ(nodes1[r - 1], nodes2[c - 1]);
    } else if (backtrack == std::make_pair(r - 1, c)) {
      nodes1[r - 1]->LazyReattachIfAttached();
    } else {
      DCHECK(backtrack == std::make_pair(r, c - 1));
      nodes2[c - 1]->LazyReattachIfAttached();
    }
    std::tie(r, c) = backtrack;
  }
  if (r > 0) {
    for (size_t i = 0; i < r; ++i)
      nodes1[i]->LazyReattachIfAttached();
  } else if (c > 0) {
    for (size_t i = 0; i < c; ++i)
      nodes2[i]->LazyReattachIfAttached();
  }
}

void HTMLSlotElement::LazyReattachDistributedNodesIfNeeded() {
  if (old_distributed_nodes_ == distributed_nodes_)
    return;
  probe::didPerformSlotDistribution(this);

  if (old_distributed_nodes_.size() + 1 > kLCSTableSizeLimit ||
      distributed_nodes_.size() + 1 > kLCSTableSizeLimit) {
    // Since DP takes O(N^2), we don't use DP if the size is larger than the
    // pre-defined limit.
    LazyReattachDistributedNodesNaive();
  } else {
    LazyReattachDistributedNodesByDynamicProgramming(old_distributed_nodes_,
                                                     distributed_nodes_);
  }
  old_distributed_nodes_.clear();
}

void HTMLSlotElement::DidSlotChangeAfterRemovedFromShadowTree() {
  DCHECK(!ContainingShadowRoot());
  EnqueueSlotChangeEvent();
  CheckSlotChange(SlotChangeType::kSuppressSlotChangeEvent);
}

void HTMLSlotElement::DidSlotChangeAfterRenaming() {
  DCHECK(SupportsDistribution());
  EnqueueSlotChangeEvent();
  ContainingShadowRoot()->Owner()->SetNeedsDistributionRecalc();
  CheckSlotChange(SlotChangeType::kSuppressSlotChangeEvent);
}

void HTMLSlotElement::LazyReattachDistributedNodesNaive() {
  // TODO(hayato): Use some heuristic to avoid reattaching all nodes
  for (auto& node : old_distributed_nodes_)
    node->LazyReattachIfAttached();
  for (auto& node : distributed_nodes_)
    node->LazyReattachIfAttached();
}

void HTMLSlotElement::DidSlotChange(SlotChangeType slot_change_type) {
  DCHECK(SupportsDistribution());
  if (slot_change_type == SlotChangeType::kSignalSlotChangeEvent)
    EnqueueSlotChangeEvent();
  ContainingShadowRoot()->Owner()->SetNeedsDistributionRecalc();
  // Check slotchange recursively since this slotchange may cause another
  // slotchange.
  CheckSlotChange(SlotChangeType::kSuppressSlotChangeEvent);
}

void HTMLSlotElement::CheckFallbackAfterInsertedIntoShadowTree() {
  DCHECK(SupportsDistribution());
  if (HasSlotableChild()) {
    // We use kSuppress here because a slotchange event shouldn't be
    // dispatched if a slot being inserted don't get any assigned
    // node, but has a slotable child, according to DOM Standard.
    DidSlotChange(SlotChangeType::kSuppressSlotChangeEvent);
  }
}

void HTMLSlotElement::CheckFallbackAfterRemovedFromShadowTree() {
  if (HasSlotableChild()) {
    // Since a slot was removed from a shadow tree,
    // we don't need to set dirty flag for a disconnected tree.
    // However, we need to call CheckSlotChange because we might need to set a
    // dirty flag for a shadow tree which a parent of the slot may host.
    CheckSlotChange(SlotChangeType::kSuppressSlotChangeEvent);
  }
}

bool HTMLSlotElement::HasSlotableChild() const {
  for (auto& child : NodeTraversal::ChildrenOf(*this)) {
    if (child.IsSlotable())
      return true;
  }
  return false;
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
