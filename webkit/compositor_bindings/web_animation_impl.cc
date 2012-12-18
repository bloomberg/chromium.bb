// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web_animation_impl.h"

#include "cc/active_animation.h"
#include "cc/animation_curve.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebAnimationCurve.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebAnimation.h"
#include "web_animation_id_provider.h"
#include "web_float_animation_curve_impl.h"
#include "web_transform_animation_curve_impl.h"

using cc::ActiveAnimation;
using webkit::WebAnimationIdProvider;

namespace WebKit {

WebAnimation* WebAnimation::create(const WebAnimationCurve& curve, TargetProperty targetProperty, int animationId)
{
    return new WebAnimationImpl(curve, targetProperty, animationId, 0);
}

WebAnimationImpl::WebAnimationImpl(const WebAnimationCurve& webCurve, TargetProperty targetProperty, int animationId, int groupId)
{
    if (!animationId)
        animationId = WebAnimationIdProvider::NextAnimationId();
    if (!groupId)
        groupId = WebAnimationIdProvider::NextGroupId();

    WebAnimationCurve::AnimationCurveType curveType = webCurve.type();
    scoped_ptr<cc::AnimationCurve> curve;
    switch (curveType) {
    case WebAnimationCurve::AnimationCurveTypeFloat: {
        const WebFloatAnimationCurveImpl* floatCurveImpl = static_cast<const WebFloatAnimationCurveImpl*>(&webCurve);
        curve = floatCurveImpl->cloneToAnimationCurve();
        break;
    }
    case WebAnimationCurve::AnimationCurveTypeTransform: {
        const WebTransformAnimationCurveImpl* transformCurveImpl = static_cast<const WebTransformAnimationCurveImpl*>(&webCurve);
        curve = transformCurveImpl->cloneToAnimationCurve();
        break;
    }
    }
    m_animation = ActiveAnimation::create(curve.Pass(), animationId, groupId, static_cast<cc::ActiveAnimation::TargetProperty>(targetProperty));
}

WebAnimationImpl::~WebAnimationImpl()
{
}

int WebAnimationImpl::id()
{
    return m_animation->id();
}

WebAnimation::TargetProperty WebAnimationImpl::targetProperty() const
{
    return static_cast<WebAnimationImpl::TargetProperty>(m_animation->targetProperty());
}

int WebAnimationImpl::iterations() const
{
    return m_animation->iterations();
}

void WebAnimationImpl::setIterations(int n)
{
    m_animation->setIterations(n);
}

double WebAnimationImpl::startTime() const
{
    return m_animation->startTime();
}

void WebAnimationImpl::setStartTime(double monotonicTime)
{
    m_animation->setStartTime(monotonicTime);
}

double WebAnimationImpl::timeOffset() const
{
    return m_animation->timeOffset();
}

void WebAnimationImpl::setTimeOffset(double monotonicTime)
{
    m_animation->setTimeOffset(monotonicTime);
}

bool WebAnimationImpl::alternatesDirection() const
{
    return m_animation->alternatesDirection();
}

void WebAnimationImpl::setAlternatesDirection(bool alternates)
{
    m_animation->setAlternatesDirection(alternates);
}

scoped_ptr<cc::ActiveAnimation> WebAnimationImpl::cloneToAnimation()
{
    scoped_ptr<cc::ActiveAnimation> toReturn(m_animation->clone(cc::ActiveAnimation::NonControllingInstance));
    toReturn->setNeedsSynchronizedStartTime(true);
    return toReturn.Pass();
}

} // namespace WebKit
