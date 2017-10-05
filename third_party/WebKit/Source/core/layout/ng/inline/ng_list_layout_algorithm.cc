// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_list_layout_algorithm.h"

#include "core/layout/LayoutListMarker.h"
#include "core/layout/ng/inline/ng_inline_box_state.h"
#include "core/layout/ng/inline/ng_inline_item_result.h"
#include "core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_layout_result.h"

namespace blink {

void NGListLayoutAlgorithm::SetListMarkerPosition(
    const NGConstraintSpace& constraint_space,
    const NGLineInfo& line_info,
    LayoutUnit line_width,
    unsigned list_marker_index,
    NGLineBoxFragmentBuilder* line_box) {
  // The list marker fragment is at next to a text fragment, see
  // NGInlineLayoutAlgorithm::PlaceLayoutResult().
  if (!RuntimeEnabledFeatures::LayoutNGPaintFragmentsEnabled())
    list_marker_index++;
  const NGPhysicalFragment& physical_fragment =
      *line_box->Children()[list_marker_index];
  NGFragment list_marker(constraint_space.WritingMode(), physical_fragment);

  // Compute the inline offset relative to the line.
  bool is_image = false;  // TODO(kojii): implement
  auto margins = LayoutListMarker::InlineMarginsForOutside(
      physical_fragment.Style(), is_image, list_marker.InlineSize());
  LayoutUnit line_offset = IsLtr(line_info.BaseDirection())
                               ? margins.first
                               : line_width + margins.second;
  Vector<NGLogicalOffset>& offsets = line_box->MutableOffsets();
  offsets[list_marker_index].inline_offset = line_offset;
  if (!RuntimeEnabledFeatures::LayoutNGPaintFragmentsEnabled())
    offsets[list_marker_index - 1].inline_offset = line_offset;
}

}  // namespace blink
