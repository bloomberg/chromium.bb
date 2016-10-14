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
    opportunities.append(node->space);
  CollectAllOpportunities(node->left, opportunities);
  CollectAllOpportunities(node->bottom, opportunities);
  CollectAllOpportunities(node->right, opportunities);
}

// Whether 2 edges overlap with each other.
bool IsOverlapping(const NGEdge& edge1, const NGEdge& edge2) {
  return std::max(edge1.start, edge2.start) <= std::min(edge1.end, edge2.end);
}

// Whether the exclusion is out of bounds of the LayoutNG constraint space.
bool IsExclusionWithinSpace(const NGConstraintSpace& space,
                            const NGExclusion& exclusion) {
  LayoutUnit left = space.Offset().inline_offset;
  LayoutUnit top = space.Offset().block_offset;
  LayoutUnit right = left + space.Size().inline_size;
  LayoutUnit bottom = top + space.Size().block_size;

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
  const NGConstraintSpace& parent_space = *parent_node->space;
  LayoutUnit left = parent_space.Offset().inline_offset;
  LayoutUnit top = exclusion.Bottom();
  LayoutUnit right = left + parent_space.Size().inline_size;
  LayoutUnit bottom =
      parent_space.Offset().block_offset + parent_space.Size().block_size;

  NGEdge exclusion_edge = {exclusion.Left(), exclusion.Right()};
  LayoutUnit block_size = bottom - top;
  if (block_size > 0) {
    auto* space =
        new NGConstraintSpace(parent_space, NGLogicalOffset(left, top),
                              NGLogicalSize(right - left, block_size));
    return new NGLayoutOpportunityTreeNode(space, exclusion_edge);
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
  const NGConstraintSpace& parent_space = *parent_node->space;
  LayoutUnit left = parent_space.Offset().inline_offset;
  LayoutUnit top = parent_space.Offset().block_offset;
  LayoutUnit right = exclusion.Left();
  LayoutUnit bottom = top + parent_space.Size().block_size;

  NGEdge node_edge = {left, right};
  LayoutUnit inline_size = right - left;
  if (inline_size > 0 &&
      IsOverlapping(parent_node->exclusion_edge, node_edge)) {
    auto* space =
        new NGConstraintSpace(parent_space, NGLogicalOffset(left, top),
                              NGLogicalSize(inline_size, bottom - top));
    return new NGLayoutOpportunityTreeNode(space);
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
  const NGConstraintSpace& parent_space = *parent_node->space;
  LayoutUnit left = exclusion.Right();
  LayoutUnit top = parent_space.Offset().block_offset;
  LayoutUnit right =
      parent_space.Offset().inline_offset + parent_space.Size().inline_size;
  LayoutUnit bottom = top + parent_space.Size().block_size;

  NGEdge node_edge = {left, right};
  LayoutUnit inline_size = right - left;
  if (inline_size > 0 &&
      IsOverlapping(parent_node->exclusion_edge, node_edge)) {
    auto* space =
        new NGConstraintSpace(parent_space, NGLogicalOffset(left, top),
                              NGLogicalSize(inline_size, bottom - top));
    return new NGLayoutOpportunityTreeNode(space);
  }
  return nullptr;
}

// Gets/Creates the "TOP" positioned constraint space by splitting
// the parent node with the exclusion.
//
// @param parent_node Node that needs to be split.
// @param exclusion Exclusion existed in the parent node constraint space.
// @return New node or nullptr if the new block size == 0.
NGConstraintSpace* GetTopSpace(const NGConstraintSpace& space,
                               const NGExclusion& exclusion) {
  LayoutUnit left = space.Offset().inline_offset;
  LayoutUnit top = space.Offset().block_offset;
  LayoutUnit right = left + space.Size().inline_size;
  LayoutUnit bottom = exclusion.Top();

  LayoutUnit block_size = bottom - top;
  if (block_size > 0)
    return new NGConstraintSpace(space, NGLogicalOffset(left, top),
                                 NGLogicalSize(right - left, block_size));
  return nullptr;
}

// Inserts the exclusion into the Layout Opportunity tree.
void InsertExclusion(NGLayoutOpportunityTreeNode* node,
                     const NGExclusion* exclusion,
                     NGLayoutOpportunities& opportunities) {
  // Base case: exclusion is not in the node's constraint space.
  if (!IsExclusionWithinSpace(*node->space, *exclusion))
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

  if (auto* topSpace = GetTopSpace(*node->space, *exclusion))
    opportunities.append(topSpace);
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
bool CompareNGLayoutOpportunitesByStartPoint(
    const Member<const NGLayoutOpportunity>& lhs,
    const Member<const NGLayoutOpportunity>& rhs) {
  // sort by TOP.
  if (rhs->Offset().block_offset > lhs->Offset().block_offset) {
    return true;
  }
  if (rhs->Offset().block_offset < lhs->Offset().block_offset) {
    return false;
  }

  // TOP is the same -> Sort by LEFT
  if (rhs->Offset().inline_offset > lhs->Offset().inline_offset) {
    return true;
  }
  if (rhs->Offset().inline_offset < lhs->Offset().inline_offset) {
    return false;
  }

  // TOP and LEFT are the same -> Sort by width
  return rhs->Size().inline_size < lhs->Size().inline_size;
}

}  // namespace

NGLayoutOpportunityIterator::NGLayoutOpportunityIterator(
    NGConstraintSpace* space,
    unsigned clear,
    bool for_inline_or_bfc)
    : constraint_space_(space) {
  // TODO(chrome-layout-team): Combine exclusions that shadow each other.
  auto exclusions = constraint_space_->PhysicalSpace()->Exclusions();
  DCHECK(std::is_sorted(exclusions.begin(), exclusions.end(),
                        &CompareNGExclusionsByTopAsc))
      << "Exclusions are expected to be sorted by TOP";

  opportunity_tree_root_ = new NGLayoutOpportunityTreeNode(space);
  for (const auto exclusion : exclusions) {
    InsertExclusion(opportunity_tree_root_, exclusion, opportunities_);
  }
  CollectAllOpportunities(opportunity_tree_root_, opportunities_);
  std::sort(opportunities_.begin(), opportunities_.end(),
            &CompareNGLayoutOpportunitesByStartPoint);
  opportunity_iter_ = opportunities_.begin();
}

const NGConstraintSpace* NGLayoutOpportunityIterator::Next() {
  if (opportunity_iter_ == opportunities_.end())
    return nullptr;
  auto* opportunity = opportunity_iter_->get();
  opportunity_iter_++;
  return opportunity;
}

}  // namespace blink
