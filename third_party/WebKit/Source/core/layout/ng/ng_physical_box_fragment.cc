// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_box_fragment.h"

#include "core/layout/ng/ng_unpositioned_float.h"

namespace blink {

NGPhysicalBoxFragment::NGPhysicalBoxFragment(
    LayoutObject* layout_object,
    NGPhysicalSize size,
    NGPhysicalSize overflow,
    Vector<RefPtr<NGPhysicalFragment>>& children,
    Vector<NGPositionedFloat>& positioned_floats,
    Vector<NGBaseline>& baselines,
    unsigned border_edges,  // NGBorderEdges::Physical
    RefPtr<NGBreakToken> break_token)
    : NGPhysicalFragment(layout_object,
                         size,
                         kFragmentBox,
                         std::move(break_token)),
      overflow_(overflow),
      positioned_floats_(positioned_floats) {
  children_.swap(children);
  baselines_.swap(baselines);
  border_edge_ = border_edges;
}

const NGBaseline* NGPhysicalBoxFragment::Baseline(
    const NGBaselineRequest& request) const {
  for (const auto& baseline : baselines_) {
    if (baseline.algorithm_type == request.algorithm_type &&
        baseline.baseline_type == request.baseline_type)
      return &baseline;
  }
  return nullptr;
}

}  // namespace blink
