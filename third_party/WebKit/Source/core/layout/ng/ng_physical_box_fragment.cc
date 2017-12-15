// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_box_fragment.h"

#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutObject.h"

namespace blink {

NGPhysicalBoxFragment::NGPhysicalBoxFragment(
    LayoutObject* layout_object,
    const ComputedStyle& style,
    NGPhysicalSize size,
    Vector<scoped_refptr<NGPhysicalFragment>>& children,
    const NGPhysicalOffsetRect& contents_visual_rect,
    Vector<NGBaseline>& baselines,
    NGBoxType box_type,
    bool is_old_layout_root,
    unsigned border_edges,  // NGBorderEdges::Physical
    scoped_refptr<NGBreakToken> break_token)
    : NGPhysicalContainerFragment(layout_object,
                                  style,
                                  size,
                                  kFragmentBox,
                                  children,
                                  contents_visual_rect,
                                  std::move(break_token)),
      baselines_(std::move(baselines)) {
  DCHECK(baselines.IsEmpty());  // Ensure move semantics is used.
  box_type_ = box_type;
  is_old_layout_root_ = is_old_layout_root;
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

bool NGPhysicalBoxFragment::HasSelfPaintingLayer() const {
  const LayoutObject* layout_object = GetLayoutObject();
  DCHECK(layout_object);
  DCHECK(layout_object->IsBoxModelObject());
  return ToLayoutBoxModelObject(layout_object)->HasSelfPaintingLayer() &&
         BoxType() != kAnonymousBox;
}

bool NGPhysicalBoxFragment::ChildrenInline() const {
  const LayoutObject* layout_object = GetLayoutObject();
  DCHECK(layout_object);
  return layout_object->ChildrenInline();
}

bool NGPhysicalBoxFragment::HasOverflowClip() const {
  const LayoutObject* layout_object = GetLayoutObject();
  DCHECK(layout_object);
  return layout_object->HasOverflowClip() && BoxType() != kAnonymousBox;
}

bool NGPhysicalBoxFragment::ShouldClipOverflow() const {
  const LayoutObject* layout_object = GetLayoutObject();
  DCHECK(layout_object);
  return layout_object->IsBox() &&
         ToLayoutBox(layout_object)->ShouldClipOverflow() &&
         BoxType() != kAnonymousBox;
}

NGPhysicalOffsetRect NGPhysicalBoxFragment::SelfVisualRect() const {
  const ComputedStyle& style = Style();
  if (!style.HasVisualOverflowingEffect())
    return {{}, Size()};

  LayoutObject* layout_object = GetLayoutObject();
  if (layout_object->IsBox()) {
    // TODO(kojii): Should move the logic to a common place.
    LayoutRect visual_rect({}, Size().ToLayoutSize());
    visual_rect.Expand(
        ToLayoutBox(layout_object)->ComputeVisualEffectOverflowOutsets());
    return NGPhysicalOffsetRect(visual_rect);
  }

  // TODO(kojii): Implement for inline boxes.
  DCHECK(layout_object->IsLayoutInline());
  return {{}, Size()};
}

NGPhysicalOffsetRect NGPhysicalBoxFragment::VisualRectWithContents() const {
  if (HasOverflowClip() || Style().HasMask())
    return SelfVisualRect();

  NGPhysicalOffsetRect visual_rect = SelfVisualRect();
  visual_rect.Unite(ContentsVisualRect());
  return visual_rect;
}

scoped_refptr<NGPhysicalFragment> NGPhysicalBoxFragment::CloneWithoutOffset()
    const {
  Vector<scoped_refptr<NGPhysicalFragment>> children_copy(children_);
  Vector<NGBaseline> baselines_copy(baselines_);
  scoped_refptr<NGPhysicalFragment> physical_fragment =
      base::AdoptRef(new NGPhysicalBoxFragment(
          layout_object_, Style(), size_, children_copy, contents_visual_rect_,
          baselines_copy, BoxType(), is_old_layout_root_, border_edge_,
          break_token_));
  return physical_fragment;
}

}  // namespace blink
