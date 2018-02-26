// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/list/ng_list_layout_algorithm.h"

#include "core/layout/LayoutListMarker.h"
#include "core/layout/ng/inline/ng_inline_box_state.h"
#include "core/layout/ng/inline/ng_inline_item_result.h"
#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_result.h"

namespace blink {

namespace {

std::pair<LayoutUnit, LayoutUnit> InlineMarginsForOutside(
    const NGFragment& list_marker,
    const NGConstraintSpace& constraint_space) {
  DCHECK(&list_marker);
  bool is_image = false;  // TODO(kojii): implement
  return LayoutListMarker::InlineMarginsForOutside(
      list_marker.Style(), is_image, list_marker.InlineSize());
}

}  // namespace

void NGListLayoutAlgorithm::SetListMarkerPosition(
    const NGConstraintSpace& constraint_space,
    const NGLineInfo& line_info,
    LayoutUnit line_width,
    NGLineBoxFragmentBuilder::Child* list_marker_child) {
  DCHECK(list_marker_child->PhysicalFragment());
  NGFragment list_marker_fragment(constraint_space.GetWritingMode(),
                                  *list_marker_child->PhysicalFragment());
  auto margins =
      InlineMarginsForOutside(list_marker_fragment, constraint_space);
  LayoutUnit line_offset = IsLtr(line_info.BaseDirection())
                               ? margins.first
                               : line_width + margins.second;
  list_marker_child->offset.inline_offset = line_offset;
}

void NGListLayoutAlgorithm::AddListMarkerForBlockContent(
    NGBlockNode list_marker_node,
    const NGConstraintSpace& constraint_space,
    const NGPhysicalFragment& content,
    NGLogicalOffset offset,
    NGFragmentBuilder* container_builder) {
  // Layout the list marker.
  scoped_refptr<NGLayoutResult> list_marker_layout_result =
      list_marker_node.LayoutAtomicInline(constraint_space,
                                          constraint_space.UseFirstLineStyle());
  DCHECK(list_marker_layout_result->PhysicalFragment());
  NGBoxFragment list_marker_fragment(
      constraint_space.GetWritingMode(),
      ToNGPhysicalBoxFragment(*list_marker_layout_result->PhysicalFragment()));

  // Compute the inline offset of the marker from its margins.
  // The marker is relative to the border box of the list item and has nothing
  // to do with the content offset.
  auto margins =
      InlineMarginsForOutside(list_marker_fragment, constraint_space);
  offset.inline_offset = margins.first;

  // Compute the block offset of the marker by aligning the baseline of the
  // marker to the first baseline of the content.
  FontBaseline baseline_type =
      IsHorizontalWritingMode(constraint_space.GetWritingMode())
          ? kAlphabeticBaseline
          : kIdeographicBaseline;
  NGLineHeightMetrics list_marker_metrics =
      list_marker_fragment.BaselineMetrics(
          {NGBaselineAlgorithmType::kAtomicInline, baseline_type},
          constraint_space);
  NGLineHeightMetrics content_metrics;
  if (content.IsLineBox()) {
    content_metrics = ToNGPhysicalLineBoxFragment(content).Metrics();
  } else {
    NGBoxFragment content_fragment(constraint_space.GetWritingMode(),
                                   ToNGPhysicalBoxFragment(content));
    content_metrics = content_fragment.BaselineMetrics(
        {NGBaselineAlgorithmType::kFirstLine, baseline_type}, constraint_space);
  }

  // |offset.block_offset| is at the top of the content. Adjust it to the top of
  // the list marker by adding the differences of the ascent between content's
  // first line and the list marker.
  offset.block_offset += content_metrics.ascent - list_marker_metrics.ascent;

  DCHECK(container_builder);
  container_builder->AddChild(std::move(list_marker_layout_result), offset);
}

}  // namespace blink
