// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/transform_stream_default_controller_wrapper.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

TransformStreamDefaultControllerWrapper::
    TransformStreamDefaultControllerWrapper(ScriptState* script_state,
                                            v8::Local<v8::Value> controller)
    : script_state_(script_state), controller_(controller) {
  DCHECK(controller->IsObject());
}

void TransformStreamDefaultControllerWrapper::Enqueue(
    v8::Local<v8::Value> chunk,
    ExceptionState& exception_state) {
  DCHECK(controller_->IsObject());
  v8::Local<v8::Value> args[] = {controller_, chunk};
  v8::TryCatch block(script_state_->GetIsolate());
  V8ScriptRunner::CallExtra(script_state_,
                            "TransformStreamDefaultControllerEnqueue", args);
  if (block.HasCaught()) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
}

}  // namespace blink
