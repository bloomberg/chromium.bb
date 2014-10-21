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

#include "core/animation/AnimationNode.h"
#include "core/animation/AnimationTranslationUtil.h"
#include "core/animation/CompositorAnimationsImpl.h"
#include "core/animation/animatable/AnimatableDouble.h"
#include "core/animation/animatable/AnimatableFilterOperations.h"
#include "core/animation/animatable/AnimatableTransform.h"
#include "core/animation/animatable/AnimatableValue.h"
#include "core/rendering/RenderBoxModelObject.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"
#include "platform/geometry/FloatBox.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorAnimation.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebFilterAnimationCurve.h"
#include "public/platform/WebFilterKeyframe.h"
#include "public/platform/WebFloatAnimationCurve.h"
#include "public/platform/WebFloatKeyframe.h"
#include "public/platform/WebTransformAnimationCurve.h"
#include "public/platform/WebTransformKeyframe.h"

#include <algorithm>
#include <cmath>

namespace blink {

namespace {

void getKeyframeValuesForProperty(const KeyframeEffectModelBase* effect, CSSPropertyID id, double scale, PropertySpecificKeyframeVector& values)
{
    ASSERT(values.isEmpty());

    for (const auto& keyframe : effect->getPropertySpecificKeyframes(id)) {
        double offset = keyframe->offset() * scale;
        values.append(keyframe->cloneWithOffset(offset));
    }
}

}

bool CompositorAnimations::getAnimatedBoundingBox(FloatBox& box, const AnimationEffect& effect, double minValue, double maxValue) const
{
    const KeyframeEffectModelBase& keyframeEffect = toKeyframeEffectModelBase(effect);

    PropertySet properties = keyframeEffect.properties();

    if (properties.isEmpty())
        return true;

    minValue = std::min(minValue, 0.0);
    maxValue = std::max(maxValue, 1.0);

    for (const auto& property : properties) {
        // TODO: Add the ability to get expanded bounds for filters as well.
        if (property != CSSPropertyTransform && property != CSSPropertyWebkitTransform)
            continue;

        const PropertySpecificKeyframeVector& frames = keyframeEffect.getPropertySpecificKeyframes(property);
        if (frames.isEmpty() || frames.size() < 2)
            continue;

        FloatBox originalBox(box);

        for (size_t j = 0; j < frames.size() - 1; ++j) {
            const AnimatableTransform* startTransform = toAnimatableTransform(frames[j]->getAnimatableValue().get());
            const AnimatableTransform* endTransform = toAnimatableTransform(frames[j+1]->getAnimatableValue().get());
            // TODO: Add support for inflating modes other than Replace.
            if (frames[j]->composite() != AnimationEffect::CompositeReplace)
                return false;

            const TimingFunction& timing = frames[j]->easing();
            double min = 0;
            double max = 1;
            if (j == 0) {
                float frameLength = frames[j+1]->offset();
                if (frameLength > 0) {
                    min = minValue / frameLength;
                }
            }

            if (j == frames.size() - 2) {
                float frameLength = frames[j+1]->offset() - frames[j]->offset();
                if (frameLength > 0) {
                    max = 1 + (maxValue - 1) / frameLength;
                }
            }

            FloatBox bounds;
            timing.range(&min, &max);
            if (!endTransform->transformOperations().blendedBoundsForBox(originalBox, startTransform->transformOperations(), min, max, &bounds))
                return false;
            box.expandTo(bounds);
        }
    }
    return true;
}

// -----------------------------------------------------------------------
// CompositorAnimations public API
// -----------------------------------------------------------------------

bool CompositorAnimations::isCandidateForAnimationOnCompositor(const Timing& timing, const AnimationEffect& effect, double playerPlaybackRate)
{
    const KeyframeEffectModelBase& keyframeEffect = toKeyframeEffectModelBase(effect);

    PropertySet properties = keyframeEffect.properties();

    if (properties.isEmpty())
        return false;

    for (const auto& property : properties) {
        const PropertySpecificKeyframeVector& keyframes = keyframeEffect.getPropertySpecificKeyframes(property);
        ASSERT(keyframes.size() >= 2);
        auto* lastKeyframe = keyframes.last().get();
        for (const auto& keyframe : keyframes) {
            // FIXME: Determine candidacy based on the CSSValue instead of a snapshot AnimatableValue.
            if (keyframe->composite() != AnimationEffect::CompositeReplace || !keyframe->getAnimatableValue())
                return false;

            switch (property) {
            case CSSPropertyOpacity:
                break;
            case CSSPropertyTransform:
                if (toAnimatableTransform(keyframe->getAnimatableValue().get())->transformOperations().dependsOnBoxSize())
                    return false;
                break;
            case CSSPropertyWebkitFilter: {
                const FilterOperations& operations = toAnimatableFilterOperations(keyframe->getAnimatableValue().get())->operations();
                if (operations.hasFilterThatMovesPixels())
                    return false;
                break;
            }
            default:
                return false;
            }

            // FIXME: Remove this check when crbug.com/229405 is resolved
            if (keyframe != lastKeyframe && keyframe->easing().type() == TimingFunction::StepsFunction)
                return false;
        }
    }

    CompositorAnimationsImpl::CompositorTiming out;
    if (!CompositorAnimationsImpl::convertTimingForCompositor(timing, 0, out, playerPlaybackRate))
        return false;

    if (timing.timingFunction->type() != TimingFunction::LinearFunction) {
        // Checks the of size of KeyframeVector instead of PropertySpecificKeyframeVector.
        const KeyframeVector& keyframes = keyframeEffect.getFrames();
        if (keyframes.size() == 2 && keyframes.first()->easing().type() == TimingFunction::LinearFunction && timing.timingFunction->type() != TimingFunction::StepsFunction)
            return true;

        // FIXME: Support non-linear timing functions in the compositor for
        // more than two keyframes and step timing functions in the compositor.
        return false;
    }

    return true;
}

bool CompositorAnimations::canStartAnimationOnCompositor(const Element& element)
{
    return element.renderer() && element.renderer()->compositingState() == PaintsIntoOwnBacking;
}

bool CompositorAnimations::startAnimationOnCompositor(const Element& element, double startTime, double timeOffset, const Timing& timing, const AnimationEffect& effect, Vector<int>& startedAnimationIds, double playerPlaybackRate)
{
    ASSERT(startedAnimationIds.isEmpty());
    ASSERT(isCandidateForAnimationOnCompositor(timing, effect, playerPlaybackRate));
    ASSERT(canStartAnimationOnCompositor(element));

    const KeyframeEffectModelBase& keyframeEffect = toKeyframeEffectModelBase(effect);

    RenderLayer* layer = toRenderBoxModelObject(element.renderer())->layer();
    ASSERT(layer);

    Vector<OwnPtr<WebCompositorAnimation>> animations;
    CompositorAnimationsImpl::getAnimationOnCompositor(timing, startTime, timeOffset, keyframeEffect, animations, playerPlaybackRate);
    ASSERT(!animations.isEmpty());
    for (auto& animation : animations) {
        int id = animation->id();
        if (!layer->compositedLayerMapping()->mainGraphicsLayer()->addAnimation(animation.release())) {
            // FIXME: We should know ahead of time whether these animations can be started.
            for (int startedAnimationId : startedAnimationIds)
                cancelAnimationOnCompositor(element, startedAnimationId);
            startedAnimationIds.clear();
            return false;
        }
        startedAnimationIds.append(id);
    }
    ASSERT(!startedAnimationIds.isEmpty());
    return true;
}

void CompositorAnimations::cancelAnimationOnCompositor(const Element& element, int id)
{
    if (!canStartAnimationOnCompositor(element)) {
        // When an element is being detached, we cancel any associated
        // AnimationPlayers for CSS animations. But by the time we get
        // here the mapping will have been removed.
        // FIXME: Defer remove/pause operations until after the
        // compositing update.
        return;
    }
    toRenderBoxModelObject(element.renderer())->layer()->compositedLayerMapping()->mainGraphicsLayer()->removeAnimation(id);
}

void CompositorAnimations::pauseAnimationForTestingOnCompositor(const Element& element, int id, double pauseTime)
{
    // FIXME: canStartAnimationOnCompositor queries compositingState, which is not necessarily up to date.
    // https://code.google.com/p/chromium/issues/detail?id=339847
    DisableCompositingQueryAsserts disabler;

    if (!canStartAnimationOnCompositor(element)) {
        ASSERT_NOT_REACHED();
        return;
    }
    toRenderBoxModelObject(element.renderer())->layer()->compositedLayerMapping()->mainGraphicsLayer()->pauseAnimation(id, pauseTime);
}

// -----------------------------------------------------------------------
// CompositorAnimationsImpl
// -----------------------------------------------------------------------

bool CompositorAnimationsImpl::convertTimingForCompositor(const Timing& timing, double timeOffset, CompositorTiming& out, double playerPlaybackRate)
{
    timing.assertValid();

    if (!timing.iterationCount || !timing.iterationDuration)
        return false;

    if (!std::isfinite(timing.iterationCount)) {
        out.adjustedIterationCount = -1;
    } else {
        out.adjustedIterationCount = timing.iterationCount;
    }

    out.scaledDuration = timing.iterationDuration;
    out.direction = timing.direction;
    // Compositor's time offset is positive for seeking into the animation.
    out.scaledTimeOffset = -timing.startDelay + timeOffset;
    out.playbackRate = timing.playbackRate * playerPlaybackRate;
    out.fillMode = timing.fillMode == Timing::FillModeAuto ? Timing::FillModeNone : timing.fillMode;
    out.iterationStart = timing.iterationStart;
    out.assertValid();
    return true;
}

namespace {

template<typename PlatformAnimationCurveType, typename PlatformAnimationKeyframeType>
void addKeyframeWithTimingFunction(PlatformAnimationCurveType& curve, const PlatformAnimationKeyframeType& keyframe, const TimingFunction* timingFunction)
{
    if (!timingFunction) {
        curve.add(keyframe);
        return;
    }

    switch (timingFunction->type()) {
    case TimingFunction::LinearFunction:
        curve.add(keyframe, WebCompositorAnimationCurve::TimingFunctionTypeLinear);
        return;

    case TimingFunction::CubicBezierFunction: {
        const CubicBezierTimingFunction* cubic = toCubicBezierTimingFunction(timingFunction);

        if (cubic->subType() == CubicBezierTimingFunction::Custom) {
            curve.add(keyframe, cubic->x1(), cubic->y1(), cubic->x2(), cubic->y2());
        } else {

            WebCompositorAnimationCurve::TimingFunctionType easeType;
            switch (cubic->subType()) {
            case CubicBezierTimingFunction::Ease:
                easeType = WebCompositorAnimationCurve::TimingFunctionTypeEase;
                break;
            case CubicBezierTimingFunction::EaseIn:
                easeType = WebCompositorAnimationCurve::TimingFunctionTypeEaseIn;
                break;
            case CubicBezierTimingFunction::EaseOut:
                easeType = WebCompositorAnimationCurve::TimingFunctionTypeEaseOut;
                break;
            case CubicBezierTimingFunction::EaseInOut:
                easeType = WebCompositorAnimationCurve::TimingFunctionTypeEaseInOut;
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
    default:
        ASSERT_NOT_REACHED();
        return;
    }
}

} // namespace anoymous

void CompositorAnimationsImpl::addKeyframesToCurve(WebCompositorAnimationCurve& curve, const PropertySpecificKeyframeVector& keyframes, const Timing& timing)
{
    auto* lastKeyframe = keyframes.last().get();
    for (const auto& keyframe : keyframes) {
        const TimingFunction* keyframeTimingFunction = 0;
        if (keyframe != lastKeyframe) { // Ignore timing function of last frame.
            if (keyframes.size() == 2 && keyframes.first()->easing().type() == TimingFunction::LinearFunction)
                keyframeTimingFunction = timing.timingFunction.get();
            else
                keyframeTimingFunction = &keyframe->easing();
        }

        // FIXME: This relies on StringKeyframes being eagerly evaluated, which will
        // not happen eventually. Instead we should extract the CSSValue here
        // and convert using another set of toAnimatableXXXOperations functions.
        const AnimatableValue* value = keyframe->getAnimatableValue().get();

        switch (curve.type()) {
        case WebCompositorAnimationCurve::AnimationCurveTypeFilter: {
            OwnPtr<WebFilterOperations> ops = adoptPtr(Platform::current()->compositorSupport()->createFilterOperations());
            toWebFilterOperations(toAnimatableFilterOperations(value)->operations(), ops.get());

            WebFilterKeyframe filterKeyframe(keyframe->offset(), ops.release());
            WebFilterAnimationCurve* filterCurve = static_cast<WebFilterAnimationCurve*>(&curve);
            addKeyframeWithTimingFunction(*filterCurve, filterKeyframe, keyframeTimingFunction);
            break;
        }
        case WebCompositorAnimationCurve::AnimationCurveTypeFloat: {
            WebFloatKeyframe floatKeyframe(keyframe->offset(), toAnimatableDouble(value)->toDouble());
            WebFloatAnimationCurve* floatCurve = static_cast<WebFloatAnimationCurve*>(&curve);
            addKeyframeWithTimingFunction(*floatCurve, floatKeyframe, keyframeTimingFunction);
            break;
        }
        case WebCompositorAnimationCurve::AnimationCurveTypeTransform: {
            OwnPtr<WebTransformOperations> ops = adoptPtr(Platform::current()->compositorSupport()->createTransformOperations());
            toWebTransformOperations(toAnimatableTransform(value)->transformOperations(), ops.get());

            WebTransformKeyframe transformKeyframe(keyframe->offset(), ops.release());
            WebTransformAnimationCurve* transformCurve = static_cast<WebTransformAnimationCurve*>(&curve);
            addKeyframeWithTimingFunction(*transformCurve, transformKeyframe, keyframeTimingFunction);
            break;
        }
        default:
            ASSERT_NOT_REACHED();
        }
    }
}

void CompositorAnimationsImpl::getAnimationOnCompositor(const Timing& timing, double startTime, double timeOffset, const KeyframeEffectModelBase& effect, Vector<OwnPtr<WebCompositorAnimation> >& animations, double playerPlaybackRate)
{
    ASSERT(animations.isEmpty());
    CompositorTiming compositorTiming;
    bool timingValid = convertTimingForCompositor(timing, timeOffset, compositorTiming, playerPlaybackRate);
    ASSERT_UNUSED(timingValid, timingValid);

    PropertySet properties = effect.properties();
    ASSERT(!properties.isEmpty());
    for (const auto& property : properties) {
        PropertySpecificKeyframeVector values;
        getKeyframeValuesForProperty(&effect, property, compositorTiming.scaledDuration, values);

        WebCompositorAnimation::TargetProperty targetProperty;
        OwnPtr<WebCompositorAnimationCurve> curve;
        switch (property) {
        case CSSPropertyOpacity: {
            targetProperty = WebCompositorAnimation::TargetPropertyOpacity;

            WebFloatAnimationCurve* floatCurve = Platform::current()->compositorSupport()->createFloatAnimationCurve();
            addKeyframesToCurve(*floatCurve, values, timing);
            curve = adoptPtr(floatCurve);
            break;
        }
        case CSSPropertyWebkitFilter: {
            targetProperty = WebCompositorAnimation::TargetPropertyFilter;
            WebFilterAnimationCurve* filterCurve = Platform::current()->compositorSupport()->createFilterAnimationCurve();
            addKeyframesToCurve(*filterCurve, values, timing);
            curve = adoptPtr(filterCurve);
            break;
        }
        case CSSPropertyTransform: {
            targetProperty = WebCompositorAnimation::TargetPropertyTransform;
            WebTransformAnimationCurve* transformCurve = Platform::current()->compositorSupport()->createTransformAnimationCurve();
            addKeyframesToCurve(*transformCurve, values, timing);
            curve = adoptPtr(transformCurve);
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            continue;
        }
        ASSERT(curve.get());

        OwnPtr<WebCompositorAnimation> animation = adoptPtr(Platform::current()->compositorSupport()->createAnimation(*curve, targetProperty));

        if (!std::isnan(startTime))
            animation->setStartTime(startTime);

        animation->setIterations(compositorTiming.adjustedIterationCount);
        animation->setIterationStart(compositorTiming.iterationStart);
        animation->setTimeOffset(compositorTiming.scaledTimeOffset);

        switch (compositorTiming.direction) {
        case Timing::PlaybackDirectionNormal:
            animation->setDirection(blink::WebCompositorAnimation::DirectionNormal);
            break;
        case Timing::PlaybackDirectionReverse:
            animation->setDirection(blink::WebCompositorAnimation::DirectionReverse);
            break;
        case Timing::PlaybackDirectionAlternate:
            animation->setDirection(blink::WebCompositorAnimation::DirectionAlternate);
            break;
        case Timing::PlaybackDirectionAlternateReverse:
            animation->setDirection(blink::WebCompositorAnimation::DirectionAlternateReverse);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        animation->setPlaybackRate(compositorTiming.playbackRate);

        switch (compositorTiming.fillMode) {
        case Timing::FillModeNone:
            animation->setFillMode(blink::WebCompositorAnimation::FillModeNone);
            break;
        case Timing::FillModeForwards:
            animation->setFillMode(blink::WebCompositorAnimation::FillModeForwards);
            break;
        case Timing::FillModeBackwards:
            animation->setFillMode(blink::WebCompositorAnimation::FillModeBackwards);
            break;
        case Timing::FillModeBoth:
            animation->setFillMode(blink::WebCompositorAnimation::FillModeBoth);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        animations.append(animation.release());
    }
    ASSERT(!animations.isEmpty());
}

} // namespace blink
