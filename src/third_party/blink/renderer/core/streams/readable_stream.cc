// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream.h"

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_writable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_operations.h"
#include "third_party/blink/renderer/core/streams/retain_wrapper_during_construction.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

namespace blink {

class ReadableStream::NoopFunction : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state) {
    auto* self = MakeGarbageCollected<NoopFunction>(script_state);
    return self->BindToV8Function();
  }
  NoopFunction(ScriptState* script_state) : ScriptFunction(script_state) {}
  ScriptValue Call(ScriptValue value) override { return value; }
};

void ReadableStream::Init(ScriptState* script_state,
                          ScriptValue underlying_source,
                          ScriptValue strategy,
                          ExceptionState& exception_state) {
  ScriptValue value = ReadableStreamOperations::CreateReadableStream(
      script_state, underlying_source, strategy, exception_state);
  if (exception_state.HadException())
    return;

  DCHECK(value.IsObject());
  InitWithInternalStream(script_state, value.V8Value().As<v8::Object>(),
                         exception_state);
}

void ReadableStream::InitWithInternalStream(ScriptState* script_state,
                                            v8::Local<v8::Object> object,
                                            ExceptionState& exception_state) {
  DCHECK(ReadableStreamOperations::IsReadableStreamForDCheck(
      script_state, ScriptValue(script_state, object)));
  object_.Set(script_state->GetIsolate(), object);

  v8::Isolate* isolate = script_state->GetIsolate();
  v8::TryCatch block(isolate);
  v8::Local<v8::Value> wrapper = ToV8(this, script_state);
  if (wrapper.IsEmpty()) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }

  v8::Local<v8::Context> context = script_state->GetContext();
  v8::Local<v8::Object> bindings =
      context->GetExtrasBindingObject().As<v8::Object>();
  v8::Local<v8::Value> symbol_value;
  if (!bindings->Get(context, V8String(isolate, "internalReadableStreamSymbol"))
           .ToLocal(&symbol_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }

  if (wrapper.As<v8::Object>()
          ->Set(context, symbol_value.As<v8::Symbol>(), object)
          .IsNothing()) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }

  // This is needed because sometimes a ReadableStream can be detached from
  // the owner object such as Response.
  if (!RetainWrapperDuringConstruction(this, script_state)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot queue task to retain wrapper");
  }
}

ReadableStream* ReadableStream::Create(ScriptState* script_state,
                                       ExceptionState& exception_state) {
  return Create(
      script_state,
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      exception_state);
}

ReadableStream* ReadableStream::Create(ScriptState* script_state,
                                       ScriptValue underlying_source,
                                       ExceptionState& exception_state) {
  return Create(
      script_state, underlying_source,
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      exception_state);
}

