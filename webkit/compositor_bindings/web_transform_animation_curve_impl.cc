// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_transform_animation_curve_impl.h"

#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/timing_function.h"
#include "cc/animation/transform_operations.h"
#include "webkit/compositor_bindings/web_animation_curve_common.h"
#include "webkit/compositor_bindings/web_transform_operations_impl.h"

using WebKit::WebTransformKeyframe;

namespace webkit {

WebTransformAnimationCurveImpl::WebTransformAnimationCurveImpl()
    : curve_(cc::KeyframedTransformAnimationCurve::Create()) {}

WebTransformAnimationCurveImpl::~WebTransformAnimationCurveImpl() {}

WebKit::WebAnimationCurve::AnimationCurveType
WebTransformAnimationCurveImpl::type() const {
  return WebAnimationCurve::AnimationCurveTypeTransform;
}

void WebTransformAnimationCurveImpl::add(const WebTransformKeyframe& keyframe) {
  add(keyframe, TimingFunctionTypeEase);
}

void WebTransformAnimationCurveImpl::add(const WebTransformKeyframe& keyframe,
                                         TimingFunctionType type) {
  const cc::TransformOperations& transform_operations =
      static_cast<const webkit::WebTransformOperationsImpl&>(keyframe.value())
      .AsTransformOperations();
  curve_->AddKeyframe(cc::TransformKeyframe::Create(
      keyframe.time(), transform_operations, CreateTimingFunction(type)));
}

void WebTransformAnimationCurveImpl::add(const WebTransformKeyframe& keyframe,
                                         double x1,
                                         double y1,
                                         double x2,
                                         double y2) {
  const cc::TransformOperations& transform_operations =
      static_cast<const webkit::WebTransformOperationsImpl&>(keyframe.value())
      .AsTransformOperations();
  curve_->AddKeyframe(cc::TransformKeyframe::Create(
      keyframe.time(),
      transform_operations,
      cc::CubicBezierTimingFunction::Create(x1, y1, x2, y2)
          .PassAs<cc::TimingFunction>()));
}

scoped_ptr<cc::AnimationCurve>
WebTransformAnimationCurveImpl::CloneToAnimationCurve() const {
  return curve_->Clone();
}

}  // namespace webkit
