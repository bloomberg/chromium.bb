// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

NGPhysicalBoxFragment::NGPhysicalBoxFragment(
    LayoutObject* layout_object,
    const ComputedStyle& style,
    NGPhysicalSize size,
    NGPhysicalSize overflow,
    Vector<RefPtr<NGPhysicalFragment>>& children,
    Vector<NGBaseline>& baselines,
    unsigned border_edges,  // NGBorderEdges::Physical
    RefPtr<NGBreakToken> break_token)
    : NGPhysicalFragment(layout_object,
                         style,
                         size,
                         kFragmentBox,
                         std::move(break_token)),
      overflow_(overflow) {
  children_.swap(children);
  baselines_.swap(baselines);
  border_edge_ = border_edges;
}

const NGBaseline* NGPhysicalBoxFragment::Baseline(
    const NGBaselineRequest& request) const {
  for (const auto& baseline : baselines_) {
    if (baseline.request == request)
      return &baseline;
  }
  return nullptr;
}

RefPtr<NGPhysicalFragment> NGPhysicalBoxFragment::CloneWithoutOffset() const {
  Vector<RefPtr<NGPhysicalFragment>> children_copy(children_);
  Vector<NGBaseline> baselines_copy(baselines_);
  RefPtr<NGPhysicalFragment> physical_fragment =
      AdoptRef(new NGPhysicalBoxFragment(
          layout_object_, Style(), size_, overflow_, children_copy,
          baselines_copy, border_edge_, break_token_));
  return physical_fragment;
}

}  // namespace blink