ReadableStream* ReadableStream::Create(ScriptState* script_state,
                                       ScriptValue underlying_source,
                                       ScriptValue strategy,
                                       ExceptionState& exception_state) {
  auto* stream = MakeGarbageCollected<ReadableStream>();
  stream->Init(script_state, underlying_source, strategy, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return stream;
}

ReadableStream* ReadableStream::CreateFromInternalStream(
    ScriptState* script_state,
    ScriptValue object,
    ExceptionState& exception_state) {
  DCHECK(object.IsObject());
  return CreateFromInternalStream(
      script_state, object.V8Value().As<v8::Object>(), exception_state);
}

ReadableStream* ReadableStream::CreateFromInternalStream(
    ScriptState* script_state,
    v8::Local<v8::Object> object,
    ExceptionState& exception_state) {
  auto* stream = MakeGarbageCollected<ReadableStream>();
  stream->InitWithInternalStream(script_state, object, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return stream;
}

ReadableStream* ReadableStream::CreateWithCountQueueingStrategy(
    ScriptState* script_state,
    UnderlyingSourceBase* underlying_source,
    size_t high_water_mark) {
  v8::TryCatch block(script_state->GetIsolate());
  ScriptValue strategy =
      ReadableStreamOperations::CreateCountQueuingStrategy(script_state, 0);
  if (strategy.IsEmpty())
    return nullptr;

  ScriptValue value = ReadableStreamOperations::CreateReadableStream(
      script_state, underlying_source, strategy);
  if (value.IsEmpty())
    return nullptr;

  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kConstructionContext,
                                 "ReadableStream");
  DCHECK(value.V8Value()->IsObject());
  auto* stream = CreateFromInternalStream(script_state, value, exception_state);
  if (exception_state.HadException())
    exception_state.ClearException();
  return stream;
}

void ReadableStream::Trace(Visitor* visitor) {
  visitor->Trace(object_);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise ReadableStream::cancel(ScriptState* script_state,
                                     ExceptionState& exception_state) {
  return cancel(
      script_state,
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      exception_state);
}

bool ReadableStream::locked(ScriptState* script_state,
                            ExceptionState& exception_state) const {
  auto result = IsLocked(script_state, exception_state);

  return !result || *result;
}

ScriptPromise ReadableStream::cancel(ScriptState* script_state,
                                     ScriptValue reason,
                                     ExceptionState& exception_state) {
  if (locked(script_state, exception_state) &&
      !exception_state.HadException()) {
    exception_state.ThrowTypeError("Cannot cancel a locked stream");
  }

  if (exception_state.HadException())
    return ScriptPromise();

  return ReadableStreamOperations::Cancel(
      script_state, GetInternalStream(script_state), reason, exception_state);
}

ScriptValue ReadableStream::getReader(ScriptState* script_state,
                                      ExceptionState& exception_state) {
  return ReadableStreamOperations::GetReader(
      script_state, GetInternalStream(script_state), exception_state);
}

ScriptValue ReadableStream::getReader(ScriptState* script_state,
                                      ScriptValue options,
                                      ExceptionState& exception_state) {
  v8::TryCatch block(script_state->GetIsolate());
  v8::Local<v8::Value> mode;
  v8::Local<v8::String> mode_string;
  v8::Local<v8::Context> context = script_state->GetContext();
  if (options.V8Value()->IsUndefined()) {
    mode = v8::Undefined(script_state->GetIsolate());
  } else {
    v8::Local<v8::Object> v8_options;
    if (!options.V8Value()->ToObject(context).ToLocal(&v8_options)) {
      exception_state.RethrowV8Exception(block.Exception());
      return ScriptValue();
    }
    if (!v8_options->Get(context, V8String(script_state->GetIsolate(), "mode"))
             .ToLocal(&mode)) {
      exception_state.RethrowV8Exception(block.Exception());
      return ScriptValue();
    }
  }

  if (!mode->ToString(context).ToLocal(&mode_string)) {
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptValue();
  }
  if (ToCoreString(mode_string) == "byob") {
    exception_state.ThrowTypeError("invalid mode");
    return ScriptValue();
  }
  if (!mode->IsUndefined()) {
    exception_state.ThrowRangeError("invalid mode");
    return ScriptValue();
  }
  return ReadableStreamOperations::GetReader(
      script_state, GetInternalStream(script_state), exception_state);
}

ScriptValue ReadableStream::pipeThrough(ScriptState* script_state,
                                        ScriptValue transform_stream,
                                        ExceptionState& exception_state) {
  return pipeThrough(
      script_state, transform_stream,
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      exception_state);
}

ScriptValue ReadableStream::pipeThrough(ScriptState* script_state,
                                        ScriptValue transform_stream,
                                        ScriptValue options,
                                        ExceptionState& exception_state) {
  v8::Local<v8::Value> pair_value = transform_stream.V8Value();
  v8::Local<v8::Context> context = script_state->GetContext();

  constexpr char kWritableIsUndefined[] =
      "Failed to execute 'pipeThrough' on 'ReadableStream': "
      "parameter 1's 'writable' property is undefined.";
  constexpr char kReadableIsUndefined[] =
      "Failed to execute 'pipeThrough' on 'ReadableStream': "
      "parameter 1's 'readable' property is undefined.";

  v8::Local<v8::Object> pair;
  if (!pair_value->ToObject(context).ToLocal(&pair)) {
    exception_state.ThrowTypeError(kWritableIsUndefined);
    return ScriptValue();
  }

  v8::TryCatch block(script_state->GetIsolate());
  v8::Local<v8::Value> writable, readable;
  if (!pair->Get(context, V8String(script_state->GetIsolate(), "writable"))
           .ToLocal(&writable)) {
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptValue();
  }
  DCHECK(!block.HasCaught());

  if (writable->IsUndefined()) {
    exception_state.ThrowTypeError(kWritableIsUndefined);
    return ScriptValue();
  }

  if (!pair->Get(context, V8String(script_state->GetIsolate(), "readable"))
           .ToLocal(&readable)) {
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptValue();
  }
  DCHECK(!block.HasCaught());

  if (readable->IsUndefined()) {
    exception_state.ThrowTypeError(kReadableIsUndefined);
    return ScriptValue();
  }

  ScriptPromise promise =
      pipeTo(script_state, ScriptValue(script_state, writable), options,
             exception_state);
  if (!exception_state.HadException()) {
    // set promise.[[PromiseIsHandled]] to true.
    // We don't have a primitive to do this, so let's attach a catch handler.
    //
    // ScriptPromise::Then(f, g) is a confusing interface, it is actually
    // |promise.then(f).catch(g)|.
    promise.Then(v8::Local<v8::Function>(),
                 NoopFunction::CreateFunction(script_state));
  }
  return ScriptValue(script_state, readable);
}

ScriptPromise ReadableStream::pipeTo(ScriptState* script_state,
                                     ScriptValue destination,
                                     ExceptionState& exception_state) {
  return pipeTo(
      script_state, destination,
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      exception_state);
}

ScriptPromise ReadableStream::pipeTo(ScriptState* script_state,
                                     ScriptValue destination_value,
                                     ScriptValue options,
                                     ExceptionState& exception_state) {
  WritableStream* destination = V8WritableStream::ToImplWithTypeCheck(
      script_state->GetIsolate(), destination_value.V8Value());

  if (!destination) {
    exception_state.ThrowTypeError("Illegal invocation");
    return ScriptPromise();
  }
  if (locked(script_state, exception_state) &&
      !exception_state.HadException()) {
    exception_state.ThrowTypeError("Cannot pipe a locked stream");
    return ScriptPromise();
  }
  if (exception_state.HadException())
    return ScriptPromise();
  if (destination->locked(script_state, exception_state) &&
      !exception_state.HadException()) {
    exception_state.ThrowTypeError("Cannot pipe to a locked stream");
    return ScriptPromise();
  }
  if (exception_state.HadException())
    return ScriptPromise();

  return ReadableStreamOperations::PipeTo(
      script_state, GetInternalStream(script_state),
      destination->GetInternalStream(script_state), options, exception_state);
}

ScriptValue ReadableStream::tee(ScriptState* script_state,
                                ExceptionState& exception_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  ReadableStream* branch1 = nullptr;
  ReadableStream* branch2 = nullptr;

  Tee(script_state, &branch1, &branch2, exception_state);

  if (!branch1 || !branch2)
    return ScriptValue();

  DCHECK(!exception_state.HadException());

  v8::TryCatch block(isolate);
  v8::Local<v8::Context> context = script_state->GetContext();
  v8::Local<v8::Array> array = v8::Array::New(isolate, 2);
  v8::Local<v8::Object> global = context->Global();

  v8::Local<v8::Value> v8_branch1 = ToV8(branch1, global, isolate);
  if (v8_branch1.IsEmpty()) {
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptValue();
  }
  v8::Local<v8::Value> v8_branch2 = ToV8(branch2, global, isolate);
  if (v8_branch1.IsEmpty()) {
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptValue();
  }
  if (array->Set(context, V8String(isolate, "0"), v8_branch1).IsNothing()) {
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptValue();
  }
  if (array->Set(context, V8String(isolate, "1"), v8_branch2).IsNothing()) {
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptValue();
  }
  return ScriptValue(script_state, array);
}

void ReadableStream::Tee(ScriptState* script_state,
                         ReadableStream** branch1,
                         ReadableStream** branch2,
                         ExceptionState& exception_state) {
  v8::Local<v8::Context> context = script_state->GetContext();

  if (locked(script_state, exception_state) &&
      !exception_state.HadException()) {
    exception_state.ThrowTypeError("The stream is already locked.");
  }

  if (exception_state.HadException())
    return;

  ScriptValue tee_result = ReadableStreamOperations::Tee(
      script_state, GetInternalStream(script_state), exception_state);
  if (tee_result.IsEmpty())
    return;

  DCHECK(!exception_state.HadException());
  DCHECK(tee_result.V8Value()->IsArray());

  v8::Local<v8::Array> branches = tee_result.V8Value().As<v8::Array>();
  v8::Local<v8::Value> v8_branch1, v8_branch2;
  v8::TryCatch block(script_state->GetIsolate());

  if (!branches->Get(context, 0).ToLocal(&v8_branch1)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (!branches->Get(context, 1).ToLocal(&v8_branch2)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }

  DCHECK(v8_branch1->IsObject());
  DCHECK(v8_branch2->IsObject());

  ReadableStream* temp_branch1 = MakeGarbageCollected<ReadableStream>();
  ReadableStream* temp_branch2 = MakeGarbageCollected<ReadableStream>();

  temp_branch1->InitWithInternalStream(
      script_state, v8_branch1.As<v8::Object>(), exception_state);
  if (exception_state.HadException())
    return;

  temp_branch2->InitWithInternalStream(
      script_state, v8_branch2.As<v8::Object>(), exception_state);
  if (exception_state.HadException())
    return;

  *branch1 = temp_branch1;
  *branch2 = temp_branch2;
}

base::Optional<bool> ReadableStream::IsLocked(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  return ReadableStreamOperations::IsLocked(
      script_state, GetInternalStream(script_state), exception_state);
}

base::Optional<bool> ReadableStream::IsDisturbed(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  return ReadableStreamOperations::IsDisturbed(
      script_state, GetInternalStream(script_state), exception_state);
}

base::Optional<bool> ReadableStream::IsReadable(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  return ReadableStreamOperations::IsReadable(
      script_state, GetInternalStream(script_state), exception_state);
}

base::Optional<bool> ReadableStream::IsClosed(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  return ReadableStreamOperations::IsClosed(
      script_state, GetInternalStream(script_state), exception_state);
}

base::Optional<bool> ReadableStream::IsErrored(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  return ReadableStreamOperations::IsErrored(
      script_state, GetInternalStream(script_state), exception_state);
}

void ReadableStream::LockAndDisturb(ScriptState* script_state,
                                    ExceptionState& exception_state) {
  ScriptState::Scope scope(script_state);

  const base::Optional<bool> is_locked =
      IsLocked(script_state, exception_state);
  if (!is_locked || is_locked.value())
    return;

  ScriptValue reader = getReader(script_state, exception_state);
  if (reader.IsEmpty())
    return;

  ReadableStreamOperations::DefaultReaderRead(script_state, reader);
}

void ReadableStream::Serialize(ScriptState* script_state,
                               MessagePort* port,
                               ExceptionState& exception_state) {
  ReadableStreamOperations::Serialize(
      script_state, GetInternalStream(script_state), port, exception_state);
}

// static
ReadableStream* ReadableStream::Deserialize(ScriptState* script_state,
                                            MessagePort* port,
                                            ExceptionState& exception_state) {
  // We need to execute V8 Extras JavaScript to create the new ReadableStream.
  // We will not run author code.
  v8::Isolate::AllowJavascriptExecutionScope allow_js(
      script_state->GetIsolate());
  ScriptValue internal_stream = ReadableStreamOperations::Deserialize(
      script_state, port, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return CreateFromInternalStream(script_state, internal_stream,
                                  exception_state);
}

ScriptValue ReadableStream::GetInternalStream(ScriptState* script_state) const {
  return ScriptValue(script_state,
                     object_.NewLocal(script_state->GetIsolate()));
}

}  // namespace blink
