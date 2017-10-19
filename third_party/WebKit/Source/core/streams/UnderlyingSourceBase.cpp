// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/streams/UnderlyingSourceBase.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/streams/ReadableStreamController.h"
#include "platform/bindings/ScriptState.h"
#include "v8/include/v8.h"

namespace blink {

ScriptPromise UnderlyingSourceBase::startWrapper(ScriptState* script_state,
                                                 ScriptValue js_controller) {
  // Cannot call start twice (e.g., cannot use the same UnderlyingSourceBase to
  // construct multiple streams).
  DCHECK(!controller_);

  controller_ = new ReadableStreamController(js_controller);

  return Start(script_state);
}

ScriptPromise UnderlyingSourceBase::Start(ScriptState* script_state) {
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise UnderlyingSourceBase::pull(ScriptState* script_state) {
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise UnderlyingSourceBase::cancelWrapper(ScriptState* script_state,
                                                  ScriptValue reason) {
  controller_->NoteHasBeenCanceled();
  return Cancel(script_state, reason);
}

ScriptPromise UnderlyingSourceBase::Cancel(ScriptState* script_state,
                                           ScriptValue reason) {
  return ScriptPromise::CastUndefined(script_state);
}

void UnderlyingSourceBase::notifyLockAcquired() {
  is_stream_locked_ = true;
}

void UnderlyingSourceBase::notifyLockReleased() {
  is_stream_locked_ = false;
}

bool UnderlyingSourceBase::HasPendingActivity() const {
  // This will return false within a finite time period _assuming_ that
  // consumers use the controller to close or error the stream.
  // Browser-created readable streams should always close or error within a
  // finite time period, due to timeouts etc.
  return controller_ && controller_->IsActive() && is_stream_locked_;
}

void UnderlyingSourceBase::ContextDestroyed(ExecutionContext*) {
  if (controller_) {
    controller_->NoteHasBeenCanceled();
    controller_.Clear();
  }
}

void UnderlyingSourceBase::Trace(blink::Visitor* visitor) {
  ContextLifecycleObserver::Trace(visitor);
  visitor->Trace(controller_);
}

}  // namespace blink
