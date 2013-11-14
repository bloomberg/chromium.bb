/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/animation/CompositorAnimations.h"

#include "core/animation/AnimatableDouble.h"
#include "core/animation/AnimatableTransform.h"
#include "core/animation/AnimatableValue.h"
#include "core/animation/CompositorAnimationsImpl.h"
#include "core/platform/animation/AnimationTranslationUtil.h"
#include "core/rendering/CompositedLayerMapping.h"
#include "core/rendering/RenderBoxModelObject.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderObject.h"
#include "public/platform/Platform.h"
#include "public/platform/WebAnimation.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebFloatAnimationCurve.h"
#include "public/platform/WebFloatKeyframe.h"

#include <algorithm>
#include <cmath>

namespace WebCore {

// -----------------------------------------------------------------------
// TimingFunctionReverser methods
// -----------------------------------------------------------------------

PassRefPtr<TimingFunction> CompositorAnimationsTimingFunctionReverser::reverse(const LinearTimingFunction* timefunc)
{
    return const_cast<LinearTimingFunction*>(timefunc);
}

PassRefPtr<TimingFunction> CompositorAnimationsTimingFunctionReverser::reverse(const CubicBezierTimingFunction* timefunc)
{
    switch (timefunc->subType()) {
    case CubicBezierTimingFunction::EaseIn:
        return CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseOut);
    case CubicBezierTimingFunction::EaseOut:
        return CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseIn);
    case CubicBezierTimingFunction::EaseInOut:
        return const_cast<CubicBezierTimingFunction*>(timefunc);
    case CubicBezierTimingFunction::Ease: // Ease is not symmetrical
    case CubicBezierTimingFunction::Custom:
        return CubicBezierTimingFunction::create(1 - timefunc->x2(), 1 - timefunc->y2(), 1 - timefunc->x1(), 1 - timefunc->y1());
    default:
        ASSERT_NOT_REACHED();
        return PassRefPtr<TimingFunction>();
    }
}

PassRefPtr<TimingFunction> CompositorAnimationsTimingFunctionReverser::reverse(const ChainedTimingFunction* timefunc)
{
    RefPtr<ChainedTimingFunction> reversed = ChainedTimingFunction::create();
    for (size_t i = 0; i < timefunc->m_segments.size(); i++) {
        size_t index = timefunc->m_segments.size() - i - 1;

        RefPtr<TimingFunction> rtf = reverse(timefunc->m_segments[index].m_timingFunction.get());
        reversed->appendSegment(1 - timefunc->m_segments[index].m_min, rtf.get());
    }
    return reversed;
}

PassRefPtr<TimingFunction> CompositorAnimationsTimingFunctionReverser::reverse(const TimingFunction* timefunc)
{
    switch (timefunc->type()) {
    case TimingFunction::LinearFunction: {
        const LinearTimingFunction* linear = static_cast<const LinearTimingFunction*>(timefunc);
        return reverse(linear);
    }
    case TimingFunction::CubicBezierFunction: {
        const CubicBezierTimingFunction* cubic = static_cast<const CubicBezierTimingFunction*>(timefunc);
        return reverse(cubic);
    }
    case TimingFunction::ChainedFunction: {
        const ChainedTimingFunction* chained = static_cast<const ChainedTimingFunction*>(timefunc);
        return reverse(chained);
    }

    // Steps function can not be reversed.
    case TimingFunction::StepsFunction:
    default:
        ASSERT_NOT_REACHED();
        return PassRefPtr<TimingFunction>();
    }
}

// -----------------------------------------------------------------------
// CompositorAnimations public API
// -----------------------------------------------------------------------

bool CompositorAnimations::isCandidateForCompositorAnimation(const Timing& timing, const AnimationEffect& effect)
{
    const KeyframeAnimationEffect& keyframeEffect = *toKeyframeAnimationEffect(&effect);

    return CompositorAnimationsImpl::isCandidateForCompositor(keyframeEffect)
        && CompositorAnimationsImpl::isCandidateForCompositor(timing, keyframeEffect.getFrames());
}

