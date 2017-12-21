// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/xr/XRFrameRequestCallbackCollection.h"

#include "bindings/modules/v8/v8_xr_frame_request_callback.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/probe/CoreProbes.h"
#include "modules/xr/XRPresentationFrame.h"
#include "modules/xr/XRSession.h"

namespace blink {

XRFrameRequestCallbackCollection::XRFrameRequestCallbackCollection(
    ExecutionContext* context)
    : context_(context) {}

XRFrameRequestCallbackCollection::CallbackId
XRFrameRequestCallbackCollection::RegisterCallback(
    V8XRFrameRequestCallback* callback) {
  CallbackId id = ++next_callback_id_;
  callbacks_.Set(id, callback);
  pending_callbacks_.push_back(id);

  probe::AsyncTaskScheduledBreakable(context_, "XRRequestFrame", callback);
  return id;
}

void XRFrameRequestCallbackCollection::CancelCallback(CallbackId id) {
  if (IsValidCallbackId(id)) {
    callbacks_.erase(id);
  }
}

void XRFrameRequestCallbackCollection::ExecuteCallbacks(
    XRSession* session,
    XRPresentationFrame* frame) {
  // First, generate a list of callbacks to consider.  Callbacks registered from
  // this point on are considered only for the "next" frame, not this one.
  DCHECK(callbacks_to_invoke_.IsEmpty());
  callbacks_to_invoke_.swap(pending_callbacks_);

  for (const auto& id : callbacks_to_invoke_) {
    V8XRFrameRequestCallback* callback = callbacks_.Take(id);

    // Callback won't be found if it was cancelled.
    if (!callback)
      continue;

    probe::AsyncTask async_task(context_, callback);
    probe::UserCallback probe(context_, "XRRequestFrame", AtomicString(), true);
    callback->InvokeAndReportException(session, 0, frame);
  }

  callbacks_to_invoke_.clear();
}

void XRFrameRequestCallbackCollection::Trace(blink::Visitor* visitor) {
  visitor->Trace(callbacks_);
  visitor->Trace(context_);
}

void XRFrameRequestCallbackCollection::TraceWrappers(
    const blink::ScriptWrappableVisitor* visitor) const {
  for (const auto& callback : callbacks_.Values()) {
    visitor->TraceWrappers(callback);
  }
}

}  // namespace blink
