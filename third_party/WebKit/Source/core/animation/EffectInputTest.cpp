// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/EffectInput.h"

#include <memory>

#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8ObjectBuilder.h"
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
  ScriptState* script_state = scope.GetScriptState();

  Vector<ScriptValue> blink_keyframes = {V8ObjectBuilder(script_state)
                                             .AddString("width", "100px")
                                             .AddString("offset", "0")
                                             .GetScriptValue(),
                                         V8ObjectBuilder(script_state)
                                             .AddString("width", "0px")
                                             .AddString("offset", "1")
                                             .GetScriptValue()};

  ScriptValue js_keyframes(
      script_state,
      ToV8(blink_keyframes, scope.GetContext()->Global(), scope.GetIsolate()));

  Element* element = AppendElement(scope.GetDocument());
  KeyframeEffectModelBase* effect = EffectInput::Convert(
      element, js_keyframes, EffectModel::kCompositeReplace,
      scope.GetScriptState(), scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(1.0, effect->GetFrames()[1]->CheckedOffset());
}

TEST(AnimationEffectInputTest, UnsortedOffsets) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  Vector<ScriptValue> blink_keyframes = {V8ObjectBuilder(script_state)
                                             .AddString("width", "0px")
                                             .AddString("offset", "1")
                                             .GetScriptValue(),
                                         V8ObjectBuilder(script_state)
                                             .AddString("width", "100px")
                                             .AddString("offset", "0")
                                             .GetScriptValue()};

  ScriptValue js_keyframes(
      script_state,
      ToV8(blink_keyframes, scope.GetContext()->Global(), scope.GetIsolate()));

  Element* element = AppendElement(scope.GetDocument());
  EffectInput::Convert(element, js_keyframes, EffectModel::kCompositeReplace,
                       scope.GetScriptState(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(kV8TypeError, scope.GetExceptionState().Code());
}

TEST(AnimationEffectInputTest, LooslySorted) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  Vector<ScriptValue> blink_keyframes = {V8ObjectBuilder(script_state)
                                             .AddString("width", "100px")
                                             .AddString("offset", "0")
                                             .GetScriptValue(),
                                         V8ObjectBuilder(script_state)
                                             .AddString("width", "200px")
                                             .GetScriptValue(),
                                         V8ObjectBuilder(script_state)
                                             .AddString("width", "0px")
                                             .AddString("offset", "1")
                                             .GetScriptValue()};

  ScriptValue js_keyframes(
      script_state,
      ToV8(blink_keyframes, scope.GetContext()->Global(), scope.GetIsolate()));

  Element* element = AppendElement(scope.GetDocument());
  KeyframeEffectModelBase* effect = EffectInput::Convert(
      element, js_keyframes, EffectModel::kCompositeReplace,
      scope.GetScriptState(), scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(1, effect->GetFrames()[2]->CheckedOffset());
}

TEST(AnimationEffectInputTest, OutOfOrderWithNullOffsets) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  Vector<ScriptValue> blink_keyframes = {V8ObjectBuilder(script_state)
                                             .AddString("height", "100px")
                                             .AddString("offset", "0.5")
                                             .GetScriptValue(),
                                         V8ObjectBuilder(script_state)
                                             .AddString("height", "150px")
                                             .GetScriptValue(),
                                         V8ObjectBuilder(script_state)
                                             .AddString("height", "200px")
                                             .AddString("offset", "0")
                                             .GetScriptValue(),
                                         V8ObjectBuilder(script_state)
                                             .AddString("height", "300px")
                                             .AddString("offset", "1")
                                             .GetScriptValue()};

  ScriptValue js_keyframes(
      script_state,
      ToV8(blink_keyframes, scope.GetContext()->Global(), scope.GetIsolate()));

  Element* element = AppendElement(scope.GetDocument());
  EffectInput::Convert(element, js_keyframes, EffectModel::kCompositeReplace,
                       scope.GetScriptState(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

TEST(AnimationEffectInputTest, Invalid) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  // Not loosely sorted by offset, and there exists a keyframe with null offset.
  Vector<ScriptValue> blink_keyframes = {V8ObjectBuilder(script_state)
                                             .AddString("width", "0px")
                                             .AddString("offset", "1")
                                             .GetScriptValue(),
                                         V8ObjectBuilder(script_state)
                                             .AddString("width", "200px")
                                             .GetScriptValue(),
                                         V8ObjectBuilder(script_state)
                                             .AddString("width", "200px")
                                             .AddString("offset", "0")
                                             .GetScriptValue()};

  ScriptValue js_keyframes(
      script_state,
      ToV8(blink_keyframes, scope.GetContext()->Global(), scope.GetIsolate()));

  Element* element = AppendElement(scope.GetDocument());
  EffectInput::Convert(element, js_keyframes, EffectModel::kCompositeReplace,
                       scope.GetScriptState(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(kV8TypeError, scope.GetExceptionState().Code());
}

}  // namespace blink
