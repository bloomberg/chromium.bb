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
#include "core/animation/Animation.h"

#include "bindings/v8/Dictionary.h"
#include "core/animation/ActiveAnimations.h"
#include "core/animation/AnimationHelpers.h"
#include "core/animation/CompositorAnimations.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/Player.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Element.h"
#include "core/rendering/RenderLayer.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

PassRefPtr<Animation> Animation::create(PassRefPtr<Element> target, PassRefPtr<AnimationEffect> effect, const Timing& timing, Priority priority, PassOwnPtr<EventDelegate> eventDelegate)
{
    return adoptRef(new Animation(target, effect, timing, priority, eventDelegate));
}

static bool checkDocumentAndRenderer(Element* element)
{
    if (!element->inActiveDocument())
        return false;
    element->document().updateStyleIfNeeded();
    if (!element->renderer())
        return false;
    return true;
}

PassRefPtr<Animation> Animation::create(Element* element, Vector<Dictionary> keyframeDictionaryVector, Dictionary timingInput)
{
    ASSERT(RuntimeEnabledFeatures::webAnimationsAPIEnabled());

    // FIXME: This test will not be neccessary once resolution of keyframe values occurs at
    // animation application time.
    if (!checkDocumentAndRenderer(element))
        return 0;

    return createUnsafe(element, keyframeDictionaryVector, timingInput);
}

PassRefPtr<Animation> Animation::create(Element* element, Vector<Dictionary> keyframeDictionaryVector, double timingInput)
{
    ASSERT(RuntimeEnabledFeatures::webAnimationsAPIEnabled());

    // FIXME: This test will not be neccessary once resolution of keyframe values occurs at
    // animation application time.
    if (!checkDocumentAndRenderer(element))
        return 0;

    return createUnsafe(element, keyframeDictionaryVector, timingInput);
}

PassRefPtr<Animation> Animation::create(Element* element, Vector<Dictionary> keyframeDictionaryVector)
{
    ASSERT(RuntimeEnabledFeatures::webAnimationsAPIEnabled());

    // FIXME: This test will not be neccessary once resolution of keyframe values occurs at
    // animation application time.
    if (!checkDocumentAndRenderer(element))
        return 0;

    return createUnsafe(element, keyframeDictionaryVector);
}

void Animation::setStartDelay(Timing& timing, double startDelay)
{
    if (std::isfinite(startDelay))
        timing.startDelay = startDelay;
    else
        timing.startDelay = 0;
}

void Animation::setEndDelay(Timing& timing, double endDelay)
{
    if (std::isfinite(endDelay))
        timing.endDelay = endDelay;
    else
        timing.endDelay = 0;
}

void Animation::setFillMode(Timing& timing, String fillMode)
{
    if (fillMode == "none") {
        timing.fillMode = Timing::FillModeNone;
    } else if (fillMode == "backwards") {
        timing.fillMode = Timing::FillModeBackwards;
    } else if (fillMode == "both") {
        timing.fillMode = Timing::FillModeBoth;
    } else if (fillMode == "forwards") {
        timing.fillMode = Timing::FillModeForwards;
    } else {
        timing.fillMode = Timing::FillModeAuto;
    }
}

void Animation::setIterationStart(Timing& timing, double iterationStart)
{
    if (!std::isnan(iterationStart) && !std::isinf(iterationStart))
        timing.iterationStart = std::max<double>(iterationStart, 0);
    else
        timing.iterationStart = 0;
}

void Animation::setIterationCount(Timing& timing, double iterationCount)
{
    if (!std::isnan(iterationCount))
        timing.iterationCount = std::max<double>(iterationCount, 0);
    else
        timing.iterationCount = 1;
}

void Animation::setIterationDuration(Timing& timing, double iterationDuration)
{
    if (!std::isnan(iterationDuration) && iterationDuration >= 0)
        timing.iterationDuration = iterationDuration;
    else
        timing.iterationDuration = std::numeric_limits<double>::quiet_NaN();
}

