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
#include "core/animation/ElementAnimation.h"

#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ScriptValue.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSTimingFunctionValue.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/css/resolver/StyleResolver.h"
#include "wtf/text/StringBuilder.h"
#include <algorithm>

namespace WebCore {

CSSPropertyID ElementAnimation::camelCaseCSSPropertyNameToID(const String& propertyName)
{
    if (propertyName.find('-') != kNotFound)
        return CSSPropertyInvalid;

    StringBuilder builder;
    size_t position = 0;
    size_t end;
    while ((end = propertyName.find(isASCIIUpper, position)) != kNotFound) {
        builder.append(propertyName.substring(position, end - position) + "-" + toASCIILower((propertyName)[end]));
        position = end + 1;
    }
    builder.append(propertyName.substring(position));
    // Doesn't handle prefixed properties.
    CSSPropertyID id = cssPropertyID(builder.toString());
    return id;
}

void ElementAnimation::populateTiming(Timing& timing, Dictionary timingInputDictionary)
{
    // FIXME: This method needs to be refactored to handle invalid
    // null, NaN, Infinity values better.
    // See: http://www.w3.org/TR/WebIDL/#es-double
    double startDelay = 0;
    timingInputDictionary.get("delay", startDelay);
    if (!std::isnan(startDelay) && !std::isinf(startDelay))
        timing.startDelay = startDelay;

    String fillMode;
    timingInputDictionary.get("fill", fillMode);
    if (fillMode == "none") {
        timing.fillMode = Timing::FillModeNone;
    } else if (fillMode == "backwards") {
        timing.fillMode = Timing::FillModeBackwards;
    } else if (fillMode == "both") {
        timing.fillMode = Timing::FillModeBoth;
    } else if (fillMode == "forwards") {
        timing.fillMode = Timing::FillModeForwards;
    }

    double iterationStart = 0;
    timingInputDictionary.get("iterationStart", iterationStart);
    if (!std::isnan(iterationStart) && !std::isinf(iterationStart))
        timing.iterationStart = std::max<double>(iterationStart, 0);

    double iterationCount = 1;
    timingInputDictionary.get("iterations", iterationCount);
    if (!std::isnan(iterationCount))
        timing.iterationCount = std::max<double>(iterationCount, 0);

    v8::Local<v8::Value> iterationDurationValue;
    bool hasIterationDurationValue = timingInputDictionary.get("duration", iterationDurationValue);
    if (hasIterationDurationValue) {
        double iterationDuration = iterationDurationValue->NumberValue();
        if (!std::isnan(iterationDuration) && iterationDuration >= 0) {
            timing.iterationDuration = iterationDuration;
            timing.hasIterationDuration = true;
        }
    }

    double playbackRate = 1;
    timingInputDictionary.get("playbackRate", playbackRate);
    if (!std::isnan(playbackRate) && !std::isinf(playbackRate))
        timing.playbackRate = playbackRate;

    String direction;
    timingInputDictionary.get("direction", direction);
    if (direction == "reverse") {
        timing.direction = Timing::PlaybackDirectionReverse;
    } else if (direction == "alternate") {
        timing.direction = Timing::PlaybackDirectionAlternate;
    } else if (direction == "alternate-reverse") {
        timing.direction = Timing::PlaybackDirectionAlternateReverse;
    }

    String timingFunctionString;
    timingInputDictionary.get("easing", timingFunctionString);
    RefPtr<CSSValue> timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue(timingFunctionString);
    if (timingFunctionValue) {
        RefPtr<TimingFunction> timingFunction = CSSToStyleMap::animationTimingFunction(timingFunctionValue.get(), false);
        if (timingFunction)
            timing.timingFunction = timingFunction;
    }

    timing.assertValid();
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

Animation* ElementAnimation::animate(Element* element, Vector<Dictionary> keyframeDictionaryVector, Dictionary timingInput)
{
    ASSERT(RuntimeEnabledFeatures::webAnimationsAPIEnabled());

    // FIXME: This test will not be neccessary once resolution of keyframe values occurs at
    // animation application time.
    if (!checkDocumentAndRenderer(element))
        return 0;

    return startAnimation(element, keyframeDictionaryVector, timingInput);
}

Animation* ElementAnimation::animate(Element* element, Vector<Dictionary> keyframeDictionaryVector, double timingInput)
{
    ASSERT(RuntimeEnabledFeatures::webAnimationsAPIEnabled());

    // FIXME: This test will not be neccessary once resolution of keyframe values occurs at
    // animation application time.
    if (!checkDocumentAndRenderer(element))
        return 0;

    return startAnimation(element, keyframeDictionaryVector, timingInput);
}

Animation* ElementAnimation::animate(Element* element, Vector<Dictionary> keyframeDictionaryVector)
{
    ASSERT(RuntimeEnabledFeatures::webAnimationsAPIEnabled());

    // FIXME: This test will not be neccessary once resolution of keyframe values occurs at
    // animation application time.
    if (!checkDocumentAndRenderer(element))
        return 0;

    return startAnimation(element, keyframeDictionaryVector);
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

        Vector<String> keyframeProperties;
        keyframeDictionaryVector[i].getOwnPropertyNames(keyframeProperties);

        for (size_t j = 0; j < keyframeProperties.size(); ++j) {
            String property = keyframeProperties[j];
            CSSPropertyID id = ElementAnimation::camelCaseCSSPropertyNameToID(property);

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

Animation* ElementAnimation::startAnimation(Element* element, Vector<Dictionary> keyframeDictionaryVector, Dictionary timingInput)
{
    RefPtr<KeyframeEffectModel> effect = createKeyframeEffectModel(element, keyframeDictionaryVector);

    Timing timing;
    populateTiming(timing, timingInput);

    RefPtr<Animation> animation = Animation::create(element, effect, timing);
    DocumentTimeline* timeline = element->document().timeline();
    ASSERT(timeline);
    timeline->play(animation.get());

    return animation.get();
}

Animation* ElementAnimation::startAnimation(Element* element, Vector<Dictionary> keyframeDictionaryVector, double timingInput)
{
    RefPtr<KeyframeEffectModel> effect = createKeyframeEffectModel(element, keyframeDictionaryVector);

    Timing timing;
    if (!std::isnan(timingInput)) {
        timing.hasIterationDuration = true;
        timing.iterationDuration = std::max<double>(timingInput, 0);
    }

    RefPtr<Animation> animation = Animation::create(element, effect, timing);
    DocumentTimeline* timeline = element->document().timeline();
    ASSERT(timeline);
    timeline->play(animation.get());

    return animation.get();
}

Animation* ElementAnimation::startAnimation(Element* element, Vector<Dictionary> keyframeDictionaryVector)
{
    RefPtr<KeyframeEffectModel> effect = createKeyframeEffectModel(element, keyframeDictionaryVector);

    Timing timing;

    RefPtr<Animation> animation = Animation::create(element, effect, timing);
    DocumentTimeline* timeline = element->document().timeline();
    ASSERT(timeline);
    timeline->play(animation.get());

    return animation.get();
}

} // namespace WebCore
