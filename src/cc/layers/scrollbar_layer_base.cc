// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/scrollbar_layer_base.h"

#include "cc/layers/painted_overlay_scrollbar_layer.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/layers/solid_color_scrollbar_layer.h"

namespace cc {

ScrollbarLayerBase::ScrollbarLayerBase(ScrollbarOrientation orientation,
                                       bool is_left_side_vertical_scrollbar)
    : orientation_(orientation),
      is_left_side_vertical_scrollbar_(is_left_side_vertical_scrollbar) {}

ScrollbarLayerBase::~ScrollbarLayerBase() = default;

scoped_refptr<ScrollbarLayerBase> ScrollbarLayerBase::CreateOrReuse(
    scoped_refptr<Scrollbar> scrollbar,
    ScrollbarLayerBase* existing_layer) {
  DCHECK(scrollbar);
  ScrollbarLayerType needed_type = kPainted;
  if (scrollbar->IsSolidColor()) {
    needed_type = kSolidColor;
  } else if (scrollbar->UsesNinePatchThumbResource()) {
    DCHECK(scrollbar->IsOverlay());
    needed_type = kPaintedOverlay;
  }

  if (existing_layer &&
      (existing_layer->GetScrollbarLayerType() != needed_type ||
       // We don't support change of these fields in a layer.
       existing_layer->orientation() != scrollbar->Orientation() ||
       existing_layer->is_left_side_vertical_scrollbar() !=
           scrollbar->IsLeftSideVerticalScrollbar())) {
    existing_layer = nullptr;
  }

  switch (needed_type) {
    case kSolidColor:
      return SolidColorScrollbarLayer::CreateOrReuse(
          std::move(scrollbar),
          static_cast<SolidColorScrollbarLayer*>(existing_layer));
    case kPainted:
      return PaintedScrollbarLayer::CreateOrReuse(
          std::move(scrollbar),
          static_cast<PaintedScrollbarLayer*>(existing_layer));
    case kPaintedOverlay:
      return PaintedOverlayScrollbarLayer::CreateOrReuse(
          std::move(scrollbar),
          static_cast<PaintedOverlayScrollbarLayer*>(existing_layer));
  }

  NOTREACHED();
  return nullptr;
}

void ScrollbarLayerBase::SetScrollElementId(ElementId element_id) {
  if (element_id == scroll_element_id_)
    return;

  scroll_element_id_ = element_id;
  SetNeedsCommit();
}

void ScrollbarLayerBase::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);

  auto* scrollbar_layer_impl = static_cast<ScrollbarLayerImplBase*>(layer);
  DCHECK_EQ(scrollbar_layer_impl->orientation(), orientation_);
  DCHECK_EQ(scrollbar_layer_impl->is_left_side_vertical_scrollbar(),
            is_left_side_vertical_scrollbar_);
  scrollbar_layer_impl->SetScrollElementId(scroll_element_id_);
}

bool ScrollbarLayerBase::IsScrollbarLayerForTesting() const {
  return true;
}

}  // namespace cc