bool CompositorAnimations::canStartCompositorAnimation(const Element& element)
{
    return element.renderer() && element.renderer()->compositingState() == PaintsIntoOwnBacking;
}

void CompositorAnimations::startCompositorAnimation(const Element& element, const Timing& timing, const AnimationEffect& effect, Vector<int>& startedAnimationIds)
{
    ASSERT(isCandidateForCompositorAnimation(timing, effect));
    ASSERT(canStartCompositorAnimation(element));

    const KeyframeAnimationEffect& keyframeEffect = *toKeyframeAnimationEffect(&effect);

    RenderLayer* layer = toRenderBoxModelObject(element.renderer())->layer();

    // FIXME: Should we check in some way if there is an animation already created?
    Vector<OwnPtr<blink::WebAnimation> > animations;
    CompositorAnimationsImpl::getCompositorAnimations(timing, keyframeEffect, animations);
    for (size_t i = 0; i < animations.size(); ++i) {
        startedAnimationIds.append(animations[i]->id());
        layer->compositedLayerMapping()->mainGraphicsLayer()->addAnimation(animations[i].release());
    }
}

void CompositorAnimations::cancelCompositorAnimation(const Element& element, int id)
{
    // FIXME: Implement.
    ASSERT_NOT_REACHED();
}

// -----------------------------------------------------------------------
// CompositorAnimationsKeyframeEffectHelper
// -----------------------------------------------------------------------

PassOwnPtr<Vector<CSSPropertyID> > CompositorAnimationsKeyframeEffectHelper::getProperties(
    const KeyframeAnimationEffect* effect)
{
    const_cast<KeyframeAnimationEffect*>(effect)->ensureKeyframeGroups();
    OwnPtr<Vector<CSSPropertyID> > propertyIds = adoptPtr(new Vector<CSSPropertyID>(effect->m_keyframeGroups->size()));
    copyKeysToVector(*(effect->m_keyframeGroups.get()), *propertyIds.get());
    return propertyIds.release();
}

PassOwnPtr<CompositorAnimationsKeyframeEffectHelper::KeyframeValues> CompositorAnimationsKeyframeEffectHelper::getKeyframeValuesForProperty(
    const KeyframeAnimationEffect* effect, CSSPropertyID id, double scale, bool reverse)
{
    const_cast<KeyframeAnimationEffect*>(effect)->ensureKeyframeGroups();
    return getKeyframeValuesForProperty(effect->m_keyframeGroups->get(id), scale, reverse);
}

PassOwnPtr<CompositorAnimationsKeyframeEffectHelper::KeyframeValues> CompositorAnimationsKeyframeEffectHelper::getKeyframeValuesForProperty(
    const KeyframeAnimationEffect::PropertySpecificKeyframeGroup* group, double scale, bool reverse)
{
    OwnPtr<CompositorAnimationsKeyframeEffectHelper::KeyframeValues> values = adoptPtr(
        new CompositorAnimationsKeyframeEffectHelper::KeyframeValues());

    for (size_t i = 0; i < group->m_keyframes.size(); i++) {
        KeyframeAnimationEffect::PropertySpecificKeyframe* frame = group->m_keyframes[i].get();

        double offset = (reverse ? (1 - frame->offset()) : frame->offset()) * scale;
        values->append(std::make_pair(offset, frame->value()));
    }
    if (reverse)
        values->reverse();

    return values.release();
}

// -----------------------------------------------------------------------
// CompositorAnimationsImpl
// -----------------------------------------------------------------------

bool CompositorAnimationsImpl::isCandidateForCompositor(const Keyframe& keyframe)
{
    // Only replace mode can be accelerated
    if (keyframe.composite() != AnimationEffect::CompositeReplace)
        return false;

    // Check all the properties can be accelerated
    const PropertySet properties = keyframe.properties(); // FIXME: properties creates a whole new PropertySet!
    for (PropertySet::const_iterator it = properties.begin(); it != properties.end(); ++it) {
        switch (*it) {
        case CSSPropertyOpacity:
        case CSSPropertyWebkitFilter: // FIXME: What about CSSPropertyFilter?
        case CSSPropertyWebkitTransform:
            continue;
        default:
            return false;
        }
    }
    return true;
}

