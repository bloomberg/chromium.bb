// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_layout_opportunity_tree_node.h"

#include "platform/heap/Handle.h"
#include "core/layout/ng/ng_constraint_space.h"

namespace blink {

NGLayoutOpportunityTreeNode::NGLayoutOpportunityTreeNode(
    const NGConstraintSpace* space)
    : space(space) {
  exclusion_edge.start = space->Offset().inline_offset;
  exclusion_edge.end = exclusion_edge.start + space->Size().inline_size;
}

NGLayoutOpportunityTreeNode::NGLayoutOpportunityTreeNode(
    NGConstraintSpace* space,
    NGEdge exclusion_edge)
    : space(space), exclusion_edge(exclusion_edge) {}

DEFINE_TRACE(NGLayoutOpportunityTreeNode) {
  visitor->trace(space);
  visitor->trace(left);
  visitor->trace(bottom);
  visitor->trace(right);
  visitor->trace(exclusion);
}

}  // namespace blink
