// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"

#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

bool CullRect::Intersects(const IntRect& rect) const {
  return IsInfinite() || rect.Intersects(rect_);
}

bool CullRect::Intersects(const LayoutRect& rect) const {
  return IsInfinite() || rect_.Intersects(EnclosingIntRect(rect));
}

bool CullRect::IntersectsTransformed(const AffineTransform& transform,
                                     const FloatRect& rect) const {
  return IsInfinite() || transform.MapRect(rect).Intersects(rect_);
}

bool CullRect::IntersectsHorizontalRange(LayoutUnit lo, LayoutUnit hi) const {
  return !(lo >= rect_.MaxX() || hi <= rect_.X());
}

bool CullRect::IntersectsVerticalRange(LayoutUnit lo, LayoutUnit hi) const {
  return !(lo >= rect_.MaxY() || hi <= rect_.Y());
}

void CullRect::MoveBy(const IntPoint& offset) {
  if (!IsInfinite())
    rect_.MoveBy(offset);
}

void CullRect::Move(const IntSize& offset) {
  if (!IsInfinite())
    rect_.Move(offset);
}

void CullRect::ApplyTransform(const TransformPaintPropertyNode* transform) {
  if (IsInfinite())
    return;

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    if (const auto* scroll = transform->ScrollNode()) {
      rect_.Intersect(scroll->ContainerRect());
      rect_ = transform->Matrix().Inverse().MapRect(rect_);

      // Expand the cull rect for scrolling contents in case of
      // composited scrolling.
      // TODO(wangxianzhu): Implement more sophisticated interst rect expansion.
      static const int kPixelDistanceToExpand = 4000;
      rect_.Inflate(kPixelDistanceToExpand);
      return;
    }
  }

  rect_ = transform->Matrix().Inverse().MapRect(rect_);
}

}  // namespace blink
