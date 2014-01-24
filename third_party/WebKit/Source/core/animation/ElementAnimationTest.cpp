/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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
#include "core/animation/AnimatableLength.h"
#include "core/animation/Animation.h"
#include "core/animation/AnimationClock.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/Timing.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"

#include <gtest/gtest.h>

namespace WebCore {

namespace {

v8::Handle<v8::Value> stringToV8Value(String string)
{
    return v8::Handle<v8::Value>::Cast(v8String(v8::Isolate::GetCurrent(), string));
}

v8::Handle<v8::Value> doubleToV8Value(double number)
{
    return v8::Handle<v8::Value>::Cast(v8::Number::New(v8::Isolate::GetCurrent(), number));
}

void setV8ObjectPropertyAsString(v8::Handle<v8::Object> object, String name, String value)
{
    object->Set(stringToV8Value(name), stringToV8Value(value));
}

void setV8ObjectPropertyAsNumber(v8::Handle<v8::Object> object, String name, double value)
{
    object->Set(stringToV8Value(name), doubleToV8Value(value));
}

} // namespace

class AnimationElementAnimationTest : public ::testing::Test {
protected:
    virtual void SetUp()
    {
        document = Document::create();
        document->animationClock().resetTimeForTesting();
        element = document->createElement("foo", ASSERT_NO_EXCEPTION);
        document->timeline()->setZeroTime(0);
        ASSERT_EQ(0, document->timeline()->currentTime());
    }

    RefPtr<Document> document;
    RefPtr<Element> element;

    Animation* startAnimation(Element* element, Vector<Dictionary> keyframesDictionaryVector, Dictionary timingInput)
    {
        return ElementAnimation::startAnimation(element, keyframesDictionaryVector, timingInput);
    }

    Animation* startAnimation(Element* element, Vector<Dictionary> keyframesDictionaryVector, double timingInput)
    {
        return ElementAnimation::startAnimation(element, keyframesDictionaryVector, timingInput);
    }

    Animation* startAnimation(Element* element, Vector<Dictionary> keyframesDictionaryVector)
    {
        return ElementAnimation::startAnimation(element, keyframesDictionaryVector);
    }

    void populateTiming(Timing& timing, Dictionary timingInputDictionary)
    {
        ElementAnimation::populateTiming(timing, timingInputDictionary);
    }

    void applyTimingInputNumber(Timing& timing, v8::Isolate* isolate, String timingProperty, double timingPropertyValue)
    {
        v8::Handle<v8::Object> timingInput = v8::Object::New(isolate);
        setV8ObjectPropertyAsNumber(timingInput, timingProperty, timingPropertyValue);
        Dictionary timingInputDictionary = Dictionary(v8::Handle<v8::Value>::Cast(timingInput), isolate);
        populateTiming(timing, timingInputDictionary);
    }

    void applyTimingInputString(Timing& timing, v8::Isolate* isolate, String timingProperty, String timingPropertyValue)
    {
        v8::Handle<v8::Object> timingInput = v8::Object::New(isolate);
        setV8ObjectPropertyAsString(timingInput, timingProperty, timingPropertyValue);
        Dictionary timingInputDictionary = Dictionary(v8::Handle<v8::Value>::Cast(timingInput), isolate);
        populateTiming(timing, timingInputDictionary);
    }
};

TEST_F(AnimationElementAnimationTest, CanStartAnAnimation)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Vector<Dictionary> jsKeyframes;
    v8::Handle<v8::Object> keyframe1 = v8::Object::New(isolate);
    v8::Handle<v8::Object> keyframe2 = v8::Object::New(isolate);

    setV8ObjectPropertyAsString(keyframe1, "width", "100px");
    setV8ObjectPropertyAsString(keyframe1, "offset", "0");
    setV8ObjectPropertyAsString(keyframe2, "width", "0px");
    setV8ObjectPropertyAsString(keyframe2, "offset", "1");

