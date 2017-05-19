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

#ifndef NDEBUG
void BeginScrollDisplayItem::DumpPropertiesAsDebugString(
    WTF::StringBuilder& string_builder) const {
  PairedBeginDisplayItem::DumpPropertiesAsDebugString(string_builder);
  string_builder.Append(WTF::String::Format(", currentOffset: [%d,%d]",
                                            current_offset_.Width(),
                                            current_offset_.Height()));
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
