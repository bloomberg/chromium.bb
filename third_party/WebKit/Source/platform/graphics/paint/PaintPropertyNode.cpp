// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "PaintPropertyNode.h"

#include "ClipPaintPropertyNode.h"
#include "EffectPaintPropertyNode.h"
#include "ScrollPaintPropertyNode.h"
#include "TransformPaintPropertyNode.h"

namespace blink {

namespace {

template <typename NodeType>
unsigned NodeDepth(const NodeType& node) {
  unsigned depth = 0;
  for (const NodeType* n = &node; n; n = n->Parent())
    depth++;
  return depth;
}

template <typename NodeType>
const NodeType& LowestCommonAncestorTemplate(const NodeType& a,
                                             const NodeType& b) {
  // Measure both depths.
  unsigned depth_a = NodeDepth(a);
  unsigned depth_b = NodeDepth(b);

  const auto* a_ptr = &a;
  const auto* b_ptr = &b;

  // Make it so depthA >= depthB.
  if (depth_a < depth_b) {
    std::swap(a_ptr, b_ptr);
    std::swap(depth_a, depth_b);
  }

  // Make it so depthA == depthB.
  while (depth_a > depth_b) {
    a_ptr = a_ptr->Parent();
    depth_a--;
  }

  // Walk up until we find the ancestor.
  while (a_ptr != b_ptr) {
    a_ptr = a_ptr->Parent();
    b_ptr = b_ptr->Parent();
  }

  DCHECK(a_ptr) << "Malformed property tree. All nodes must be descendant of "
                   "the same root.";
  return *a_ptr;
}

}  // namespace

const TransformPaintPropertyNode& LowestCommonAncestor(
    const TransformPaintPropertyNode& a,
    const TransformPaintPropertyNode& b) {
  return LowestCommonAncestorTemplate(a, b);
}

const ClipPaintPropertyNode& LowestCommonAncestor(
    const ClipPaintPropertyNode& a,
    const ClipPaintPropertyNode& b) {
  return LowestCommonAncestorTemplate(a, b);
}

const EffectPaintPropertyNode& LowestCommonAncestor(
    const EffectPaintPropertyNode& a,
    const EffectPaintPropertyNode& b) {
  return LowestCommonAncestorTemplate(a, b);
}

const ScrollPaintPropertyNode& LowestCommonAncestor(
    const ScrollPaintPropertyNode& a,
    const ScrollPaintPropertyNode& b) {
  return LowestCommonAncestorTemplate(a, b);
}

}  // namespace blink