    jsKeyframes.append(Dictionary(keyframe1, isolate));
    jsKeyframes.append(Dictionary(keyframe2, isolate));

    String value1;
    ASSERT_TRUE(jsKeyframes[0].get("width", value1));
    ASSERT_EQ("100px", value1);

    String value2;
    ASSERT_TRUE(jsKeyframes[1].get("width", value2));
    ASSERT_EQ("0px", value2);

    Animation* animation = startAnimation(element.get(), jsKeyframes, 0);

    Player* player = document->timeline()->players().at(0).get();
    EXPECT_EQ(animation, player->source());

    Element* target = animation->target();
    EXPECT_EQ(*element.get(), *target);

    const KeyframeEffectModel::KeyframeVector keyframes =
        toKeyframeEffectModel(animation->effect())->getFrames();

    EXPECT_EQ(0, keyframes[0]->offset());
    EXPECT_EQ(1, keyframes[1]->offset());

    const AnimatableValue* keyframe1Width = keyframes[0]->propertyValue(CSSPropertyWidth);
    const AnimatableValue* keyframe2Width = keyframes[1]->propertyValue(CSSPropertyWidth);
    ASSERT(keyframe1Width);
    ASSERT(keyframe2Width);

    EXPECT_TRUE(keyframe1Width->isLength());
    EXPECT_TRUE(keyframe2Width->isLength());

    EXPECT_EQ("100px", toAnimatableLength(keyframe1Width)->toCSSValue()->cssText());
    EXPECT_EQ("0px", toAnimatableLength(keyframe2Width)->toCSSValue()->cssText());
}

TEST_F(AnimationElementAnimationTest, ParseCamelCasePropertyNames)
{
    EXPECT_EQ(CSSPropertyInvalid, ElementAnimation::camelCaseCSSPropertyNameToID(String("line-height")));
    EXPECT_EQ(CSSPropertyLineHeight, ElementAnimation::camelCaseCSSPropertyNameToID(String("lineHeight")));
    EXPECT_EQ(CSSPropertyBorderTopWidth, ElementAnimation::camelCaseCSSPropertyNameToID(String("borderTopWidth")));
    EXPECT_EQ(CSSPropertyWidth, ElementAnimation::camelCaseCSSPropertyNameToID(String("width")));
    EXPECT_EQ(CSSPropertyInvalid, ElementAnimation::camelCaseCSSPropertyNameToID(String("Width")));
    EXPECT_EQ(CSSPropertyInvalid, ElementAnimation::camelCaseCSSPropertyNameToID(String("-webkit-transform")));
    EXPECT_EQ(CSSPropertyInvalid, ElementAnimation::camelCaseCSSPropertyNameToID(String("webkitTransform")));
    EXPECT_EQ(CSSPropertyInvalid, ElementAnimation::camelCaseCSSPropertyNameToID(String("cssFloat")));
}

TEST_F(AnimationElementAnimationTest, CanSetDuration)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Vector<Dictionary, 0> jsKeyframes;
    double duration = 2;

    Animation* animation = startAnimation(element.get(), jsKeyframes, duration);

    Player* player = document->timeline()->players().at(0).get();

    EXPECT_EQ(animation, player->source());
    EXPECT_TRUE(player->source()->specified().hasIterationDuration);
    EXPECT_EQ(duration, player->source()->specified().iterationDuration);
}

TEST_F(AnimationElementAnimationTest, CanOmitSpecifiedDuration)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Vector<Dictionary, 0> jsKeyframes;

    Animation* animation = startAnimation(element.get(), jsKeyframes);

    Player* player = document->timeline()->players().at(0).get();
    EXPECT_EQ(animation, player->source());

    EXPECT_FALSE(player->source()->specified().hasIterationDuration);
    EXPECT_EQ(0, player->source()->specified().iterationDuration);
}

