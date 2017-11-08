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

#if DCHECK_IS_ON()
void BeginTransformDisplayItem::PropertiesAsJSON(JSONObject& json) const {
  PairedBeginDisplayItem::PropertiesAsJSON(json);
  json.SetString("transform", transform_.ToString());
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
