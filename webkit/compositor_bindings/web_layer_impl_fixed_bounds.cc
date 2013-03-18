// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_layer_impl_fixed_bounds.h"

#include "cc/layers/layer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/skia/include/utils/SkMatrix44.h"

using cc::Layer;

namespace webkit {

WebLayerImplFixedBounds::WebLayerImplFixedBounds() {
}

WebLayerImplFixedBounds::WebLayerImplFixedBounds(scoped_refptr<Layer> layer)
    : WebLayerImpl(layer) {
}


WebLayerImplFixedBounds::~WebLayerImplFixedBounds() {
}

void WebLayerImplFixedBounds::invalidateRect(const WebKit::WebFloatRect& rect) {
  // Partial invalidations seldom occur for such layers.
  // Simply invalidate the whole layer to avoid transformation of coordinates.
  invalidate();
}

void WebLayerImplFixedBounds::setAnchorPoint(
    const WebKit::WebFloatPoint& anchor_point) {
  if (anchor_point != this->anchorPoint()) {
    layer_->SetAnchorPoint(anchor_point);
    UpdateLayerBoundsAndTransform();
  }
}

void WebLayerImplFixedBounds::setBounds(const WebKit::WebSize& bounds) {
  if (original_bounds_ != gfx::Size(bounds)) {
      original_bounds_ = bounds;
      UpdateLayerBoundsAndTransform();
  }
}

WebKit::WebSize WebLayerImplFixedBounds::bounds() const {
  return original_bounds_;
}

void WebLayerImplFixedBounds::setSublayerTransform(const SkMatrix44& matrix) {
  gfx::Transform transform;
  transform.matrix() = matrix;
  SetSublayerTransformInternal(transform);
}

SkMatrix44 WebLayerImplFixedBounds::sublayerTransform() const {
  return original_sublayer_transform_.matrix();
}

void WebLayerImplFixedBounds::setTransform(const SkMatrix44& matrix) {
  gfx::Transform transform;
  transform.matrix() = matrix;
  SetTransformInternal(transform);
}

SkMatrix44 WebLayerImplFixedBounds::transform() const {
  return original_transform_.matrix();
}

void WebLayerImplFixedBounds::SetFixedBounds(gfx::Size fixed_bounds) {
  if (fixed_bounds_ != fixed_bounds) {
    fixed_bounds_ = fixed_bounds;
    UpdateLayerBoundsAndTransform();
  }
}

void WebLayerImplFixedBounds::SetSublayerTransformInternal(
    const gfx::Transform& transform) {
  if (original_sublayer_transform_ != transform) {
    original_sublayer_transform_ = transform;
    UpdateLayerBoundsAndTransform();
  }
}

void WebLayerImplFixedBounds::SetTransformInternal(
    const gfx::Transform& transform) {
  if (original_transform_ != transform) {
    original_transform_ = transform;
    UpdateLayerBoundsAndTransform();
  }
}

void WebLayerImplFixedBounds::UpdateLayerBoundsAndTransform() {
  if (fixed_bounds_.IsEmpty() || original_bounds_.IsEmpty() ||
      fixed_bounds_ == original_bounds_ ||
      // For now fall back to non-fixed bounds for non-zero anchor point.
      // TODO(wangxianzhu): Support non-zero anchor point for fixed bounds.
      anchorPoint().x || anchorPoint().y) {
    layer_->SetBounds(original_bounds_);
    layer_->SetTransform(original_transform_);
    layer_->SetSublayerTransform(original_sublayer_transform_);
    return;
  }

  layer_->SetBounds(fixed_bounds_);

  // Apply bounds scale (bounds/fixed_bounds) over original transform.
  gfx::Transform transform_with_bounds_scale(original_transform_);
  float bounds_scale_x =
      static_cast<float>(original_bounds_.width()) / fixed_bounds_.width();
  float bounds_scale_y =
      static_cast<float>(original_bounds_.height()) / fixed_bounds_.height();
  transform_with_bounds_scale.Scale(bounds_scale_x, bounds_scale_y);
  layer_->SetTransform(transform_with_bounds_scale);

  // As we apply extra scale transform on this layer which will propagate to the
  // sublayers, here undo the scale on sublayers.
  gfx::Transform sublayer_transform_with_inverse_bounds_scale;
  sublayer_transform_with_inverse_bounds_scale.Scale(1.f / bounds_scale_x,
                                                     1.f / bounds_scale_y);
  sublayer_transform_with_inverse_bounds_scale.PreconcatTransform(
      original_sublayer_transform_);
  layer_->SetSublayerTransform(sublayer_transform_with_inverse_bounds_scale);
}

}  // namespace WebKit
