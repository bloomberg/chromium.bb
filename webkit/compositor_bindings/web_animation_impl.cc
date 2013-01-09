// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_animation_impl.h"

#include "cc/animation.h"
#include "cc/animation_curve.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebAnimationCurve.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebAnimation.h"
#include "webkit/compositor_bindings/web_animation_id_provider.h"
#include "webkit/compositor_bindings/web_float_animation_curve_impl.h"
#include "webkit/compositor_bindings/web_transform_animation_curve_impl.h"

using cc::Animation;
using webkit::WebAnimationIdProvider;

namespace WebKit {

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
    m_animation = Animation::create(curve.Pass(), animationId, groupId, static_cast<cc::Animation::TargetProperty>(targetProperty));
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

scoped_ptr<cc::Animation> WebAnimationImpl::cloneToAnimation()
{
    scoped_ptr<cc::Animation> toReturn(m_animation->clone(cc::Animation::NonControllingInstance));
    toReturn->setNeedsSynchronizedStartTime(true);
    return toReturn.Pass();
}

} // namespace WebKit
