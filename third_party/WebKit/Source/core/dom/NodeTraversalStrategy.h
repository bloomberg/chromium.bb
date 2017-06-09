// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NodeTraversalStrategy_h
#define NodeTraversalStrategy_h

#include "core/dom/Node.h"

namespace blink {

// NodeTraversalStrategy is helpful to implement algorithm templates independent
// from tree traversal direction, and to instantiate them with specific
// direction.
//
// For example,
//
// template <Strategy> void IterateOverChildren(const Node& parent) {
//   for (Node* child = Strategy::StartNode(parent); child;
//       child = Strategy::NextNode(*child)) {
//     ...
//   }
// }
// IterateOverChildren<NextNodeTraversalStrategy>(parent) iterates forward,
// IterateOverChildren<PreviousNodeTraversalStrategy>(parent) iterates backward.

class NextNodeTraversalStrategy {
  STATIC_ONLY(NextNodeTraversalStrategy);

 public:
  static Node* StartNode(const Node& parent) { return parent.firstChild(); }
  static Node* NextNode(const Node& current) { return current.nextSibling(); }
};

class PreviousNodeTraversalStrategy {
  STATIC_ONLY(PreviousNodeTraversalStrategy);

 public:
  static Node* StartNode(const Node& parent) { return parent.lastChild(); }
  static Node* NextNode(const Node& current) {
    return current.previousSibling();
  }
};

}  // namespace blink

#endif  // NodeTraversalStrategy_h
