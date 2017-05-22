// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/CompositingDisplayItem.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "public/platform/WebDisplayItemList.h"

namespace blink {

void BeginCompositingDisplayItem::Replay(GraphicsContext& context) const {
  context.BeginLayer(opacity_, xfer_mode_, has_bounds_ ? &bounds_ : nullptr,
                     color_filter_);
}

void BeginCompositingDisplayItem::AppendToWebDisplayItemList(
    const IntRect& visual_rect,
    WebDisplayItemList* list) const {
  SkRect bounds = bounds_;
  list->AppendCompositingItem(
      opacity_, xfer_mode_, has_bounds_ ? &bounds : nullptr,
      GraphicsContext::WebCoreColorFilterToSkiaColorFilter(color_filter_)
          .get());
}

#ifndef NDEBUG
void BeginCompositingDisplayItem::DumpPropertiesAsDebugString(
    WTF::StringBuilder& string_builder) const {
  DisplayItem::DumpPropertiesAsDebugString(string_builder);
  string_builder.Append(WTF::String::Format(
      ", xferMode: %d, opacity: %f", static_cast<int>(xfer_mode_), opacity_));
  if (has_bounds_) {
    string_builder.Append(
        WTF::String::Format(", bounds: [%f, %f, %f, %f]",
                            bounds_.Location().X(), bounds_.Location().Y(),
                            bounds_.Size().Width(), bounds_.Size().Height()));
  }
}
#endif

void EndCompositingDisplayItem::Replay(GraphicsContext& context) const {
  context.EndLayer();
}

void EndCompositingDisplayItem::AppendToWebDisplayItemList(
    const IntRect& visual_rect,
    WebDisplayItemList* list) const {
  list->AppendEndCompositingItem();
}

}  // namespace blink
