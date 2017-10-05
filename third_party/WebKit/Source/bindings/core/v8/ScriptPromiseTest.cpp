/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "bindings/core/v8/ScriptPromise.h"

#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/testing/NullExecutionContext.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

typedef ScriptPromise::InternalResolver Resolver;

class FunctionForScriptPromiseTest : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state,
                                                ScriptValue* output) {
    FunctionForScriptPromiseTest* self =
        new FunctionForScriptPromiseTest(script_state, output);
    return self->BindToV8Function();
  }

 private:
  FunctionForScriptPromiseTest(ScriptState* script_state, ScriptValue* output)
      : ScriptFunction(script_state), output_(output) {}

  ScriptValue Call(ScriptValue value) override {
    DCHECK(!value.IsEmpty());
    *output_ = value;
    return value;
  }

  ScriptValue* output_;
};

class TryCatchScope {
 public:
  explicit TryCatchScope(v8::Isolate* isolate)
      : isolate_(isolate), trycatch_(isolate) {}

  ~TryCatchScope() {
    // Execute all pending microtasks
    v8::MicrotasksScope::PerformCheckpoint(isolate_);
  }

  bool HasCaught() const { return trycatch_.HasCaught(); }

 private:
  v8::Isolate* isolate_;
  v8::TryCatch trycatch_;
};

String ToString(v8::Local<v8::Context> context, const ScriptValue& value) {
  return ToCoreString(value.V8Value()->ToString(context).ToLocalChecked());
}

Vector<String> ToStringArray(v8::Isolate* isolate, const ScriptValue& value) {
  NonThrowableExceptionState exception_state;
  return NativeValueTraits<IDLSequence<IDLString>>::NativeValue(
      isolate, value.V8Value(), exception_state);
}

TEST(ScriptPromiseTest, constructFromNonPromise) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  ScriptPromise promise(scope.GetScriptState(),
                        v8::Undefined(scope.GetIsolate()));
  ASSERT_TRUE(try_catch_scope.HasCaught());
  ASSERT_TRUE(promise.IsEmpty());
}

TEST(ScriptPromiseTest, thenResolve) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  Resolver resolver(scope.GetScriptState());
  ScriptPromise promise = resolver.Promise();
  ScriptValue on_fulfilled, on_rejected;
  promise.Then(FunctionForScriptPromiseTest::CreateFunction(
                   scope.GetScriptState(), &on_fulfilled),
               FunctionForScriptPromiseTest::CreateFunction(
                   scope.GetScriptState(), &on_rejected));

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  resolver.Resolve(V8String(scope.GetIsolate(), "hello"));

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  EXPECT_EQ("hello", ToString(scope.GetContext(), on_fulfilled));
  EXPECT_TRUE(on_rejected.IsEmpty());
}

TEST(ScriptPromiseTest, resolveThen) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  Resolver resolver(scope.GetScriptState());
  ScriptPromise promise = resolver.Promise();
  ScriptValue on_fulfilled, on_rejected;
  resolver.Resolve(V8String(scope.GetIsolate(), "hello"));
  promise.Then(FunctionForScriptPromiseTest::CreateFunction(
                   scope.GetScriptState(), &on_fulfilled),
               FunctionForScriptPromiseTest::CreateFunction(
                   scope.GetScriptState(), &on_rejected));

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  EXPECT_EQ("hello", ToString(scope.GetContext(), on_fulfilled));
  EXPECT_TRUE(on_rejected.IsEmpty());
}

TEST(ScriptPromiseTest, thenReject) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  Resolver resolver(scope.GetScriptState());
  ScriptPromise promise = resolver.Promise();
  ScriptValue on_fulfilled, on_rejected;
  promise.Then(FunctionForScriptPromiseTest::CreateFunction(
                   scope.GetScriptState(), &on_fulfilled),
               FunctionForScriptPromiseTest::CreateFunction(
                   scope.GetScriptState(), &on_rejected));

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  resolver.Reject(V8String(scope.GetIsolate(), "hello"));

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_EQ("hello", ToString(scope.GetContext(), on_rejected));
}

TEST(ScriptPromiseTest, rejectThen) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  Resolver resolver(scope.GetScriptState());
  ScriptPromise promise = resolver.Promise();
  ScriptValue on_fulfilled, on_rejected;
  resolver.Reject(V8String(scope.GetIsolate(), "hello"));
  promise.Then(FunctionForScriptPromiseTest::CreateFunction(
                   scope.GetScriptState(), &on_fulfilled),
               FunctionForScriptPromiseTest::CreateFunction(
                   scope.GetScriptState(), &on_rejected));

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_EQ("hello", ToString(scope.GetContext(), on_rejected));
}

TEST(ScriptPromiseTest, castPromise) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  ScriptPromise promise = Resolver(scope.GetScriptState()).Promise();
  ScriptPromise new_promise =
      ScriptPromise::Cast(scope.GetScriptState(), promise.V8Value());

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_EQ(promise.V8Value(), new_promise.V8Value());
}

