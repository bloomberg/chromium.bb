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

#include "core/animation/CompositorAnimations.h"

#include "core/animation/AnimationEffect.h"
#include "core/animation/AnimationTranslationUtil.h"
#include "core/animation/CompositorAnimationsImpl.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/animatable/AnimatableDouble.h"
#include "core/animation/animatable/AnimatableFilterOperations.h"
#include "core/animation/animatable/AnimatableTransform.h"
#include "core/animation/animatable/AnimatableValue.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/paint/PaintLayer.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/animation/CompositorAnimation.h"
#include "platform/animation/CompositorAnimationPlayer.h"
#include "platform/animation/CompositorFilterAnimationCurve.h"
#include "platform/animation/CompositorFilterKeyframe.h"
#include "platform/animation/CompositorFloatAnimationCurve.h"
#include "platform/animation/CompositorFloatKeyframe.h"
#include "platform/animation/CompositorTransformAnimationCurve.h"
#include "platform/animation/CompositorTransformKeyframe.h"
#include "platform/geometry/FloatBox.h"
#include "platform/graphics/CompositorFactory.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"

#include <algorithm>
#include <cmath>

namespace blink {

namespace {

void getKeyframeValuesForProperty(const KeyframeEffectModelBase* effect, PropertyHandle property, double scale, PropertySpecificKeyframeVector& values)
{
    ASSERT(values.isEmpty());

    for (const auto& keyframe : effect->getPropertySpecificKeyframes(property)) {
        double offset = keyframe->offset() * scale;
        values.append(keyframe->cloneWithOffset(offset));
    }
}

bool considerAnimationAsIncompatible(const Animation& animation, const Animation& animationToAdd)
{
    if (&animation == &animationToAdd)
        return false;

    switch (animation.playStateInternal()) {
    case Animation::Idle:
        return false;
    case Animation::Pending:
    case Animation::Running:
        return true;
    case Animation::Paused:
    case Animation::Finished:
        return Animation::hasLowerPriority(&animationToAdd, &animation);
    default:
        ASSERT_NOT_REACHED();
        return true;
    }
}

bool isTransformRelatedCSSProperty(const PropertyHandle property)
{
    return property.isCSSProperty()
        && (property.cssProperty() == CSSPropertyRotate
        || property.cssProperty() == CSSPropertyScale
        || property.cssProperty() == CSSPropertyTransform
        || property.cssProperty() == CSSPropertyTranslate);
}


bool isTransformRelatedAnimation(const Element& targetElement, const Animation* animation)
{
    return animation->affects(targetElement, CSSPropertyTransform)
        || animation->affects(targetElement, CSSPropertyRotate)
        || animation->affects(targetElement, CSSPropertyScale)
        || animation->affects(targetElement, CSSPropertyTranslate);
}

bool hasIncompatibleAnimations(const Element& targetElement, const Animation& animationToAdd, const EffectModel& effectToAdd)
{
    const bool affectsOpacity = effectToAdd.affects(PropertyHandle(CSSPropertyOpacity));
    const bool affectsTransform = effectToAdd.isTransformRelatedEffect();
    const bool affectsFilter = effectToAdd.affects(PropertyHandle(CSSPropertyWebkitFilter));
    const bool affectsBackdropFilter = effectToAdd.affects(PropertyHandle(CSSPropertyBackdropFilter));

    if (!targetElement.hasAnimations())
        return false;

    ElementAnimations* elementAnimations = targetElement.elementAnimations();
    ASSERT(elementAnimations);

    for (const auto& entry : elementAnimations->animations()) {
        const Animation* attachedAnimation = entry.key;
        if (!considerAnimationAsIncompatible(*attachedAnimation, animationToAdd))
            continue;

        if ((affectsOpacity && attachedAnimation->affects(targetElement, CSSPropertyOpacity))
            || (affectsTransform && isTransformRelatedAnimation(targetElement, attachedAnimation))
            || (affectsFilter && attachedAnimation->affects(targetElement, CSSPropertyWebkitFilter))
            || (affectsBackdropFilter && attachedAnimation->affects(targetElement, CSSPropertyBackdropFilter)))
            return true;
    }

    return false;
}

} // namespace

CompositorAnimations::CompositorAnimations()
{
}

CompositorAnimations* CompositorAnimations::instance(CompositorAnimations* newInstance)
{
    static CompositorAnimations* instance = new CompositorAnimations();
    if (newInstance) {
        instance = newInstance;
    }
    return instance;
}

bool CompositorAnimations::isCompositableProperty(CSSPropertyID property)
{
    for (CSSPropertyID id : compositableProperties) {
        if (property == id)
            return true;
    }
    return false;
}

const CSSPropertyID CompositorAnimations::compositableProperties[7] = {
    CSSPropertyOpacity,
    CSSPropertyRotate,
    CSSPropertyScale,
    CSSPropertyTransform,
    CSSPropertyTranslate,
    CSSPropertyWebkitFilter,
    CSSPropertyBackdropFilter
};

bool CompositorAnimations::getAnimatedBoundingBox(FloatBox& box, const EffectModel& effect, double minValue, double maxValue) const
{
    const KeyframeEffectModelBase& keyframeEffect = toKeyframeEffectModelBase(effect);

    PropertyHandleSet properties = keyframeEffect.properties();

    if (properties.isEmpty())
        return true;

    minValue = std::min(minValue, 0.0);
    maxValue = std::max(maxValue, 1.0);

    for (const auto& property : properties) {
        if (!property.isCSSProperty())
            continue;

        // TODO: Add the ability to get expanded bounds for filters as well.
        if (!isTransformRelatedCSSProperty(property))
            continue;

        const PropertySpecificKeyframeVector& frames = keyframeEffect.getPropertySpecificKeyframes(property);
        if (frames.isEmpty() || frames.size() < 2)
            continue;

        FloatBox originalBox(box);

        for (size_t j = 0; j < frames.size() - 1; ++j) {
            const AnimatableTransform* startTransform = toAnimatableTransform(frames[j]->getAnimatableValue().get());
            const AnimatableTransform* endTransform = toAnimatableTransform(frames[j+1]->getAnimatableValue().get());
            if (!startTransform || !endTransform)
                return false;

            // TODO: Add support for inflating modes other than Replace.
            if (frames[j]->composite() != EffectModel::CompositeReplace)
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

bool CompositorAnimations::isCandidateForAnimationOnCompositor(const Timing& timing, const Element& targetElement, const Animation* animationToAdd, const EffectModel& effect, double animationPlaybackRate)
{
    const KeyframeEffectModelBase& keyframeEffect = toKeyframeEffectModelBase(effect);

    PropertyHandleSet properties = keyframeEffect.properties();
    if (properties.isEmpty())
        return false;

    unsigned transformPropertyCount = 0;
    for (const auto& property : properties) {
        if (!property.isCSSProperty())
            return false;

        if (isTransformRelatedCSSProperty(property)) {
            if (targetElement.layoutObject() && targetElement.layoutObject()->isInline()) {
                return false;
            }
            transformPropertyCount++;
        }

        const PropertySpecificKeyframeVector& keyframes = keyframeEffect.getPropertySpecificKeyframes(property);
        ASSERT(keyframes.size() >= 2);
        for (const auto& keyframe : keyframes) {
            // FIXME: Determine candidacy based on the CSSValue instead of a snapshot AnimatableValue.
            bool isNeutralKeyframe = keyframe->isCSSPropertySpecificKeyframe() && !toCSSPropertySpecificKeyframe(keyframe.get())->value() && keyframe->composite() == EffectModel::CompositeAdd;
            if ((keyframe->composite() != EffectModel::CompositeReplace && !isNeutralKeyframe) || !keyframe->getAnimatableValue())
                return false;

            switch (property.cssProperty()) {
            case CSSPropertyOpacity:
                break;
            case CSSPropertyRotate:
            case CSSPropertyScale:
            case CSSPropertyTranslate:
            case CSSPropertyTransform:
                if (toAnimatableTransform(keyframe->getAnimatableValue().get())->transformOperations().dependsOnBoxSize())
                    return false;
                break;
            case CSSPropertyWebkitFilter:
            case CSSPropertyBackdropFilter: {
                const FilterOperations& operations = toAnimatableFilterOperations(keyframe->getAnimatableValue().get())->operations();
                if (operations.hasFilterThatMovesPixels())
                    return false;
                break;
            }
            default:
                // any other types are not allowed to run on compositor.
                return false;
            }
        }
    }

    // TODO: Support multiple transform property animations on the compositor
    if (transformPropertyCount > 1)
        return false;

    if (animationToAdd && hasIncompatibleAnimations(targetElement, *animationToAdd, effect))
        return false;

    CompositorAnimationsImpl::CompositorTiming out;
    if (!CompositorAnimationsImpl::convertTimingForCompositor(timing, 0, out, animationPlaybackRate))
        return false;

    return true;
}

void CompositorAnimations::cancelIncompatibleAnimationsOnCompositor(const Element& targetElement, const Animation& animationToAdd, const EffectModel& effectToAdd)
{
    const bool affectsOpacity = effectToAdd.affects(PropertyHandle(CSSPropertyOpacity));
    const bool affectsTransform = effectToAdd.isTransformRelatedEffect();
    const bool affectsFilter = effectToAdd.affects(PropertyHandle(CSSPropertyWebkitFilter));
    const bool affectsBackdropFilter = effectToAdd.affects(PropertyHandle(CSSPropertyBackdropFilter));

    if (!targetElement.hasAnimations())
        return;

    ElementAnimations* elementAnimations = targetElement.elementAnimations();
    ASSERT(elementAnimations);

    for (const auto& entry : elementAnimations->animations()) {
        Animation* attachedAnimation = entry.key;
        if (!considerAnimationAsIncompatible(*attachedAnimation, animationToAdd))
            continue;

        if ((affectsOpacity && attachedAnimation->affects(targetElement, CSSPropertyOpacity))
            || (affectsTransform && isTransformRelatedAnimation(targetElement, attachedAnimation))
            || (affectsFilter && attachedAnimation->affects(targetElement, CSSPropertyWebkitFilter))
            || (affectsBackdropFilter && attachedAnimation->affects(targetElement, CSSPropertyBackdropFilter)))
            attachedAnimation->cancelAnimationOnCompositor();
    }
}

bool CompositorAnimations::canStartAnimationOnCompositor(const Element& element)
{
    if (!Platform::current()->isThreadedAnimationEnabled())
        return false;
    return element.layoutObject() && element.layoutObject()->compositingState() == PaintsIntoOwnBacking;
}

bool CompositorAnimations::startAnimationOnCompositor(const Element& element, int group, double startTime, double timeOffset, const Timing& timing, const Animation& animation, const EffectModel& effect, Vector<int>& startedAnimationIds, double animationPlaybackRate)
{
    ASSERT(startedAnimationIds.isEmpty());
    ASSERT(isCandidateForAnimationOnCompositor(timing, element, &animation, effect, animationPlaybackRate));
    ASSERT(canStartAnimationOnCompositor(element));

    const KeyframeEffectModelBase& keyframeEffect = toKeyframeEffectModelBase(effect);

    PaintLayer* layer = toLayoutBoxModelObject(element.layoutObject())->layer();
    ASSERT(layer);

    Vector<OwnPtr<CompositorAnimation>> animations;
    CompositorAnimationsImpl::getAnimationOnCompositor(timing, group, startTime, timeOffset, keyframeEffect, animations, animationPlaybackRate);
    ASSERT(!animations.isEmpty());
    for (auto& compositorAnimation : animations) {
        int id = compositorAnimation->id();
        if (RuntimeEnabledFeatures::compositorAnimationTimelinesEnabled()) {
            CompositorAnimationPlayer* compositorPlayer = animation.compositorPlayer();
            ASSERT(compositorPlayer);
            compositorPlayer->addAnimation(compositorAnimation.leakPtr());
        } else if (!layer->compositedLayerMapping()->mainGraphicsLayer()->addAnimation(compositorAnimation.release())) {
            // FIXME: We should know ahead of time whether these animations can be started.
            for (int startedAnimationId : startedAnimationIds)
                cancelAnimationOnCompositor(element, animation, startedAnimationId);
            startedAnimationIds.clear();
            return false;
        }
        startedAnimationIds.append(id);
    }
    ASSERT(!startedAnimationIds.isEmpty());
    return true;
}

void CompositorAnimations::cancelAnimationOnCompositor(const Element& element, const Animation& animation, int id)
{
    if (!canStartAnimationOnCompositor(element)) {
        // When an element is being detached, we cancel any associated
        // Animations for CSS animations. But by the time we get
        // here the mapping will have been removed.
        // FIXME: Defer remove/pause operations until after the
        // compositing update.
        return;
    }
    if (RuntimeEnabledFeatures::compositorAnimationTimelinesEnabled()) {
        CompositorAnimationPlayer* compositorPlayer = animation.compositorPlayer();
        if (compositorPlayer)
            compositorPlayer->removeAnimation(id);
    } else {
        toLayoutBoxModelObject(element.layoutObject())->layer()->compositedLayerMapping()->mainGraphicsLayer()->removeAnimation(id);
    }
}

void CompositorAnimations::pauseAnimationForTestingOnCompositor(const Element& element, const Animation& animation, int id, double pauseTime)
{
    // FIXME: canStartAnimationOnCompositor queries compositingState, which is not necessarily up to date.
    // https://code.google.com/p/chromium/issues/detail?id=339847
    DisableCompositingQueryAsserts disabler;

    if (!canStartAnimationOnCompositor(element)) {
        ASSERT_NOT_REACHED();
        return;
    }
    if (RuntimeEnabledFeatures::compositorAnimationTimelinesEnabled()) {
        CompositorAnimationPlayer* compositorPlayer = animation.compositorPlayer();
        ASSERT(compositorPlayer);
        compositorPlayer->pauseAnimation(id, pauseTime);
    } else {
        toLayoutBoxModelObject(element.layoutObject())->layer()->compositedLayerMapping()->mainGraphicsLayer()->pauseAnimation(id, pauseTime);
    }
}

bool CompositorAnimations::canAttachCompositedLayers(const Element& element, const Animation& animation)
{
    if (!RuntimeEnabledFeatures::compositorAnimationTimelinesEnabled())
        return false;

    if (!animation.compositorPlayer())
        return false;

    if (!element.layoutObject() || !element.layoutObject()->isBoxModelObject())
        return false;

    PaintLayer* layer = toLayoutBoxModelObject(element.layoutObject())->layer();

    if (!layer || !layer->isAllowedToQueryCompositingState()
        || !layer->compositedLayerMapping()
        || !layer->compositedLayerMapping()->mainGraphicsLayer())
        return false;

    if (!layer->compositedLayerMapping()->mainGraphicsLayer()->platformLayer())
        return false;

    return true;
}

void CompositorAnimations::attachCompositedLayers(const Element& element, const Animation& animation)
{
    ASSERT(element.layoutObject());

    PaintLayer* layer = toLayoutBoxModelObject(element.layoutObject())->layer();
    ASSERT(layer);

    CompositorAnimationPlayer* compositorPlayer = animation.compositorPlayer();
    ASSERT(compositorPlayer);

    ASSERT(layer->compositedLayerMapping());
    compositorPlayer->attachLayer(layer->compositedLayerMapping()->mainGraphicsLayer()->platformLayer());
}

// -----------------------------------------------------------------------
// CompositorAnimationsImpl
// -----------------------------------------------------------------------

bool CompositorAnimationsImpl::convertTimingForCompositor(const Timing& timing, double timeOffset, CompositorTiming& out, double animationPlaybackRate)
{
    timing.assertValid();

    // FIXME: Compositor does not know anything about endDelay.
    if (timing.endDelay != 0)
        return false;

    if (std::isnan(timing.iterationDuration) || !timing.iterationCount || !timing.iterationDuration)
        return false;

    if (!std::isfinite(timing.iterationCount)) {
        out.adjustedIterationCount = -1;
    } else {
        out.adjustedIterationCount = timing.iterationCount;
    }

    out.scaledDuration = timing.iterationDuration;
    out.direction = timing.direction;
    // Compositor's time offset is positive for seeking into the animation.
    out.scaledTimeOffset = -timing.startDelay / animationPlaybackRate + timeOffset;
    out.playbackRate = timing.playbackRate * animationPlaybackRate;
    out.fillMode = timing.fillMode == Timing::FillModeAuto ? Timing::FillModeNone : timing.fillMode;
    out.iterationStart = timing.iterationStart;
    out.assertValid();
    return true;
}

namespace {

void getCubicBezierTimingFunctionParameters(const TimingFunction& timingFunction, bool& outCustom,
    CompositorAnimationCurve::TimingFunctionType& outEaseSubType,
    double& outX1, double& outY1, double& outX2, double& outY2)
{
    const CubicBezierTimingFunction& cubic = toCubicBezierTimingFunction(timingFunction);
    outCustom = false;

    switch (cubic.subType()) {
    case CubicBezierTimingFunction::Ease:
        outEaseSubType = CompositorAnimationCurve::TimingFunctionTypeEase;
        break;
    case CubicBezierTimingFunction::EaseIn:
        outEaseSubType = CompositorAnimationCurve::TimingFunctionTypeEaseIn;
        break;
    case CubicBezierTimingFunction::EaseOut:
        outEaseSubType = CompositorAnimationCurve::TimingFunctionTypeEaseOut;
        break;
    case CubicBezierTimingFunction::EaseInOut:
        outEaseSubType = CompositorAnimationCurve::TimingFunctionTypeEaseInOut;
        break;
    case CubicBezierTimingFunction::Custom:
        outCustom = true;
        outX1 = cubic.x1();
        outY1 = cubic.y1();
        outX2 = cubic.x2();
        outY2 = cubic.y2();
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void getStepsTimingFunctionParameters(const TimingFunction& timingFunction, int& outSteps, float& outStepsStartOffset)
{
    const StepsTimingFunction& steps = toStepsTimingFunction(timingFunction);

    outSteps = steps.numberOfSteps();
    switch (steps.stepAtPosition()) {
    case StepsTimingFunction::Start:
        outStepsStartOffset = 1;
        break;
    case StepsTimingFunction::Middle:
        outStepsStartOffset = 0.5;
        break;
    case StepsTimingFunction::End:
        outStepsStartOffset = 0;
        break;
    default:
        ASSERT_NOT_REACHED();
        outStepsStartOffset = 0;
        break;
    }
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
        curve.add(keyframe, CompositorAnimationCurve::TimingFunctionTypeLinear);
        break;

    case TimingFunction::CubicBezierFunction: {
        bool custom;
        CompositorAnimationCurve::TimingFunctionType easeSubType;
        double x1, y1;
        double x2, y2;
        getCubicBezierTimingFunctionParameters(*timingFunction, custom, easeSubType, x1, y1, x2, y2);

        if (custom)
            curve.add(keyframe, x1, y1, x2, y2);
        else
            curve.add(keyframe, easeSubType);
        break;
    }

    case TimingFunction::StepsFunction: {
        int steps;
        float stepsStartOffset;
        getStepsTimingFunctionParameters(*timingFunction, steps, stepsStartOffset);

        curve.add(keyframe, steps, stepsStartOffset);
        break;
    }

    default:
        ASSERT_NOT_REACHED();
    }
}

template <typename PlatformAnimationCurveType>
void setTimingFunctionOnCurve(PlatformAnimationCurveType& curve, TimingFunction* timingFunction)
{
    if (!timingFunction) {
        curve.setLinearTimingFunction();
        return;
    }

    switch (timingFunction->type()) {
    case TimingFunction::LinearFunction:
        curve.setLinearTimingFunction();
        break;

    case TimingFunction::CubicBezierFunction: {
        bool custom;
        CompositorAnimationCurve::TimingFunctionType easeSubType;
        double x1, y1;
        double x2, y2;
        getCubicBezierTimingFunctionParameters(*timingFunction, custom, easeSubType, x1, y1, x2, y2);

        if (custom)
            curve.setCubicBezierTimingFunction(x1, y1, x2, y2);
        else
            curve.setCubicBezierTimingFunction(easeSubType);
        break;
    }

    case TimingFunction::StepsFunction: {
        int steps;
        float stepsStartOffset;
        getStepsTimingFunctionParameters(*timingFunction, steps, stepsStartOffset);

        curve.setStepsTimingFunction(steps, stepsStartOffset);
        break;
    }

    default:
        ASSERT_NOT_REACHED();
    }
}

} // namespace

void CompositorAnimationsImpl::addKeyframesToCurve(CompositorAnimationCurve& curve, const PropertySpecificKeyframeVector& keyframes, const Timing& timing)
{
    auto* lastKeyframe = keyframes.last().get();
    for (const auto& keyframe : keyframes) {
        const TimingFunction* keyframeTimingFunction = 0;
        if (keyframe != lastKeyframe) { // Ignore timing function of last frame.
            keyframeTimingFunction = &keyframe->easing();
        }

        // FIXME: This relies on StringKeyframes being eagerly evaluated, which will
        // not happen eventually. Instead we should extract the CSSValue here
        // and convert using another set of toAnimatableXXXOperations functions.
        const AnimatableValue* value = keyframe->getAnimatableValue().get();

        switch (curve.type()) {
        case CompositorAnimationCurve::AnimationCurveTypeFilter: {
            OwnPtr<CompositorFilterOperations> ops = adoptPtr(CompositorFactory::current().createFilterOperations());
            toCompositorFilterOperations(toAnimatableFilterOperations(value)->operations(), ops.get());

            CompositorFilterKeyframe filterKeyframe(keyframe->offset(), ops.release());
            CompositorFilterAnimationCurve* filterCurve = static_cast<CompositorFilterAnimationCurve*>(&curve);
            addKeyframeWithTimingFunction(*filterCurve, filterKeyframe, keyframeTimingFunction);
            break;
        }
        case CompositorAnimationCurve::AnimationCurveTypeFloat: {
            CompositorFloatKeyframe floatKeyframe(keyframe->offset(), toAnimatableDouble(value)->toDouble());
            CompositorFloatAnimationCurve* floatCurve = static_cast<CompositorFloatAnimationCurve*>(&curve);
            addKeyframeWithTimingFunction(*floatCurve, floatKeyframe, keyframeTimingFunction);
            break;
        }
        case CompositorAnimationCurve::AnimationCurveTypeTransform: {
            OwnPtr<CompositorTransformOperations> ops = adoptPtr(CompositorFactory::current().createTransformOperations());
            toCompositorTransformOperations(toAnimatableTransform(value)->transformOperations(), ops.get());

            CompositorTransformKeyframe transformKeyframe(keyframe->offset(), ops.release());
            CompositorTransformAnimationCurve* transformCurve = static_cast<CompositorTransformAnimationCurve*>(&curve);
            addKeyframeWithTimingFunction(*transformCurve, transformKeyframe, keyframeTimingFunction);
            break;
        }
        default:
            ASSERT_NOT_REACHED();
        }
    }
}

void CompositorAnimationsImpl::getAnimationOnCompositor(const Timing& timing, int group, double startTime, double timeOffset, const KeyframeEffectModelBase& effect, Vector<OwnPtr<CompositorAnimation>>& animations, double animationPlaybackRate)
{
    ASSERT(animations.isEmpty());
    CompositorTiming compositorTiming;
    bool timingValid = convertTimingForCompositor(timing, timeOffset, compositorTiming, animationPlaybackRate);
    ASSERT_UNUSED(timingValid, timingValid);

    PropertyHandleSet properties = effect.properties();
    ASSERT(!properties.isEmpty());
    for (const auto& property : properties) {
        PropertySpecificKeyframeVector values;
        getKeyframeValuesForProperty(&effect, property, compositorTiming.scaledDuration, values);

        CompositorAnimation::TargetProperty targetProperty;
        OwnPtr<CompositorAnimationCurve> curve;
        switch (property.cssProperty()) {
        case CSSPropertyOpacity: {
            targetProperty = CompositorAnimation::TargetPropertyOpacity;

            CompositorFloatAnimationCurve* floatCurve = CompositorFactory::current().createFloatAnimationCurve();
            addKeyframesToCurve(*floatCurve, values, timing);
            setTimingFunctionOnCurve(*floatCurve, timing.timingFunction.get());
            curve = adoptPtr(floatCurve);
            break;
        }
        case CSSPropertyWebkitFilter:
        case CSSPropertyBackdropFilter: {
            targetProperty = CompositorAnimation::TargetPropertyFilter;
            CompositorFilterAnimationCurve* filterCurve = CompositorFactory::current().createFilterAnimationCurve();
            addKeyframesToCurve(*filterCurve, values, timing);
            setTimingFunctionOnCurve(*filterCurve, timing.timingFunction.get());
            curve = adoptPtr(filterCurve);
            break;
        }
        case CSSPropertyRotate:
        case CSSPropertyScale:
        case CSSPropertyTranslate:
        case CSSPropertyTransform: {
            targetProperty = CompositorAnimation::TargetPropertyTransform;
            CompositorTransformAnimationCurve* transformCurve = CompositorFactory::current().createTransformAnimationCurve();
            addKeyframesToCurve(*transformCurve, values, timing);
            setTimingFunctionOnCurve(*transformCurve, timing.timingFunction.get());
            curve = adoptPtr(transformCurve);
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            continue;
        }
        ASSERT(curve.get());

        OwnPtr<CompositorAnimation> animation = adoptPtr(CompositorFactory::current().createAnimation(*curve, targetProperty, group, 0));

        if (!std::isnan(startTime))
            animation->setStartTime(startTime);

        animation->setIterations(compositorTiming.adjustedIterationCount);
        animation->setIterationStart(compositorTiming.iterationStart);
        animation->setTimeOffset(compositorTiming.scaledTimeOffset);

        switch (compositorTiming.direction) {
        case Timing::PlaybackDirectionNormal:
            animation->setDirection(CompositorAnimation::DirectionNormal);
            break;
        case Timing::PlaybackDirectionReverse:
            animation->setDirection(CompositorAnimation::DirectionReverse);
            break;
        case Timing::PlaybackDirectionAlternate:
            animation->setDirection(CompositorAnimation::DirectionAlternate);
            break;
        case Timing::PlaybackDirectionAlternateReverse:
            animation->setDirection(CompositorAnimation::DirectionAlternateReverse);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        animation->setPlaybackRate(compositorTiming.playbackRate);

        switch (compositorTiming.fillMode) {
        case Timing::FillModeNone:
            animation->setFillMode(CompositorAnimation::FillModeNone);
            break;
        case Timing::FillModeForwards:
            animation->setFillMode(CompositorAnimation::FillModeForwards);
            break;
        case Timing::FillModeBackwards:
            animation->setFillMode(CompositorAnimation::FillModeBackwards);
            break;
        case Timing::FillModeBoth:
            animation->setFillMode(CompositorAnimation::FillModeBoth);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        animations.append(animation.release());
    }
    ASSERT(!animations.isEmpty());
}

} // namespace blink
