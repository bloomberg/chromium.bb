// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_animation_impl.h"

#include "cc/animation.h"
#include "cc/animation_curve.h"
#include "cc/animation_id_provider.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebAnimationCurve.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebAnimation.h"
#include "webkit/compositor_bindings/web_float_animation_curve_impl.h"
#include "webkit/compositor_bindings/web_transform_animation_curve_impl.h"

using cc::Animation;
using cc::AnimationIdProvider;

namespace WebKit {

WebAnimationImpl::WebAnimationImpl(const WebAnimationCurve& web_curve,
                                   TargetProperty target_property,
                                   int animation_id,
                                   int group_id) {
  if (!animation_id)
    animation_id = AnimationIdProvider::NextAnimationId();
  if (!group_id)
    group_id = AnimationIdProvider::NextGroupId();

  WebAnimationCurve::AnimationCurveType curve_type = web_curve.type();
  scoped_ptr<cc::AnimationCurve> curve;
  switch (curve_type) {
    case WebAnimationCurve::AnimationCurveTypeFloat: {
      const WebFloatAnimationCurveImpl* float_curve_impl =
          static_cast<const WebFloatAnimationCurveImpl*>(&web_curve);
      curve = float_curve_impl->cloneToAnimationCurve();
      break;
    }
    case WebAnimationCurve::AnimationCurveTypeTransform: {
      const WebTransformAnimationCurveImpl* transform_curve_impl =
          static_cast<const WebTransformAnimationCurveImpl*>(&web_curve);
      curve = transform_curve_impl->cloneToAnimationCurve();
      break;
    }
  }
  animation_ = Animation::create(
      curve.Pass(),
      animation_id,
      group_id,
      static_cast<cc::Animation::TargetProperty>(target_property));
}

WebAnimationImpl::~WebAnimationImpl() {}

int WebAnimationImpl::id() { return animation_->id(); }

WebAnimation::TargetProperty WebAnimationImpl::targetProperty() const {
  return static_cast<WebAnimationImpl::TargetProperty>(
      animation_->targetProperty());
}

int WebAnimationImpl::iterations() const { return animation_->iterations(); }

void WebAnimationImpl::setIterations(int n) { animation_->setIterations(n); }

double WebAnimationImpl::startTime() const { return animation_->startTime(); }

void WebAnimationImpl::setStartTime(double monotonic_time) {
  animation_->setStartTime(monotonic_time);
}

double WebAnimationImpl::timeOffset() const { return animation_->timeOffset(); }

void WebAnimationImpl::setTimeOffset(double monotonic_time) {
  animation_->setTimeOffset(monotonic_time);
}

bool WebAnimationImpl::alternatesDirection() const {
  return animation_->alternatesDirection();
}

void WebAnimationImpl::setAlternatesDirection(bool alternates) {
  animation_->setAlternatesDirection(alternates);
}

scoped_ptr<cc::Animation> WebAnimationImpl::cloneToAnimation() {
  scoped_ptr<cc::Animation> to_return(
      animation_->clone(cc::Animation::NonControllingInstance));
  to_return->setNeedsSynchronizedStartTime(true);
  return to_return.Pass();
}

}  // namespace WebKit
