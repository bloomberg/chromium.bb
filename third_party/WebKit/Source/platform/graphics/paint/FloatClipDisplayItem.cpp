// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/FloatClipDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"
#include "public/platform/WebDisplayItemList.h"
#include "third_party/skia/include/core/SkScalar.h"

namespace blink {

void FloatClipDisplayItem::Replay(GraphicsContext& context) const {
  context.Save();
  context.Clip(clip_rect_);
}

void FloatClipDisplayItem::AppendToWebDisplayItemList(
    const LayoutSize&,
    WebDisplayItemList* list) const {
  list->AppendFloatClipItem(clip_rect_);
}

void EndFloatClipDisplayItem::Replay(GraphicsContext& context) const {
  context.Restore();
}

void EndFloatClipDisplayItem::AppendToWebDisplayItemList(
    const LayoutSize&,
    WebDisplayItemList* list) const {
  list->AppendEndFloatClipItem();
}

#if DCHECK_IS_ON()
void FloatClipDisplayItem::PropertiesAsJSON(JSONObject& json) const {
  DisplayItem::PropertiesAsJSON(json);
  json.SetString("floatClipRect", clip_rect_.ToString());
}
#endif

}  // namespace blink
