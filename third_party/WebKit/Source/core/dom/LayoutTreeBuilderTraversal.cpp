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

#include "core/dom/LayoutTreeBuilderTraversal.h"

#include "core/HTMLNames.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/shadow/FlatTreeTraversal.h"
#include "core/layout/LayoutObject.h"

namespace blink {

inline static bool hasDisplayContentsStyle(const Node& node) {
  return node.isElementNode() && toElement(node).hasDisplayContentsStyle();
}

static bool isLayoutObjectReparented(const LayoutObject* layoutObject) {
  if (!layoutObject->node()->isElementNode())
    return false;
  return toElement(layoutObject->node())->isInTopLayer();
}

void LayoutTreeBuilderTraversal::ParentDetails::didTraverseInsertionPoint(
    const InsertionPoint* insertionPoint) {
  if (!m_insertionPoint) {
    m_insertionPoint = insertionPoint;
  }
}

inline static void assertPseudoElementParent(
    const PseudoElement& pseudoElement) {
  DCHECK(pseudoElement.parentNode());
  DCHECK(pseudoElement.parentNode()->canParticipateInFlatTree());
}

ContainerNode* LayoutTreeBuilderTraversal::parent(const Node& node,
                                                  ParentDetails* details) {
  // TODO(hayato): Uncomment this once we can be sure
  // LayoutTreeBuilderTraversal::parent() is used only for a node which is
  // connected.
  // DCHECK(node.isConnected());
  if (node.isPseudoElement()) {
    assertPseudoElementParent(toPseudoElement(node));
    return node.parentNode();
  }
  return FlatTreeTraversal::parent(node, details);
}

ContainerNode* LayoutTreeBuilderTraversal::layoutParent(
    const Node& node,
    ParentDetails* details) {
  ContainerNode* parent = LayoutTreeBuilderTraversal::parent(node, details);

  while (parent && hasDisplayContentsStyle(*parent))
    parent = LayoutTreeBuilderTraversal::parent(*parent, details);

  return parent;
}

LayoutObject* LayoutTreeBuilderTraversal::parentLayoutObject(const Node& node) {
  ContainerNode* parent = LayoutTreeBuilderTraversal::layoutParent(node);
  return parent ? parent->layoutObject() : nullptr;
}

Node* LayoutTreeBuilderTraversal::nextSibling(const Node& node) {
  if (node.isBeforePseudoElement()) {
    assertPseudoElementParent(toPseudoElement(node));
    if (Node* next = FlatTreeTraversal::firstChild(*node.parentNode()))
      return next;
  } else {
    if (node.isAfterPseudoElement())
      return nullptr;
    if (Node* next = FlatTreeTraversal::nextSibling(node))
      return next;
  }

  Node* parent = FlatTreeTraversal::parent(node);
  if (parent && parent->isElementNode())
    return toElement(parent)->pseudoElement(PseudoIdAfter);

  return nullptr;
}

Node* LayoutTreeBuilderTraversal::previousSibling(const Node& node) {
  if (node.isAfterPseudoElement()) {
    assertPseudoElementParent(toPseudoElement(node));
    if (Node* previous = FlatTreeTraversal::lastChild(*node.parentNode()))
      return previous;
  } else {
    if (node.isBeforePseudoElement())
      return nullptr;
    if (Node* previous = FlatTreeTraversal::previousSibling(node))
      return previous;
  }

  Node* parent = FlatTreeTraversal::parent(node);
  if (parent && parent->isElementNode())
    return toElement(parent)->pseudoElement(PseudoIdBefore);

  return nullptr;
}

static Node* lastChild(const Node& node) {
  return FlatTreeTraversal::lastChild(node);
}

static Node* pseudoAwarePreviousSibling(const Node& node) {
  Node* previousNode = LayoutTreeBuilderTraversal::previousSibling(node);
  Node* parentNode = LayoutTreeBuilderTraversal::parent(node);

  if (parentNode && parentNode->isElementNode() && !previousNode) {
    if (node.isAfterPseudoElement()) {
      if (Node* child = lastChild(*parentNode))
        return child;
    }
    if (!node.isBeforePseudoElement())
      return toElement(parentNode)->pseudoElement(PseudoIdBefore);
  }
  return previousNode;
}

static Node* pseudoAwareLastChild(const Node& node) {
  if (node.isElementNode()) {
    const Element& currentElement = toElement(node);
    Node* last = currentElement.pseudoElement(PseudoIdAfter);
    if (last)
      return last;

    last = lastChild(currentElement);
    if (!last)
      last = currentElement.pseudoElement(PseudoIdBefore);
    return last;
  }

  return lastChild(node);
}

Node* LayoutTreeBuilderTraversal::previous(const Node& node,
                                           const Node* stayWithin) {
  if (node == stayWithin)
    return 0;

  if (Node* previousNode = pseudoAwarePreviousSibling(node)) {
    while (Node* previousLastChild = pseudoAwareLastChild(*previousNode))
      previousNode = previousLastChild;
    return previousNode;
  }
  return parent(node);
}

Node* LayoutTreeBuilderTraversal::firstChild(const Node& node) {
  return FlatTreeTraversal::firstChild(node);
}

static Node* pseudoAwareNextSibling(const Node& node) {
  Node* parentNode = LayoutTreeBuilderTraversal::parent(node);
  Node* nextNode = LayoutTreeBuilderTraversal::nextSibling(node);

  if (parentNode && parentNode->isElementNode() && !nextNode) {
    if (node.isBeforePseudoElement()) {
      if (Node* child = LayoutTreeBuilderTraversal::firstChild(*parentNode))
        return child;
    }
    if (!node.isAfterPseudoElement())
      return toElement(parentNode)->pseudoElement(PseudoIdAfter);
  }
  return nextNode;
}

static Node* pseudoAwareFirstChild(const Node& node) {
  if (node.isElementNode()) {
    const Element& currentElement = toElement(node);
    Node* first = currentElement.pseudoElement(PseudoIdBefore);
    if (first)
      return first;
    first = LayoutTreeBuilderTraversal::firstChild(currentElement);
    if (!first)
      first = currentElement.pseudoElement(PseudoIdAfter);
    return first;
  }

  return LayoutTreeBuilderTraversal::firstChild(node);
}

static Node* nextAncestorSibling(const Node& node, const Node* stayWithin) {
  DCHECK(!pseudoAwareNextSibling(node));
  DCHECK_NE(node, stayWithin);
  for (Node* parentNode = LayoutTreeBuilderTraversal::parent(node); parentNode;
       parentNode = LayoutTreeBuilderTraversal::parent(*parentNode)) {
    if (parentNode == stayWithin)
      return 0;
    if (Node* nextNode = pseudoAwareNextSibling(*parentNode))
      return nextNode;
  }
  return 0;
}

Node* LayoutTreeBuilderTraversal::nextSkippingChildren(const Node& node,
                                                       const Node* stayWithin) {
  if (node == stayWithin)
    return 0;
  if (Node* nextNode = pseudoAwareNextSibling(node))
    return nextNode;
  return nextAncestorSibling(node, stayWithin);
}

Node* LayoutTreeBuilderTraversal::next(const Node& node,
                                       const Node* stayWithin) {
  if (Node* child = pseudoAwareFirstChild(node))
    return child;
  return nextSkippingChildren(node, stayWithin);
}

static LayoutObject* nextSiblingLayoutObjectInternal(Node* node,
                                                     int32_t& limit) {
  for (Node* sibling = node; sibling && limit-- != 0;
       sibling = LayoutTreeBuilderTraversal::nextSibling(*sibling)) {
    LayoutObject* layoutObject = sibling->layoutObject();

#if DCHECK_IS_ON()
    if (hasDisplayContentsStyle(*sibling))
      DCHECK(!layoutObject);
#endif

    if (!layoutObject && hasDisplayContentsStyle(*sibling)) {
      layoutObject = nextSiblingLayoutObjectInternal(
          pseudoAwareFirstChild(*sibling), limit);
      if (layoutObject)
        return layoutObject;
      if (limit == -1)
        return nullptr;
    }

    if (layoutObject && !isLayoutObjectReparented(layoutObject))
      return layoutObject;
  }

  return nullptr;
}

LayoutObject* LayoutTreeBuilderTraversal::nextSiblingLayoutObject(
    const Node& node,
    int32_t limit) {
  DCHECK(limit == kTraverseAllSiblings || limit >= 0) << limit;
  if (LayoutObject* sibling =
          nextSiblingLayoutObjectInternal(nextSibling(node), limit))
    return sibling;

  Node* parent = LayoutTreeBuilderTraversal::parent(node);
  while (limit != -1 && parent && hasDisplayContentsStyle(*parent)) {
    if (LayoutObject* sibling =
            nextSiblingLayoutObjectInternal(nextSibling(*parent), limit))
      return sibling;
    parent = LayoutTreeBuilderTraversal::parent(*parent);
  }

  return nullptr;
}

static LayoutObject* previousSiblingLayoutObjectInternal(Node* node,
                                                         int32_t& limit) {
  for (Node* sibling = node; sibling && limit-- != 0;
       sibling = LayoutTreeBuilderTraversal::previousSibling(*sibling)) {
    LayoutObject* layoutObject = sibling->layoutObject();

#if DCHECK_IS_ON()
    if (hasDisplayContentsStyle(*sibling))
      DCHECK(!layoutObject);
#endif

    if (!layoutObject && hasDisplayContentsStyle(*sibling)) {
      layoutObject = previousSiblingLayoutObjectInternal(
          pseudoAwareLastChild(*sibling), limit);
      if (layoutObject)
        return layoutObject;
      if (limit == -1)
        return nullptr;
    }

    if (layoutObject && !isLayoutObjectReparented(layoutObject))
      return layoutObject;
  }

  return nullptr;
}

LayoutObject* LayoutTreeBuilderTraversal::previousSiblingLayoutObject(
    const Node& node,
    int32_t limit) {
  DCHECK(limit == kTraverseAllSiblings || limit >= 0) << limit;
  if (LayoutObject* sibling =
          previousSiblingLayoutObjectInternal(previousSibling(node), limit))
    return sibling;

  Node* parent = LayoutTreeBuilderTraversal::parent(node);
  while (limit != -1 && parent && hasDisplayContentsStyle(*parent)) {
    if (LayoutObject* sibling = previousSiblingLayoutObjectInternal(
            previousSibling(*parent), limit))
      return sibling;
    parent = LayoutTreeBuilderTraversal::parent(*parent);
  }

  return nullptr;
}

LayoutObject* LayoutTreeBuilderTraversal::nextInTopLayer(
    const Element& element) {
  if (!element.isInTopLayer())
    return 0;
  const HeapVector<Member<Element>>& topLayerElements =
      element.document().topLayerElements();
  size_t position = topLayerElements.find(&element);
  DCHECK_NE(position, kNotFound);
  for (size_t i = position + 1; i < topLayerElements.size(); ++i) {
    if (LayoutObject* layoutObject = topLayerElements[i]->layoutObject())
      return layoutObject;
  }
  return 0;
}

}  // namespace blink
