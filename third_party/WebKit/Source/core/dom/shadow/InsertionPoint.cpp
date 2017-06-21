/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "core/dom/shadow/InsertionPoint.h"

#include "core/HTMLNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/StaticNodeList.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ElementShadowV0.h"

namespace blink {

using namespace HTMLNames;

InsertionPoint::InsertionPoint(const QualifiedName& tag_name,
                               Document& document)
    : HTMLElement(tag_name, document, kCreateInsertionPoint),
      registered_with_shadow_root_(false) {
  SetHasCustomStyleCallbacks();
}

InsertionPoint::~InsertionPoint() {}

void InsertionPoint::SetDistributedNodes(DistributedNodes& distributed_nodes) {
  // Attempt not to reattach nodes that would be distributed to the exact same
  // location by comparing the old and new distributions.

  size_t i = 0;
  size_t j = 0;

  for (; i < distributed_nodes_.size() && j < distributed_nodes.size();
       ++i, ++j) {
    if (distributed_nodes_.size() < distributed_nodes.size()) {
      // If the new distribution is larger than the old one, reattach all nodes
      // in the new distribution that were inserted.
      for (; j < distributed_nodes.size() &&
             distributed_nodes_.at(i) != distributed_nodes.at(j);
           ++j)
        distributed_nodes.at(j)->LazyReattachIfAttached();
      if (j == distributed_nodes.size())
        break;
    } else if (distributed_nodes_.size() > distributed_nodes.size()) {
      // If the old distribution is larger than the new one, reattach all nodes
      // in the old distribution that were removed.
      for (; i < distributed_nodes_.size() &&
             distributed_nodes_.at(i) != distributed_nodes.at(j);
           ++i)
        distributed_nodes_.at(i)->LazyReattachIfAttached();
      if (i == distributed_nodes_.size())
        break;
    } else if (distributed_nodes_.at(i) != distributed_nodes.at(j)) {
      // If both distributions are the same length reattach both old and new.
      distributed_nodes_.at(i)->LazyReattachIfAttached();
      distributed_nodes.at(j)->LazyReattachIfAttached();
    }
  }

  // If we hit the end of either list above we need to reattach all remaining
  // nodes.

  for (; i < distributed_nodes_.size(); ++i)
    distributed_nodes_.at(i)->LazyReattachIfAttached();

  for (; j < distributed_nodes.size(); ++j)
    distributed_nodes.at(j)->LazyReattachIfAttached();

  distributed_nodes_.Swap(distributed_nodes);
  // Deallocate a Vector and a HashMap explicitly so that
  // Oilpan can recycle them without an intervening GC.
  distributed_nodes.Clear();
  distributed_nodes_.ShrinkToFit();
}

void InsertionPoint::AttachLayoutTree(AttachContext& context) {
  // We need to attach the distribution here so that they're inserted in the
  // right order otherwise the n^2 protection inside LayoutTreeBuilder will
  // cause them to be inserted in the wrong place later. This also lets
  // distributed nodes benefit from the n^2 protection.
  AttachContext children_context(context);
  children_context.resolved_style = nullptr;

  for (size_t i = 0; i < distributed_nodes_.size(); ++i) {
    Node* child = distributed_nodes_.at(i);
    if (child->NeedsAttach())
      child->AttachLayoutTree(children_context);
  }
  if (children_context.previous_in_flow)
    context.previous_in_flow = children_context.previous_in_flow;

  HTMLElement::AttachLayoutTree(context);
}

void InsertionPoint::DetachLayoutTree(const AttachContext& context) {
  for (size_t i = 0; i < distributed_nodes_.size(); ++i)
    distributed_nodes_.at(i)->LazyReattachIfAttached();

  HTMLElement::DetachLayoutTree(context);
}

void InsertionPoint::RebuildDistributedChildrenLayoutTrees() {
  Text* next_text_sibling = nullptr;
  // This loop traverses the nodes from right to left for the same reason as the
  // one described in ContainerNode::RebuildChildrenLayoutTrees().
  for (size_t i = distributed_nodes_.size(); i > 0; --i)
    RebuildLayoutTreeForChild(distributed_nodes_.at(i - 1), next_text_sibling);
}

void InsertionPoint::WillRecalcStyle(StyleRecalcChange change) {
  StyleChangeType style_change_type = kNoStyleChange;

  if (change > kInherit || GetStyleChangeType() > kLocalStyleChange)
    style_change_type = kSubtreeStyleChange;
  else if (change > kNoInherit)
    style_change_type = kLocalStyleChange;
  else
    return;

  for (size_t i = 0; i < distributed_nodes_.size(); ++i)
    distributed_nodes_.at(i)->SetNeedsStyleRecalc(
        style_change_type,
        StyleChangeReasonForTracing::Create(
            StyleChangeReason::kPropagateInheritChangeToDistributedNodes));
}

bool InsertionPoint::CanBeActive() const {
  ShadowRoot* shadow_root = ContainingShadowRoot();
  if (!shadow_root)
    return false;
  if (shadow_root->IsV1())
    return false;
  return !Traversal<InsertionPoint>::FirstAncestor(*this);
}

bool InsertionPoint::IsActive() const {
  if (!CanBeActive())
    return false;
  ShadowRoot* shadow_root = ContainingShadowRoot();
  DCHECK(shadow_root);
  if (!isHTMLShadowElement(*this) ||
      shadow_root->DescendantShadowElementCount() <= 1)
    return true;

  // Slow path only when there are more than one shadow elements in a shadow
  // tree. That should be a rare case.
  for (const auto& point : shadow_root->DescendantInsertionPoints()) {
    if (isHTMLShadowElement(*point))
      return point == this;
  }
  return true;
}

bool InsertionPoint::IsShadowInsertionPoint() const {
  return isHTMLShadowElement(*this) && IsActive();
}

bool InsertionPoint::IsContentInsertionPoint() const {
  return isHTMLContentElement(*this) && IsActive();
}

StaticNodeList* InsertionPoint::getDistributedNodes() {
  UpdateDistribution();

  HeapVector<Member<Node>> nodes;
  nodes.ReserveInitialCapacity(distributed_nodes_.size());
  for (size_t i = 0; i < distributed_nodes_.size(); ++i)
    nodes.UncheckedAppend(distributed_nodes_.at(i));

  return StaticNodeList::Adopt(nodes);
}

bool InsertionPoint::LayoutObjectIsNeeded(const ComputedStyle& style) {
  return !IsActive() && HTMLElement::LayoutObjectIsNeeded(style);
}

void InsertionPoint::ChildrenChanged(const ChildrenChange& change) {
  HTMLElement::ChildrenChanged(change);
  if (ShadowRoot* root = ContainingShadowRoot()) {
    if (ElementShadow* root_owner = root->Owner())
      root_owner->SetNeedsDistributionRecalc();
  }
}

Node::InsertionNotificationRequest InsertionPoint::InsertedInto(
    ContainerNode* insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  if (ShadowRoot* root = ContainingShadowRoot()) {
    if (!root->IsV1()) {
      if (ElementShadow* root_owner = root->Owner()) {
        root_owner->SetNeedsDistributionRecalc();
        if (CanBeActive() && !registered_with_shadow_root_ &&
            insertion_point->GetTreeScope().RootNode() == root) {
          registered_with_shadow_root_ = true;
          root->DidAddInsertionPoint(this);
          if (CanAffectSelector())
            root_owner->V0().WillAffectSelector();
        }
      }
    }
  }

  // We could have been distributed into in a detached subtree, make sure to
  // clear the distribution when inserted again to avoid cycles.
  ClearDistribution();

  return kInsertionDone;
}

void InsertionPoint::RemovedFrom(ContainerNode* insertion_point) {
  ShadowRoot* root = ContainingShadowRoot();
  if (!root)
    root = insertion_point->ContainingShadowRoot();

  if (root) {
    if (ElementShadow* root_owner = root->Owner())
      root_owner->SetNeedsDistributionRecalc();
  }

  // host can be null when removedFrom() is called from ElementShadow
  // destructor.
  ElementShadow* root_owner = root ? root->Owner() : 0;

  // Since this insertion point is no longer visible from the shadow subtree, it
  // need to clean itself up.
  ClearDistribution();

  if (registered_with_shadow_root_ &&
      insertion_point->GetTreeScope().RootNode() == root) {
    DCHECK(root);
    registered_with_shadow_root_ = false;
    root->DidRemoveInsertionPoint(this);
    if (!root->IsV1() && root_owner) {
      if (CanAffectSelector())
        root_owner->V0().WillAffectSelector();
    }
  }

  HTMLElement::RemovedFrom(insertion_point);
}

DEFINE_TRACE(InsertionPoint) {
  visitor->Trace(distributed_nodes_);
  HTMLElement::Trace(visitor);
}

const InsertionPoint* ResolveReprojection(const Node* projected_node) {
  DCHECK(projected_node);
  const InsertionPoint* insertion_point = 0;
  const Node* current = projected_node;
  ElementShadow* last_element_shadow = 0;
  while (true) {
    ElementShadow* shadow = ShadowWhereNodeCanBeDistributedForV0(*current);
    if (!shadow || shadow->IsV1() || shadow == last_element_shadow)
      break;
    last_element_shadow = shadow;
    const InsertionPoint* inserted_to =
        shadow->V0().FinalDestinationInsertionPointFor(projected_node);
    if (!inserted_to)
      break;
    DCHECK_NE(current, inserted_to);
    current = inserted_to;
    insertion_point = inserted_to;
  }
  return insertion_point;
}

void CollectDestinationInsertionPoints(
    const Node& node,
    HeapVector<Member<InsertionPoint>, 8>& results) {
  const Node* current = &node;
  ElementShadow* last_element_shadow = 0;
  while (true) {
    ElementShadow* shadow = ShadowWhereNodeCanBeDistributedForV0(*current);
    if (!shadow || shadow->IsV1() || shadow == last_element_shadow)
      return;
    last_element_shadow = shadow;
    const DestinationInsertionPoints* insertion_points =
        shadow->V0().DestinationInsertionPointsFor(&node);
    if (!insertion_points)
      return;
    for (size_t i = 0; i < insertion_points->size(); ++i)
      results.push_back(insertion_points->at(i).Get());
    DCHECK_NE(current, insertion_points->back().Get());
    current = insertion_points->back().Get();
  }
}

}  // namespace blink
