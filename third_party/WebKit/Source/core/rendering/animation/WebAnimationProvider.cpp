/*
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/rendering/animation/WebAnimationProvider.h"

#include "core/platform/animation/AnimationTranslationUtil.h"
#include "core/platform/animation/CSSAnimationData.h"
#include "core/rendering/style/KeyframeList.h"
#include "core/rendering/style/RenderStyle.h"
#include "public/platform/WebAnimation.h"
#include "wtf/text/StringBuilder.h"

using WebKit::WebAnimation;

namespace WebCore {

namespace {

String animationNameForTransition(AnimatedPropertyID property)
{
    // | is not a valid identifier character in CSS, so this can never conflict with a keyframe identifier.
    StringBuilder id;
    id.appendLiteral("-|transition");
    id.appendNumber(static_cast<int>(property));
    id.append('-');
    return id.toString();
}

AnimatedPropertyID cssToGraphicsLayerProperty(CSSPropertyID cssProperty)
{
    switch (cssProperty) {
    case CSSPropertyWebkitTransform:
        return AnimatedPropertyWebkitTransform;
    case CSSPropertyOpacity:
        return AnimatedPropertyOpacity;
    case CSSPropertyBackgroundColor:
        return AnimatedPropertyInvalid; // Chromium compositor cannot accelerate background color yet.
    case CSSPropertyWebkitFilter:
        return AnimatedPropertyInvalid; // Chromium compositor cannot accelerate filter yet.
    default:
        // It's fine if we see other css properties here; they are just not accelerated.
        break;
    }
    return AnimatedPropertyInvalid;
}

} // namespace

WebAnimations::WebAnimations()
{
}

WebAnimations::~WebAnimations()
{
}

// Copy constructor is needed to use this struct as a return value. It actually moves the ownership, not copy.
WebAnimations::WebAnimations(const WebAnimations& other)
{
    ASSERT(isEmpty());
    m_transformAnimation.swap(const_cast<OwnPtr<WebAnimation>& >(other.m_transformAnimation));
    m_opacityAnimation.swap(const_cast<OwnPtr<WebAnimation>& >(other.m_opacityAnimation));
    m_filterAnimation.swap(const_cast<OwnPtr<WebAnimation>& >(other.m_filterAnimation));
    ASSERT(other.isEmpty());
}

bool WebAnimations::isEmpty() const
{
    return !m_transformAnimation && !m_opacityAnimation && !m_filterAnimation;
}

WebAnimationProvider::WebAnimationProvider()
{
}

WebAnimationProvider::~WebAnimationProvider()
{
}

int WebAnimationProvider::getWebAnimationId(const String& animationName) const
{
    if (!m_animationIdMap.contains(animationName))
        return 0;
    return m_animationIdMap.get(animationName);
}

int WebAnimationProvider::getWebAnimationId(CSSPropertyID property) const
{
    AnimatedPropertyID animatedProperty = cssToGraphicsLayerProperty(property);
    if (animatedProperty == AnimatedPropertyInvalid)
        return 0;
    return getWebAnimationId(animationNameForTransition(animatedProperty));
}

WebAnimations WebAnimationProvider::startAnimation(double timeOffset, const CSSAnimationData* anim, const KeyframeList& keyframes, bool hasTransform, const IntSize& boxSize)
{
    ASSERT(hasTransform || boxSize.isEmpty());
    bool hasOpacity = keyframes.containsProperty(CSSPropertyOpacity);
    bool hasFilter = keyframes.containsProperty(CSSPropertyWebkitFilter);

    if (!hasOpacity && !hasTransform && !hasFilter)
        return WebAnimations();

    KeyframeValueList transformVector(AnimatedPropertyWebkitTransform);
    KeyframeValueList opacityVector(AnimatedPropertyOpacity);
    KeyframeValueList filterVector(AnimatedPropertyWebkitFilter);

    size_t numKeyframes = keyframes.size();
    for (size_t i = 0; i < numKeyframes; ++i) {
        const KeyframeValue& currentKeyframe = keyframes[i];
        const RenderStyle* keyframeStyle = currentKeyframe.style();
        double key = currentKeyframe.key();

        if (!keyframeStyle)
            continue;

        // Get timing function.
        RefPtr<TimingFunction> tf = KeyframeValue::timingFunction(keyframeStyle, keyframes.animationName());

        bool isFirstOrLastKeyframe = !key || key == 1;
        if ((hasTransform && isFirstOrLastKeyframe) || currentKeyframe.containsProperty(CSSPropertyWebkitTransform))
            transformVector.insert(adoptPtr(new TransformAnimationValue(key, &(keyframeStyle->transform()), tf)));

        if ((hasOpacity && isFirstOrLastKeyframe) || currentKeyframe.containsProperty(CSSPropertyOpacity))
            opacityVector.insert(adoptPtr(new FloatAnimationValue(key, keyframeStyle->opacity(), tf)));

        if ((hasFilter && isFirstOrLastKeyframe) || currentKeyframe.containsProperty(CSSPropertyWebkitFilter))
            filterVector.insert(adoptPtr(new FilterAnimationValue(key, &(keyframeStyle->filter()), tf)));
    }
    WebAnimations resultAnimations;
    if (hasTransform)
        resultAnimations.m_transformAnimation = createWebAnimationAndStoreId(transformVector, boxSize, anim, keyframes.animationName(), timeOffset);
    if (hasOpacity)
        resultAnimations.m_opacityAnimation = createWebAnimationAndStoreId(opacityVector, IntSize(), anim, keyframes.animationName(), timeOffset);
    if (hasFilter)
        resultAnimations.m_filterAnimation = createWebAnimationAndStoreId(filterVector, IntSize(), anim, keyframes.animationName(), timeOffset);

    return resultAnimations;
}

WebAnimations WebAnimationProvider::startTransition(double timeOffset, CSSPropertyID property, const RenderStyle* fromStyle, const RenderStyle* toStyle, bool hasTransform, bool hasFilter, const IntSize& boxSize, float fromOpacity, float toOpacity)
{
    ASSERT(property != CSSPropertyInvalid);
    ASSERT(property == CSSPropertyOpacity || (!fromOpacity && !toOpacity));

    WebAnimations resultAnimations;
    if (property == CSSPropertyOpacity) {
        const CSSAnimationData* opacityAnim = toStyle->transitionForProperty(CSSPropertyOpacity);
        if (opacityAnim && !opacityAnim->isEmptyOrZeroDuration()) {
            KeyframeValueList opacityVector(AnimatedPropertyOpacity);
            opacityVector.insert(adoptPtr(new FloatAnimationValue(0, fromOpacity)));
            opacityVector.insert(adoptPtr(new FloatAnimationValue(1, toOpacity)));
            resultAnimations.m_opacityAnimation = createWebAnimationAndStoreId(opacityVector, IntSize(), opacityAnim, animationNameForTransition(AnimatedPropertyOpacity), timeOffset);
        }
    }
    if (property == CSSPropertyWebkitTransform && hasTransform) {
        const CSSAnimationData* transformAnim = toStyle->transitionForProperty(CSSPropertyWebkitTransform);
        if (transformAnim && !transformAnim->isEmptyOrZeroDuration()) {
            KeyframeValueList transformVector(AnimatedPropertyWebkitTransform);
            transformVector.insert(adoptPtr(new TransformAnimationValue(0, &fromStyle->transform())));
            transformVector.insert(adoptPtr(new TransformAnimationValue(1, &toStyle->transform())));
            resultAnimations.m_transformAnimation = createWebAnimationAndStoreId(transformVector, boxSize, transformAnim, animationNameForTransition(AnimatedPropertyWebkitTransform), timeOffset);
        }
    }
    if (property == CSSPropertyWebkitFilter && hasFilter) {
        const CSSAnimationData* filterAnim = toStyle->transitionForProperty(CSSPropertyWebkitFilter);
        if (filterAnim && !filterAnim->isEmptyOrZeroDuration()) {
            KeyframeValueList filterVector(AnimatedPropertyWebkitFilter);
            filterVector.insert(adoptPtr(new FilterAnimationValue(0, &fromStyle->filter())));
            filterVector.insert(adoptPtr(new FilterAnimationValue(1, &toStyle->filter())));
            resultAnimations.m_filterAnimation = createWebAnimationAndStoreId(filterVector, IntSize(), filterAnim, animationNameForTransition(AnimatedPropertyWebkitFilter), timeOffset);
        }
    }

    return resultAnimations;
}

PassOwnPtr<WebAnimation> WebAnimationProvider::createWebAnimationAndStoreId(const KeyframeValueList& values, const IntSize& boxSize, const CSSAnimationData* animation, const String& animationName, double timeOffset)
{
    int animationId = getWebAnimationId(animationName);
    OwnPtr<WebAnimation> webAnimation(createWebAnimation(values, animation, animationId, timeOffset, boxSize));
    if (!webAnimation)
        return PassOwnPtr<WebAnimation>();

    if (!animationId)
        m_animationIdMap.set(animationName, webAnimation->id());
    return webAnimation.release();
}

} // namespace WebCore
