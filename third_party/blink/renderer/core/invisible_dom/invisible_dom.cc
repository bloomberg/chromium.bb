// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/invisible_dom/invisible_dom.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"

namespace blink {
bool InvisibleDOM::IsInsideInvisibleSubtree(const Node& node) {
  for (Node& ancestor : FlatTreeTraversal::InclusiveAncestorsOf(node)) {
    if (ancestor.IsElementNode() &&
        ToElement(ancestor).HasInvisibleAttribute()) {
      return true;
    }
  }
  return false;
}

Element* InvisibleDOM::InvisibleRoot(const Node& node) {
  Element* root = nullptr;
  for (Node& ancestor : FlatTreeTraversal::InclusiveAncestorsOf(node)) {
    if (ancestor.IsElementNode() &&
        ToElement(ancestor).HasInvisibleAttribute()) {
      root = &ToElement(ancestor);
    }
  }
  return root;
}

}  // namespace blink
