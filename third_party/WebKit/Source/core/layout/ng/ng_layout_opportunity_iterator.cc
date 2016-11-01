// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_layout_opportunity_iterator.h"

#include "core/layout/ng/ng_physical_constraint_space.h"
#include "core/layout/ng/ng_units.h"
#include "wtf/NonCopyingSort.h"

namespace blink {
namespace {

// Collects all opportunities from leaves of Layout Opportunity spatial tree.
void CollectAllOpportunities(const NGLayoutOpportunityTreeNode* node,
                             NGLayoutOpportunities& opportunities) {
  if (!node)
    return;
  if (node->IsLeafNode())
    opportunities.append(node->opportunity);
  CollectAllOpportunities(node->left, opportunities);
  CollectAllOpportunities(node->bottom, opportunities);
  CollectAllOpportunities(node->right, opportunities);
}

// Creates layout opportunity from the provided space and the origin point.
NGLayoutOpportunity CreateLayoutOpportunityFromConstraintSpace(
    const NGConstraintSpace& space,
    const NGLogicalOffset& origin_point) {
  NGLayoutOpportunity opportunity;
  opportunity.offset = space.Offset();
  opportunity.size = space.Size();

  // adjust to the origin_point.
  opportunity.offset += origin_point;
  opportunity.size.inline_size -= origin_point.inline_offset;
  opportunity.size.block_size -= origin_point.block_offset;
  return opportunity;
}

// Whether 2 edges overlap with each other.
bool IsOverlapping(const NGEdge& edge1, const NGEdge& edge2) {
  return std::max(edge1.start, edge2.start) <= std::min(edge1.end, edge2.end);
}

// Whether the exclusion is out of bounds of the LayoutNG constraint space.
bool IsExclusionWithinSpace(const NGLayoutOpportunity& opportunity,
                            const NGExclusion& exclusion) {
  LayoutUnit left = opportunity.offset.inline_offset;
  LayoutUnit top = opportunity.offset.block_offset;
  LayoutUnit right = left + opportunity.size.inline_size;
  LayoutUnit bottom = top + opportunity.size.block_size;

  return !(exclusion.Right() <= left || exclusion.Bottom() <= top ||
           exclusion.Left() >= right || exclusion.Top() >= bottom);
}

// Creates the *BOTTOM* positioned Layout Opportunity tree node by splitting
// the parent node with the exclusion.
//
// @param parent_node Node that needs to be split.
// @param exclusion Exclusion existed in the parent node constraint space.
// @return New node or nullptr if the new block size == 0.
NGLayoutOpportunityTreeNode* CreateBottomNGLayoutOpportunityTreeNode(
    const NGLayoutOpportunityTreeNode* parent_node,
    const NGExclusion& exclusion) {
  const NGLayoutOpportunity& parent_opportunity = parent_node->opportunity;
  LayoutUnit left = parent_opportunity.offset.inline_offset;
  LayoutUnit top = exclusion.Bottom();
  LayoutUnit right = left + parent_opportunity.size.inline_size;
  LayoutUnit bottom = parent_opportunity.offset.block_offset +
                      parent_opportunity.size.block_size;

  NGEdge exclusion_edge = {exclusion.Left(), exclusion.Right()};
  LayoutUnit block_size = bottom - top;
  if (block_size > 0) {
    NGLayoutOpportunity opportunity(left, top, right - left, block_size);
    return new NGLayoutOpportunityTreeNode(opportunity, exclusion_edge);
  }
  return nullptr;
}

// Creates the *LEFT* positioned Layout Opportunity tree node by splitting
// the parent node with the exclusion.
//
// @param parent_node Node that needs to be split.
// @param exclusion Exclusion existed in the parent node constraint space.
// @return New node or nullptr if the new inline size == 0 or the parent's
// exclusion edge doesn't limit the new node's constraint space.
NGLayoutOpportunityTreeNode* CreateLeftNGLayoutOpportunityTreeNode(
    const NGLayoutOpportunityTreeNode* parent_node,
    const NGExclusion& exclusion) {
  const NGLayoutOpportunity& parent_opportunity = parent_node->opportunity;
  LayoutUnit left = parent_opportunity.offset.inline_offset;
  LayoutUnit top = parent_opportunity.offset.block_offset;
  LayoutUnit right = exclusion.Left();
  LayoutUnit bottom = top + parent_opportunity.size.block_size;

  NGEdge node_edge = {left, right};
  LayoutUnit inline_size = right - left;
  if (inline_size > 0 &&
      IsOverlapping(parent_node->exclusion_edge, node_edge)) {
    NGLayoutOpportunity opportunity(left, top, inline_size, bottom - top);
    return new NGLayoutOpportunityTreeNode(opportunity);
  }
  return nullptr;
}

// Creates the *RIGHT* positioned Layout Opportunity tree node by splitting
// the parent node with the exclusion.
//
// @param parent_node Node that needs to be split.
// @param exclusion Exclusion existed in the parent node constraint space.
// @return New node or nullptr if the new inline size == 0 or the parent's
// exclusion edge doesn't limit the new node's constraint space.
NGLayoutOpportunityTreeNode* CreateRightNGLayoutOpportunityTreeNode(
    const NGLayoutOpportunityTreeNode* parent_node,
    const NGExclusion& exclusion) {
  const NGLayoutOpportunity& parent_opportunity = parent_node->opportunity;
  LayoutUnit left = exclusion.Right();
  LayoutUnit top = parent_opportunity.offset.block_offset;
  LayoutUnit right = parent_opportunity.offset.inline_offset +
                     parent_opportunity.size.inline_size;
  LayoutUnit bottom = top + parent_opportunity.size.block_size;

  NGEdge node_edge = {left, right};
  LayoutUnit inline_size = right - left;
  if (inline_size > 0 &&
      IsOverlapping(parent_node->exclusion_edge, node_edge)) {
    NGLayoutOpportunity opportunity(left, top, inline_size, bottom - top);
    return new NGLayoutOpportunityTreeNode(opportunity);
  }
  return nullptr;
}

// Gets/Creates the "TOP" positioned constraint space by splitting
// the parent node with the exclusion.
//
// @param parent_node Node that needs to be split.
// @param exclusion Exclusion existed in the parent node constraint space.
// @return New node or nullptr if the new block size == 0.
NGLayoutOpportunity GetTopSpace(const NGLayoutOpportunity& rect,
                                const NGExclusion& exclusion) {
  LayoutUnit left = rect.offset.inline_offset;
  LayoutUnit top = rect.offset.block_offset;
  LayoutUnit right = left + rect.size.inline_size;
  LayoutUnit bottom = exclusion.Top();

  LayoutUnit block_size = bottom - top;
  if (block_size > 0)
    return NGLayoutOpportunity(left, top, right - left, block_size);

  return NGLayoutOpportunity();
}

// Inserts the exclusion into the Layout Opportunity tree.
void InsertExclusion(NGLayoutOpportunityTreeNode* node,
                     const NGExclusion* exclusion,
                     NGLayoutOpportunities& opportunities) {
  // Base case: there is no node.
  if (!node)
    return;

  // Base case: exclusion is not in the node's constraint space.
  if (!IsExclusionWithinSpace(node->opportunity, *exclusion))
    return;

  if (node->exclusion) {
    InsertExclusion(node->left, exclusion, opportunities);
    InsertExclusion(node->bottom, exclusion, opportunities);
    InsertExclusion(node->right, exclusion, opportunities);
    return;
  }

  // Split the current node.
  node->left = CreateLeftNGLayoutOpportunityTreeNode(node, *exclusion);
  node->right = CreateRightNGLayoutOpportunityTreeNode(node, *exclusion);
  node->bottom = CreateBottomNGLayoutOpportunityTreeNode(node, *exclusion);

  NGLayoutOpportunity top_layout_opp =
      GetTopSpace(node->opportunity, *exclusion);
  if (!top_layout_opp.IsEmpty())
    opportunities.append(top_layout_opp);

  node->exclusion = exclusion;
}

// Compares exclusions by their top position.
bool CompareNGExclusionsByTopAsc(const Member<const NGExclusion>& lhs,
                                 const Member<const NGExclusion>& rhs) {
  return rhs->Top() > lhs->Top();
}

// Compares Layout Opportunities by Start Point.
// Start point is a TopLeft position from where inline content can potentially
// start positioning itself.
bool CompareNGLayoutOpportunitesByStartPoint(const NGLayoutOpportunity& lhs,
                                             const NGLayoutOpportunity& rhs) {
  // sort by TOP.
  if (rhs.offset.block_offset > lhs.offset.block_offset) {
    return true;
  }
  if (rhs.offset.block_offset < lhs.offset.block_offset) {
    return false;
  }

  // TOP is the same -> Sort by LEFT
  if (rhs.offset.inline_offset > lhs.offset.inline_offset) {
    return true;
  }
  if (rhs.offset.inline_offset < lhs.offset.inline_offset) {
    return false;
  }

  // TOP and LEFT are the same -> Sort by width
  return rhs.size.inline_size < lhs.size.inline_size;
}

}  // namespace

NGLayoutOpportunityIterator::NGLayoutOpportunityIterator(
    NGConstraintSpace* space,
    const NGLogicalOffset origin_point,
    const NGLogicalOffset leader_point)
    : constraint_space_(space), leader_point_(leader_point) {
  // TODO(chrome-layout-team): Combine exclusions that shadow each other.
  auto exclusions = constraint_space_->PhysicalSpace()->Exclusions();
  DCHECK(std::is_sorted(exclusions.begin(), exclusions.end(),
                        &CompareNGExclusionsByTopAsc))
      << "Exclusions are expected to be sorted by TOP";

  NGLayoutOpportunity initial_opportunity =
      CreateLayoutOpportunityFromConstraintSpace(*space, origin_point);
  opportunity_tree_root_ = new NGLayoutOpportunityTreeNode(initial_opportunity);

  for (const auto exclusion : exclusions) {
    InsertExclusion(MutableOpportunityTreeRoot(), exclusion, opportunities_);
  }
  CollectAllOpportunities(OpportunityTreeRoot(), opportunities_);
  std::sort(opportunities_.begin(), opportunities_.end(),
            &CompareNGLayoutOpportunitesByStartPoint);

  opportunity_iter_ = opportunities_.begin();
}

const NGLayoutOpportunity NGLayoutOpportunityIterator::Next() {
  if (opportunity_iter_ == opportunities_.end())
    return NGLayoutOpportunity();
  auto* opportunity = opportunity_iter_;
  opportunity_iter_++;
  return NGLayoutOpportunity(*opportunity);
}

}  // namespace blink
