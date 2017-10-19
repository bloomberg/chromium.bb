// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/streams/ReadableStreamOperations.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8IteratorResultValue.h"
#include "core/dom/Document.h"
#include "core/streams/ReadableStreamController.h"
#include "core/streams/UnderlyingSourceBase.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8BindingMacros.h"
#include "platform/bindings/V8ThrowException.h"
#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class NotReached : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state) {
    NotReached* self = new NotReached(script_state);
    return self->BindToV8Function();
  }

 private:
  explicit NotReached(ScriptState* script_state)
      : ScriptFunction(script_state) {}

  ScriptValue Call(ScriptValue) override;
};

ScriptValue NotReached::Call(ScriptValue) {
  EXPECT_TRUE(false) << "'Unreachable' code was reached";
  return ScriptValue();
}

class Iteration final : public GarbageCollectedFinalized<Iteration> {
 public:
  Iteration() : is_set_(false), is_done_(false), is_valid_(true) {}

  void Set(ScriptValue v) {
    DCHECK(!v.IsEmpty());
    is_set_ = true;
    v8::TryCatch block(v.GetScriptState()->GetIsolate());
    v8::Local<v8::Value> value;
    v8::Local<v8::Value> item = v.V8Value();
    if (!item->IsObject() ||
        !V8UnpackIteratorResult(v.GetScriptState(), item.As<v8::Object>(),
                                &is_done_)
             .ToLocal(&value)) {
      is_valid_ = false;
      return;
    }
    value_ = ToCoreString(value->ToString());
  }

  bool IsSet() const { return is_set_; }
  bool IsDone() const { return is_done_; }
  bool IsValid() const { return is_valid_; }
  const String& Value() const { return value_; }

  void Trace(blink::Visitor* visitor) {}

 private:
  bool is_set_;
  bool is_done_;
  bool is_valid_;
  String value_;
};

class ReaderFunction : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state,
                                                Iteration* iteration) {
    ReaderFunction* self = new ReaderFunction(script_state, iteration);
    return self->BindToV8Function();
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(iteration_);
    ScriptFunction::Trace(visitor);
  }

 private:
  ReaderFunction(ScriptState* script_state, Iteration* iteration)
      : ScriptFunction(script_state), iteration_(iteration) {}

  ScriptValue Call(ScriptValue value) override {
    iteration_->Set(value);
    return value;
  }

  Member<Iteration> iteration_;
};

class TestUnderlyingSource final : public UnderlyingSourceBase {
 public:
  explicit TestUnderlyingSource(ScriptState* script_state)
      : UnderlyingSourceBase(script_state) {}

  // Just expose the controller methods for easy testing
  void Enqueue(ScriptValue value) { Controller()->Enqueue(value); }
  void Close() { Controller()->Close(); }
  void GetError(ScriptValue value) { Controller()->GetError(value); }
  double DesiredSize() { return Controller()->DesiredSize(); }
};

class TryCatchScope {
 public:
  explicit TryCatchScope(v8::Isolate* isolate)
      : isolate_(isolate), trycatch_(isolate) {}

  ~TryCatchScope() {
    v8::MicrotasksScope::PerformCheckpoint(isolate_);
    EXPECT_FALSE(trycatch_.HasCaught());
  }

 private:
  v8::Isolate* isolate_;
  v8::TryCatch trycatch_;
};

ScriptValue Eval(V8TestingScope* scope, const char* s) {
  v8::Local<v8::String> source;
  v8::Local<v8::Script> script;
  v8::MicrotasksScope microtasks(scope->GetIsolate(),
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);
  if (!v8::String::NewFromUtf8(scope->GetIsolate(), s,
                               v8::NewStringType::kNormal)
           .ToLocal(&source)) {
    ADD_FAILURE();
    return ScriptValue();
  }
  if (!v8::Script::Compile(scope->GetContext(), source).ToLocal(&script)) {
    ADD_FAILURE() << "Compilation fails";
    return ScriptValue();
  }
  return ScriptValue(scope->GetScriptState(), script->Run(scope->GetContext()));
}

