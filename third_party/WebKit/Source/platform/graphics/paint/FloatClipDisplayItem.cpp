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
    const IntRect& visual_rect,
    WebDisplayItemList* list) const {
  list->AppendFloatClipItem(clip_rect_);
}

void EndFloatClipDisplayItem::Replay(GraphicsContext& context) const {
  context.Restore();
}

void EndFloatClipDisplayItem::AppendToWebDisplayItemList(
    const IntRect& visual_rect,
    WebDisplayItemList* list) const {
  list->AppendEndFloatClipItem();
}

#ifndef NDEBUG
void FloatClipDisplayItem::DumpPropertiesAsDebugString(
    WTF::StringBuilder& string_builder) const {
  DisplayItem::DumpPropertiesAsDebugString(string_builder);
  string_builder.Append(WTF::String::Format(
      ", floatClipRect: [%f,%f,%f,%f]}", clip_rect_.X(), clip_rect_.Y(),
      clip_rect_.Width(), clip_rect_.Height()));
}

#endif

}  // namespace blink