void Animation::setPlaybackRate(Timing& timing, double playbackRate)
{
    if (!std::isnan(playbackRate) && !std::isinf(playbackRate))
        timing.playbackRate = playbackRate;
    else
        timing.playbackRate = 1;
}

void Animation::setPlaybackDirection(Timing& timing, String direction)
{
    if (direction == "reverse") {
        timing.direction = Timing::PlaybackDirectionReverse;
    } else if (direction == "alternate") {
        timing.direction = Timing::PlaybackDirectionAlternate;
    } else if (direction == "alternate-reverse") {
        timing.direction = Timing::PlaybackDirectionAlternateReverse;
    } else {
        timing.direction = Timing::PlaybackDirectionNormal;
    }
}

void Animation::setTimingFunction(Timing& timing, String timingFunctionString)
{
    RefPtrWillBeRawPtr<CSSValue> timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue(timingFunctionString);
    if (timingFunctionValue) {
        RefPtr<TimingFunction> timingFunction = CSSToStyleMap::animationTimingFunction(timingFunctionValue.get(), false);
        if (timingFunction) {
            timing.timingFunction = timingFunction;
            return;
        }
    }
    timing.timingFunction = LinearTimingFunction::create();
}

void Animation::populateTiming(Timing& timing, Dictionary timingInputDictionary)
{
    // FIXME: This method needs to be refactored to handle invalid
    // null, NaN, Infinity values better.
    // See: http://www.w3.org/TR/WebIDL/#es-double
    double startDelay = 0;
    timingInputDictionary.get("delay", startDelay);
    setStartDelay(timing, startDelay);

    double endDelay = 0;
    timingInputDictionary.get("endDelay", endDelay);
    setEndDelay(timing, endDelay);

    String fillMode;
    timingInputDictionary.get("fill", fillMode);
    setFillMode(timing, fillMode);

    double iterationStart = 0;
    timingInputDictionary.get("iterationStart", iterationStart);
    setIterationStart(timing, iterationStart);

    double iterationCount = 1;
    timingInputDictionary.get("iterations", iterationCount);
    setIterationCount(timing, iterationCount);

    v8::Local<v8::Value> iterationDurationValue;
    if (timingInputDictionary.get("duration", iterationDurationValue)) {
        double iterationDuration = iterationDurationValue->NumberValue();
        setIterationDuration(timing, iterationDuration);
    }

    double playbackRate = 1;
    timingInputDictionary.get("playbackRate", playbackRate);
    setPlaybackRate(timing, playbackRate);

    String direction;
    timingInputDictionary.get("direction", direction);
    setPlaybackDirection(timing, direction);

    String timingFunctionString;
    timingInputDictionary.get("easing", timingFunctionString);
    setTimingFunction(timing, timingFunctionString);

    timing.assertValid();
}

static PassRefPtr<KeyframeEffectModel> createKeyframeEffectModel(Element* element, Vector<Dictionary> keyframeDictionaryVector)
{
    KeyframeEffectModel::KeyframeVector keyframes;
    Vector<RefPtr<MutableStylePropertySet> > propertySetVector;

    for (size_t i = 0; i < keyframeDictionaryVector.size(); ++i) {
        RefPtr<MutableStylePropertySet> propertySet = MutableStylePropertySet::create();
        propertySetVector.append(propertySet);

        RefPtr<Keyframe> keyframe = Keyframe::create();
        keyframes.append(keyframe);

        double offset;
        if (keyframeDictionaryVector[i].get("offset", offset)) {
            keyframe->setOffset(offset);
        }

        String compositeString;
        keyframeDictionaryVector[i].get("composite", compositeString);
        if (compositeString == "add")
            keyframe->setComposite(AnimationEffect::CompositeAdd);

        String timingFunctionString;
        if (keyframeDictionaryVector[i].get("easing", timingFunctionString)) {
            RefPtrWillBeRawPtr<CSSValue> timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue(timingFunctionString);
            if (timingFunctionValue) {
                keyframe->setEasing(CSSToStyleMap::animationTimingFunction(timingFunctionValue.get(), false));
            }
        }

        Vector<String> keyframeProperties;
        keyframeDictionaryVector[i].getOwnPropertyNames(keyframeProperties);

        for (size_t j = 0; j < keyframeProperties.size(); ++j) {
            String property = keyframeProperties[j];
            CSSPropertyID id = camelCaseCSSPropertyNameToID(property);

            // FIXME: There is no way to store invalid properties or invalid values
            // in a Keyframe object, so for now I just skip over them. Eventually we
            // will need to support getFrames(), which should return exactly the
            // keyframes that were input through the API. We will add a layer to wrap
            // KeyframeEffectModel, store input keyframes and implement getFrames.
            if (id == CSSPropertyInvalid || !CSSAnimations::isAnimatableProperty(id))
                continue;

            String value;
            keyframeDictionaryVector[i].get(property, value);
            propertySet->setProperty(id, value);
        }
    }

    // FIXME: Replace this with code that just parses, when that code is available.
    RefPtr<KeyframeEffectModel> effect = StyleResolver::createKeyframeEffectModel(*element, propertySetVector, keyframes);
    return effect;
}