ScriptValue EvalWithPrintingError(V8TestingScope* scope, const char* s) {
  v8::TryCatch block(scope->GetIsolate());
  ScriptValue r = Eval(scope, s);
  if (block.HasCaught()) {
    ADD_FAILURE() << ToCoreString(
                         block.Exception()->ToString(scope->GetIsolate()))
                         .Utf8()
                         .data();
    block.ReThrow();
  }
  return r;
}

TEST(ReadableStreamOperationsTest, IsReadableStream) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  EXPECT_FALSE(ReadableStreamOperations::IsReadableStream(
      scope.GetScriptState(),
      ScriptValue(scope.GetScriptState(), v8::Undefined(scope.GetIsolate()))));
  EXPECT_FALSE(ReadableStreamOperations::IsReadableStream(
      scope.GetScriptState(), ScriptValue::CreateNull(scope.GetScriptState())));
  EXPECT_FALSE(ReadableStreamOperations::IsReadableStream(
      scope.GetScriptState(),
      ScriptValue(scope.GetScriptState(),
                  v8::Object::New(scope.GetIsolate()))));
  ScriptValue stream = EvalWithPrintingError(&scope, "new ReadableStream()");
  EXPECT_FALSE(stream.IsEmpty());
  EXPECT_TRUE(ReadableStreamOperations::IsReadableStream(scope.GetScriptState(),
                                                         stream));
}

TEST(ReadableStreamOperationsTest, IsReadableStreamDefaultReaderInvalid) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  EXPECT_FALSE(ReadableStreamOperations::IsReadableStreamDefaultReader(
      scope.GetScriptState(),
      ScriptValue(scope.GetScriptState(), v8::Undefined(scope.GetIsolate()))));
  EXPECT_FALSE(ReadableStreamOperations::IsReadableStreamDefaultReader(
      scope.GetScriptState(), ScriptValue::CreateNull(scope.GetScriptState())));
  EXPECT_FALSE(ReadableStreamOperations::IsReadableStreamDefaultReader(
      scope.GetScriptState(),
      ScriptValue(scope.GetScriptState(),
                  v8::Object::New(scope.GetIsolate()))));
  ScriptValue stream = EvalWithPrintingError(&scope, "new ReadableStream()");
  EXPECT_FALSE(stream.IsEmpty());

  EXPECT_FALSE(ReadableStreamOperations::IsReadableStreamDefaultReader(
      scope.GetScriptState(), stream));
}

TEST(ReadableStreamOperationsTest, GetReader) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  ScriptValue stream = EvalWithPrintingError(&scope, "new ReadableStream()");
  EXPECT_FALSE(stream.IsEmpty());

  EXPECT_FALSE(
      ReadableStreamOperations::IsLocked(scope.GetScriptState(), stream));
  ScriptValue reader;
  {
    DummyExceptionStateForTesting es;
    reader =
        ReadableStreamOperations::GetReader(scope.GetScriptState(), stream, es);
    ASSERT_FALSE(es.HadException());
  }
  EXPECT_TRUE(
      ReadableStreamOperations::IsLocked(scope.GetScriptState(), stream));
  ASSERT_FALSE(reader.IsEmpty());

  EXPECT_FALSE(ReadableStreamOperations::IsReadableStream(
      scope.GetScriptState(), reader));
  EXPECT_TRUE(ReadableStreamOperations::IsReadableStreamDefaultReader(
      scope.GetScriptState(), reader));

  // Already locked!
  {
    DummyExceptionStateForTesting es;
    reader =
        ReadableStreamOperations::GetReader(scope.GetScriptState(), stream, es);
    ASSERT_TRUE(es.HadException());
  }
  ASSERT_TRUE(reader.IsEmpty());
}

