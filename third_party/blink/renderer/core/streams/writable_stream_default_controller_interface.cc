// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/writable_stream_default_controller_interface.h"

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_writable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

namespace {

class WritableStreamDefaultControllerWrapper final
    : public WritableStreamDefaultControllerInterface {
 public:
  explicit WritableStreamDefaultControllerWrapper(ScriptValue controller)
      : controller_(controller.GetIsolate(), controller.V8Value()) {}

  void Error(ScriptState* script_state,
             v8::Local<v8::Value> js_error) override {
    v8::Isolate* isolate = script_state->GetIsolate();

    v8::Local<v8::Value> controller = controller_.NewLocal(isolate);
    v8::Local<v8::Value> args[] = {controller, js_error};
    v8::MaybeLocal<v8::Value> result = V8ScriptRunner::CallExtra(
        script_state, "WritableStreamDefaultControllerError", args);
    result.ToLocalChecked();
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(controller_);
    WritableStreamDefaultControllerInterface::Trace(visitor);
  }

 private:
  TraceWrapperV8Reference<v8::Value> controller_;
};

class WritableStreamDefaultControllerNative final
    : public WritableStreamDefaultControllerInterface {
 public:
  explicit WritableStreamDefaultControllerNative(ScriptValue controller) {
    DCHECK(RuntimeEnabledFeatures::StreamsNativeEnabled());
    DCHECK(controller.IsObject());
    controller_ = V8WritableStreamDefaultController::ToImpl(
        controller.V8Value().As<v8::Object>());
    DCHECK(controller_);
  }

  void Error(ScriptState* script_state, v8::Local<v8::Value> error) override {
    WritableStreamDefaultController::Error(script_state, controller_, error);
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(controller_);
    WritableStreamDefaultControllerInterface::Trace(visitor);
  }

 private:
  Member<WritableStreamDefaultController> controller_;
};

}  // namespace

WritableStreamDefaultControllerInterface::
    WritableStreamDefaultControllerInterface() = default;
WritableStreamDefaultControllerInterface::
    ~WritableStreamDefaultControllerInterface() = default;

WritableStreamDefaultControllerInterface*
WritableStreamDefaultControllerInterface::Create(ScriptValue controller) {
  if (RuntimeEnabledFeatures::StreamsNativeEnabled()) {
    return MakeGarbageCollected<WritableStreamDefaultControllerNative>(
        controller);
  }

  return MakeGarbageCollected<WritableStreamDefaultControllerWrapper>(
      controller);
}

}  // namespace blink
