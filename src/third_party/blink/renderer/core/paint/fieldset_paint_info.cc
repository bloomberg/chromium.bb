// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/fieldset_paint_info.h"

#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

FieldsetPaintInfo::FieldsetPaintInfo(const ComputedStyle& fieldset_style,
                                     LayoutSize fieldset_size,
                                     LayoutRectOutsets fieldset_borders,
                                     LayoutRect legend_border_box) {
  if (fieldset_style.IsHorizontalWritingMode()) {
    // horizontal-tb
    LayoutUnit legend_size = legend_border_box.Height();
    LayoutUnit border_size = fieldset_borders.Top();
    LayoutUnit legend_excess_size = legend_size - border_size;
    if (legend_excess_size > LayoutUnit())
      border_outsets.SetTop(legend_excess_size / 2);
    legend_cutout_rect = LayoutRect(legend_border_box.X(), LayoutUnit(),
                                    legend_border_box.Width(),
                                    std::max(legend_size, border_size));
  } else {
    LayoutUnit legend_size = legend_border_box.Width();
    LayoutUnit border_size;
    if (fieldset_style.IsFlippedBlocksWritingMode()) {
      // vertical-rl
      border_size = fieldset_borders.Right();
      LayoutUnit legend_excess_size = legend_size - border_size;
      if (legend_excess_size > LayoutUnit())
        border_outsets.SetRight(legend_excess_size / 2);
    } else {
      // vertical-lr
      border_size = fieldset_borders.Left();
      LayoutUnit legend_excess_size = legend_size - border_size;
      if (legend_excess_size > LayoutUnit())
        border_outsets.SetLeft(legend_excess_size / 2);
    }
    LayoutUnit legend_total_block_size = std::max(legend_size, border_size);
    legend_cutout_rect =
        LayoutRect(LayoutUnit(), legend_border_box.Y(), legend_total_block_size,
                   legend_border_box.Height());
    if (fieldset_style.IsFlippedBlocksWritingMode()) {
      // Offset cutout to right fieldset edge for vertical-rl
      LayoutUnit clip_x = fieldset_size.Width() - legend_total_block_size;
      legend_cutout_rect.Move(clip_x, LayoutUnit());
    }
  }
}

}  // namespace blink