bool CompositorAnimationsImpl::isCandidateForCompositor(const KeyframeAnimationEffect& effect)
{
    const KeyframeAnimationEffect::KeyframeVector frames = effect.getFrames();
    for (size_t i = 0; i < frames.size(); ++i) {
        if (!isCandidateForCompositor(*frames[i].get()))
            return false;
    }
    return true;
}

bool CompositorAnimationsImpl::isCandidateForCompositor(const Timing& timing, const KeyframeAnimationEffect::KeyframeVector& frames)
{

    CompositorTiming out;
    if (!convertTimingForCompositor(timing, out))
        return false;

    return isCandidateForCompositor(*timing.timingFunction.get(), &frames);
}

bool CompositorAnimationsImpl::isCandidateForCompositor(const TimingFunction& timingFunction, const KeyframeAnimationEffect::KeyframeVector* frames, bool isNestedCall)
{
    switch (timingFunction.type()) {
    case TimingFunction::LinearFunction:
        return true;

    case TimingFunction::CubicBezierFunction:
        // Can have a cubic if we don't have to split it (IE only have two frames).
        if (!(isNestedCall || (frames && frames->size() == 2)))
            return false;

        ASSERT(!frames || (frames->at(0)->offset() == 0.0 && frames->at(1)->offset() == 1.0));

        return true;

    case TimingFunction::StepsFunction:
        return false;

    case TimingFunction::ChainedFunction: {
        // Currently we only support chained segments in the form the CSS code
        // generates. These chained segments are only one level deep and have
        // one timing function per frame.
        const ChainedTimingFunction* chained = static_cast<const ChainedTimingFunction*>(&timingFunction);
        if (isNestedCall)
            return false;

        if (!chained->m_segments.size())
            return false;

        if (frames->size() != chained->m_segments.size() + 1)
            return false;

        for (size_t timeIndex = 0; timeIndex < chained->m_segments.size(); timeIndex++) {
            const ChainedTimingFunction::Segment& segment = chained->m_segments[timeIndex];

            if (frames->at(timeIndex)->offset() != segment.m_min || frames->at(timeIndex + 1)->offset() != segment.m_max)
                return false;

            if (!isCandidateForCompositor(*segment.m_timingFunction.get(), 0, true))
                return false;
        }
        return true;
    }
    default:
        ASSERT_NOT_REACHED();
    };
    return false;
}

bool CompositorAnimationsImpl::convertTimingForCompositor(const Timing& timing, CompositorTiming& out)
{
    timing.assertValid();

    // FIXME: Support positive startDelay
    if (timing.startDelay > 0.0)
        return false;

    // All fill modes are supported (the calling code handles them).

    // FIXME: Support non-zero iteration start.
    if (timing.iterationStart)
        return false;

    // FIXME: Compositor only supports finite, non-zero, integer iteration
    // counts.
    if (!std::isfinite(timing.iterationCount) || (std::floor(timing.iterationCount) != timing.iterationCount) || !timing.iterationCount)
        return false;

    if (!timing.iterationDuration)
        return false;

    // FIXME: Support other playback rates
    if (timing.playbackRate != 1)
        return false;

    // All directions are supported.

    // Now attempt an actual conversion
    out.scaledDuration = timing.iterationDuration;
    ASSERT(out.scaledDuration > 0);

    double scaledStartDelay = timing.startDelay;
    ASSERT(scaledStartDelay <= 0);

    int skippedIterations = std::floor(std::abs(scaledStartDelay) / out.scaledDuration);
    ASSERT(skippedIterations >= 0);
    if (skippedIterations >= timing.iterationCount)
        return false;

    out.reverse = (timing.direction == Timing::PlaybackDirectionReverse
        || timing.direction == Timing::PlaybackDirectionAlternateReverse);
    out.alternate = (timing.direction == Timing::PlaybackDirectionAlternate
        || timing.direction == Timing::PlaybackDirectionAlternateReverse);
    if (out.alternate && (skippedIterations % 2))
        out.reverse = !out.reverse;

    out.adjustedIterationCount = std::floor(timing.iterationCount) - skippedIterations;
    ASSERT(out.adjustedIterationCount > 0);

    out.scaledTimeOffset = scaledStartDelay + skippedIterations * out.scaledDuration;
    ASSERT(out.scaledTimeOffset <= 0);
    return true;
}

