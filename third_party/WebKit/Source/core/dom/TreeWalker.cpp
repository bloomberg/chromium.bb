/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/dom/TreeWalker.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/NodeTraversalStrategy.h"

namespace blink {

TreeWalker::TreeWalker(Node* root_node,
                       unsigned what_to_show,
                       V8NodeFilterCondition* filter)
    : NodeIteratorBase(this, root_node, what_to_show, filter),
      current_(root()) {}

void TreeWalker::setCurrentNode(Node* node) {
  DCHECK(node);
  current_ = node;
}

inline Node* TreeWalker::SetCurrent(Node* node) {
  current_ = node;
  return current_.Get();
}

Node* TreeWalker::parentNode(ExceptionState& exception_state) {
  Node* node = current_;
  while (node != root()) {
    node = node->parentNode();
    if (!node)
      return 0;
    unsigned accept_node_result = AcceptNode(node, exception_state);
    if (exception_state.HadException())
      return 0;
    if (accept_node_result == NodeFilter::kFilterAccept)
      return SetCurrent(node);
  }
  return 0;
}

Node* TreeWalker::firstChild(ExceptionState& exception_state) {
  for (Node* node = current_->firstChild(); node;) {
    unsigned accept_node_result = AcceptNode(node, exception_state);
    if (exception_state.HadException())
      return 0;
    switch (accept_node_result) {
      case NodeFilter::kFilterAccept:
        current_ = node;
        return current_.Get();
      case NodeFilter::kFilterSkip:
        if (node->hasChildren()) {
          node = node->firstChild();
          continue;
        }
        break;
      case NodeFilter::kFilterReject:
        break;
    }
    do {
      if (node->nextSibling()) {
        node = node->nextSibling();
        break;
      }
      ContainerNode* parent = node->parentNode();
      if (!parent || parent == root() || parent == current_)
        return 0;
      node = parent;
    } while (node);
  }
  return 0;
}

Node* TreeWalker::lastChild(ExceptionState& exception_state) {
  for (Node* node = current_->lastChild(); node;) {
    unsigned accept_node_result = AcceptNode(node, exception_state);
    if (exception_state.HadException())
      return 0;
    switch (accept_node_result) {
      case NodeFilter::kFilterAccept:
        current_ = node;
        return current_.Get();
      case NodeFilter::kFilterSkip:
        if (node->lastChild()) {
          node = node->lastChild();
          continue;
        }
        break;
      case NodeFilter::kFilterReject:
        break;
    }
    do {
      if (node->previousSibling()) {
        node = node->previousSibling();
        break;
      }
      ContainerNode* parent = node->parentNode();
      if (!parent || parent == root() || parent == current_)
        return 0;
      node = parent;
    } while (node);
  }
  return 0;
}

// https://dom.spec.whatwg.org/#concept-traverse-siblings
template <typename Strategy>
Node* TreeWalker::TraverseSiblings(ExceptionState& exception_state) {
  Node* node = current_;
  if (node == root())
    return 0;
  while (1) {
    for (Node* sibling = Strategy::NextNode(*node); sibling;) {
      unsigned accept_node_result = AcceptNode(sibling, exception_state);
      if (exception_state.HadException())
        return 0;
      switch (accept_node_result) {
        case NodeFilter::kFilterAccept:
          current_ = sibling;
          return current_.Get();
        case NodeFilter::kFilterSkip:
          if (sibling->hasChildren()) {
            sibling = Strategy::StartNode(*sibling);
            node = sibling;
            continue;
          }
          break;
        case NodeFilter::kFilterReject:
          break;
      }
      sibling = Strategy::NextNode(*sibling);
    }
    node = node->parentNode();
    if (!node || node == root())
      return 0;
    unsigned accept_node_result = AcceptNode(node, exception_state);
    if (exception_state.HadException())
      return 0;
    if (accept_node_result == NodeFilter::kFilterAccept)
      return 0;
  }
}

Node* TreeWalker::previousSibling(ExceptionState& exception_state) {
  return TraverseSiblings<PreviousNodeTraversalStrategy>(exception_state);
}

Node* TreeWalker::nextSibling(ExceptionState& exception_state) {
  return TraverseSiblings<NextNodeTraversalStrategy>(exception_state);
}

Node* TreeWalker::previousNode(ExceptionState& exception_state) {
  Node* node = current_;
  while (node != root()) {
    while (Node* previous_sibling = node->previousSibling()) {
      node = previous_sibling;
      unsigned accept_node_result = AcceptNode(node, exception_state);
      if (exception_state.HadException())
        return 0;
      if (accept_node_result == NodeFilter::kFilterReject)
        continue;
      while (Node* last_child = node->lastChild()) {
        node = last_child;
        accept_node_result = AcceptNode(node, exception_state);
        if (exception_state.HadException())
          return 0;
        if (accept_node_result == NodeFilter::kFilterReject)
          break;
      }
      if (accept_node_result == NodeFilter::kFilterAccept) {
        current_ = node;
        return current_.Get();
      }
    }
    if (node == root())
      return 0;
    ContainerNode* parent = node->parentNode();
    if (!parent)
      return 0;
    node = parent;
    unsigned accept_node_result = AcceptNode(node, exception_state);
    if (exception_state.HadException())
      return 0;
    if (accept_node_result == NodeFilter::kFilterAccept)
      return SetCurrent(node);
  }
  return 0;
}

Node* TreeWalker::nextNode(ExceptionState& exception_state) {
  Node* node = current_;
Children:
  while (Node* first_child = node->firstChild()) {
    node = first_child;
    unsigned accept_node_result = AcceptNode(node, exception_state);
    if (exception_state.HadException())
      return 0;
    if (accept_node_result == NodeFilter::kFilterAccept)
      return SetCurrent(node);
    if (accept_node_result == NodeFilter::kFilterReject)
      break;
  }
  while (Node* next_sibling =
             NodeTraversal::NextSkippingChildren(*node, root())) {
    node = next_sibling;
    unsigned accept_node_result = AcceptNode(node, exception_state);
    if (exception_state.HadException())
      return 0;
    if (accept_node_result == NodeFilter::kFilterAccept)
      return SetCurrent(node);
    if (accept_node_result == NodeFilter::kFilterSkip)
      goto Children;
  }
  return 0;
}

DEFINE_TRACE(TreeWalker) {
  visitor->Trace(current_);
  NodeIteratorBase::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(TreeWalker) {
  NodeIteratorBase::TraceWrappers(visitor);
}

}  // namespace blink
