// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"

namespace blink {

NGPhysicalLineBoxFragment::NGPhysicalLineBoxFragment(
    const ComputedStyle& style,
    NGPhysicalSize size,
    Vector<RefPtr<NGPhysicalFragment>>& children,
    const NGLineHeightMetrics& metrics,
    RefPtr<NGBreakToken> break_token)
    : NGPhysicalFragment(nullptr,
                         style,
                         size,
                         kFragmentLineBox,
                         std::move(break_token)),
      metrics_(metrics) {
  children_.swap(children);
}

LayoutUnit NGPhysicalLineBoxFragment::BaselinePosition(FontBaseline) const {
  // TODO(kojii): Computing other baseline types than the used one is not
  // implemented yet.
  // TODO(kojii): We might need locale/script to look up OpenType BASE table.
  return metrics_.ascent;
}

void NGPhysicalLineBoxFragment::UpdateVisualRect() const {
  LayoutRect visual_rect(LayoutPoint(),
                         LayoutSize(Size().width, Size().height));
  for (const auto& child : children_) {
    LayoutRect child_visual_rect = child->VisualRect();
    child_visual_rect.Move(child->Offset().left, child->Offset().top);
    visual_rect.Unite(child_visual_rect);
  }
  SetVisualRect(visual_rect);
}

}  // namespace blink
