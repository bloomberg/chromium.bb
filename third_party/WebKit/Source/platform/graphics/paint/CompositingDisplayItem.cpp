// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/CompositingDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "public/platform/WebDisplayItemList.h"

namespace blink {

void BeginCompositingDisplayItem::Replay(GraphicsContext& context) const {
  context.BeginLayer(opacity_, xfer_mode_, has_bounds_ ? &bounds_ : nullptr,
                     color_filter_);
}

void BeginCompositingDisplayItem::AppendToWebDisplayItemList(
    const LayoutSize&,
    WebDisplayItemList* list) const {
  SkRect bounds = bounds_;
  list->AppendCompositingItem(
      opacity_, xfer_mode_, has_bounds_ ? &bounds : nullptr,
      GraphicsContext::WebCoreColorFilterToSkiaColorFilter(color_filter_)
          .get());
}

#if DCHECK_IS_ON()
void BeginCompositingDisplayItem::PropertiesAsJSON(JSONObject& json) const {
  DisplayItem::PropertiesAsJSON(json);
  json.SetInteger("xferMode", static_cast<int>(xfer_mode_));
  json.SetDouble("opacity", opacity_);
  if (has_bounds_)
    json.SetString("bounds", bounds_.ToString());
}
#endif

void EndCompositingDisplayItem::Replay(GraphicsContext& context) const {
  context.EndLayer();
}

void EndCompositingDisplayItem::AppendToWebDisplayItemList(
    const LayoutSize&,
    WebDisplayItemList* list) const {
  list->AppendEndCompositingItem();
}

}  // namespace blink
