// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_transform_animation_curve_impl.h"

#include "cc/keyframed_animation_curve.h"
#include "cc/timing_function.h"
#include "cc/transform_operations.h"
#include "webkit/compositor_bindings/web_animation_curve_common.h"
#include "webkit/compositor_bindings/web_transform_operations_impl.h"
#include "webkit/compositor_bindings/web_transformation_matrix_util.h"

namespace WebKit {

WebTransformAnimationCurveImpl::WebTransformAnimationCurveImpl()
    : curve_(cc::KeyframedTransformAnimationCurve::create()) {}

WebTransformAnimationCurveImpl::~WebTransformAnimationCurveImpl() {}

WebAnimationCurve::AnimationCurveType
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
  curve_->addKeyframe(cc::TransformKeyframe::create(
      keyframe.time(), transform_operations, createTimingFunction(type)));
}

void WebTransformAnimationCurveImpl::add(const WebTransformKeyframe& keyframe,
                                         double x1,
                                         double y1,
                                         double x2,
                                         double y2) {
  const cc::TransformOperations& transform_operations =
      static_cast<const webkit::WebTransformOperationsImpl&>(keyframe.value())
      .AsTransformOperations();
  curve_->addKeyframe(cc::TransformKeyframe::create(
      keyframe.time(),
      transform_operations,
      cc::CubicBezierTimingFunction::create(x1, y1, x2, y2)
          .PassAs<cc::TimingFunction>()));
}

WebTransformationMatrix WebTransformAnimationCurveImpl::getValue(
    double time) const {
  return webkit::WebTransformationMatrixUtil::ToWebTransformationMatrix(
      curve_->getValue(time));
}

scoped_ptr<cc::AnimationCurve>
WebTransformAnimationCurveImpl::cloneToAnimationCurve() const {
  return curve_->clone();
}

}  // namespace WebKit
