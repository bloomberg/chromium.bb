// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_layout_opportunity_tree_node.h"

namespace blink {

NGLayoutOpportunityTreeNode::NGLayoutOpportunityTreeNode(
    const NGLogicalRect opportunity)
    : opportunity(opportunity), combined_exclusion(nullptr) {
  exclusion_edge.start = opportunity.offset.inline_offset;
  exclusion_edge.end = exclusion_edge.start + opportunity.size.inline_size;
}

NGLayoutOpportunityTreeNode::NGLayoutOpportunityTreeNode(
    const NGLogicalRect opportunity,
    NGEdge exclusion_edge)
    : opportunity(opportunity),
      exclusion_edge(exclusion_edge),
      combined_exclusion(nullptr) {}

String NGLayoutOpportunityTreeNode::ToString() const {
  return String::Format("Opportunity: '%s' Exclusion: '%s'",
                        opportunity.ToString().Ascii().Data(),
                        combined_exclusion
                            ? combined_exclusion->ToString().Ascii().Data()
                            : "null");
}

}  // namespace blink
