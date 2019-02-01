// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"

namespace blink {

void WritableStreamDefaultController::error(ScriptState* script_state) {
  return;
}

void WritableStreamDefaultController::error(ScriptState* script_state,
                                            ScriptValue e) {
  return;
}

}  // namespace blink
