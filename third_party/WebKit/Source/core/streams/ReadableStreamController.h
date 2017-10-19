// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReadableStreamController_h
#define ReadableStreamController_h

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/CoreExport.h"
#include "platform/bindings/ScopedPersistent.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/RefPtr.h"
#include "v8/include/v8.h"

namespace blink {

// TODO(tyoshino): Rename this to ReadableStreamDefaultControllerWrapper.
class CORE_EXPORT ReadableStreamController final
    : public GarbageCollectedFinalized<ReadableStreamController> {
 public:
  void Trace(blink::Visitor* visitor) {}

  explicit ReadableStreamController(ScriptValue controller)
      : script_state_(controller.GetScriptState()),
        js_controller_(controller.GetIsolate(), controller.V8Value()) {
    js_controller_.SetPhantom();
  }

  // Users of the ReadableStreamController can call this to note that the stream
  // has been canceled and thus they don't anticipate using the
  // ReadableStreamController anymore.  (close/desiredSize/enqueue/error will
  // become no-ops afterward.)
  void NoteHasBeenCanceled() { js_controller_.Clear(); }

  bool IsActive() const { return !js_controller_.IsEmpty(); }

  void Close() {
    ScriptState* script_state = script_state_.get();
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

  double DesiredSize() const {
    ScriptState* script_state = script_state_.get();
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

  template <typename ChunkType>
  void Enqueue(ChunkType chunk) const {
    ScriptState* script_state = script_state_.get();
    // This will assert that the context is valid; do not call this method when
    // the context is invalidated.
    ScriptState::Scope scope(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();

    v8::Local<v8::Value> controller = js_controller_.NewLocal(isolate);
    if (controller.IsEmpty())
      return;

    v8::Local<v8::Value> js_chunk = ToV8(chunk, script_state);
    v8::Local<v8::Value> args[] = {controller, js_chunk};
    v8::MaybeLocal<v8::Value> result = V8ScriptRunner::CallExtra(
        script_state, "ReadableStreamDefaultControllerEnqueue", args);
    result.ToLocalChecked();
  }

  template <typename ErrorType>
  void GetError(ErrorType error) {
    ScriptState* script_state = script_state_.get();
    // This will assert that the context is valid; do not call this method when
    // the context is invalidated.
    ScriptState::Scope scope(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();

    v8::Local<v8::Value> controller = js_controller_.NewLocal(isolate);
    if (controller.IsEmpty())
      return;

    v8::Local<v8::Value> js_error = ToV8(error, script_state);
    v8::Local<v8::Value> args[] = {controller, js_error};
    v8::MaybeLocal<v8::Value> result = V8ScriptRunner::CallExtra(
        script_state, "ReadableStreamDefaultControllerError", args);
    js_controller_.Clear();
    result.ToLocalChecked();
  }

 private:
  RefPtr<ScriptState> script_state_;
  ScopedPersistent<v8::Value> js_controller_;
};

}  // namespace blink

#endif  // ReadableStreamController_h