namespace {

template<typename PlatformAnimationKeyframeType>
static PassOwnPtr<PlatformAnimationKeyframeType> createPlatformKeyframe(double offset, const AnimatableValue&)
{
    // Only the specialized versions of this templated function (found in
    // the cpp file) should ever be called.
    // FIXME: COMPILE_ASSERT(false, CompositorAnimationsNonSpecializedCreateKeyframeShouldNeverBeUsed);
    ASSERT_NOT_REACHED();
}

template<>
PassOwnPtr<blink::WebFloatKeyframe> createPlatformKeyframe<blink::WebFloatKeyframe>(
    double offset, const AnimatableValue& value)
{
    const AnimatableDouble* d = toAnimatableDouble(&value);
    return adoptPtr(new blink::WebFloatKeyframe(offset, d->toDouble()));
}

template<typename PlatformAnimationCurveType, typename PlatformAnimationKeyframeType>
void addKeyframeWithTimingFunction(PlatformAnimationCurveType& curve, const PlatformAnimationKeyframeType& keyframe, const TimingFunction* timingFunction)
{
    if (!timingFunction) {
        curve.add(keyframe);
        return;
    }

    switch (timingFunction->type()) {
    case TimingFunction::LinearFunction:
        curve.add(keyframe, blink::WebAnimationCurve::TimingFunctionTypeLinear);
        return;

    case TimingFunction::CubicBezierFunction: {
        const CubicBezierTimingFunction* cubic = static_cast<const CubicBezierTimingFunction*>(timingFunction);

        if (cubic->subType() == CubicBezierTimingFunction::Custom) {
            curve.add(keyframe, cubic->x1(), cubic->y1(), cubic->x2(), cubic->y2());
        } else {

            blink::WebAnimationCurve::TimingFunctionType easeType;
            switch (cubic->subType()) {
            case CubicBezierTimingFunction::Ease:
                easeType = blink::WebAnimationCurve::TimingFunctionTypeEase;
                break;
            case CubicBezierTimingFunction::EaseIn:
                easeType = blink::WebAnimationCurve::TimingFunctionTypeEaseIn;
                break;
            case CubicBezierTimingFunction::EaseOut:
                easeType = blink::WebAnimationCurve::TimingFunctionTypeEaseOut;
                break;
            case CubicBezierTimingFunction::EaseInOut:
                easeType = blink::WebAnimationCurve::TimingFunctionTypeEaseInOut;
                break;

            // Custom Bezier are handled seperately.
            case CubicBezierTimingFunction::Custom:
            default:
                ASSERT_NOT_REACHED();
                return;
            }

            curve.add(keyframe, easeType);
        }
        return;
    }

    case TimingFunction::StepsFunction:
    case TimingFunction::ChainedFunction:
    default:
        ASSERT_NOT_REACHED();
        return;
    }
}

} // namespace anoymous

