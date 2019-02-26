// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/writable_stream.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_writable_stream.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/streams/retain_wrapper_during_construction.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

void WritableStream::Init(ScriptState* script_state,
                          ScriptValue underlying_sink,
                          ScriptValue strategy,
                          ExceptionState& exception_state) {
  v8::Local<v8::Object> internal_stream;
  v8::TryCatch block(script_state->GetIsolate());

  if (!CreateInternalStream(script_state, underlying_sink.V8Value(),
                            strategy.V8Value())
           .ToLocal(&internal_stream)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }

  if (!InitInternal(script_state, internal_stream)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
}

WritableStream* WritableStream::Create(ScriptState* script_state,
                                       ExceptionState& exception_state) {
  return Create(
      script_state,
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      exception_state);
}

WritableStream* WritableStream::Create(ScriptState* script_state,
                                       ScriptValue underlying_sink,
                                       ExceptionState& exception_state) {
  return Create(
      script_state, underlying_sink,
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      exception_state);
}

WritableStream* WritableStream::Create(ScriptState* script_state,
                                       ScriptValue underlying_sink,
                                       ScriptValue strategy,
                                       ExceptionState& exception_state) {
  auto* stream = MakeGarbageCollected<WritableStream>();
  stream->Init(script_state, underlying_sink, strategy, exception_state);
  if (exception_state.HadException())
    return nullptr;

  return stream;
}

WritableStream* WritableStream::CreateFromInternalStream(
    ScriptState* script_state,
    v8::Local<v8::Object> internal_stream,
    ExceptionState& exception_state) {
  v8::TryCatch block(script_state->GetIsolate());
  auto* stream = MakeGarbageCollected<WritableStream>();
  if (!stream->InitInternal(script_state, internal_stream)) {
    exception_state.RethrowV8Exception(block.Exception());
    return nullptr;
  }
  return stream;
}

bool WritableStream::InitInternal(ScriptState* script_state,
                                  v8::Local<v8::Object> internal_stream) {
  v8::Isolate* isolate = script_state->GetIsolate();

#if DCHECK_IS_ON()
  v8::Local<v8::Value> args[] = {internal_stream};
  v8::Local<v8::Value> result_value;

  if (!V8ScriptRunner::CallExtra(script_state, "IsWritableStream", args)
           .ToLocal(&result_value)) {
    DLOG(FATAL) << "Failing to call IsWritableStream for DCHECK.";
    return false;
  }
  DCHECK(result_value->BooleanValue(isolate));
#endif  // DCHECK_IS_ON()

  internal_stream_.Set(isolate, internal_stream);

  v8::Local<v8::Value> wrapper = ToV8(this, script_state);
  if (wrapper.IsEmpty())
    return false;

  v8::Local<v8::Context> context = script_state->GetContext();
  v8::Local<v8::Object> bindings =
      context->GetExtrasBindingObject().As<v8::Object>();
  v8::Local<v8::Value> symbol_value;
  if (!bindings->Get(context, V8String(isolate, "internalWritableStreamSymbol"))
           .ToLocal(&symbol_value)) {
    return false;
  }

  if (wrapper.As<v8::Object>()
          ->Set(context, symbol_value.As<v8::Symbol>(),
                internal_stream_.NewLocal(isolate))
          .IsNothing()) {
    return false;
  }

  return RetainWrapperDuringConstruction(this, script_state);
}

v8::MaybeLocal<v8::Object> WritableStream::CreateInternalStream(
    ScriptState* script_state,
    v8::Local<v8::Value> underlying_sink,
    v8::Local<v8::Value> strategy) {
  v8::Local<v8::Value> args[] = {underlying_sink, strategy};
  v8::Local<v8::Value> stream;

  if (!V8ScriptRunner::CallExtra(script_state, "createWritableStream", args)
           .ToLocal(&stream)) {
    return v8::MaybeLocal<v8::Object>();
  }

  DCHECK(stream->IsObject());
  return v8::MaybeLocal<v8::Object>(stream.As<v8::Object>());
}

void WritableStream::Trace(Visitor* visitor) {
  visitor->Trace(internal_stream_);
  ScriptWrappable::Trace(visitor);
}

bool WritableStream::locked(ScriptState* script_state,
                            ExceptionState& exception_state) const {
  auto result = IsLocked(script_state, exception_state);

  return !result || *result;
}

ScriptPromise WritableStream::abort(ScriptState* script_state,
                                    ExceptionState& exception_state) {
  return abort(
      script_state,
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      exception_state);
}

ScriptPromise WritableStream::abort(ScriptState* script_state,
                                    ScriptValue reason,
                                    ExceptionState& exception_state) {
  if (locked(script_state, exception_state) &&
      !exception_state.HadException()) {
    exception_state.ThrowTypeError("Cannot abort a locked stream");
  }

  v8::Local<v8::Value> args[] = {
      internal_stream_.NewLocal(script_state->GetIsolate()), reason.V8Value()};
  v8::TryCatch block(script_state->GetIsolate());
  v8::Local<v8::Value> result;

  if (!V8ScriptRunner::CallExtra(script_state, "WritableStreamAbort", args)
           .ToLocal(&result)) {
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptPromise();
  }
  return ScriptPromise(script_state, result);
}

ScriptValue WritableStream::getWriter(ScriptState* script_state,
                                      ExceptionState& exception_state) {
  v8::TryCatch block(script_state->GetIsolate());
  v8::Local<v8::Value> args[] = {
      internal_stream_.NewLocal(script_state->GetIsolate())};
  v8::Local<v8::Value> result;

  if (!V8ScriptRunner::CallExtra(script_state,
                                 "AcquireWritableStreamDefaultWriter", args)
           .ToLocal(&result)) {
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptValue();
  }
  return ScriptValue(script_state, result);
}

base::Optional<bool> WritableStream::IsLocked(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  v8::TryCatch block(script_state->GetIsolate());
  v8::Local<v8::Value> args[] = {
      internal_stream_.NewLocal(script_state->GetIsolate())};
  v8::Local<v8::Value> result_value;

  if (!V8ScriptRunner::CallExtra(script_state, "IsWritableStreamLocked", args)
           .ToLocal(&result_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return base::nullopt;
  }
  return result_value->BooleanValue(script_state->GetIsolate());
}

void WritableStream::Serialize(ScriptState* script_state,
                               MessagePort* port,
                               ExceptionState& exception_state) {
  DCHECK(port);
  DCHECK(RuntimeEnabledFeatures::TransferableStreamsEnabled());
  v8::TryCatch block(script_state->GetIsolate());
  v8::Local<v8::Value> port_v8_value = ToV8(port, script_state);
  DCHECK(!port_v8_value.IsEmpty());
  v8::Local<v8::Value> args[] = {ToV8(this, script_state), port_v8_value};
  V8ScriptRunner::CallExtra(script_state, "WritableStreamSerialize", args);
  if (block.HasCaught()) {
    exception_state.RethrowV8Exception(block.Exception());
  }
}

// static
WritableStream* WritableStream::Deserialize(ScriptState* script_state,
                                            MessagePort* port,
                                            ExceptionState& exception_state) {
  // We need to execute V8 Extras JavaScript to create the new WritableStream.
  // We will not run author code.
  auto* isolate = script_state->GetIsolate();
  v8::Isolate::AllowJavascriptExecutionScope allow_js(isolate);
  DCHECK(port);
  DCHECK(RuntimeEnabledFeatures::TransferableStreamsEnabled());
  v8::TryCatch block(isolate);
  v8::Local<v8::Value> port_v8 = ToV8(port, script_state);
  DCHECK(!port_v8.IsEmpty());
  v8::Local<v8::Value> args[] = {port_v8};
  ScriptValue internal_stream(
      script_state, V8ScriptRunner::CallExtra(
                        script_state, "WritableStreamDeserialize", args));
  if (block.HasCaught()) {
    exception_state.RethrowV8Exception(block.Exception());
    return nullptr;
  }
  DCHECK(!internal_stream.IsEmpty());
  return CreateFromInternalStream(script_state, internal_stream,
                                  exception_state);
}

ScriptValue WritableStream::GetInternalStream(ScriptState* script_state) const {
  return ScriptValue(script_state,
                     internal_stream_.NewLocal(script_state->GetIsolate()));
}

}  // namespace blink