PassRefPtr<Animation> Animation::createUnsafe(Element* element, Vector<Dictionary> keyframeDictionaryVector, Dictionary timingInput)
{
    RefPtr<KeyframeEffectModel> effect = createKeyframeEffectModel(element, keyframeDictionaryVector);

    Timing timing;
    populateTiming(timing, timingInput);

    return create(element, effect, timing);
}

PassRefPtr<Animation> Animation::createUnsafe(Element* element, Vector<Dictionary> keyframeDictionaryVector, double timingInput)
{
    RefPtr<KeyframeEffectModel> effect = createKeyframeEffectModel(element, keyframeDictionaryVector);

    Timing timing;
    if (!std::isnan(timingInput))
        timing.iterationDuration = std::max<double>(timingInput, 0);

    return create(element, effect, timing);
}

PassRefPtr<Animation> Animation::createUnsafe(Element* element, Vector<Dictionary> keyframeDictionaryVector)
{
    RefPtr<KeyframeEffectModel> effect = createKeyframeEffectModel(element, keyframeDictionaryVector);
    Timing timing;

    return create(element, effect, timing);
}

Animation::Animation(PassRefPtr<Element> target, PassRefPtr<AnimationEffect> effect, const Timing& timing, Priority priority, PassOwnPtr<EventDelegate> eventDelegate)
    : TimedItem(timing, eventDelegate)
    , m_target(target)
    , m_effect(effect)
    , m_activeInAnimationStack(false)
    , m_priority(priority)
{
}

void Animation::didAttach()
{
    if (m_target)
        m_target->ensureActiveAnimations()->players().add(player());
}

void Animation::willDetach()
{
    if (m_target)
        m_target->activeAnimations()->players().remove(player());
    if (m_activeInAnimationStack)
        clearEffects();
}

static AnimationStack& ensureAnimationStack(Element* element)
{
    return element->ensureActiveAnimations()->defaultStack();
}

void Animation::applyEffects(bool previouslyInEffect)
{
    ASSERT(isInEffect());
    if (!m_target || !m_effect)
        return;

    if (player() && !previouslyInEffect) {
        ensureAnimationStack(m_target.get()).add(this);
        m_activeInAnimationStack = true;
    }

    double iteration = currentIteration();
    ASSERT(iteration >= 0);
    // FIXME: Handle iteration values which overflow int.
    m_compositableValues = m_effect->sample(static_cast<int>(iteration), timeFraction());
    if (player())
        m_target->setNeedsAnimationStyleRecalc();
}