template<typename PlatformAnimationCurveType, typename PlatformAnimationKeyframeType>
void CompositorAnimationsImpl::addKeyframesToCurve(PlatformAnimationCurveType& curve, const CompositorAnimationsKeyframeEffectHelper::KeyframeValues& values, const TimingFunction& timingFunction)
{
    for (size_t i = 0; i < values.size(); i++) {
        const TimingFunction* keyframeTimingFunction = 0;
        if (i + 1 < values.size()) { // Last keyframe has no timing function
            switch (timingFunction.type()) {
            case TimingFunction::LinearFunction:
            case TimingFunction::CubicBezierFunction:
                keyframeTimingFunction = &timingFunction;
                break;

            case TimingFunction::ChainedFunction: {
                const ChainedTimingFunction* chained = static_cast<const ChainedTimingFunction*>(&timingFunction);
                // ChainedTimingFunction criteria was checked in isCandidate,
                // assert it is valid.
                ASSERT(values.size() == chained->m_segments.size() + 1);
                ASSERT(values[i].first == chained->m_segments[i].m_min);

                keyframeTimingFunction = chained->m_segments[i].m_timingFunction.get();
                break;
            }
            case TimingFunction::StepsFunction:
            default:
                ASSERT_NOT_REACHED();
            }
        }

        ASSERT(!values[i].second->dependsOnUnderlyingValue());
        RefPtr<AnimatableValue> value = values[i].second->compositeOnto(0);
        OwnPtr<PlatformAnimationKeyframeType> keyframe = createPlatformKeyframe<PlatformAnimationKeyframeType>(values[i].first, *value.get());
        addKeyframeWithTimingFunction(curve, *keyframe.get(), keyframeTimingFunction);
    }
}

void CompositorAnimationsImpl::getCompositorAnimations(
    const Timing& timing, const KeyframeAnimationEffect& effect, Vector<OwnPtr<blink::WebAnimation> >& animations)
{
    CompositorTiming compositorTiming;
    bool timingValid = convertTimingForCompositor(timing, compositorTiming);
    ASSERT_UNUSED(timingValid, timingValid);

    RefPtr<TimingFunction> timingFunction = timing.timingFunction;
    if (compositorTiming.reverse)
        timingFunction = CompositorAnimationsTimingFunctionReverser::reverse(timingFunction.get());

    OwnPtr<Vector<CSSPropertyID> > properties = CompositorAnimationsKeyframeEffectHelper::getProperties(&effect);
    for (size_t i = 0; i < properties->size(); i++) {

        OwnPtr<CompositorAnimationsKeyframeEffectHelper::KeyframeValues> values = CompositorAnimationsKeyframeEffectHelper::getKeyframeValuesForProperty(
            &effect, properties->at(i), compositorTiming.scaledDuration, compositorTiming.reverse);

        blink::WebAnimation::TargetProperty targetProperty;
        OwnPtr<blink::WebAnimationCurve> curve;
        switch (properties->at(i)) {
        case CSSPropertyOpacity: {
            targetProperty = blink::WebAnimation::TargetPropertyOpacity;

            OwnPtr<blink::WebFloatAnimationCurve> floatCurve = adoptPtr(blink::Platform::current()->compositorSupport()->createFloatAnimationCurve());
            addKeyframesToCurve<blink::WebFloatAnimationCurve, blink::WebFloatKeyframe>(*floatCurve.get(), *values.get(), *timingFunction.get());
            curve = floatCurve.release();

            break;
        }
        case CSSPropertyWebkitFilter: {
            targetProperty = blink::WebAnimation::TargetPropertyFilter;
            ASSERT_NOT_REACHED();
            break;
        }
        case CSSPropertyWebkitTransform: {
            targetProperty = blink::WebAnimation::TargetPropertyTransform;
            ASSERT_NOT_REACHED();
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            continue;
        }
        ASSERT(curve.get());

        OwnPtr<blink::WebAnimation> animation = adoptPtr(
            blink::Platform::current()->compositorSupport()->createAnimation(*curve, targetProperty));

        animation->setIterations(compositorTiming.adjustedIterationCount);
        animation->setTimeOffset(compositorTiming.scaledTimeOffset);
        animation->setAlternatesDirection(compositorTiming.alternate);

        animations.append(animation.release());
    }
}

} // namespace WebCore
