/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#include "core/dom/FlatTreeTraversal.h"

#include "core/dom/Element.h"
#include "core/dom/ElementShadow.h"
#include "core/html/HTMLShadowElement.h"
#include "core/html/HTMLSlotElement.h"

namespace blink {

static inline ElementShadow* ShadowFor(const Node& node) {
  return node.IsElementNode() ? ToElement(node).Shadow() : nullptr;
}

static inline bool CanBeDistributedToInsertionPoint(const Node& node) {
  return node.IsInV0ShadowTree() || node.IsChildOfV0ShadowHost();
}

Node* FlatTreeTraversal::TraverseChild(const Node& node,
                                       TraversalDirection direction) {
  ElementShadow* shadow = ShadowFor(node);
  if (shadow) {
    ShadowRoot& shadow_root = shadow->YoungestShadowRoot();
    return ResolveDistributionStartingAt(direction == kTraversalDirectionForward
                                             ? shadow_root.firstChild()
                                             : shadow_root.lastChild(),
                                         direction);
  }
  return ResolveDistributionStartingAt(direction == kTraversalDirectionForward
                                           ? node.firstChild()
                                           : node.lastChild(),
                                       direction);
}

Node* FlatTreeTraversal::ResolveDistributionStartingAt(
    const Node* node,
    TraversalDirection direction) {
  if (!node)
    return nullptr;
  for (const Node* sibling = node; sibling;
       sibling = (direction == kTraversalDirectionForward
                      ? sibling->nextSibling()
                      : sibling->previousSibling())) {
    if (isHTMLSlotElement(*sibling)) {
      const HTMLSlotElement& slot = toHTMLSlotElement(*sibling);
      if (slot.SupportsDistribution()) {
        if (Node* found = (direction == kTraversalDirectionForward
                               ? slot.FirstDistributedNode()
                               : slot.LastDistributedNode()))
          return found;
        continue;
      }
    }
    if (node->IsInV0ShadowTree())
      return V0ResolveDistributionStartingAt(*sibling, direction);
    return const_cast<Node*>(sibling);
  }
  return nullptr;
}

Node* FlatTreeTraversal::V0ResolveDistributionStartingAt(
    const Node& node,
    TraversalDirection direction) {
  DCHECK(!isHTMLSlotElement(node) ||
         !toHTMLSlotElement(node).SupportsDistribution());
  for (const Node* sibling = &node; sibling;
       sibling = (direction == kTraversalDirectionForward
                      ? sibling->nextSibling()
                      : sibling->previousSibling())) {
    if (!IsActiveInsertionPoint(*sibling))
      return const_cast<Node*>(sibling);
    const InsertionPoint& insertion_point = ToInsertionPoint(*sibling);
    if (Node* found = (direction == kTraversalDirectionForward
                           ? insertion_point.FirstDistributedNode()
                           : insertion_point.LastDistributedNode()))
      return found;
    DCHECK(isHTMLShadowElement(insertion_point) ||
           (isHTMLContentElement(insertion_point) &&
            !insertion_point.HasChildren()));
  }
  return nullptr;
}

// TODO(hayato): This may return a wrong result for a node which is not in a
// document flat tree.  See FlatTreeTraversalTest's redistribution test for
// details.
Node* FlatTreeTraversal::TraverseSiblings(const Node& node,
                                          TraversalDirection direction) {
  if (node.IsChildOfV1ShadowHost())
    return TraverseSiblingsForV1HostChild(node, direction);

  if (ShadowWhereNodeCanBeDistributedForV0(node))
    return TraverseSiblingsForV0Distribution(node, direction);

  if (Node* found = ResolveDistributionStartingAt(
          direction == kTraversalDirectionForward ? node.nextSibling()
                                                  : node.previousSibling(),
          direction))
    return found;

  // Slotted nodes are already handled in traverseSiblingsForV1HostChild()
  // above, here is for fallback contents.
  Element* parent = node.parentElement();
  if (parent && isHTMLSlotElement(parent)) {
    HTMLSlotElement& slot = toHTMLSlotElement(*parent);
    if (slot.SupportsDistribution() && slot.AssignedNodes().IsEmpty())
      return TraverseSiblings(slot, direction);
  }

  if (!node.IsInV0ShadowTree())
    return nullptr;

  // For v0 older shadow tree
  if (node.parentNode() && node.parentNode()->IsShadowRoot()) {
    ShadowRoot* parent_shadow_root = ToShadowRoot(node.parentNode());
    if (!parent_shadow_root->IsYoungest()) {
      HTMLShadowElement* assigned_insertion_point =
          parent_shadow_root->ShadowInsertionPointOfYoungerShadowRoot();
      DCHECK(assigned_insertion_point);
      return TraverseSiblings(*assigned_insertion_point, direction);
    }
  }
  return nullptr;
}

Node* FlatTreeTraversal::TraverseSiblingsForV1HostChild(
    const Node& node,
    TraversalDirection direction) {
  HTMLSlotElement* slot = node.FinalDestinationSlot();
  if (!slot)
    return nullptr;
  if (Node* sibling_in_distributed_nodes =
          (direction == kTraversalDirectionForward
               ? slot->DistributedNodeNextTo(node)
               : slot->DistributedNodePreviousTo(node)))
    return sibling_in_distributed_nodes;
  return TraverseSiblings(*slot, direction);
}

Node* FlatTreeTraversal::TraverseSiblingsForV0Distribution(
    const Node& node,
    TraversalDirection direction) {
  const InsertionPoint* final_destination = ResolveReprojection(&node);
  if (!final_destination)
    return nullptr;
  if (Node* found = (direction == kTraversalDirectionForward
                         ? final_destination->DistributedNodeNextTo(&node)
                         : final_destination->DistributedNodePreviousTo(&node)))
    return found;
  return TraverseSiblings(*final_destination, direction);
}

ContainerNode* FlatTreeTraversal::TraverseParent(
    const Node& node,
    ParentTraversalDetails* details) {
  // TODO(hayato): Stop this hack for a pseudo element because a pseudo element
  // is not a child of its parentOrShadowHostNode() in a flat tree.
  if (node.IsPseudoElement())
    return node.ParentOrShadowHostNode();

  if (node.IsChildOfV1ShadowHost()) {
    HTMLSlotElement* slot = node.FinalDestinationSlot();
    if (!slot)
      return nullptr;
    return TraverseParent(*slot);
  }

  Element* parent = node.parentElement();
  if (parent && isHTMLSlotElement(parent)) {
    HTMLSlotElement& slot = toHTMLSlotElement(*parent);
    if (slot.SupportsDistribution()) {
      if (!slot.AssignedNodes().IsEmpty())
        return nullptr;
      return TraverseParent(slot, details);
    }
  }

  if (CanBeDistributedToInsertionPoint(node))
    return TraverseParentForV0(node, details);

  DCHECK(!ShadowWhereNodeCanBeDistributedForV0(node));
  return TraverseParentOrHost(node);
}

ContainerNode* FlatTreeTraversal::TraverseParentForV0(
    const Node& node,
    ParentTraversalDetails* details) {
  if (ShadowWhereNodeCanBeDistributedForV0(node)) {
    if (const InsertionPoint* insertion_point = ResolveReprojection(&node)) {
      if (details)
        details->DidTraverseInsertionPoint(insertion_point);
      // The node is distributed. But the distribution was stopped at this
      // insertion point.
      if (ShadowWhereNodeCanBeDistributedForV0(*insertion_point))
        return nullptr;
      return TraverseParent(*insertion_point);
    }
    return nullptr;
  }
  ContainerNode* parent = TraverseParentOrHost(node);
  if (IsActiveInsertionPoint(*parent))
    return nullptr;
  return parent;
}

ContainerNode* FlatTreeTraversal::TraverseParentOrHost(const Node& node) {
  ContainerNode* parent = node.parentNode();
  if (!parent)
    return nullptr;
  if (!parent->IsShadowRoot())
    return parent;
  ShadowRoot* shadow_root = ToShadowRoot(parent);
  DCHECK(!shadow_root->ShadowInsertionPointOfYoungerShadowRoot());
  if (!shadow_root->IsYoungest())
    return nullptr;
  return &shadow_root->host();
}

Node* FlatTreeTraversal::ChildAt(const Node& node, unsigned index) {
  AssertPrecondition(node);
  Node* child = TraverseFirstChild(node);
  while (child && index--)
    child = NextSibling(*child);
  AssertPostcondition(child);
  return child;
}

Node* FlatTreeTraversal::NextSkippingChildren(const Node& node) {
  if (Node* next_sibling = TraverseNextSibling(node))
    return next_sibling;
  return TraverseNextAncestorSibling(node);
}

bool FlatTreeTraversal::ContainsIncludingPseudoElement(
    const ContainerNode& container,
    const Node& node) {
  AssertPrecondition(container);
  AssertPrecondition(node);
  // This can be slower than FlatTreeTraversal::contains() because we
  // can't early exit even when container doesn't have children.
  for (const Node* current = &node; current;
       current = TraverseParent(*current)) {
    if (current == &container)
      return true;
  }
  return false;
}

Node* FlatTreeTraversal::PreviousSkippingChildren(const Node& node) {
  if (Node* previous_sibling = TraversePreviousSibling(node))
    return previous_sibling;
  return TraversePreviousAncestorSibling(node);
}

Node* FlatTreeTraversal::PreviousAncestorSiblingPostOrder(
    const Node& current,
    const Node* stay_within) {
  DCHECK(!FlatTreeTraversal::PreviousSibling(current));
  for (Node* parent = FlatTreeTraversal::Parent(current); parent;
       parent = FlatTreeTraversal::Parent(*parent)) {
    if (parent == stay_within)
      return nullptr;
    if (Node* previous_sibling = FlatTreeTraversal::PreviousSibling(*parent))
      return previous_sibling;
  }
  return nullptr;
}

// TODO(yosin) We should consider introducing template class to share code
// between DOM tree traversal and flat tree tarversal.
Node* FlatTreeTraversal::PreviousPostOrder(const Node& current,
                                           const Node* stay_within) {
  AssertPrecondition(current);
  if (stay_within)
    AssertPrecondition(*stay_within);
  if (Node* last_child = TraverseLastChild(current)) {
    AssertPostcondition(last_child);
    return last_child;
  }
  if (current == stay_within)
    return nullptr;
  if (Node* previous_sibling = TraversePreviousSibling(current)) {
    AssertPostcondition(previous_sibling);
    return previous_sibling;
  }
  return PreviousAncestorSiblingPostOrder(current, stay_within);
}

bool FlatTreeTraversal::IsDescendantOf(const Node& node, const Node& other) {
  AssertPrecondition(node);
  AssertPrecondition(other);
  if (!HasChildren(other) || node.isConnected() != other.isConnected())
    return false;
  for (const ContainerNode* n = TraverseParent(node); n;
       n = TraverseParent(*n)) {
    if (n == other)
      return true;
  }
  return false;
}

Node* FlatTreeTraversal::CommonAncestor(const Node& node_a,
                                        const Node& node_b) {
  AssertPrecondition(node_a);
  AssertPrecondition(node_b);
  Node* result = node_a.CommonAncestor(
      node_b, [](const Node& node) { return FlatTreeTraversal::Parent(node); });
  AssertPostcondition(result);
  return result;
}

Node* FlatTreeTraversal::TraverseNextAncestorSibling(const Node& node) {
  DCHECK(!TraverseNextSibling(node));
  for (Node* parent = TraverseParent(node); parent;
       parent = TraverseParent(*parent)) {
    if (Node* next_sibling = TraverseNextSibling(*parent))
      return next_sibling;
  }
  return nullptr;
}

Node* FlatTreeTraversal::TraversePreviousAncestorSibling(const Node& node) {
  DCHECK(!TraversePreviousSibling(node));
  for (Node* parent = TraverseParent(node); parent;
       parent = TraverseParent(*parent)) {
    if (Node* previous_sibling = TraversePreviousSibling(*parent))
      return previous_sibling;
  }
  return nullptr;
}

unsigned FlatTreeTraversal::Index(const Node& node) {
  AssertPrecondition(node);
  unsigned count = 0;
  for (Node* runner = TraversePreviousSibling(node); runner;
       runner = PreviousSibling(*runner))
    ++count;
  return count;
}

unsigned FlatTreeTraversal::CountChildren(const Node& node) {
  AssertPrecondition(node);
  unsigned count = 0;
  for (Node* runner = TraverseFirstChild(node); runner;
       runner = TraverseNextSibling(*runner))
    ++count;
  return count;
}

Node* FlatTreeTraversal::LastWithin(const Node& node) {
  AssertPrecondition(node);
  Node* descendant = TraverseLastChild(node);
  for (Node* child = descendant; child; child = LastChild(*child))
    descendant = child;
  AssertPostcondition(descendant);
  return descendant;
}

Node& FlatTreeTraversal::LastWithinOrSelf(const Node& node) {
  AssertPrecondition(node);
  Node* last_descendant = LastWithin(node);
  Node& result = last_descendant ? *last_descendant : const_cast<Node&>(node);
  AssertPostcondition(&result);
  return result;
}

}  // namespace blink
