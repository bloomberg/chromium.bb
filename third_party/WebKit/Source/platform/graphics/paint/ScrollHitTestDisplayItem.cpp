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
    Type type)
    : DisplayItem(client, type, sizeof(*this)) {
  DCHECK(RuntimeEnabledFeatures::SlimmingPaintV2Enabled());
  DCHECK(IsScrollHitTestType(type));
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
  return DisplayItem::Equals(other);
}

#ifndef NDEBUG
void ScrollHitTestDisplayItem::DumpPropertiesAsDebugString(
    StringBuilder& string_builder) const {
  DisplayItem::DumpPropertiesAsDebugString(string_builder);
}
#endif  // NDEBUG

void ScrollHitTestDisplayItem::Record(GraphicsContext& context,
                                      const DisplayItemClient& client,
                                      DisplayItem::Type type) {
  PaintController& paint_controller = context.GetPaintController();
  if (paint_controller.DisplayItemConstructionIsDisabled())
    return;

  // Should be contained in a scroll translation node.
  DCHECK(paint_controller.CurrentPaintChunkProperties()
             .property_tree_state.Transform()
             ->ScrollNode());

  paint_controller.CreateAndAppend<ScrollHitTestDisplayItem>(client, type);
}

}  // namespace blink
