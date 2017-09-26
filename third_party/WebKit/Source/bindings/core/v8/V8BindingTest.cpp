// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8BindingForCore.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "platform/wtf/Vector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(V8BindingTest, ToImplArray) {
  V8TestingScope scope;
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
    Vector<ScriptValue> script_value_vector =
        NativeValueTraits<IDLSequence<ScriptValue>>::NativeValue(
            scope.GetIsolate(), v8_array, exception_state);
    Vector<String> string_vector = ToImplArray<Vector<String>>(
        script_value_vector, scope.GetIsolate(), exception_state);
    EXPECT_EQ(3U, string_vector.size());
    EXPECT_EQ("Vini, vidi, vici.", string_vector[0]);
    EXPECT_EQ("65535", string_vector[1]);
  }
}

}  // namespace

}  // namespace blink
