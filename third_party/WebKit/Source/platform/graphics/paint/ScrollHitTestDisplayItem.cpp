// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/ScrollHitTestDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/wtf/Assertions.h"

namespace blink {

ScrollHitTestDisplayItem::ScrollHitTestDisplayItem(
    const DisplayItemClient& client,
    Type type,
    PassRefPtr<const TransformPaintPropertyNode> scroll_offset_node)
    : DisplayItem(client, type, sizeof(*this)),
      scroll_offset_node_(std::move(scroll_offset_node)) {
  DCHECK(RuntimeEnabledFeatures::SlimmingPaintV2Enabled());
  DCHECK(IsScrollHitTestType(type));
  // The scroll offset transform node should have an associated scroll node.
  DCHECK(scroll_offset_node_->ScrollNode());
}

ScrollHitTestDisplayItem::~ScrollHitTestDisplayItem() {}

void ScrollHitTestDisplayItem::Replay(GraphicsContext&) const {
  NOTREACHED();
}

void ScrollHitTestDisplayItem::AppendToWebDisplayItemList(
    const LayoutSize&,
    WebDisplayItemList*) const {
  NOTREACHED();
}

bool ScrollHitTestDisplayItem::Equals(const DisplayItem& other) const {
  return DisplayItem::Equals(other) &&
         &scroll_node() ==
             &static_cast<const ScrollHitTestDisplayItem&>(other).scroll_node();
}

#ifndef NDEBUG
void ScrollHitTestDisplayItem::DumpPropertiesAsDebugString(
    StringBuilder& string_builder) const {
  DisplayItem::DumpPropertiesAsDebugString(string_builder);
}
#endif  // NDEBUG

void ScrollHitTestDisplayItem::Record(
    GraphicsContext& context,
    const DisplayItemClient& client,
    DisplayItem::Type type,
    PassRefPtr<const TransformPaintPropertyNode> scroll_offset_node) {
  PaintController& paint_controller = context.GetPaintController();

  // The scroll hit test should be in the non-scrolled transform space and
  // therefore should not be scrolled by the associated scroll offset.
  DCHECK(paint_controller.CurrentPaintChunkProperties()
             .property_tree_state.Transform() != scroll_offset_node);

  if (paint_controller.DisplayItemConstructionIsDisabled())
    return;

  paint_controller.CreateAndAppend<ScrollHitTestDisplayItem>(
      client, type, std::move(scroll_offset_node));
}

}  // namespace blink