TEST(ReadableStreamOperationsTest, IsDisturbed) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  ScriptValue stream =
      EvalWithPrintingError(&scope, "stream = new ReadableStream()");
  EXPECT_FALSE(stream.IsEmpty());

  EXPECT_FALSE(
      ReadableStreamOperations::IsDisturbed(scope.GetScriptState(), stream));

  ASSERT_FALSE(EvalWithPrintingError(&scope, "stream.cancel()").IsEmpty());

  EXPECT_TRUE(
      ReadableStreamOperations::IsDisturbed(scope.GetScriptState(), stream));
}

TEST(ReadableStreamOperationsTest, Read) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  ScriptValue reader =
      EvalWithPrintingError(&scope,
                            "var controller;"
                            "function start(c) { controller = c; }"
                            "new ReadableStream({start}).getReader()");
  EXPECT_FALSE(reader.IsEmpty());
  ASSERT_TRUE(ReadableStreamOperations::IsReadableStreamDefaultReader(
      scope.GetScriptState(), reader));

  Iteration* it1 = new Iteration();
  Iteration* it2 = new Iteration();
  ReadableStreamOperations::DefaultReaderRead(scope.GetScriptState(), reader)
      .Then(ReaderFunction::CreateFunction(scope.GetScriptState(), it1),
            NotReached::CreateFunction(scope.GetScriptState()));
  ReadableStreamOperations::DefaultReaderRead(scope.GetScriptState(), reader)
      .Then(ReaderFunction::CreateFunction(scope.GetScriptState(), it2),
            NotReached::CreateFunction(scope.GetScriptState()));

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_FALSE(it1->IsSet());
  EXPECT_FALSE(it2->IsSet());

  ASSERT_FALSE(
      EvalWithPrintingError(&scope, "controller.enqueue('hello')").IsEmpty());
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(it1->IsSet());
  EXPECT_TRUE(it1->IsValid());
  EXPECT_FALSE(it1->IsDone());
  EXPECT_EQ("hello", it1->Value());
  EXPECT_FALSE(it2->IsSet());

  ASSERT_FALSE(EvalWithPrintingError(&scope, "controller.close()").IsEmpty());
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(it1->IsSet());
  EXPECT_TRUE(it1->IsValid());
  EXPECT_FALSE(it1->IsDone());
  EXPECT_EQ("hello", it1->Value());
  EXPECT_TRUE(it2->IsSet());
  EXPECT_TRUE(it2->IsValid());
  EXPECT_TRUE(it2->IsDone());
}

TEST(ReadableStreamOperationsTest,
     CreateReadableStreamWithCustomUnderlyingSourceAndStrategy) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  auto underlying_source = new TestUnderlyingSource(scope.GetScriptState());

  ScriptValue strategy = ReadableStreamOperations::CreateCountQueuingStrategy(
      scope.GetScriptState(), 10);
  ASSERT_FALSE(strategy.IsEmpty());

  ScriptValue stream = ReadableStreamOperations::CreateReadableStream(
      scope.GetScriptState(), underlying_source, strategy);
  ASSERT_FALSE(stream.IsEmpty());

  EXPECT_EQ(10, underlying_source->DesiredSize());

  underlying_source->Enqueue(ScriptValue::From(scope.GetScriptState(), "a"));
  EXPECT_EQ(9, underlying_source->DesiredSize());

  underlying_source->Enqueue(ScriptValue::From(scope.GetScriptState(), "b"));
  EXPECT_EQ(8, underlying_source->DesiredSize());

  ScriptValue reader;
  {
    DummyExceptionStateForTesting es;
    reader =
        ReadableStreamOperations::GetReader(scope.GetScriptState(), stream, es);
    ASSERT_FALSE(es.HadException());
  }
  ASSERT_FALSE(reader.IsEmpty());

  Iteration* it1 = new Iteration();
  Iteration* it2 = new Iteration();
  Iteration* it3 = new Iteration();
  ReadableStreamOperations::DefaultReaderRead(scope.GetScriptState(), reader)
      .Then(ReaderFunction::CreateFunction(scope.GetScriptState(), it1),
            NotReached::CreateFunction(scope.GetScriptState()));
  ReadableStreamOperations::DefaultReaderRead(scope.GetScriptState(), reader)
      .Then(ReaderFunction::CreateFunction(scope.GetScriptState(), it2),
            NotReached::CreateFunction(scope.GetScriptState()));
  ReadableStreamOperations::DefaultReaderRead(scope.GetScriptState(), reader)
      .Then(ReaderFunction::CreateFunction(scope.GetScriptState(), it3),
            NotReached::CreateFunction(scope.GetScriptState()));

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  EXPECT_EQ(10, underlying_source->DesiredSize());

  EXPECT_TRUE(it1->IsSet());
  EXPECT_TRUE(it1->IsValid());
  EXPECT_FALSE(it1->IsDone());
  EXPECT_EQ("a", it1->Value());

  EXPECT_TRUE(it2->IsSet());
  EXPECT_TRUE(it2->IsValid());
  EXPECT_FALSE(it2->IsDone());
  EXPECT_EQ("b", it2->Value());

  EXPECT_FALSE(it3->IsSet());

  underlying_source->Close();
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  EXPECT_TRUE(it3->IsSet());
  EXPECT_TRUE(it3->IsValid());
  EXPECT_TRUE(it3->IsDone());
}

