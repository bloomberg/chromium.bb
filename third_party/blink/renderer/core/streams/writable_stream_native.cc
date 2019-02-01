// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/writable_stream_native.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

WritableStreamNative::WritableStreamNative() = default;

WritableStreamNative::WritableStreamNative(ScriptState* script_state,
                                           ScriptValue raw_underlying_sink,
                                           ScriptValue raw_strategy,
                                           ExceptionState& exception_state) {
  ThrowUnimplemented(exception_state);
}

WritableStreamNative::~WritableStreamNative() = default;

bool WritableStreamNative::locked(ScriptState* script_state,
                                  ExceptionState& exception_state) const {
  ThrowUnimplemented(exception_state);
  return false;
}

ScriptPromise WritableStreamNative::abort(ScriptState* script_state,
                                          ExceptionState& exception_state) {
  ThrowUnimplemented(exception_state);
  return ScriptPromise();
}

ScriptPromise WritableStreamNative::abort(ScriptState* script_state,
                                          ScriptValue reason,
                                          ExceptionState& exception_state) {
  ThrowUnimplemented(exception_state);
  return ScriptPromise();
}

ScriptValue WritableStreamNative::getWriter(ScriptState* script_state,
                                            ExceptionState& exception_state) {
  ThrowUnimplemented(exception_state);
  return ScriptValue();
}

base::Optional<bool> WritableStreamNative::IsLocked(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  ThrowUnimplemented(exception_state);
  return base::nullopt;
}

void WritableStreamNative::ThrowUnimplemented(ExceptionState& exception_state) {
  exception_state.ThrowTypeError("unimplemented");
}

}  // namespace blink
