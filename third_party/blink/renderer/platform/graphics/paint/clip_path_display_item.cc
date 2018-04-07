// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/clip_path_display_item.h"

#include "third_party/blink/public/platform/web_display_item_list.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/skia/include/core/SkScalar.h"

namespace blink {

void BeginClipPathDisplayItem::Replay(GraphicsContext& context) const {
  context.Save();
  context.ClipPath(clip_path_, kAntiAliased);
}

void BeginClipPathDisplayItem::AppendToWebDisplayItemList(
    const FloatSize&,
    WebDisplayItemList* list) const {
  list->AppendClipPathItem(clip_path_, true);
}

void EndClipPathDisplayItem::Replay(GraphicsContext& context) const {
  context.Restore();
}

void EndClipPathDisplayItem::AppendToWebDisplayItemList(
    const FloatSize&,
    WebDisplayItemList* list) const {
  list->AppendEndClipPathItem();
}

#if DCHECK_IS_ON()
void BeginClipPathDisplayItem::PropertiesAsJSON(JSONObject& json) const {
  DisplayItem::PropertiesAsJSON(json);
  json.SetInteger("pathVerbs", clip_path_.countVerbs());
  json.SetInteger("pathPoints", clip_path_.countPoints());
  json.SetString("windRule",
                 clip_path_.getFillType() == SkPath::kWinding_FillType
                     ? "nonzero"
                     : "evenodd");
}

#endif

}  // namespace blink
