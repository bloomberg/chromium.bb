// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/streams/stream_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "v8/include/v8.h"

namespace blink {

WritableStreamDefaultWriter* WritableStreamDefaultWriter::Create(
    ScriptState* script_state,
    WritableStream* stream,
    ExceptionState& exception_state) {
  ThrowUnimplemented(exception_state);
  return nullptr;
}

WritableStreamDefaultWriter::WritableStreamDefaultWriter(
    ScriptState* script_state,
    WritableStreamNative* stream,
    ExceptionState& exception_state) {
  ThrowUnimplemented(exception_state);
  return;
}

WritableStreamDefaultWriter::~WritableStreamDefaultWriter() = default;

ScriptPromise WritableStreamDefaultWriter::closed(
    ScriptState* script_state) const {
  return RejectUnimplemented(script_state);
}

ScriptValue WritableStreamDefaultWriter::desiredSize(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  ThrowUnimplemented(exception_state);
  return ScriptValue();
}

ScriptPromise WritableStreamDefaultWriter::ready(
    ScriptState* script_state) const {
  return RejectUnimplemented(script_state);
}

ScriptPromise WritableStreamDefaultWriter::abort(ScriptState* script_state) {
  return RejectUnimplemented(script_state);
}

ScriptPromise WritableStreamDefaultWriter::abort(ScriptState* script_state,
                                                 ScriptValue reason) {
  return RejectUnimplemented(script_state);
}

ScriptPromise WritableStreamDefaultWriter::close(ScriptState* script_state) {
  return RejectUnimplemented(script_state);
}

void WritableStreamDefaultWriter::releaseLock(ScriptState* script_state) {
  return;
}

ScriptPromise WritableStreamDefaultWriter::write(ScriptState* script_state) {
  return RejectUnimplemented(script_state);
}

ScriptPromise WritableStreamDefaultWriter::write(ScriptState* script_state,
                                                 ScriptValue chunk) {
  return RejectUnimplemented(script_state);
}

void WritableStreamDefaultWriter::ThrowUnimplemented(
    ExceptionState& exception_state) {
  exception_state.ThrowTypeError("unimplemented");
}

ScriptPromise WritableStreamDefaultWriter::RejectUnimplemented(
    ScriptState* script_state) {
  return StreamPromiseResolver::CreateRejected(
             script_state, v8::Exception::TypeError(V8String(
                               script_state->GetIsolate(), "unimplemented")))
      ->GetScriptPromise(script_state);
}

}  // namespace blink