TEST(ScriptPromiseTest, castNonPromise) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  ScriptValue on_fulfilled1, on_fulfilled2, on_rejected1, on_rejected2;

  ScriptValue value = ScriptValue(scope.GetScriptState(),
                                  V8String(scope.GetIsolate(), "hello"));
  ScriptPromise promise1 =
      ScriptPromise::Cast(scope.GetScriptState(), ScriptValue(value));
  ScriptPromise promise2 =
      ScriptPromise::Cast(scope.GetScriptState(), ScriptValue(value));
  promise1.Then(FunctionForScriptPromiseTest::CreateFunction(
                    scope.GetScriptState(), &on_fulfilled1),
                FunctionForScriptPromiseTest::CreateFunction(
                    scope.GetScriptState(), &on_rejected1));
  promise2.Then(FunctionForScriptPromiseTest::CreateFunction(
                    scope.GetScriptState(), &on_fulfilled2),
                FunctionForScriptPromiseTest::CreateFunction(
                    scope.GetScriptState(), &on_rejected2));

  ASSERT_FALSE(promise1.IsEmpty());
  ASSERT_FALSE(promise2.IsEmpty());
  EXPECT_NE(promise1.V8Value(), promise2.V8Value());

  ASSERT_TRUE(promise1.V8Value()->IsPromise());
  ASSERT_TRUE(promise2.V8Value()->IsPromise());

  EXPECT_TRUE(on_fulfilled1.IsEmpty());
  EXPECT_TRUE(on_fulfilled2.IsEmpty());
  EXPECT_TRUE(on_rejected1.IsEmpty());
  EXPECT_TRUE(on_rejected2.IsEmpty());

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  EXPECT_EQ("hello", ToString(scope.GetContext(), on_fulfilled1));
  EXPECT_EQ("hello", ToString(scope.GetContext(), on_fulfilled2));
  EXPECT_TRUE(on_rejected1.IsEmpty());
  EXPECT_TRUE(on_rejected2.IsEmpty());
}

TEST(ScriptPromiseTest, reject) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  ScriptValue on_fulfilled, on_rejected;

  ScriptValue value = ScriptValue(scope.GetScriptState(),
                                  V8String(scope.GetIsolate(), "hello"));
  ScriptPromise promise =
      ScriptPromise::Reject(scope.GetScriptState(), ScriptValue(value));
  promise.Then(FunctionForScriptPromiseTest::CreateFunction(
                   scope.GetScriptState(), &on_fulfilled),
               FunctionForScriptPromiseTest::CreateFunction(
                   scope.GetScriptState(), &on_rejected));

  ASSERT_FALSE(promise.IsEmpty());
  ASSERT_TRUE(promise.V8Value()->IsPromise());

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_EQ("hello", ToString(scope.GetContext(), on_rejected));
}

TEST(ScriptPromiseTest, rejectWithExceptionState) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  ScriptValue on_fulfilled, on_rejected;
  ScriptPromise promise = ScriptPromise::RejectWithDOMException(
      scope.GetScriptState(),
      DOMException::Create(kSyntaxError, "some syntax error"));
  promise.Then(FunctionForScriptPromiseTest::CreateFunction(
                   scope.GetScriptState(), &on_fulfilled),
               FunctionForScriptPromiseTest::CreateFunction(
                   scope.GetScriptState(), &on_rejected));

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_EQ("SyntaxError: some syntax error",
            ToString(scope.GetContext(), on_rejected));
}

TEST(ScriptPromiseTest, allWithEmptyPromises) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  ScriptValue on_fulfilled, on_rejected;

  ScriptPromise promise =
      ScriptPromise::All(scope.GetScriptState(), Vector<ScriptPromise>());
  ASSERT_FALSE(promise.IsEmpty());

  promise.Then(FunctionForScriptPromiseTest::CreateFunction(
                   scope.GetScriptState(), &on_fulfilled),
               FunctionForScriptPromiseTest::CreateFunction(
                   scope.GetScriptState(), &on_rejected));

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  EXPECT_FALSE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(ToStringArray(scope.GetIsolate(), on_fulfilled).IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());
}

TEST(ScriptPromiseTest, allWithResolvedPromises) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  ScriptValue on_fulfilled, on_rejected;

  Vector<ScriptPromise> promises;
  promises.push_back(ScriptPromise::Cast(
      scope.GetScriptState(), V8String(scope.GetIsolate(), "hello")));
  promises.push_back(ScriptPromise::Cast(
      scope.GetScriptState(), V8String(scope.GetIsolate(), "world")));

  ScriptPromise promise = ScriptPromise::All(scope.GetScriptState(), promises);
  ASSERT_FALSE(promise.IsEmpty());
  promise.Then(FunctionForScriptPromiseTest::CreateFunction(
                   scope.GetScriptState(), &on_fulfilled),
               FunctionForScriptPromiseTest::CreateFunction(
                   scope.GetScriptState(), &on_rejected));

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  EXPECT_FALSE(on_fulfilled.IsEmpty());
  Vector<String> values = ToStringArray(scope.GetIsolate(), on_fulfilled);
  EXPECT_EQ(2u, values.size());
  EXPECT_EQ("hello", values[0]);
  EXPECT_EQ("world", values[1]);
  EXPECT_TRUE(on_rejected.IsEmpty());
}

TEST(ScriptPromiseTest, allWithRejectedPromise) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  ScriptValue on_fulfilled, on_rejected;

  Vector<ScriptPromise> promises;
  promises.push_back(ScriptPromise::Cast(
      scope.GetScriptState(), V8String(scope.GetIsolate(), "hello")));
  promises.push_back(ScriptPromise::Reject(
      scope.GetScriptState(), V8String(scope.GetIsolate(), "world")));

  ScriptPromise promise = ScriptPromise::All(scope.GetScriptState(), promises);
  ASSERT_FALSE(promise.IsEmpty());
  promise.Then(FunctionForScriptPromiseTest::CreateFunction(
                   scope.GetScriptState(), &on_fulfilled),
               FunctionForScriptPromiseTest::CreateFunction(
                   scope.GetScriptState(), &on_rejected));

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_FALSE(on_rejected.IsEmpty());
  EXPECT_EQ("world", ToString(scope.GetContext(), on_rejected));
}

}  // namespace

}  // namespace blink