TEST(ReadableStreamOperationsTest,
     UnderlyingSourceShouldHavePendingActivityWhenLockedAndControllerIsActive) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  auto underlying_source = new TestUnderlyingSource(scope.GetScriptState());

  ScriptValue strategy = ReadableStreamOperations::CreateCountQueuingStrategy(
      scope.GetScriptState(), 10);
  ASSERT_FALSE(strategy.IsEmpty());

  ScriptValue stream = ReadableStreamOperations::CreateReadableStream(
      scope.GetScriptState(), underlying_source, strategy);
  ASSERT_FALSE(stream.IsEmpty());

  v8::Local<v8::Object> global = scope.GetScriptState()->GetContext()->Global();
  ASSERT_TRUE(global
                  ->Set(scope.GetContext(),
                        V8String(scope.GetIsolate(), "stream"),
                        stream.V8Value())
                  .IsJust());

  EXPECT_FALSE(underlying_source->HasPendingActivity());
  EvalWithPrintingError(&scope, "let reader = stream.getReader();");
  EXPECT_TRUE(underlying_source->HasPendingActivity());
  EvalWithPrintingError(&scope, "reader.releaseLock();");
  EXPECT_FALSE(underlying_source->HasPendingActivity());
  EvalWithPrintingError(&scope, "reader = stream.getReader();");
  EXPECT_TRUE(underlying_source->HasPendingActivity());
  underlying_source->Enqueue(
      ScriptValue(scope.GetScriptState(), v8::Undefined(scope.GetIsolate())));
  underlying_source->Close();
  EXPECT_FALSE(underlying_source->HasPendingActivity());
}

TEST(ReadableStreamOperationsTest, IsReadable) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  ScriptValue readable = EvalWithPrintingError(&scope, "new ReadableStream()");
  ScriptValue closed = EvalWithPrintingError(
      &scope, "new ReadableStream({start: c => c.close()})");
  ScriptValue errored = EvalWithPrintingError(
      &scope, "new ReadableStream({start: c => c.error()})");
  ASSERT_FALSE(readable.IsEmpty());
  ASSERT_FALSE(closed.IsEmpty());
  ASSERT_FALSE(errored.IsEmpty());

  EXPECT_TRUE(
      ReadableStreamOperations::IsReadable(scope.GetScriptState(), readable));
  EXPECT_FALSE(
      ReadableStreamOperations::IsReadable(scope.GetScriptState(), closed));
  EXPECT_FALSE(
      ReadableStreamOperations::IsReadable(scope.GetScriptState(), errored));
}

