// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8Binding.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "platform/wtf/Vector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

template <typename T>
v8::Local<v8::Value> ToV8(V8TestingScope* scope, T value) {
  return blink::ToV8(value, scope->GetContext()->Global(), scope->GetIsolate());
}

TEST(V8BindingTest, toImplSequence) {
  V8TestingScope scope;
  {
    v8::Local<v8::Array> v8_string_array =
        v8::Array::New(scope.GetIsolate(), 2);
    v8_string_array
        ->Set(scope.GetContext(), ToV8(&scope, 0),
              ToV8(&scope, "Hello, World!"))
        .ToChecked();
    v8_string_array
        ->Set(scope.GetContext(), ToV8(&scope, 1), ToV8(&scope, "Hi, Mom!"))
        .ToChecked();

    NonThrowableExceptionState exception_state;
    Vector<String> string_vector = ToImplSequence<Vector<String>>(
        scope.GetIsolate(), v8_string_array, exception_state);
    EXPECT_EQ(2U, string_vector.size());
    EXPECT_EQ("Hello, World!", string_vector[0]);
    EXPECT_EQ("Hi, Mom!", string_vector[1]);
  }
}

TEST(V8BindingTest, toImplArray) {
  V8TestingScope scope;
  {
    v8::Local<v8::Array> v8_string_array =
        v8::Array::New(scope.GetIsolate(), 2);
    EXPECT_TRUE(V8CallBoolean(v8_string_array->Set(
        scope.GetContext(), ToV8(&scope, 0), ToV8(&scope, "Hello, World!"))));
    EXPECT_TRUE(V8CallBoolean(v8_string_array->Set(
        scope.GetContext(), ToV8(&scope, 1), ToV8(&scope, "Hi, Mom!"))));

    NonThrowableExceptionState exception_state;
    Vector<String> string_vector = ToImplArray<Vector<String>>(
        v8_string_array, 0, scope.GetIsolate(), exception_state);
    EXPECT_EQ(2U, string_vector.size());
    EXPECT_EQ("Hello, World!", string_vector[0]);
    EXPECT_EQ("Hi, Mom!", string_vector[1]);
  }
  {
    v8::Local<v8::Array> v8_unsigned_array =
        v8::Array::New(scope.GetIsolate(), 3);
    EXPECT_TRUE(V8CallBoolean(v8_unsigned_array->Set(
        scope.GetContext(), ToV8(&scope, 0), ToV8(&scope, 42))));
    EXPECT_TRUE(V8CallBoolean(v8_unsigned_array->Set(
        scope.GetContext(), ToV8(&scope, 1), ToV8(&scope, 1729))));
    EXPECT_TRUE(V8CallBoolean(v8_unsigned_array->Set(
        scope.GetContext(), ToV8(&scope, 2), ToV8(&scope, 31773))));

    NonThrowableExceptionState exception_state;
    Vector<unsigned> unsigned_vector = ToImplArray<Vector<unsigned>>(
        v8_unsigned_array, 0, scope.GetIsolate(), exception_state);
    EXPECT_EQ(3U, unsigned_vector.size());
    EXPECT_EQ(42U, unsigned_vector[0]);
    EXPECT_EQ(1729U, unsigned_vector[1]);
    EXPECT_EQ(31773U, unsigned_vector[2]);
  }
  {
    const double kDoublePi = 3.141592653589793238;
    const float kFloatPi = kDoublePi;
    v8::Local<v8::Array> v8_real_array = v8::Array::New(scope.GetIsolate(), 1);
    EXPECT_TRUE(V8CallBoolean(v8_real_array->Set(
        scope.GetContext(), ToV8(&scope, 0), ToV8(&scope, kDoublePi))));

    NonThrowableExceptionState exception_state;
    Vector<double> double_vector = ToImplArray<Vector<double>>(
        v8_real_array, 0, scope.GetIsolate(), exception_state);
    EXPECT_EQ(1U, double_vector.size());
    EXPECT_EQ(kDoublePi, double_vector[0]);

    Vector<float> float_vector = ToImplArray<Vector<float>>(
        v8_real_array, 0, scope.GetIsolate(), exception_state);
    EXPECT_EQ(1U, float_vector.size());
    EXPECT_EQ(kFloatPi, float_vector[0]);
  }
  {
    v8::Local<v8::Array> v8_array = v8::Array::New(scope.GetIsolate(), 3);
    EXPECT_TRUE(
        V8CallBoolean(v8_array->Set(scope.GetContext(), ToV8(&scope, 0),
                                    ToV8(&scope, "Vini, vidi, vici."))));
    EXPECT_TRUE(V8CallBoolean(v8_array->Set(scope.GetContext(), ToV8(&scope, 1),
                                            ToV8(&scope, 65535))));
    EXPECT_TRUE(V8CallBoolean(v8_array->Set(scope.GetContext(), ToV8(&scope, 2),
                                            ToV8(&scope, 0.125))));

    NonThrowableExceptionState exception_state;
    Vector<v8::Local<v8::Value>> v8_handle_vector =
        ToImplArray<Vector<v8::Local<v8::Value>>>(
            v8_array, 0, scope.GetIsolate(), exception_state);
    EXPECT_EQ(3U, v8_handle_vector.size());
    EXPECT_EQ(
        "Vini, vidi, vici.",
        ToUSVString(scope.GetIsolate(), v8_handle_vector[0], exception_state));
    EXPECT_EQ(65535U, ToUInt32(scope.GetIsolate(), v8_handle_vector[1],
                               kNormalConversion, exception_state));

    Vector<ScriptValue> script_value_vector = ToImplArray<Vector<ScriptValue>>(
        v8_array, 0, scope.GetIsolate(), exception_state);
    EXPECT_EQ(3U, script_value_vector.size());
    String report_on_zela;
    EXPECT_TRUE(script_value_vector[0].ToString(report_on_zela));
    EXPECT_EQ("Vini, vidi, vici.", report_on_zela);
    EXPECT_EQ(65535U,
              ToUInt32(scope.GetIsolate(), script_value_vector[1].V8Value(),
                       kNormalConversion, exception_state));
  }
  {
    v8::Local<v8::Array> v8_string_array1 =
        v8::Array::New(scope.GetIsolate(), 2);
    EXPECT_TRUE(V8CallBoolean(v8_string_array1->Set(
        scope.GetContext(), ToV8(&scope, 0), ToV8(&scope, "foo"))));
    EXPECT_TRUE(V8CallBoolean(v8_string_array1->Set(
        scope.GetContext(), ToV8(&scope, 1), ToV8(&scope, "bar"))));
    v8::Local<v8::Array> v8_string_array2 =
        v8::Array::New(scope.GetIsolate(), 3);
    EXPECT_TRUE(V8CallBoolean(v8_string_array2->Set(
        scope.GetContext(), ToV8(&scope, 0), ToV8(&scope, "x"))));
    EXPECT_TRUE(V8CallBoolean(v8_string_array2->Set(
        scope.GetContext(), ToV8(&scope, 1), ToV8(&scope, "y"))));
    EXPECT_TRUE(V8CallBoolean(v8_string_array2->Set(
        scope.GetContext(), ToV8(&scope, 2), ToV8(&scope, "z"))));
    v8::Local<v8::Array> v8_string_array_array =
        v8::Array::New(scope.GetIsolate(), 2);
    EXPECT_TRUE(V8CallBoolean(v8_string_array_array->Set(
        scope.GetContext(), ToV8(&scope, 0), v8_string_array1)));
    EXPECT_TRUE(V8CallBoolean(v8_string_array_array->Set(
        scope.GetContext(), ToV8(&scope, 1), v8_string_array2)));

    NonThrowableExceptionState exception_state;
    Vector<Vector<String>> string_vector_vector =
        ToImplArray<Vector<Vector<String>>>(
            v8_string_array_array, 0, scope.GetIsolate(), exception_state);
    EXPECT_EQ(2U, string_vector_vector.size());
    EXPECT_EQ(2U, string_vector_vector[0].size());
    EXPECT_EQ("foo", string_vector_vector[0][0]);
    EXPECT_EQ("bar", string_vector_vector[0][1]);
    EXPECT_EQ(3U, string_vector_vector[1].size());
    EXPECT_EQ("x", string_vector_vector[1][0]);
    EXPECT_EQ("y", string_vector_vector[1][1]);
    EXPECT_EQ("z", string_vector_vector[1][2]);
  }
}

}  // namespace

}  // namespace blink
