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
    const IntRect& visual_rect,
    WebDisplayItemList* list) const {
  // TODO(jbroman): The compositor will need the transform origin separately.
  TransformationMatrix transform(transform_);
  transform.ApplyTransformOrigin(transform_origin_);
  list->AppendTransformItem(TransformationMatrix::ToSkMatrix44(transform));
}

#ifndef NDEBUG
void BeginTransform3DDisplayItem::DumpPropertiesAsDebugString(
    StringBuilder& string_builder) const {
  PairedBeginDisplayItem::DumpPropertiesAsDebugString(string_builder);
  TransformationMatrix::DecomposedType decomposition;
  if (transform_.Decompose(decomposition)) {
    string_builder.Append(String::Format(
        ", translate: [%lf,%lf,%lf], scale: [%lf,%lf,%lf], skew: "
        "[%lf,%lf,%lf], quaternion: [%lf,%lf,%lf,%lf], perspective: "
        "[%lf,%lf,%lf,%lf]",
        decomposition.translate_x, decomposition.translate_y,
        decomposition.translate_z, decomposition.scale_x, decomposition.scale_y,
        decomposition.scale_z, decomposition.skew_xy, decomposition.skew_xz,
        decomposition.skew_yz, decomposition.quaternion_x,
        decomposition.quaternion_y, decomposition.quaternion_z,
        decomposition.quaternion_w, decomposition.perspective_x,
        decomposition.perspective_y, decomposition.perspective_z,
        decomposition.perspective_w));
  }
  string_builder.Append(
      String::Format(", transformOrigin: [%f,%f,%f]", transform_origin_.X(),
                     transform_origin_.Y(), transform_origin_.Z()));
}
#endif

void EndTransform3DDisplayItem::Replay(GraphicsContext& context) const {
  context.Restore();
}

void EndTransform3DDisplayItem::AppendToWebDisplayItemList(
    const IntRect& visual_rect,
    WebDisplayItemList* list) const {
  list->AppendEndTransformItem();
}

}  // namespace blink
