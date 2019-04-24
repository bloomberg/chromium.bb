// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_wrapper.h"

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_writable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_operations.h"
#include "third_party/blink/renderer/core/streams/retain_wrapper_during_construction.h"
#include "third_party/blink/renderer/core/streams/writable_stream_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

namespace blink {

class ReadableStreamWrapper::ReadHandleImpl final
    : public ReadableStream::ReadHandle {
 public:
  ReadHandleImpl(v8::Isolate* isolate, v8::Local<v8::Value> reader)
      : reader_(isolate, reader) {}

  ~ReadHandleImpl() override = default;

  ScriptPromise Read(ScriptState* script_state) override {
    return ReadableStreamOperations::DefaultReaderRead(
        script_state,
        ScriptValue(script_state,
                    reader_.NewLocal(script_state->GetIsolate())));
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(reader_);
    ReadHandle::Trace(visitor);
  }

 private:
  TraceWrapperV8Reference<v8::Value> reader_;
};

void ReadableStreamWrapper::Init(ScriptState* script_state,
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

void ReadableStreamWrapper::InitWithInternalStream(
    ScriptState* script_state,
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

ReadableStreamWrapper* ReadableStreamWrapper::Create(
    ScriptState* script_state,
    ScriptValue underlying_source,
    ScriptValue strategy,
    ExceptionState& exception_state) {
  auto* stream = MakeGarbageCollected<ReadableStreamWrapper>();
  stream->Init(script_state, underlying_source, strategy, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return stream;
}

ReadableStreamWrapper* ReadableStreamWrapper::CreateFromInternalStream(
    ScriptState* script_state,
    ScriptValue object,
    ExceptionState& exception_state) {
  DCHECK(object.IsObject());
  return CreateFromInternalStream(
      script_state, object.V8Value().As<v8::Object>(), exception_state);
}

ReadableStreamWrapper* ReadableStreamWrapper::CreateFromInternalStream(
    ScriptState* script_state,
    v8::Local<v8::Object> object,
    ExceptionState& exception_state) {
  auto* stream = MakeGarbageCollected<ReadableStreamWrapper>();
  stream->InitWithInternalStream(script_state, object, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return stream;
}

ReadableStreamWrapper* ReadableStreamWrapper::CreateWithCountQueueingStrategy(
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

void ReadableStreamWrapper::Trace(Visitor* visitor) {
  visitor->Trace(object_);
  ReadableStream::Trace(visitor);
}

ScriptPromise ReadableStreamWrapper::cancel(ScriptState* script_state,
                                            ExceptionState& exception_state) {
  return cancel(
      script_state,
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      exception_state);
}

bool ReadableStreamWrapper::locked(ScriptState* script_state,
                                   ExceptionState& exception_state) const {
  auto result = IsLocked(script_state, exception_state);

  return !result || *result;
}

ScriptPromise ReadableStreamWrapper::cancel(ScriptState* script_state,
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

ScriptValue ReadableStreamWrapper::getReader(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  return ReadableStreamOperations::GetReader(
      script_state, GetInternalStream(script_state), exception_state);
}

ScriptValue ReadableStreamWrapper::getReader(ScriptState* script_state,
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

ScriptValue ReadableStreamWrapper::pipeThrough(
    ScriptState* script_state,
    ScriptValue transform_stream,
    ExceptionState& exception_state) {
  return pipeThrough(
      script_state, transform_stream,
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      exception_state);
}

// https://streams.spec.whatwg.org/#rs-pipe-through
ScriptValue ReadableStreamWrapper::pipeThrough(
    ScriptState* script_state,
    ScriptValue transform_stream,
    ScriptValue options,
    ExceptionState& exception_state) {
  v8::Local<v8::Value> pair_value = transform_stream.V8Value();
  v8::Local<v8::Context> context = script_state->GetContext();

  constexpr char kWritableIsNotWritableStream[] =
      "parameter 1's 'writable' property is not a WritableStream.";
  constexpr char kReadableIsNotReadableStream[] =
      "parameter 1's 'readable' property is not a ReadableStream.";
  constexpr char kWritableIsLocked[] = "parameter 1's 'writable' is locked.";

  v8::Local<v8::Object> pair;
  if (!pair_value->ToObject(context).ToLocal(&pair)) {
    exception_state.ThrowTypeError(kWritableIsNotWritableStream);
    return ScriptValue();
  }

  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Value> writable, readable;
  {
    v8::TryCatch block(isolate);
    if (!pair->Get(context, V8String(isolate, "writable")).ToLocal(&writable)) {
      exception_state.RethrowV8Exception(block.Exception());
      return ScriptValue();
    }
    DCHECK(!block.HasCaught());

    if (!pair->Get(context, V8String(isolate, "readable")).ToLocal(&readable)) {
      exception_state.RethrowV8Exception(block.Exception());
      return ScriptValue();
    }
    DCHECK(!block.HasCaught());
  }

  // 2. If ! IsWritableStream(_writable_) is *false*, throw a *TypeError*
  //    exception.
  WritableStream* dom_writable =
      V8WritableStream::ToImplWithTypeCheck(isolate, writable);
  if (!dom_writable) {
    exception_state.ThrowTypeError(kWritableIsNotWritableStream);
    return ScriptValue();
  }

  // 3. If ! IsReadableStream(_readable_) is *false*, throw a *TypeError*
  //    exception.
  if (!V8ReadableStream::HasInstance(readable, isolate)) {
    exception_state.ThrowTypeError(kReadableIsNotReadableStream);
    return ScriptValue();
  }

  // TODO(ricea): When aborting pipes is supported, implement step 5:
  // 5. If _signal_ is not *undefined*, and _signal_ is not an instance of the
  //    `AbortSignal` interface, throw a *TypeError* exception.

  // 6. If ! IsReadableStreamLocked(*this*) is *true*, throw a *TypeError*
  //    exception.
  if (IsLocked(script_state, exception_state).value_or(false)) {
    exception_state.ThrowTypeError("Cannot pipe a locked stream");
    return ScriptValue();
  }
  if (exception_state.HadException()) {
    return ScriptValue();
  }

  // 7. If ! IsWritableStreamLocked(_writable_) is *true*, throw a *TypeError*
  //    exception.
  if (dom_writable->IsLocked(script_state, exception_state).value_or(false)) {
    exception_state.ThrowTypeError(kWritableIsLocked);
    return ScriptValue();
  }
  if (exception_state.HadException()) {
    return ScriptValue();
  }

  if (RuntimeEnabledFeatures::StreamsNativeEnabled()) {
    // TODO(ricea): Replace this with a DCHECK once ReadableStreamNative is
    // implemented.
    exception_state.ThrowTypeError(
        "pipeThrough disabled because StreamsNative feature is enabled");
    return ScriptValue();
  }

  // This cast is safe because the following code will only be run when the
  // native version of WritableStream is not in use.
  WritableStreamWrapper* writable_wrapper =
      static_cast<WritableStreamWrapper*>(dom_writable);

  // 8. Let _promise_ be ! ReadableStreamPipeTo(*this*, _writable_,
  //    _preventClose_, _preventAbort_, _preventCancel_,
  //   _signal_).

  // TODO(ricea): Maybe change the parameters to
  // ReadableStreamOperations::PipeTo to match ReadableStreamPipeTo() in the
  // standard?
  ScriptPromise promise = ReadableStreamOperations::PipeTo(
      script_state, GetInternalStream(script_state),
      writable_wrapper->GetInternalStream(script_state), options,
      exception_state);
  if (exception_state.HadException()) {
    return ScriptValue();
  }

  // 9. Set _promise_.[[PromiseIsHandled]] to *true*.
  promise.MarkAsHandled();

  // 10. Return _readable_.
  return ScriptValue(script_state, readable);
}

ScriptPromise ReadableStreamWrapper::pipeTo(ScriptState* script_state,
                                            ScriptValue destination,
                                            ExceptionState& exception_state) {
  return pipeTo(
      script_state, destination,
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      exception_state);
}

ScriptPromise ReadableStreamWrapper::pipeTo(ScriptState* script_state,
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

  if (RuntimeEnabledFeatures::StreamsNativeEnabled()) {
    // TODO(ricea): Replace this with a DCHECK once ReadableStreamNative is
    // implemented.
    exception_state.ThrowTypeError(
        "pipeTo disabled because StreamsNative feature is enabled");
    return ScriptPromise();
  }

  // This cast is safe because the following code will only be run when the
  // native version of WritableStream is not in use.
  WritableStreamWrapper* destination_wrapper =
      static_cast<WritableStreamWrapper*>(destination);

  return ReadableStreamOperations::PipeTo(
      script_state, GetInternalStream(script_state),
      destination_wrapper->GetInternalStream(script_state), options,
      exception_state);
}

ScriptValue ReadableStreamWrapper::tee(ScriptState* script_state,
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

void ReadableStreamWrapper::Tee(ScriptState* script_state,
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

  ReadableStreamWrapper* temp_branch1 =
      MakeGarbageCollected<ReadableStreamWrapper>();
  ReadableStreamWrapper* temp_branch2 =
      MakeGarbageCollected<ReadableStreamWrapper>();

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

ReadableStream::ReadHandle* ReadableStreamWrapper::GetReadHandle(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  ScriptValue reader = ReadableStreamOperations::GetReader(
      script_state, GetInternalStream(script_state), exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  return MakeGarbageCollected<ReadHandleImpl>(script_state->GetIsolate(),
                                              reader.V8Value());
}

base::Optional<bool> ReadableStreamWrapper::IsLocked(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  return ReadableStreamOperations::IsLocked(
      script_state, GetInternalStream(script_state), exception_state);
}

base::Optional<bool> ReadableStreamWrapper::IsDisturbed(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  return ReadableStreamOperations::IsDisturbed(
      script_state, GetInternalStream(script_state), exception_state);
}

base::Optional<bool> ReadableStreamWrapper::IsReadable(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  return ReadableStreamOperations::IsReadable(
      script_state, GetInternalStream(script_state), exception_state);
}

base::Optional<bool> ReadableStreamWrapper::IsClosed(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  return ReadableStreamOperations::IsClosed(
      script_state, GetInternalStream(script_state), exception_state);
}

base::Optional<bool> ReadableStreamWrapper::IsErrored(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  return ReadableStreamOperations::IsErrored(
      script_state, GetInternalStream(script_state), exception_state);
}

void ReadableStreamWrapper::LockAndDisturb(ScriptState* script_state,
                                           ExceptionState& exception_state) {
  ScriptState::Scope scope(script_state);

  const base::Optional<bool> is_locked =
      IsLocked(script_state, exception_state);
  if (!is_locked || is_locked.value())
    return;

  ScriptValue reader = getReader(script_state, exception_state);
  if (reader.IsEmpty())
    return;

  ScriptPromise promise =
      ReadableStreamOperations::DefaultReaderRead(script_state, reader);
  promise.MarkAsHandled();
}

void ReadableStreamWrapper::Serialize(ScriptState* script_state,
                                      MessagePort* port,
                                      ExceptionState& exception_state) {
  ReadableStreamOperations::Serialize(
      script_state, GetInternalStream(script_state), port, exception_state);
}

// static
ReadableStreamWrapper* ReadableStreamWrapper::Deserialize(
    ScriptState* script_state,
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

ScriptValue ReadableStreamWrapper::GetInternalStream(
    ScriptState* script_state) const {
  return ScriptValue(script_state,
                     object_.NewLocal(script_state->GetIsolate()));
}

}  // namespace blink
