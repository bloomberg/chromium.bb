// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutOpportunityTreeNode_h
#define NGLayoutOpportunityTreeNode_h

#include "platform/heap/Handle.h"
#include "core/layout/ng/ng_units.h"

namespace blink {

// 3 node R-Tree that represents available space(left, bottom, right) or
// layout opportunity after the parent spatial rectangle is split by the
// exclusion rectangle.
struct NGLayoutOpportunityTreeNode
    : public GarbageCollected<NGLayoutOpportunityTreeNode> {
  // Default constructor.
  // Creates a Layout Opportunity tree node that is limited by it's own edge
  // from above.
  // @param space Constraint space associated with this node.
  NGLayoutOpportunityTreeNode(const NGConstraintSpace* space) : space(space) {
    exclusion_edge.start = space->Offset().inline_offset;
    exclusion_edge.end = exclusion_edge.start + space->Size().inline_size;
  }

  // Constructor that creates a node with explicitly set exclusion edge.
  // @param space Constraint space associated with this node.
  // @param exclusion_edge Edge that limits this node's space from above.
  NGLayoutOpportunityTreeNode(NGConstraintSpace* space, NGEdge exclusion_edge)
      : space(space), exclusion_edge(exclusion_edge) {}

  // Constraint space that is associated with this node.
  Member<const NGConstraintSpace> space;

  // Children of the node.
  Member<NGLayoutOpportunityTreeNode> left;
  Member<NGLayoutOpportunityTreeNode> bottom;
  Member<NGLayoutOpportunityTreeNode> right;

  // Exclusion that split apart this layout opportunity.
  Member<const NGExclusion> exclusion;

  // Edge that limits this layout opportunity from above.
  NGEdge exclusion_edge;

  // Whether this node is a leaf node.
  // The node is a leaf if it doen't have an exclusion that splits it apart.
  bool IsLeafNode() const { return !exclusion; }

  DEFINE_INLINE_TRACE() {
    visitor->trace(space);
    visitor->trace(left);
    visitor->trace(bottom);
    visitor->trace(right);
    visitor->trace(exclusion);
  }
};

}  // namespace blink
#endif  // NGLayoutOpportunityTreeNode_h
