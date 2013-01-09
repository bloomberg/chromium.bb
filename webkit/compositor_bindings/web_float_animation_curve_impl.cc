// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_float_animation_curve_impl.h"

#include "cc/animation_curve.h"
#include "cc/keyframed_animation_curve.h"
#include "cc/timing_function.h"
#include "webkit/compositor_bindings/web_animation_curve_common.h"

namespace WebKit {

WebFloatAnimationCurveImpl::WebFloatAnimationCurveImpl()
    : m_curve(cc::KeyframedFloatAnimationCurve::create())
{
}

WebFloatAnimationCurveImpl::~WebFloatAnimationCurveImpl()
{
}

WebAnimationCurve::AnimationCurveType WebFloatAnimationCurveImpl::type() const
{
    return WebAnimationCurve::AnimationCurveTypeFloat;
}

void WebFloatAnimationCurveImpl::add(const WebFloatKeyframe& keyframe)
{
    add(keyframe, TimingFunctionTypeEase);
}

void WebFloatAnimationCurveImpl::add(const WebFloatKeyframe& keyframe, TimingFunctionType type)
{
    m_curve->addKeyframe(cc::FloatKeyframe::create(keyframe.time, keyframe.value, createTimingFunction(type)));
}

void WebFloatAnimationCurveImpl::add(const WebFloatKeyframe& keyframe, double x1, double y1, double x2, double y2)
{
    m_curve->addKeyframe(cc::FloatKeyframe::create(keyframe.time, keyframe.value, cc::CubicBezierTimingFunction::create(x1, y1, x2, y2).PassAs<cc::TimingFunction>()));
}

float WebFloatAnimationCurveImpl::getValue(double time) const
{
    return m_curve->getValue(time);
}

scoped_ptr<cc::AnimationCurve> WebFloatAnimationCurveImpl::cloneToAnimationCurve() const
{
    return m_curve->clone();
}

} // namespace WebKit
