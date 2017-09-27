// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_container_fragment.h"

namespace blink {

NGPhysicalContainerFragment::NGPhysicalContainerFragment(
    LayoutObject* layout_object,
    const ComputedStyle& style,
    NGPhysicalSize size,
    NGFragmentType type,
    Vector<RefPtr<NGPhysicalFragment>>& children,
    RefPtr<NGBreakToken> break_token)
    : NGPhysicalFragment(layout_object,
                         style,
                         size,
                         type,
                         std::move(break_token)),
      children_(std::move(children)) {
  DCHECK(children.IsEmpty());  // Ensure move semantics is used.
}

void NGPhysicalContainerFragment::UpdateVisualRect() const {
  LayoutRect visual_rect(LayoutPoint(),
                         LayoutSize(Size().width, Size().height));
  for (const auto& child : children_) {
    LayoutRect child_visual_rect = child->LocalVisualRect();
    child_visual_rect.Move(child->Offset().left, child->Offset().top);
    visual_rect.Unite(child_visual_rect);
  }
  SetVisualRect(visual_rect);
}

}  // namespace blink
