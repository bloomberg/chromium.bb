// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_interface.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/platform/bindings/scoped_persistent.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

namespace {

class ReadableStreamDefaultControllerWrapper final
    : public ReadableStreamDefaultControllerInterface {
 public:
  explicit ReadableStreamDefaultControllerWrapper(ScriptValue controller)
      : ReadableStreamDefaultControllerInterface(controller.GetScriptState()),
        js_controller_(controller.GetIsolate(), controller.V8Value()) {
    js_controller_.SetPhantom();
  }

  // Users of the ReadableStreamDefaultControllerWrapper can call this to note
  // that the stream has been canceled and thus they don't anticipate using the
  // ReadableStreamDefaultControllerWrapper anymore.
  // (close/desiredSize/enqueue/error will become no-ops afterward.)
  void NoteHasBeenCanceled() override { js_controller_.Clear(); }

  bool IsActive() const override { return !js_controller_.IsEmpty(); }

  void Close() override {
    ScriptState* script_state = script_state_;
    // This will assert that the context is valid; do not call this method when
    // the context is invalidated.
    ScriptState::Scope scope(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();

    v8::Local<v8::Value> controller = js_controller_.NewLocal(isolate);
    if (controller.IsEmpty())
      return;

    v8::Local<v8::Value> args[] = {controller};
    v8::MaybeLocal<v8::Value> result = V8ScriptRunner::CallExtra(
        script_state, "ReadableStreamDefaultControllerClose", args);
    js_controller_.Clear();
    result.ToLocalChecked();
  }

  double DesiredSize() const override {
    ScriptState* script_state = script_state_;
    // This will assert that the context is valid; do not call this method when
    // the context is invalidated.
    ScriptState::Scope scope(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();

    v8::Local<v8::Value> controller = js_controller_.NewLocal(isolate);
    if (controller.IsEmpty())
      return 0;

    v8::Local<v8::Value> args[] = {controller};
    v8::MaybeLocal<v8::Value> result = V8ScriptRunner::CallExtra(
        script_state, "ReadableStreamDefaultControllerGetDesiredSize", args);

    return result.ToLocalChecked().As<v8::Number>()->Value();
  }

  void Enqueue(v8::Local<v8::Value> js_chunk) const override {
    ScriptState* script_state = script_state_;
    // This will assert that the context is valid; do not call this method when
    // the context is invalidated.
    ScriptState::Scope scope(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();

    v8::Local<v8::Value> controller = js_controller_.NewLocal(isolate);
    if (controller.IsEmpty())
      return;

    v8::Local<v8::Value> args[] = {controller, js_chunk};
    v8::MaybeLocal<v8::Value> result = V8ScriptRunner::CallExtra(
        script_state, "ReadableStreamDefaultControllerEnqueue", args);
    result.ToLocalChecked();
  }

  void Error(v8::Local<v8::Value> js_error) override {
    ScriptState* script_state = script_state_;
    // This will assert that the context is valid; do not call this method when
    // the context is invalidated.
    ScriptState::Scope scope(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();

    v8::Local<v8::Value> controller = js_controller_.NewLocal(isolate);
    if (controller.IsEmpty())
      return;

    v8::Local<v8::Value> args[] = {controller, js_error};
    v8::MaybeLocal<v8::Value> result = V8ScriptRunner::CallExtra(
        script_state, "ReadableStreamDefaultControllerError", args);
    js_controller_.Clear();
    result.ToLocalChecked();
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(script_state_);
    ReadableStreamDefaultControllerInterface::Trace(visitor);
  }

 private:
  ScopedPersistent<v8::Value> js_controller_;
};

}  // namespace

ReadableStreamDefaultControllerInterface*
ReadableStreamDefaultControllerInterface::Create(ScriptValue controller) {
  // TODO(ricea): Support the StreamsNative implementation.
  return MakeGarbageCollected<ReadableStreamDefaultControllerWrapper>(
      controller);
}

ReadableStreamDefaultControllerInterface::
    ReadableStreamDefaultControllerInterface(ScriptState* script_state)
    : script_state_(script_state) {}

ReadableStreamDefaultControllerInterface::
    ~ReadableStreamDefaultControllerInterface() = default;

void ReadableStreamDefaultControllerInterface::Trace(Visitor* visitor) {
  visitor->Trace(script_state_);
}

}  // namespace blink
