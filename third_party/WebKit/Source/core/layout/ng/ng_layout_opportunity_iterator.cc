// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_layout_opportunity_iterator.h"
#include "core/layout/ng/ng_exclusion.h"
#include "core/layout/ng/ng_exclusion_space.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {
namespace {

void AppendNodeToString(const NGLayoutOpportunityTreeNode* node,
                        StringBuilder* string_builder,
                        unsigned indent = 0) {
  DCHECK(string_builder);
  if (!node) {
    string_builder->Append("'null'\n");
    return;
  }

  string_builder->Append(node->ToString());
  string_builder->Append("\n");

  StringBuilder indent_builder;
  for (unsigned i = 0; i < indent; i++)
    indent_builder.Append("\t");

  if (node->IsLeafNode())
    return;

  string_builder->Append(indent_builder.ToString());
  string_builder->Append("Left:\t");
  AppendNodeToString(node->left.get(), string_builder, indent + 2);
  string_builder->Append(indent_builder.ToString());
  string_builder->Append("Right:\t");
  AppendNodeToString(node->right.get(), string_builder, indent + 2);
  string_builder->Append(indent_builder.ToString());
  string_builder->Append("Bottom:\t");
  AppendNodeToString(node->bottom.get(), string_builder, indent + 2);
}

// Collects all opportunities from leaves of Layout Opportunity spatial tree.
void CollectAllOpportunities(const NGLayoutOpportunityTreeNode* node,
                             NGLayoutOpportunities& opportunities) {
  if (!node)
    return;
  if (node->IsLeafNode())
    opportunities.push_back(node->opportunity);
  CollectAllOpportunities(node->left.get(), opportunities);
  CollectAllOpportunities(node->bottom.get(), opportunities);
  CollectAllOpportunities(node->right.get(), opportunities);
}

// Creates layout opportunity from the provided size and the origin point.
NGLayoutOpportunity CreateInitialOpportunity(const NGLogicalSize& size,
                                             const NGBfcOffset& origin_point) {
  NGLayoutOpportunity opportunity;
  // TODO(glebl): Perhaps fix other methods (e.g IsContained) instead of using
  // INT_MAX here.
  opportunity.size.block_size =
      size.block_size >= 0 ? size.block_size : LayoutUnit(INT_MAX);
  opportunity.size.inline_size =
      size.inline_size >= 0 ? size.inline_size : LayoutUnit(INT_MAX);

  // adjust to the origin_point.
  opportunity.offset = origin_point;
  return opportunity;
}

// Whether 2 edges overlap with each other.
bool IsOverlapping(const NGEdge& edge1, const NGEdge& edge2) {
  return std::max(edge1.start, edge2.start) <= std::min(edge1.end, edge2.end);
}

// Creates the *BOTTOM* positioned Layout Opportunity tree node by splitting
// the parent node with the exclusion.
//
// @param parent_node Node that needs to be split.
// @param exclusion Exclusion existed in the parent node constraint space.
// @return New node or nullptr if the new block size == 0.
NGLayoutOpportunityTreeNode* CreateBottomNGLayoutOpportunityTreeNode(
    const NGLayoutOpportunityTreeNode* parent_node,
    const NGBfcRect& exclusion) {
  const NGLayoutOpportunity& parent_opportunity = parent_node->opportunity;
  LayoutUnit bottom_opportunity_block_size =
      parent_opportunity.BlockEndOffset() - exclusion.BlockEndOffset();
  if (bottom_opportunity_block_size > 0) {
    NGLayoutOpportunity opportunity;
    opportunity.offset.line_offset = parent_opportunity.LineStartOffset();
    opportunity.offset.block_offset = exclusion.BlockEndOffset();
    opportunity.size.inline_size = parent_opportunity.InlineSize();
    opportunity.size.block_size = bottom_opportunity_block_size;
    NGEdge exclusion_edge = {/* start */ exclusion.LineStartOffset(),
                             /* end */ exclusion.LineEndOffset()};
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
    const NGBfcRect& exclusion) {
  const NGLayoutOpportunity& parent_opportunity = parent_node->opportunity;

  LayoutUnit left_opportunity_inline_size =
      exclusion.LineStartOffset() - parent_opportunity.LineStartOffset();
  NGEdge node_edge = {/* start */ parent_opportunity.LineStartOffset(),
                      /* end */ exclusion.LineStartOffset()};

  if (left_opportunity_inline_size > 0 &&
      IsOverlapping(parent_node->exclusion_edge, node_edge)) {
    NGLayoutOpportunity opportunity;
    opportunity.offset.line_offset = parent_opportunity.LineStartOffset();
    opportunity.offset.block_offset = parent_opportunity.BlockStartOffset();
    opportunity.size.inline_size = left_opportunity_inline_size;
    opportunity.size.block_size = parent_opportunity.BlockSize();
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
    const NGBfcRect& exclusion) {
  const NGLayoutOpportunity& parent_opportunity = parent_node->opportunity;

  NGEdge node_edge = {/* start */ exclusion.LineEndOffset(),
                      /* end */ parent_opportunity.LineEndOffset()};
  LayoutUnit right_opportunity_inline_size =
      parent_opportunity.LineEndOffset() - exclusion.LineEndOffset();
  if (right_opportunity_inline_size > 0 &&
      IsOverlapping(parent_node->exclusion_edge, node_edge)) {
    NGLayoutOpportunity opportunity;
    opportunity.offset.line_offset = exclusion.LineEndOffset();
    opportunity.offset.block_offset = parent_opportunity.BlockStartOffset();
    opportunity.size.inline_size = right_opportunity_inline_size;
    opportunity.size.block_size = parent_opportunity.BlockSize();
    return new NGLayoutOpportunityTreeNode(opportunity);
  }
  return nullptr;
}

void SplitNGLayoutOpportunityTreeNode(const NGBfcRect& rect,
                                      NGLayoutOpportunityTreeNode* node) {
  node->left.reset(CreateLeftNGLayoutOpportunityTreeNode(node, rect));
  node->right.reset(CreateRightNGLayoutOpportunityTreeNode(node, rect));
  node->bottom.reset(CreateBottomNGLayoutOpportunityTreeNode(node, rect));
}

// Gets/Creates the "TOP" positioned constraint space by splitting
// the parent node with the exclusion.
//
// @param parent_opportunity Parent opportunity that is being split.
// @param exclusion Exclusion existed in the parent node constraint space.
// @return New node or nullptr if the new block size == 0.
NGLayoutOpportunity GetTopSpace(const NGLayoutOpportunity& parent_opportunity,
                                const NGBfcRect& exclusion) {
  LayoutUnit top_opportunity_block_size =
      exclusion.BlockStartOffset() - parent_opportunity.BlockStartOffset();
  if (top_opportunity_block_size > 0) {
    NGLayoutOpportunity opportunity;
    opportunity.offset.line_offset = parent_opportunity.LineStartOffset();
    opportunity.offset.block_offset = parent_opportunity.BlockStartOffset();
    opportunity.size.inline_size = parent_opportunity.InlineSize();
    opportunity.size.block_size = top_opportunity_block_size;
    return opportunity;
  }
  return NGLayoutOpportunity();
}

// Inserts the exclusion into the Layout Opportunity tree.
void InsertExclusion(NGLayoutOpportunityTreeNode* node,
                     const NGExclusion* exclusion,
                     NGLayoutOpportunities& opportunities) {
  // Base case: size of the exclusion is empty.
  if (exclusion->rect.size.IsEmpty())
    return;

  // Base case: there is no node.
  if (!node)
    return;

  // Base case: exclusion is not in the node's constraint space.
  if (!exclusion->rect.IsContained(node->opportunity))
    return;

  if (node->exclusions.IsEmpty()) {
    SplitNGLayoutOpportunityTreeNode(exclusion->rect, node);

    NGLayoutOpportunity top_layout_opp =
        GetTopSpace(node->opportunity, exclusion->rect);
    if (!top_layout_opp.IsEmpty())
      opportunities.push_back(top_layout_opp);

    node->exclusions.push_back(exclusion);
    node->combined_exclusion = WTF::MakeUnique<NGExclusion>(*exclusion);
    return;
  }

  DCHECK(!node->exclusions.IsEmpty());

  if (node->combined_exclusion->MaybeCombineWith(*exclusion)) {
    SplitNGLayoutOpportunityTreeNode(node->combined_exclusion->rect, node);
    node->exclusions.push_back(exclusion);
  } else {
    InsertExclusion(node->left.get(), exclusion, opportunities);
    InsertExclusion(node->bottom.get(), exclusion, opportunities);
    InsertExclusion(node->right.get(), exclusion, opportunities);
  }
}

// Compares exclusions by their top position.
bool CompareNGExclusionsByTopAsc(const NGExclusion& lhs,
                                 const NGExclusion& rhs) {
  return rhs.rect.offset.block_offset > lhs.rect.offset.block_offset;
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
  if (rhs.offset.line_offset > lhs.offset.line_offset) {
    return true;
  }
  if (rhs.offset.line_offset < lhs.offset.line_offset) {
    return false;
  }

  // TOP and LEFT are the same -> Sort by width
  return rhs.size.inline_size < lhs.size.inline_size;
}
}  // namespace

NGLayoutOpportunityIterator::NGLayoutOpportunityIterator(
    const NGExclusionSpace& exclusion_space,
    const NGLogicalSize& available_size,
    const NGBfcOffset& offset)
    : offset_(offset) {
  DCHECK(std::is_sorted(exclusion_space.storage_.begin(),
                        exclusion_space.storage_.end(),
                        &CompareNGExclusionsByTopAsc))
      << "Exclusions are expected to be sorted by TOP";

  NGLayoutOpportunity initial_opportunity =
      CreateInitialOpportunity(available_size, Offset());

  NGLayoutOpportunityTreeNode tree(initial_opportunity);
  for (const auto& exclusion : exclusion_space.storage_) {
    InsertExclusion(&tree, &exclusion, opportunities_);
  }
  CollectAllOpportunities(&tree, opportunities_);
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
