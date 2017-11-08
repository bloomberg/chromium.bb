// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/ScrollDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/transforms/AffineTransform.h"
#include "public/platform/WebDisplayItemList.h"

namespace blink {

void BeginScrollDisplayItem::Replay(GraphicsContext& context) const {
  context.Save();
  context.Translate(-current_offset_.Width(), -current_offset_.Height());
}

void BeginScrollDisplayItem::AppendToWebDisplayItemList(
    const LayoutSize&,
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
    const LayoutSize&,
    WebDisplayItemList* list) const {
  list->AppendEndScrollItem();
}

}  // namespace blink
