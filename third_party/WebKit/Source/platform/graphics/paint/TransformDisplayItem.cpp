// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/TransformDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/transforms/AffineTransform.h"
#include "public/platform/WebDisplayItemList.h"

namespace blink {

void BeginTransformDisplayItem::Replay(GraphicsContext& context) const {
  context.Save();
  context.ConcatCTM(transform_);
}

void BeginTransformDisplayItem::AppendToWebDisplayItemList(
    const LayoutSize&,
    WebDisplayItemList* list) const {
  list->AppendTransformItem(AffineTransformToSkMatrix(transform_));
}

#ifndef NDEBUG
void BeginTransformDisplayItem::DumpPropertiesAsDebugString(
    WTF::StringBuilder& string_builder) const {
  PairedBeginDisplayItem::DumpPropertiesAsDebugString(string_builder);
  string_builder.Append(WTF::String::Format(
      ", transform: [%lf,%lf,%lf,%lf,%lf,%lf]", transform_.A(), transform_.B(),
      transform_.C(), transform_.D(), transform_.E(), transform_.F()));
}
#endif

void EndTransformDisplayItem::Replay(GraphicsContext& context) const {
  context.Restore();
}

void EndTransformDisplayItem::AppendToWebDisplayItemList(
    const LayoutSize&,
    WebDisplayItemList* list) const {
  list->AppendEndTransformItem();
}

}  // namespace blink
