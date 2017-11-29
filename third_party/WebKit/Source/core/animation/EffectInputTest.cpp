// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/EffectInput.h"

#include <memory>

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/dictionary_sequence_or_dictionary.h"
#include "core/animation/AnimationTestHelper.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ExceptionCode.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {

Element* AppendElement(Document& document) {
  Element* element = document.createElement("foo");
  document.documentElement()->AppendChild(element);
  return element;
}

TEST(AnimationEffectInputTest, SortedOffsets) {
  V8TestingScope scope;
  Vector<Dictionary> js_keyframes;
  v8::Local<v8::Object> keyframe1 = v8::Object::New(scope.GetIsolate());
  v8::Local<v8::Object> keyframe2 = v8::Object::New(scope.GetIsolate());

  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe1, "width", "100px");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe1, "offset", "0");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe2, "width", "0px");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe2, "offset", "1");

  js_keyframes.push_back(
      Dictionary(scope.GetIsolate(), keyframe1, scope.GetExceptionState()));
  js_keyframes.push_back(
      Dictionary(scope.GetIsolate(), keyframe2, scope.GetExceptionState()));

  Element* element = AppendElement(scope.GetDocument());
  KeyframeEffectModelBase* effect = EffectInput::Convert(
      element,
      DictionarySequenceOrDictionary::FromDictionarySequence(js_keyframes),
      EffectModel::kCompositeReplace, nullptr, scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(1.0, effect->GetFrames()[1]->Offset());
}

TEST(AnimationEffectInputTest, UnsortedOffsets) {
  V8TestingScope scope;
  Vector<Dictionary> js_keyframes;
  v8::Local<v8::Object> keyframe1 = v8::Object::New(scope.GetIsolate());
  v8::Local<v8::Object> keyframe2 = v8::Object::New(scope.GetIsolate());

  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe1, "width", "0px");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe1, "offset", "1");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe2, "width", "100px");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe2, "offset", "0");

  js_keyframes.push_back(
      Dictionary(scope.GetIsolate(), keyframe1, scope.GetExceptionState()));
  js_keyframes.push_back(
      Dictionary(scope.GetIsolate(), keyframe2, scope.GetExceptionState()));

  Element* element = AppendElement(scope.GetDocument());
  EffectInput::Convert(
      element,
      DictionarySequenceOrDictionary::FromDictionarySequence(js_keyframes),
      EffectModel::kCompositeReplace, nullptr, scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(kV8TypeError, scope.GetExceptionState().Code());
}

TEST(AnimationEffectInputTest, LooslySorted) {
  V8TestingScope scope;
  Vector<Dictionary> js_keyframes;
  v8::Local<v8::Object> keyframe1 = v8::Object::New(scope.GetIsolate());
  v8::Local<v8::Object> keyframe2 = v8::Object::New(scope.GetIsolate());
  v8::Local<v8::Object> keyframe3 = v8::Object::New(scope.GetIsolate());

  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe1, "width", "100px");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe1, "offset", "0");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe2, "width", "200px");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe3, "width", "0px");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe3, "offset", "1");

  js_keyframes.push_back(
      Dictionary(scope.GetIsolate(), keyframe1, scope.GetExceptionState()));
  js_keyframes.push_back(
      Dictionary(scope.GetIsolate(), keyframe2, scope.GetExceptionState()));
  js_keyframes.push_back(
      Dictionary(scope.GetIsolate(), keyframe3, scope.GetExceptionState()));

  Element* element = AppendElement(scope.GetDocument());
  KeyframeEffectModelBase* effect = EffectInput::Convert(
      element,
      DictionarySequenceOrDictionary::FromDictionarySequence(js_keyframes),
      EffectModel::kCompositeReplace, nullptr, scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(1, effect->GetFrames()[2]->Offset());
}

TEST(AnimationEffectInputTest, OutOfOrderWithNullOffsets) {
  V8TestingScope scope;
  Vector<Dictionary> js_keyframes;
  v8::Local<v8::Object> keyframe1 = v8::Object::New(scope.GetIsolate());
  v8::Local<v8::Object> keyframe2 = v8::Object::New(scope.GetIsolate());
  v8::Local<v8::Object> keyframe3 = v8::Object::New(scope.GetIsolate());
  v8::Local<v8::Object> keyframe4 = v8::Object::New(scope.GetIsolate());

  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe1, "height", "100px");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe1, "offset", "0.5");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe2, "height", "150px");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe3, "height", "200px");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe3, "offset", "0");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe4, "height", "300px");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe4, "offset", "1");

  js_keyframes.push_back(
      Dictionary(scope.GetIsolate(), keyframe1, scope.GetExceptionState()));
  js_keyframes.push_back(
      Dictionary(scope.GetIsolate(), keyframe2, scope.GetExceptionState()));
  js_keyframes.push_back(
      Dictionary(scope.GetIsolate(), keyframe3, scope.GetExceptionState()));
  js_keyframes.push_back(
      Dictionary(scope.GetIsolate(), keyframe4, scope.GetExceptionState()));

  Element* element = AppendElement(scope.GetDocument());
  EffectInput::Convert(
      element,
      DictionarySequenceOrDictionary::FromDictionarySequence(js_keyframes),
      EffectModel::kCompositeReplace, nullptr, scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

TEST(AnimationEffectInputTest, Invalid) {
  V8TestingScope scope;
  // Not loosely sorted by offset, and there exists a keyframe with null offset.
  Vector<Dictionary> js_keyframes;
  v8::Local<v8::Object> keyframe1 = v8::Object::New(scope.GetIsolate());
  v8::Local<v8::Object> keyframe2 = v8::Object::New(scope.GetIsolate());
  v8::Local<v8::Object> keyframe3 = v8::Object::New(scope.GetIsolate());

  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe1, "width", "0px");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe1, "offset", "1");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe2, "width", "200px");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe3, "width", "100px");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe3, "offset", "0");

  js_keyframes.push_back(
      Dictionary(scope.GetIsolate(), keyframe1, scope.GetExceptionState()));
  js_keyframes.push_back(
      Dictionary(scope.GetIsolate(), keyframe2, scope.GetExceptionState()));
  js_keyframes.push_back(
      Dictionary(scope.GetIsolate(), keyframe3, scope.GetExceptionState()));

  Element* element = AppendElement(scope.GetDocument());
  EffectInput::Convert(
      element,
      DictionarySequenceOrDictionary::FromDictionarySequence(js_keyframes),
      EffectModel::kCompositeReplace, nullptr, scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(kV8TypeError, scope.GetExceptionState().Code());
}

}  // namespace blink
