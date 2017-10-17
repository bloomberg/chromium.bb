// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/Dictionary.h"

#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "platform/bindings/ScriptState.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class DictionaryTest : public ::testing::Test {
 protected:
  static Dictionary CreateDictionary(ScriptState* script_state, const char* s) {
    v8::Local<v8::String> source =
        v8::String::NewFromUtf8(script_state->GetIsolate(), s,
                                v8::NewStringType::kNormal)
            .ToLocalChecked();
    v8::Local<v8::Script> script =
        v8::Script::Compile(script_state->GetContext(), source)
            .ToLocalChecked();
    v8::Local<v8::Value> value =
        script->Run(script_state->GetContext()).ToLocalChecked();
    DCHECK(!value.IsEmpty());
    DCHECK(value->IsObject());
    NonThrowableExceptionState exception_state;
    Dictionary dictionary(script_state->GetIsolate(), value, exception_state);
    return dictionary;
  }
};

TEST_F(DictionaryTest, Get_Empty) {
  V8TestingScope scope;
  Dictionary dictionary = CreateDictionary(scope.GetScriptState(), "({})");

  auto r = dictionary.Get<IDLByteString>("key", scope.GetExceptionState());

  ASSERT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_FALSE(r.has_value());
}

TEST_F(DictionaryTest, Get_NonPresentForNonEmpty) {
  V8TestingScope scope;
  Dictionary dictionary =
      CreateDictionary(scope.GetScriptState(), "({foo: 3})");

  auto r = dictionary.Get<IDLByteString>("key", scope.GetExceptionState());

  ASSERT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_FALSE(r.has_value());
}

TEST_F(DictionaryTest, Get_UndefinedValue) {
  V8TestingScope scope;
  Dictionary dictionary =
      CreateDictionary(scope.GetScriptState(), "({foo: undefined})");

  auto r = dictionary.Get<IDLByteString>("foo", scope.GetExceptionState());

  ASSERT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_FALSE(r.has_value());
}

TEST_F(DictionaryTest, Get_Found) {
  V8TestingScope scope;
  Dictionary dictionary =
      CreateDictionary(scope.GetScriptState(), "({foo: 3})");

  auto r = dictionary.Get<IDLByteString>("foo", scope.GetExceptionState());

  ASSERT_FALSE(scope.GetExceptionState().HadException());
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, "3");
}

TEST_F(DictionaryTest, Get_Found2) {
  V8TestingScope scope;
  Dictionary dictionary =
      CreateDictionary(scope.GetScriptState(), "({foo: '3'})");

  auto r = dictionary.Get<IDLLong>("foo", scope.GetExceptionState());

  ASSERT_FALSE(scope.GetExceptionState().HadException());
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, 3);
}

TEST_F(DictionaryTest, Get_Getter) {
  V8TestingScope scope;
  Dictionary dictionary = CreateDictionary(scope.GetScriptState(),
                                           "({get foo() { return 'xy'; }})");

  auto r = dictionary.Get<IDLByteString>("foo", scope.GetExceptionState());

  ASSERT_FALSE(scope.GetExceptionState().HadException());
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, "xy");
}

TEST_F(DictionaryTest, Get_ExceptionOnAccess) {
  V8TestingScope scope;
  Dictionary dictionary = CreateDictionary(scope.GetScriptState(),
                                           "({get foo() { throw Error(2); }})");

  auto r = dictionary.Get<IDLByteString>("foo", scope.GetExceptionState());

  ASSERT_TRUE(scope.GetExceptionState().HadException());
  ASSERT_FALSE(r.has_value());
}

TEST_F(DictionaryTest, Get_TypeConversion) {
  V8TestingScope scope;
  Dictionary dictionary = CreateDictionary(
      scope.GetScriptState(), "({foo: { toString() { return 'hello'; } } })");

  auto r = dictionary.Get<IDLByteString>("foo", scope.GetExceptionState());

  ASSERT_FALSE(scope.GetExceptionState().HadException());
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, "hello");
}

TEST_F(DictionaryTest, Get_ConversionError) {
  V8TestingScope scope;
  Dictionary dictionary = CreateDictionary(
      scope.GetScriptState(),
      "({get foo() { return { toString() { throw Error(88); } };} })");

  auto r = dictionary.Get<IDLByteString>("foo", scope.GetExceptionState());

  ASSERT_TRUE(scope.GetExceptionState().HadException());
  ASSERT_FALSE(r.has_value());
}

TEST_F(DictionaryTest, Get_ConversionError2) {
  V8TestingScope scope;
  Dictionary dictionary =
      CreateDictionary(scope.GetScriptState(), "({foo: NaN})");

  auto r = dictionary.Get<IDLDouble>("foo", scope.GetExceptionState());

  ASSERT_TRUE(scope.GetExceptionState().HadException());
  ASSERT_FALSE(r.has_value());
}

}  // namespace

}  // namespace blink
