// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/streams/stream_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "v8/include/v8.h"

namespace blink {

ReadableStreamDefaultReader* ReadableStreamDefaultReader::Create(
    ScriptState* script_state,
    ReadableStream* stream,
    ExceptionState& exception_state) {
  ThrowUnimplemented(exception_state);
  return nullptr;
}

ReadableStreamDefaultReader::ReadableStreamDefaultReader(
    ScriptState* script_state,
    ReadableStreamNative* stream,
    ExceptionState& exception_state) {
  ThrowUnimplemented(exception_state);
  return;
}

ReadableStreamDefaultReader::~ReadableStreamDefaultReader() = default;

ScriptPromise ReadableStreamDefaultReader::closed(
    ScriptState* script_state) const {
  return RejectUnimplemented(script_state);
}

ScriptPromise ReadableStreamDefaultReader::cancel(ScriptState* script_state) {
  return RejectUnimplemented(script_state);
}

ScriptPromise ReadableStreamDefaultReader::cancel(ScriptState* script_state,
                                                  ScriptValue reason) {
  return RejectUnimplemented(script_state);
}

ScriptPromise ReadableStreamDefaultReader::read(ScriptState* script_state) {
  return RejectUnimplemented(script_state);
}

ScriptPromise ReadableStreamDefaultReader::read(ScriptState* script_state,
                                                ScriptValue chunk) {
  return RejectUnimplemented(script_state);
}

void ReadableStreamDefaultReader::releaseLock(ScriptState* script_state) {
  return;
}

void ReadableStreamDefaultReader::ThrowUnimplemented(
    ExceptionState& exception_state) {
  exception_state.ThrowTypeError("unimplemented");
}

ScriptPromise ReadableStreamDefaultReader::RejectUnimplemented(
    ScriptState* script_state) {
  return StreamPromiseResolver::CreateRejected(
             script_state, v8::Exception::TypeError(V8String(
                               script_state->GetIsolate(), "unimplemented")))
      ->GetScriptPromise(script_state);
}

}  // namespace blink
