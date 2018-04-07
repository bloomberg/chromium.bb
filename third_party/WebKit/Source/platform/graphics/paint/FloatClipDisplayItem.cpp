// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/float_clip_display_item.h"

#include "third_party/blink/public/platform/web_display_item_list.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/skia/include/core/SkScalar.h"

namespace blink {

void FloatClipDisplayItem::Replay(GraphicsContext& context) const {
  context.Save();
  context.ClipRect(clip_rect_, kAntiAliased);
}

void FloatClipDisplayItem::AppendToWebDisplayItemList(
    const FloatSize&,
    WebDisplayItemList* list) const {
  list->AppendFloatClipItem(clip_rect_);
}

void EndFloatClipDisplayItem::Replay(GraphicsContext& context) const {
  context.Restore();
}

void EndFloatClipDisplayItem::AppendToWebDisplayItemList(
    const FloatSize&,
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
