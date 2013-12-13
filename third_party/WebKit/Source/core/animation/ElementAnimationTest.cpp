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

#include "core/animation/AnimatableLength.h"
#include "core/animation/Animation.h"
#include "core/animation/AnimationClock.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/KeyframeAnimationEffect.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"

#include <gtest/gtest.h>

namespace WebCore {

namespace {

v8::Handle<v8::Value> stringToV8Value(String string)
{
    return v8::Handle<v8::Value>::Cast(v8String(v8::Isolate::GetCurrent(), string));
}

void setV8ObjectProperty(v8::Handle<v8::Object> object, String name, String value)
{
    object->Set(stringToV8Value(name), stringToV8Value(value));
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

    void startAnimation(Element* element, Vector<Dictionary> keyframesDictionaryVector)
    {
        ElementAnimation::startAnimation(element, keyframesDictionaryVector);
    }

    void startAnimationWithSpecifiedDuration(Element* element, Vector<Dictionary> keyframesDictionaryVector, double duration)
    {
        ElementAnimation::startAnimation(element, keyframesDictionaryVector, duration);
    }
};

TEST_F(AnimationElementAnimationTest, CanStartAnAnimation)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Vector<Dictionary> jsKeyframes;
    v8::Handle<v8::Object> keyframe1 = v8::Object::New();
    v8::Handle<v8::Object> keyframe2 = v8::Object::New();

    setV8ObjectProperty(keyframe1, "width", "100px");
    setV8ObjectProperty(keyframe1, "offset", "0");
    setV8ObjectProperty(keyframe2, "width", "0px");
    setV8ObjectProperty(keyframe2, "offset", "1");

    jsKeyframes.append(Dictionary(keyframe1, isolate));
    jsKeyframes.append(Dictionary(keyframe2, isolate));

    String value1;
    ASSERT_TRUE(jsKeyframes[0].get("width", value1));
    ASSERT_EQ("100px", value1);

    String value2;
    ASSERT_TRUE(jsKeyframes[1].get("width", value2));
    ASSERT_EQ("0px", value2);

    startAnimationWithSpecifiedDuration(element.get(), jsKeyframes, 0);

    Player* player = document->timeline()->players().at(0).get();

    Animation* animation = toAnimation(player->source());

    Element* target = animation->target();
    EXPECT_EQ(*element.get(), *target);

    const KeyframeAnimationEffect::KeyframeVector keyframes =
        toKeyframeAnimationEffect(animation->effect())->getFrames();

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
    EXPECT_EQ(CSSPropertyInvalid, ElementAnimation::camelCaseCSSPropertyNameToID(String("line-height").impl()));
    EXPECT_EQ(CSSPropertyLineHeight, ElementAnimation::camelCaseCSSPropertyNameToID(String("lineHeight").impl()));
    EXPECT_EQ(CSSPropertyBorderTopWidth, ElementAnimation::camelCaseCSSPropertyNameToID(String("borderTopWidth").impl()));
    EXPECT_EQ(CSSPropertyWidth, ElementAnimation::camelCaseCSSPropertyNameToID(String("width").impl()));
    EXPECT_EQ(CSSPropertyInvalid, ElementAnimation::camelCaseCSSPropertyNameToID(String("Width").impl()));
    EXPECT_EQ(CSSPropertyInvalid, ElementAnimation::camelCaseCSSPropertyNameToID(String("-webkit-transform").impl()));
    EXPECT_EQ(CSSPropertyInvalid, ElementAnimation::camelCaseCSSPropertyNameToID(String("webkitTransform").impl()));
    EXPECT_EQ(CSSPropertyInvalid, ElementAnimation::camelCaseCSSPropertyNameToID(String("cssFloat").impl()));
}

TEST_F(AnimationElementAnimationTest, CanSetDuration)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Vector<Dictionary, 0> jsKeyframes;
    double duration = 2;

    startAnimationWithSpecifiedDuration(element.get(), jsKeyframes, duration);

    Player* player = document->timeline()->players().at(0).get();

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

    startAnimation(element.get(), jsKeyframes);

    Player* player = document->timeline()->players().at(0).get();

    // FIXME: This is correct for the moment, as using c++ default arguments means
    // there is no way to tell whether a duration has been specified by the user.
    // Once we implment timing object arguments we should be able to tell, and this
    // check should be changed to EXPECT_FALSE.
    EXPECT_TRUE(player->source()->specified().hasIterationDuration);
    EXPECT_EQ(0, player->source()->specified().iterationDuration);
}

TEST_F(AnimationElementAnimationTest, ClipNegativeDurationToZero)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Vector<Dictionary, 0> jsKeyframes;
    double duration = -2;

    startAnimationWithSpecifiedDuration(element.get(), jsKeyframes, duration);

    Player* player = document->timeline()->players().at(0).get();

    EXPECT_TRUE(player->source()->specified().hasIterationDuration);
    EXPECT_EQ(0, player->source()->specified().iterationDuration);
}

} // namespace WebCore
