// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/scroll_display_item.h"

#include "third_party/blink/public/platform/web_display_item_list.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

void BeginScrollDisplayItem::Replay(GraphicsContext& context) const {
  context.Save();
  context.Translate(-current_offset_.Width(), -current_offset_.Height());
}

void BeginScrollDisplayItem::AppendToWebDisplayItemList(
    const FloatSize&,
    WebDisplayItemList* list) const {
  WebDisplayItemList::ScrollContainerId scroll_container_id = &Client();
  list->AppendScrollItem(current_offset_, scroll_container_id);
}

#if DCHECK_IS_ON()
void BeginScrollDisplayItem::PropertiesAsJSON(JSONObject& json) const {
  PairedBeginDisplayItem::PropertiesAsJSON(json);
  json.SetString("currentOffset", current_offset_.ToString());
}
#endif

void EndScrollDisplayItem::Replay(GraphicsContext& context) const {
  context.Restore();
}

void EndScrollDisplayItem::AppendToWebDisplayItemList(
    const FloatSize&,
    WebDisplayItemList* list) const {
  list->AppendEndScrollItem();
}

}  // namespace blink
