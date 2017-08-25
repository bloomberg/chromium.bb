// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_layout_opportunity_tree_node.h"

#include "platform/wtf/text/WTFString.h"

namespace blink {

NGLayoutOpportunityTreeNode::NGLayoutOpportunityTreeNode(
    const NGBfcRect opportunity)
    : opportunity(opportunity), combined_exclusion(nullptr) {
  exclusion_edge.start = opportunity.offset.line_offset;
  exclusion_edge.end = exclusion_edge.start + opportunity.size.inline_size;
}

NGLayoutOpportunityTreeNode::NGLayoutOpportunityTreeNode(
    const NGBfcRect opportunity,
    NGEdge exclusion_edge)
    : opportunity(opportunity),
      exclusion_edge(exclusion_edge),
      combined_exclusion(nullptr) {}

String NGLayoutOpportunityTreeNode::ToString() const {
  return String::Format("Opportunity: '%s' Exclusion: '%s'",
                        opportunity.ToString().Ascii().data(),
                        combined_exclusion
                            ? combined_exclusion->ToString().Ascii().data()
                            : "null");
}

std::ostream& operator<<(std::ostream& stream,
                         const NGLayoutOpportunityTreeNode& value) {
  return stream << value.ToString();
}

std::ostream& operator<<(std::ostream& out,
                         const NGLayoutOpportunityTreeNode* value) {
  return out << (value ? value->ToString() : "(null)");
}

}  // namespace blink