void Animation::clearEffects()
{
    ASSERT(player());
    ASSERT(m_activeInAnimationStack);
    ensureAnimationStack(m_target.get()).remove(this);

    {
        // FIXME: clearEffects is called from withins style recalc.
        // This queries compositingState, which is not necessarily up to date.
        // https://code.google.com/p/chromium/issues/detail?id=339847
        DisableCompositingQueryAsserts disabler;
        cancelAnimationOnCompositor();
    }

    m_activeInAnimationStack = false;
    m_compositableValues.clear();
    m_target->setNeedsAnimationStyleRecalc();
    invalidate();
}

void Animation::updateChildrenAndEffects() const
{
    if (!m_effect)
        return;
    if (isInEffect())
        const_cast<Animation*>(this)->applyEffects(m_activeInAnimationStack);
    else if (m_activeInAnimationStack)
        const_cast<Animation*>(this)->clearEffects();
}

double Animation::calculateTimeToEffectChange(bool forwards, double localTime, double timeToNextIteration) const
{
    const double start = startTime() + specifiedTiming().startDelay;
    const double end = start + activeDuration();

    switch (phase()) {
    case PhaseBefore:
        ASSERT(start >= localTime);
        return forwards
            ? start - localTime
            : std::numeric_limits<double>::infinity();
    case PhaseActive:
        if (forwards && hasActiveAnimationsOnCompositor()) {
            ASSERT(specifiedTiming().playbackRate == 1);
            // Need service to apply fill / fire events.
            return std::min(end - localTime, timeToNextIteration);
        }
        return 0;
    case PhaseAfter:
        ASSERT(localTime >= end);
        // If this Animation is still in effect then it will need to update
        // when its parent goes out of effect. We have no way of knowing when
        // that will be, however, so the parent will need to supply it.
        return forwards
            ? std::numeric_limits<double>::infinity()
            : localTime - end;
    case PhaseNone:
        ASSERT(player() && player()->timeline() && !player()->timeline()->hasStarted());
        return std::numeric_limits<double>::infinity();
    default:
        ASSERT_NOT_REACHED();
        return std::numeric_limits<double>::infinity();
    }
}

bool Animation::isCandidateForAnimationOnCompositor() const
{
    if (!effect() || !m_target)
        return false;
    return CompositorAnimations::instance()->isCandidateForAnimationOnCompositor(specifiedTiming(), *effect());
}

bool Animation::maybeStartAnimationOnCompositor()
{
    ASSERT(!hasActiveAnimationsOnCompositor());
    if (!isCandidateForAnimationOnCompositor())
        return false;
    if (!CompositorAnimations::instance()->canStartAnimationOnCompositor(*m_target.get()))
        return false;
    if (!CompositorAnimations::instance()->startAnimationOnCompositor(*m_target.get(), specifiedTiming(), *effect(), m_compositorAnimationIds))
        return false;
    ASSERT(!m_compositorAnimationIds.isEmpty());
    return true;
}

bool Animation::hasActiveAnimationsOnCompositor() const
{
    return !m_compositorAnimationIds.isEmpty();
}

bool Animation::hasActiveAnimationsOnCompositor(CSSPropertyID property) const
{
    return hasActiveAnimationsOnCompositor() && affects(property);
}

bool Animation::affects(CSSPropertyID property) const
{
    return m_effect && m_effect->affects(property);
}

void Animation::cancelAnimationOnCompositor()
{
    if (!hasActiveAnimationsOnCompositor())
        return;
    if (!m_target || !m_target->renderer())
        return;
    for (size_t i = 0; i < m_compositorAnimationIds.size(); ++i)
        CompositorAnimations::instance()->cancelAnimationOnCompositor(*m_target.get(), m_compositorAnimationIds[i]);
    m_compositorAnimationIds.clear();
}

void Animation::pauseAnimationForTestingOnCompositor(double pauseTime)
{
    ASSERT(hasActiveAnimationsOnCompositor());
    if (!m_target || !m_target->renderer())
        return;
    for (size_t i = 0; i < m_compositorAnimationIds.size(); ++i)
        CompositorAnimations::instance()->pauseAnimationForTestingOnCompositor(*m_target.get(), m_compositorAnimationIds[i], pauseTime);
}

} // namespace WebCore
