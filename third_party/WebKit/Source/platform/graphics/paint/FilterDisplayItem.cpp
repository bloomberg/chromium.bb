// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/FilterDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"
#include "public/platform/WebDisplayItemList.h"

namespace blink {

void BeginFilterDisplayItem::Replay(GraphicsContext& context) const {
  FloatRect image_filter_bounds(bounds_);
  image_filter_bounds.Move(-origin_.X(), -origin_.Y());
  context.Save();
  context.Translate(origin_.X(), origin_.Y());
  context.BeginLayer(1, SkBlendMode::kSrcOver, &image_filter_bounds,
                     kColorFilterNone, image_filter_);
  context.Translate(-origin_.X(), -origin_.Y());
}

void BeginFilterDisplayItem::AppendToWebDisplayItemList(
    const LayoutSize&,
    WebDisplayItemList* list) const {
  list->AppendFilterItem(compositor_filter_operations_.AsCcFilterOperations(),
                         bounds_, origin_);
}

bool BeginFilterDisplayItem::DrawsContent() const {
  // Skia cannot currently tell us if a filter will draw content,
  // even when no input primitives are drawn.
  return true;
}

#if DCHECK_IS_ON()
void BeginFilterDisplayItem::PropertiesAsJSON(JSONObject& json) const {
  DisplayItem::PropertiesAsJSON(json);
  json.SetString("filterBounds", bounds_.ToString());
}
#endif

void EndFilterDisplayItem::Replay(GraphicsContext& context) const {
  context.EndLayer();
  context.Restore();
}

void EndFilterDisplayItem::AppendToWebDisplayItemList(
    const LayoutSize&,
    WebDisplayItemList* list) const {
  list->AppendEndFilterItem();
}

}  // namespace blink
