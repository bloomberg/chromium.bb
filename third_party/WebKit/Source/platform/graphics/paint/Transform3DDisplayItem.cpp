// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/Transform3DDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/transforms/AffineTransform.h"
#include "public/platform/WebDisplayItemList.h"

namespace blink {

void BeginTransform3DDisplayItem::Replay(GraphicsContext& context) const {
  TransformationMatrix transform(transform_);
  transform.ApplyTransformOrigin(transform_origin_);
  context.Save();
  context.ConcatCTM(transform.ToAffineTransform());
}

void BeginTransform3DDisplayItem::AppendToWebDisplayItemList(
    const LayoutSize&,
    WebDisplayItemList* list) const {
  // TODO(jbroman): The compositor will need the transform origin separately.
  TransformationMatrix transform(transform_);
  transform.ApplyTransformOrigin(transform_origin_);
  list->AppendTransformItem(TransformationMatrix::ToSkMatrix44(transform));
}

#if DCHECK_IS_ON()
void BeginTransform3DDisplayItem::PropertiesAsJSON(JSONObject& json) const {
  PairedBeginDisplayItem::PropertiesAsJSON(json);
  json.SetString("transform", transform_.ToString());
  json.SetString("origin", transform_origin_.ToString());
}
#endif

void EndTransform3DDisplayItem::Replay(GraphicsContext& context) const {
  context.Restore();
}

void EndTransform3DDisplayItem::AppendToWebDisplayItemList(
    const LayoutSize&,
    WebDisplayItemList* list) const {
  list->AppendEndTransformItem();
}

}  // namespace blink
