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

#ifndef FlatTreeTraversalNg_h
#define FlatTreeTraversalNg_h

#include "core/CoreExport.h"
#include "core/dom/Document.h"
#include "core/dom/LayoutTreeBuilderTraversal.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/V0InsertionPoint.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class ContainerNode;
class ElementShadow;
class Node;

ElementShadow* ShadowFor(const Node& node);
bool CanBeDistributedToV0InsertionPoint(const Node& node);

// Flat tree version of |NodeTraversal|.
//
// None of member functions takes a |ShadowRoot| or an active insertion point,
// e.g. roughly speaking <content> and <shadow> in the shadow tree, see
// |InsertionPoint::isActive()| for details of active insertion points, since
// they aren't appeared in the flat tree. |assertPrecondition()| and
// |assertPostCondition()| check this condition.
//
// FIXME: Make some functions inline to optimise the performance.
// https://bugs.webkit.org/show_bug.cgi?id=82702
class CORE_EXPORT FlatTreeTraversalNg {
  STATIC_ONLY(FlatTreeTraversalNg);

 public:
  typedef LayoutTreeBuilderTraversal::ParentDetails ParentTraversalDetails;
  using TraversalNodeType = Node;

  static Node* Next(const Node&);
  static Node* Next(const Node&, const Node* stay_within);
  static Node* Previous(const Node&);

  static Node* FirstChild(const Node&);
  static Node* LastChild(const Node&);
  static bool HasChildren(const Node&);

  static ContainerNode* Parent(const Node&, ParentTraversalDetails* = nullptr);
  static Element* ParentElement(const Node&);

  static Node* NextSibling(const Node&);
  static Node* PreviousSibling(const Node&);

  // Returns a child node at |index|. If |index| is greater than or equal to
  // the children, this function returns |nullptr|.
  static Node* ChildAt(const Node&, unsigned index);

  // Flat tree version of |NodeTraversal::nextSkippingChildren()|. This
  // function is similar to |next()| but skips child nodes of a specified
  // node.
  static Node* NextSkippingChildren(const Node&);
  static Node* NextSkippingChildren(const Node&, const Node* stay_within);

  static Node* FirstWithin(const Node& current) { return FirstChild(current); }

  // Flat tree version of |NodeTraversal::previousSkippingChildren()|
  // similar to |previous()| but skipping child nodes of the specified node.
  static Node* PreviousSkippingChildren(const Node&);

  // Like previous, but visits parents before their children.
  static Node* PreviousPostOrder(const Node&,
                                 const Node* stay_within = nullptr);

  // Flat tree version of |Node::isDescendantOf(other)|. This function
  // returns true if |other| contains |node|, otherwise returns
  // false. If |other| is |node|, this function returns false.
  static bool IsDescendantOf(const Node& /*node*/, const Node& other);

  static bool Contains(const ContainerNode& container, const Node& node) {
    AssertPrecondition(container);
    AssertPrecondition(node);
    return container == node || IsDescendantOf(node, container);
  }

  static bool ContainsIncludingPseudoElement(const ContainerNode&, const Node&);

  // Returns a common ancestor of |nodeA| and |nodeB| if exists, otherwise
  // returns |nullptr|.
  static Node* CommonAncestor(const Node& node_a, const Node& node_b);

  // Flat tree version of |Node::nodeIndex()|. This function returns a
  // zero base position number of the specified node in child nodes list, or
  // zero if the specified node has no parent.
  static unsigned Index(const Node&);

  // Flat tree version of |ContainerNode::countChildren()|. This function
  // returns the number of the child nodes of the specified node in the
  // flat tree.
  static unsigned CountChildren(const Node&);

  static Node* LastWithin(const Node&);
  static Node& LastWithinOrSelf(const Node&);

  // Flat tree range helper functions for range based for statement.
  // TODO(dom-team): We should have following functions to match with
  // |NodeTraversal|:
  //   - AncestorsOf()
  //   - DescendantsOf()
  //   - InclusiveAncestorsOf()
  //   - InclusiveDescendantsOf()
  //   - StartsAt()
  //   - StartsAfter()
  static TraversalRange<TraversalChildrenIterator<FlatTreeTraversalNg>>
  ChildrenOf(const Node&);

 private:
  enum TraversalDirection {
    kTraversalDirectionForward,
    kTraversalDirectionBackward
  };

  static void AssertPrecondition(const Node& node) {
    DCHECK(!node.NeedsDistributionRecalc());
    DCHECK(node.CanParticipateInFlatTree());
  }

  static void AssertPostcondition(const Node* node) {
#if DCHECK_IS_ON()
    if (node)
      AssertPrecondition(*node);
#endif
  }

  // static Node* ResolveDistributionStartingAt(const Node*,
  // TraversalDirection);
  static Node* V0ResolveDistributionStartingAt(const Node&, TraversalDirection);

  static Node* TraverseNext(const Node&);
  static Node* TraverseNext(const Node&, const Node* stay_within);
  static Node* TraverseNextSkippingChildren(const Node&,
                                            const Node* stay_within);
  static Node* TraversePrevious(const Node&);

  static Node* TraverseFirstChild(const Node&);
  static Node* TraverseLastChild(const Node&);
  static Node* TraverseChild(const Node&, TraversalDirection);

  static ContainerNode* TraverseParent(const Node&,
                                       ParentTraversalDetails* = nullptr);
  // TODO(hayato): Make ParentTraversalDetails be aware of slot elements too.
  static ContainerNode* TraverseParentForV0(const Node&,
                                            ParentTraversalDetails* = nullptr);
  static ContainerNode* TraverseParentOrHost(const Node&);