TEST_F(AnimationElementAnimationTest, ClipNegativeDurationToZero)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Vector<Dictionary, 0> jsKeyframes;

    Animation* animation = startAnimation(element.get(), jsKeyframes, -2);

    Player* player = document->timeline()->players().at(0).get();
    EXPECT_EQ(animation, player->source());

    EXPECT_TRUE(player->source()->specified().hasIterationDuration);
    EXPECT_EQ(0, player->source()->specified().iterationDuration);
}

TEST_F(AnimationElementAnimationTest, TimingInputStartDelay)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Timing timing;
    EXPECT_EQ(0, timing.startDelay);

    applyTimingInputNumber(timing, isolate, "delay", 1.1);
    EXPECT_EQ(1.1, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputNumber(timing, isolate, "delay", -1);
    EXPECT_EQ(-1, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputString(timing, isolate, "delay", "1");
    EXPECT_EQ(1, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputString(timing, isolate, "delay", "1s");
    EXPECT_EQ(0, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputString(timing, isolate, "delay", "Infinity");
    EXPECT_EQ(0, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputString(timing, isolate, "delay", "-Infinity");
    EXPECT_EQ(0, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputString(timing, isolate, "delay", "NaN");
    EXPECT_EQ(0, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputString(timing, isolate, "delay", "rubbish");
    EXPECT_EQ(0, timing.startDelay);
    timing.startDelay = 0;
}

TEST_F(AnimationElementAnimationTest, TimingInputFillMode)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Timing timing;
    Timing::FillMode defaultFillMode = Timing::FillModeForwards;
    EXPECT_EQ(defaultFillMode, timing.fillMode);

    applyTimingInputString(timing, isolate, "fill", "forwards");
    EXPECT_EQ(Timing::FillModeForwards, timing.fillMode);
    timing.fillMode = defaultFillMode;

    applyTimingInputString(timing, isolate, "fill", "none");
    EXPECT_EQ(Timing::FillModeNone, timing.fillMode);
    timing.fillMode = defaultFillMode;

    applyTimingInputString(timing, isolate, "fill", "backwards");
    EXPECT_EQ(Timing::FillModeBackwards, timing.fillMode);
    timing.fillMode = defaultFillMode;

    applyTimingInputString(timing, isolate, "fill", "both");
    EXPECT_EQ(Timing::FillModeBoth, timing.fillMode);
    timing.fillMode = defaultFillMode;

    applyTimingInputString(timing, isolate, "fill", "everything!");
    EXPECT_EQ(defaultFillMode, timing.fillMode);
    timing.fillMode = defaultFillMode;

    applyTimingInputString(timing, isolate, "fill", "backwardsandforwards");
    EXPECT_EQ(defaultFillMode, timing.fillMode);
    timing.fillMode = defaultFillMode;

    applyTimingInputNumber(timing, isolate, "fill", 2);
    EXPECT_EQ(defaultFillMode, timing.fillMode);
    timing.fillMode = defaultFillMode;
}

TEST_F(AnimationElementAnimationTest, TimingInputIterationStart)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Timing timing;
    EXPECT_EQ(0, timing.iterationStart);

    applyTimingInputNumber(timing, isolate, "iterationStart", 1.1);
    EXPECT_EQ(1.1, timing.iterationStart);
    timing.iterationStart = 0;

    applyTimingInputNumber(timing, isolate, "iterationStart", -1);
    EXPECT_EQ(0, timing.iterationStart);
    timing.iterationStart = 0;

    applyTimingInputString(timing, isolate, "iterationStart", "Infinity");
    EXPECT_EQ(0, timing.iterationStart);
    timing.iterationStart = 0;

    applyTimingInputString(timing, isolate, "iterationStart", "-Infinity");
    EXPECT_EQ(0, timing.iterationStart);
    timing.iterationStart = 0;

    applyTimingInputString(timing, isolate, "iterationStart", "NaN");
    EXPECT_EQ(0, timing.iterationStart);
    timing.iterationStart = 0;

    applyTimingInputString(timing, isolate, "iterationStart", "rubbish");
    EXPECT_EQ(0, timing.iterationStart);
    timing.iterationStart = 0;
}

TEST_F(AnimationElementAnimationTest, TimingInputIterationCount)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Timing timing;
    EXPECT_EQ(1, timing.iterationCount);

    applyTimingInputNumber(timing, isolate, "iterations", 2.1);
    EXPECT_EQ(2.1, timing.iterationCount);
    timing.iterationCount = 1;

    applyTimingInputNumber(timing, isolate, "iterations", -1);
    EXPECT_EQ(0, timing.iterationCount);
    timing.iterationCount = 1;

    applyTimingInputString(timing, isolate, "iterations", "Infinity");
    EXPECT_TRUE(std::isinf(timing.iterationCount) && (timing.iterationCount > 0));
    timing.iterationCount = 1;

    applyTimingInputString(timing, isolate, "iterations", "-Infinity");
    EXPECT_EQ(0, timing.iterationCount);
    timing.iterationCount = 1;

    applyTimingInputString(timing, isolate, "iterations", "NaN");
    EXPECT_EQ(1, timing.iterationCount);
    timing.iterationCount = 1;

    applyTimingInputString(timing, isolate, "iterations", "rubbish");
    EXPECT_EQ(1, timing.iterationCount);
    timing.iterationCount = 1;
}

TEST_F(AnimationElementAnimationTest, TimingInputIterationDuration)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Timing timing;
    EXPECT_EQ(0, timing.iterationDuration);
    EXPECT_FALSE(timing.hasIterationDuration);

    applyTimingInputNumber(timing, isolate, "duration", 1.1);
    EXPECT_EQ(1.1, timing.iterationDuration);
    EXPECT_TRUE(timing.hasIterationDuration);
    timing.hasIterationDuration = false;
    timing.iterationDuration = 0;

    applyTimingInputNumber(timing, isolate, "duration", -1);
    EXPECT_EQ(0, timing.iterationDuration);
    EXPECT_FALSE(timing.hasIterationDuration);
    timing.hasIterationDuration = false;
    timing.iterationDuration = 0;

    applyTimingInputString(timing, isolate, "duration", "1");
    EXPECT_EQ(1, timing.iterationDuration);
    EXPECT_TRUE(timing.hasIterationDuration);
    timing.hasIterationDuration = false;
    timing.iterationDuration = 0;

    applyTimingInputString(timing, isolate, "duration", "Infinity");
    EXPECT_TRUE(std::isinf(timing.iterationDuration) && (timing.iterationDuration > 0));
    EXPECT_TRUE(timing.hasIterationDuration);
    timing.hasIterationDuration = false;
    timing.iterationDuration = 0;

    applyTimingInputString(timing, isolate, "duration", "-Infinity");
    EXPECT_EQ(0, timing.iterationDuration);
    EXPECT_FALSE(timing.hasIterationDuration);
    timing.hasIterationDuration = false;
    timing.iterationDuration = 0;

    applyTimingInputString(timing, isolate, "duration", "NaN");
    EXPECT_EQ(0, timing.iterationDuration);
    EXPECT_FALSE(timing.hasIterationDuration);
    timing.hasIterationDuration = false;
    timing.iterationDuration = 0;

    applyTimingInputString(timing, isolate, "duration", "auto");
    EXPECT_EQ(0, timing.iterationDuration);
    EXPECT_FALSE(timing.hasIterationDuration);
    timing.hasIterationDuration = false;
    timing.iterationDuration = 0;

    applyTimingInputString(timing, isolate, "duration", "rubbish");
    EXPECT_EQ(0, timing.iterationDuration);
    EXPECT_FALSE(timing.hasIterationDuration);
    timing.hasIterationDuration = false;
    timing.iterationDuration = 0;
}

TEST_F(AnimationElementAnimationTest, TimingInputPlaybackRate)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Timing timing;
    EXPECT_EQ(1, timing.playbackRate);

    applyTimingInputNumber(timing, isolate, "playbackRate", 2.1);
    EXPECT_EQ(2.1, timing.playbackRate);
    timing.playbackRate = 1;

    applyTimingInputNumber(timing, isolate, "playbackRate", -1);
    EXPECT_EQ(-1, timing.playbackRate);
    timing.playbackRate = 1;

    applyTimingInputString(timing, isolate, "playbackRate", "Infinity");
    EXPECT_EQ(1, timing.playbackRate);
    timing.playbackRate = 1;

    applyTimingInputString(timing, isolate, "playbackRate", "-Infinity");
    EXPECT_EQ(1, timing.playbackRate);
    timing.playbackRate = 1;

    applyTimingInputString(timing, isolate, "playbackRate", "NaN");
    EXPECT_EQ(1, timing.playbackRate);
    timing.playbackRate = 1;

    applyTimingInputString(timing, isolate, "playbackRate", "rubbish");
    EXPECT_EQ(1, timing.playbackRate);
    timing.playbackRate = 1;
}

TEST_F(AnimationElementAnimationTest, TimingInputDirection)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Timing timing;
    Timing::PlaybackDirection defaultPlaybackDirection = Timing::PlaybackDirectionNormal;
    EXPECT_EQ(defaultPlaybackDirection, timing.direction);

    applyTimingInputString(timing, isolate, "direction", "normal");
    EXPECT_EQ(Timing::PlaybackDirectionNormal, timing.direction);
    timing.direction = defaultPlaybackDirection;

    applyTimingInputString(timing, isolate, "direction", "reverse");
    EXPECT_EQ(Timing::PlaybackDirectionReverse, timing.direction);
    timing.direction = defaultPlaybackDirection;

    applyTimingInputString(timing, isolate, "direction", "alternate");
    EXPECT_EQ(Timing::PlaybackDirectionAlternate, timing.direction);
    timing.direction = defaultPlaybackDirection;

    applyTimingInputString(timing, isolate, "direction", "alternate-reverse");
    EXPECT_EQ(Timing::PlaybackDirectionAlternateReverse, timing.direction);
    timing.direction = defaultPlaybackDirection;

    applyTimingInputString(timing, isolate, "direction", "rubbish");
    EXPECT_EQ(defaultPlaybackDirection, timing.direction);
    timing.direction = defaultPlaybackDirection;

    applyTimingInputNumber(timing, isolate, "direction", 2);
    EXPECT_EQ(defaultPlaybackDirection, timing.direction);
    timing.direction = defaultPlaybackDirection;
}

TEST_F(AnimationElementAnimationTest, TimingInputEmpty)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Timing updatedTiming;
    Timing controlTiming;

    v8::Handle<v8::Object> timingInput = v8::Object::New(isolate);
    Dictionary timingInputDictionary = Dictionary(v8::Handle<v8::Value>::Cast(timingInput), isolate);
    populateTiming(updatedTiming, timingInputDictionary);

    EXPECT_EQ(controlTiming.startDelay, updatedTiming.startDelay);
    EXPECT_EQ(controlTiming.fillMode, updatedTiming.fillMode);
    EXPECT_EQ(controlTiming.iterationStart, updatedTiming.iterationStart);
    EXPECT_EQ(controlTiming.iterationCount, updatedTiming.iterationCount);
    EXPECT_EQ(controlTiming.iterationDuration, updatedTiming.iterationDuration);
    EXPECT_EQ(controlTiming.hasIterationDuration, updatedTiming.hasIterationDuration);
    EXPECT_EQ(controlTiming.playbackRate, updatedTiming.playbackRate);
    EXPECT_EQ(controlTiming.direction, updatedTiming.direction);
}

} // namespace WebCore
