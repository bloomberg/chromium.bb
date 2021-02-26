// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_types.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_node.h"

namespace blink {

class NGBlockBreakToken;
class NGTableBorders;

class CORE_EXPORT NGTableLayoutAlgorithm
    : public NGLayoutAlgorithm<NGTableNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  explicit NGTableLayoutAlgorithm(const NGLayoutAlgorithmParams& params)
      : NGLayoutAlgorithm(params) {}
  scoped_refptr<const NGLayoutResult> Layout() override;

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesInput&) const override;

  static LayoutUnit ComputeTableInlineSize(const NGTableNode& node,
                                           const NGConstraintSpace& space,
                                           const NGBoxStrut& border_padding);

 private:
  void ComputeRows(const LayoutUnit table_grid_inline_size,
                   const NGTableGroupedChildren& grouped_children,
                   const NGTableTypes::ColumnLocations& column_locations,
                   const NGTableBorders& table_borders,
                   const LogicalSize& border_spacing,
                   const NGBoxStrut& table_border_padding,
                   bool is_fixed_layout,
                   NGTableTypes::Rows* rows,
                   NGTableTypes::CellBlockConstraints* cell_block_constraints,
                   NGTableTypes::Sections* sections,
                   LayoutUnit* minimal_table_grid_block_size);

  void GenerateCaptionFragments(const NGTableGroupedChildren& grouped_children,
                                LayoutUnit table_inline_size,
                                ECaptionSide caption_side,
                                LayoutUnit* table_block_offset);

  void ComputeTableSpecificFragmentData(
      const NGTableGroupedChildren& grouped_children,
      const NGTableTypes::ColumnLocations& column_locations,
      const NGTableTypes::Rows& rows,
      const NGTableBorders& table_borders,
      const PhysicalRect& table_grid_rect,
      const LogicalSize& border_spacing,
      LayoutUnit table_grid_block_size);

  scoped_refptr<const NGLayoutResult> GenerateFragment(
      LayoutUnit table_inline_size,
      LayoutUnit minimal_table_grid_block_size,
      const NGTableGroupedChildren& grouped_children,
      const NGTableTypes::ColumnLocations& column_locations,
      const NGTableTypes::Rows& rows,
      const NGTableTypes::CellBlockConstraints& cell_block_constraints,
      const NGTableTypes::Sections& sections,
      const NGTableBorders& table_borders,
      const LogicalSize& border_spacing);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_H_
