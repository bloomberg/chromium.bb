// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/ClipDisplayItem.h"

#include "platform/geometry/FloatRoundedRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "public/platform/WebDisplayItemList.h"
#include "third_party/skia/include/core/SkScalar.h"

namespace blink {

void ClipDisplayItem::Replay(GraphicsContext& context) const {
  context.Save();

  // RoundedInnerRectClipper only cares about rounded-rect clips,
  // and passes an "infinite" rect clip; there is no reason to apply this clip.
  // TODO(fmalita): convert RoundedInnerRectClipper to a better suited
  //   DisplayItem so we don't have to special-case its semantics.
  if (clip_rect_ != LayoutRect::InfiniteIntRect())
    context.ClipRect(clip_rect_, kAntiAliased);

  for (const FloatRoundedRect& rounded_rect : rounded_rect_clips_)
    context.ClipRoundedRect(rounded_rect);
}

void ClipDisplayItem::AppendToWebDisplayItemList(
    const IntRect& visual_rect,
    WebDisplayItemList* list) const {
  WebVector<SkRRect> web_rounded_rects(rounded_rect_clips_.size());
  for (size_t i = 0; i < rounded_rect_clips_.size(); ++i)
    web_rounded_rects[i] = rounded_rect_clips_[i];

  list->AppendClipItem(clip_rect_, web_rounded_rects);
}

void EndClipDisplayItem::Replay(GraphicsContext& context) const {
  context.Restore();
}

void EndClipDisplayItem::AppendToWebDisplayItemList(
    const IntRect& visual_rect,
    WebDisplayItemList* list) const {
  list->AppendEndClipItem();
}

#ifndef NDEBUG
void ClipDisplayItem::DumpPropertiesAsDebugString(
    WTF::StringBuilder& string_builder) const {
  DisplayItem::DumpPropertiesAsDebugString(string_builder);
  string_builder.Append(WTF::String::Format(
      ", clipRect: [%d,%d,%d,%d]", clip_rect_.X(), clip_rect_.Y(),
      clip_rect_.Width(), clip_rect_.Height()));
}
#endif

}  // namespace blink