TEST(ReadableStreamOperationsTest, IsClosed) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  ScriptValue readable = EvalWithPrintingError(&scope, "new ReadableStream()");
  ScriptValue closed = EvalWithPrintingError(
      &scope, "new ReadableStream({start: c => c.close()})");
  ScriptValue errored = EvalWithPrintingError(
      &scope, "new ReadableStream({start: c => c.error()})");
  ASSERT_FALSE(readable.IsEmpty());
  ASSERT_FALSE(closed.IsEmpty());
  ASSERT_FALSE(errored.IsEmpty());

  EXPECT_FALSE(
      ReadableStreamOperations::IsClosed(scope.GetScriptState(), readable));
  EXPECT_TRUE(
      ReadableStreamOperations::IsClosed(scope.GetScriptState(), closed));
  EXPECT_FALSE(
      ReadableStreamOperations::IsClosed(scope.GetScriptState(), errored));
}

TEST(ReadableStreamOperationsTest, IsErrored) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  ScriptValue readable = EvalWithPrintingError(&scope, "new ReadableStream()");
  ScriptValue closed = EvalWithPrintingError(
      &scope, "new ReadableStream({start: c => c.close()})");
  ScriptValue errored = EvalWithPrintingError(
      &scope, "new ReadableStream({start: c => c.error()})");
  ASSERT_FALSE(readable.IsEmpty());
  ASSERT_FALSE(closed.IsEmpty());
  ASSERT_FALSE(errored.IsEmpty());

  EXPECT_FALSE(
      ReadableStreamOperations::IsErrored(scope.GetScriptState(), readable));
  EXPECT_FALSE(
      ReadableStreamOperations::IsErrored(scope.GetScriptState(), closed));
  EXPECT_TRUE(
      ReadableStreamOperations::IsErrored(scope.GetScriptState(), errored));
}

TEST(ReadableStreamOperationsTest, Tee) {
  V8TestingScope scope;
  TryCatchScope try_catch_scope(scope.GetIsolate());
  ScriptValue original =
      EvalWithPrintingError(&scope,
                            "var controller;"
                            "new ReadableStream({start: c => controller = c})");
  ASSERT_FALSE(original.IsEmpty());
  ScriptValue new1, new2;
  ReadableStreamOperations::Tee(scope.GetScriptState(), original, &new1, &new2);

  NonThrowableExceptionState ec;
  ScriptValue reader1 =
      ReadableStreamOperations::GetReader(scope.GetScriptState(), new1, ec);
  ScriptValue reader2 =
      ReadableStreamOperations::GetReader(scope.GetScriptState(), new2, ec);

  Iteration* it1 = new Iteration();
  Iteration* it2 = new Iteration();
  ReadableStreamOperations::DefaultReaderRead(scope.GetScriptState(), reader1)
      .Then(ReaderFunction::CreateFunction(scope.GetScriptState(), it1),
            NotReached::CreateFunction(scope.GetScriptState()));
  ReadableStreamOperations::DefaultReaderRead(scope.GetScriptState(), reader2)
      .Then(ReaderFunction::CreateFunction(scope.GetScriptState(), it2),
            NotReached::CreateFunction(scope.GetScriptState()));

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_FALSE(it1->IsSet());
  EXPECT_FALSE(it2->IsSet());

  ASSERT_FALSE(
      EvalWithPrintingError(&scope, "controller.enqueue('hello')").IsEmpty());
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  EXPECT_TRUE(it1->IsSet());
  EXPECT_TRUE(it1->IsValid());
  EXPECT_FALSE(it1->IsDone());
  EXPECT_EQ("hello", it1->Value());
  EXPECT_TRUE(it2->IsSet());
  EXPECT_TRUE(it2->IsValid());
  EXPECT_FALSE(it2->IsDone());
  EXPECT_EQ("hello", it2->Value());
}

}  // namespace

}  // namespace blink
