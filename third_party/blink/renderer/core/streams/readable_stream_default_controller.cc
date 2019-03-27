// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"

namespace blink {

void ReadableStreamDefaultController::close(ScriptState* script_state) {}

void ReadableStreamDefaultController::enqueue(ScriptState* script_state,
                                              ExceptionState& exception_state) {
}

void ReadableStreamDefaultController::enqueue(ScriptState* script_state,
                                              ScriptValue chunk,
                                              ExceptionState& exception_state) {
}

void ReadableStreamDefaultController::error(ScriptState* script_state) {
  return;
}

void ReadableStreamDefaultController::error(ScriptState* script_state,
                                            ScriptValue e) {
  return;
}

}  // namespace blink
