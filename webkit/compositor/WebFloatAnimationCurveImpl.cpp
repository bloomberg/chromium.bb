// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "WebFloatAnimationCurveImpl.h"

#include "CCAnimationCurve.h"
#include "CCKeyframedAnimationCurve.h"
#include "CCTimingFunction.h"
#include "WebAnimationCurveCommon.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebKit {

WebFloatAnimationCurve* WebFloatAnimationCurve::create()
{
    return new WebFloatAnimationCurveImpl();
}

WebFloatAnimationCurveImpl::WebFloatAnimationCurveImpl()
    : m_curve(WebCore::CCKeyframedFloatAnimationCurve::create())
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
    m_curve->addKeyframe(WebCore::CCFloatKeyframe::create(keyframe.time, keyframe.value, createTimingFunction(type)));
}

void WebFloatAnimationCurveImpl::add(const WebFloatKeyframe& keyframe, double x1, double y1, double x2, double y2)
{
    m_curve->addKeyframe(WebCore::CCFloatKeyframe::create(keyframe.time, keyframe.value, WebCore::CCCubicBezierTimingFunction::create(x1, y1, x2, y2)));
}

float WebFloatAnimationCurveImpl::getValue(double time) const
{
    return m_curve->getValue(time);
}

PassOwnPtr<WebCore::CCAnimationCurve> WebFloatAnimationCurveImpl::cloneToCCAnimationCurve() const
{
    return m_curve->clone();
}

} // namespace WebKit
