// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/stream_promise_resolver.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/streams/stream_script_function.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

namespace {

TEST(StreamPromiseResolverTest, Construct) {
  V8TestingScope scope;
  auto* promise =
      MakeGarbageCollected<StreamPromiseResolver>(scope.GetScriptState());
  EXPECT_TRUE(promise->V8Promise(scope.GetIsolate())->IsPromise());
}

TEST(StreamPromiseResolverTest, Resolve) {
  V8TestingScope scope;
  auto* isolate = scope.GetIsolate();
  auto* promise =
      MakeGarbageCollected<StreamPromiseResolver>(scope.GetScriptState());
  promise->Resolve(scope.GetScriptState(), v8::Null(isolate));
  ASSERT_EQ(promise->State(isolate), v8::Promise::kFulfilled);
  EXPECT_TRUE(promise->V8Promise(isolate)->Result()->IsNull());
}

TEST(StreamPromiseResolverTest, ResolveWithUndefined) {
  V8TestingScope scope;
  auto* isolate = scope.GetIsolate();
  auto* promise =
      MakeGarbageCollected<StreamPromiseResolver>(scope.GetScriptState());
  promise->ResolveWithUndefined(scope.GetScriptState());
  ASSERT_EQ(promise->State(isolate), v8::Promise::kFulfilled);
  EXPECT_TRUE(promise->V8Promise(isolate)->Result()->IsUndefined());
}

TEST(StreamPromiseResolverTest, Reject) {
  V8TestingScope scope;
  auto* isolate = scope.GetIsolate();
  auto* promise =
      MakeGarbageCollected<StreamPromiseResolver>(scope.GetScriptState());
  promise->Reject(scope.GetScriptState(), v8::Number::New(isolate, 2));
  ASSERT_EQ(promise->State(isolate), v8::Promise::kRejected);
  auto result = promise->V8Promise(isolate)->Result();
  ASSERT_TRUE(result->IsNumber());
  EXPECT_EQ(result.As<v8::Number>()->Value(), 2.0);
}

TEST(StreamPromiseResolverTest, RejectDoesNothingAfterResolve) {
  V8TestingScope scope;
  auto* isolate = scope.GetIsolate();
  auto* promise =
      MakeGarbageCollected<StreamPromiseResolver>(scope.GetScriptState());
  promise->Resolve(scope.GetScriptState(), v8::Undefined(isolate));
  promise->Reject(scope.GetScriptState(), v8::Null(isolate));
  ASSERT_EQ(promise->State(isolate), v8::Promise::kFulfilled);
  EXPECT_TRUE(promise->V8Promise(isolate)->Result()->IsUndefined());
}

TEST(StreamPromiseResolverTest, ResolveDoesNothingAfterReject) {
  V8TestingScope scope;
  auto* isolate = scope.GetIsolate();
  auto* promise =
      MakeGarbageCollected<StreamPromiseResolver>(scope.GetScriptState());
  promise->Reject(scope.GetScriptState(), v8::Null(isolate));
  promise->Resolve(scope.GetScriptState(), v8::Undefined(isolate));
  ASSERT_EQ(promise->State(isolate), v8::Promise::kRejected);
  EXPECT_TRUE(promise->V8Promise(isolate)->Result()->IsNull());
}

TEST(StreamPromiseResolverTest, GetScriptPromise) {
  V8TestingScope scope;
  auto* promise =
      MakeGarbageCollected<StreamPromiseResolver>(scope.GetScriptState());
  ScriptPromise script_promise =
      promise->GetScriptPromise(scope.GetScriptState());
  EXPECT_FALSE(script_promise.IsEmpty());
}

TEST(StreamPromiseResolverTest, MarkAsHandled) {
  V8TestingScope scope;
  auto* promise =
      MakeGarbageCollected<StreamPromiseResolver>(scope.GetScriptState());
  auto* isolate = scope.GetIsolate();
  promise->MarkAsHandled(isolate);
  EXPECT_TRUE(promise->V8Promise(isolate)->HasHandler());
}

TEST(StreamPromiseResolverTest, CreateResolved) {
  V8TestingScope scope;
  auto* isolate = scope.GetIsolate();
  auto* promise = StreamPromiseResolver::CreateResolved(scope.GetScriptState(),
                                                        v8::Null(isolate));
  ASSERT_EQ(promise->State(isolate), v8::Promise::kFulfilled);
  EXPECT_TRUE(promise->V8Promise(isolate)->Result()->IsNull());
}

TEST(StreamPromiseResolverTest, CreateResolvedWithUndefined) {
  V8TestingScope scope;
  auto* isolate = scope.GetIsolate();
  auto* promise = StreamPromiseResolver::CreateResolvedWithUndefined(
      scope.GetScriptState());
  ASSERT_EQ(promise->State(isolate), v8::Promise::kFulfilled);
  EXPECT_TRUE(promise->V8Promise(isolate)->Result()->IsUndefined());
}

TEST(StreamPromiseResolverTest, CreateRejected) {
  V8TestingScope scope;
  auto* isolate = scope.GetIsolate();
  auto* promise = StreamPromiseResolver::CreateRejected(scope.GetScriptState(),
                                                        v8::Null(isolate));
  ASSERT_EQ(promise->State(isolate), v8::Promise::kRejected);
  EXPECT_TRUE(promise->V8Promise(isolate)->Result()->IsNull());
}

}  // namespace

}  // namespace blink