  static Node* TraverseNextSibling(const Node&);
  static Node* TraversePreviousSibling(const Node&);

  static Node* TraverseSiblings(const Node&, TraversalDirection);
  static Node* TraverseSiblingsForV1HostChild(const Node&, TraversalDirection);
  static Node* TraverseSiblingsForV0Distribution(const Node&,
                                                 TraversalDirection);

  static Node* TraverseNextAncestorSibling(const Node&);
  static Node* TraversePreviousAncestorSibling(const Node&);
  static Node* PreviousAncestorSiblingPostOrder(const Node& current,
                                                const Node* stay_within);
};

inline ContainerNode* FlatTreeTraversalNg::Parent(
    const Node& node,
    ParentTraversalDetails* details) {
  AssertPrecondition(node);
  ContainerNode* result = TraverseParent(node, details);
  AssertPostcondition(result);
  return result;
}

inline Element* FlatTreeTraversalNg::ParentElement(const Node& node) {
  ContainerNode* parent = FlatTreeTraversalNg::Parent(node);
  return parent && parent->IsElementNode() ? ToElement(parent) : nullptr;
}

inline Node* FlatTreeTraversalNg::NextSibling(const Node& node) {
  AssertPrecondition(node);
  Node* result = TraverseSiblings(node, kTraversalDirectionForward);
  AssertPostcondition(result);
  return result;
}

inline Node* FlatTreeTraversalNg::PreviousSibling(const Node& node) {
  AssertPrecondition(node);
  Node* result = TraverseSiblings(node, kTraversalDirectionBackward);
  AssertPostcondition(result);
  return result;
}

inline Node* FlatTreeTraversalNg::Next(const Node& node) {
  AssertPrecondition(node);
  Node* result = TraverseNext(node);
  AssertPostcondition(result);
  return result;
}

inline Node* FlatTreeTraversalNg::Next(const Node& node,
                                       const Node* stay_within) {
  AssertPrecondition(node);
  Node* result = TraverseNext(node, stay_within);
  AssertPostcondition(result);
  return result;
}

inline Node* FlatTreeTraversalNg::NextSkippingChildren(
    const Node& node,
    const Node* stay_within) {
  AssertPrecondition(node);
  Node* result = TraverseNextSkippingChildren(node, stay_within);
  AssertPostcondition(result);
  return result;
}

inline Node* FlatTreeTraversalNg::TraverseNext(const Node& node) {
  if (Node* next = TraverseFirstChild(node))
    return next;
  for (const Node* next = &node; next; next = TraverseParent(*next)) {
    if (Node* sibling = TraverseNextSibling(*next))
      return sibling;
  }
  return nullptr;
}

inline Node* FlatTreeTraversalNg::TraverseNext(const Node& node,
                                               const Node* stay_within) {
  if (Node* next = TraverseFirstChild(node))
    return next;
  return TraverseNextSkippingChildren(node, stay_within);
}

inline Node* FlatTreeTraversalNg::TraverseNextSkippingChildren(
    const Node& node,
    const Node* stay_within) {
  for (const Node* next = &node; next; next = TraverseParent(*next)) {
    if (next == stay_within)
      return nullptr;
    if (Node* sibling = TraverseNextSibling(*next))
      return sibling;
  }
  return nullptr;
}

inline Node* FlatTreeTraversalNg::Previous(const Node& node) {
  AssertPrecondition(node);
  Node* result = TraversePrevious(node);
  AssertPostcondition(result);
  return result;
}

inline Node* FlatTreeTraversalNg::TraversePrevious(const Node& node) {
  if (Node* previous = TraversePreviousSibling(node)) {
    while (Node* child = TraverseLastChild(*previous))
      previous = child;
    return previous;
  }
  return TraverseParent(node);
}

inline Node* FlatTreeTraversalNg::FirstChild(const Node& node) {
  AssertPrecondition(node);
  Node* result = TraverseChild(node, kTraversalDirectionForward);
  AssertPostcondition(result);
  return result;
}

inline Node* FlatTreeTraversalNg::LastChild(const Node& node) {
  AssertPrecondition(node);
  Node* result = TraverseLastChild(node);
  AssertPostcondition(result);
  return result;
}

inline bool FlatTreeTraversalNg::HasChildren(const Node& node) {
  return FirstChild(node);
}

inline Node* FlatTreeTraversalNg::TraverseNextSibling(const Node& node) {
  return TraverseSiblings(node, kTraversalDirectionForward);
}

inline Node* FlatTreeTraversalNg::TraversePreviousSibling(const Node& node) {
  return TraverseSiblings(node, kTraversalDirectionBackward);
}

inline Node* FlatTreeTraversalNg::TraverseFirstChild(const Node& node) {
  return TraverseChild(node, kTraversalDirectionForward);
}

inline Node* FlatTreeTraversalNg::TraverseLastChild(const Node& node) {
  return TraverseChild(node, kTraversalDirectionBackward);
}

// TraverseRange<T> implementations
inline TraversalRange<TraversalChildrenIterator<FlatTreeTraversalNg>>
FlatTreeTraversalNg::ChildrenOf(const Node& parent) {
  return TraversalRange<TraversalChildrenIterator<FlatTreeTraversalNg>>(
      &parent);
}

}  // namespace blink

#endif
