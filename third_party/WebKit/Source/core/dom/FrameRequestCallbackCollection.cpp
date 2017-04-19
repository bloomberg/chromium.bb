// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/FrameRequestCallbackCollection.h"

#include "core/dom/FrameRequestCallback.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/probe/CoreProbes.h"

namespace blink {

FrameRequestCallbackCollection::FrameRequestCallbackCollection(
    ExecutionContext* context)
    : context_(context) {}

FrameRequestCallbackCollection::CallbackId
FrameRequestCallbackCollection::RegisterCallback(
    FrameRequestCallback* callback) {
  FrameRequestCallbackCollection::CallbackId id = ++next_callback_id_;
  callback->cancelled_ = false;
  callback->id_ = id;
  callbacks_.push_back(callback);

  TRACE_EVENT_INSTANT1("devtools.timeline", "RequestAnimationFrame",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorAnimationFrameEvent::Data(context_, id));
  probe::AsyncTaskScheduledBreakable(context_, "requestAnimationFrame",
                                     callback);
  return id;
}

void FrameRequestCallbackCollection::CancelCallback(CallbackId id) {
  for (size_t i = 0; i < callbacks_.size(); ++i) {
    if (callbacks_[i]->id_ == id) {
      probe::AsyncTaskCanceledBreakable(context_, "cancelAnimationFrame",
                                        callbacks_[i]);
      callbacks_.erase(i);
      TRACE_EVENT_INSTANT1("devtools.timeline", "CancelAnimationFrame",
                           TRACE_EVENT_SCOPE_THREAD, "data",
                           InspectorAnimationFrameEvent::Data(context_, id));
      return;
    }
  }
  for (const auto& callback : callbacks_to_invoke_) {
    if (callback->id_ == id) {
      probe::AsyncTaskCanceledBreakable(context_, "cancelAnimationFrame",
                                        callback);
      TRACE_EVENT_INSTANT1("devtools.timeline", "CancelAnimationFrame",
                           TRACE_EVENT_SCOPE_THREAD, "data",
                           InspectorAnimationFrameEvent::Data(context_, id));
      callback->cancelled_ = true;
      // will be removed at the end of executeCallbacks()
      return;
    }
  }
}

void FrameRequestCallbackCollection::ExecuteCallbacks(
    double high_res_now_ms,
    double high_res_now_ms_legacy) {
  // First, generate a list of callbacks to consider.  Callbacks registered from
  // this point on are considered only for the "next" frame, not this one.
  DCHECK(callbacks_to_invoke_.IsEmpty());
  callbacks_to_invoke_.Swap(callbacks_);

  for (const auto& callback : callbacks_to_invoke_) {
    if (!callback->cancelled_) {
      TRACE_EVENT1("devtools.timeline", "FireAnimationFrame", "data",
                   InspectorAnimationFrameEvent::Data(context_, callback->id_));
      probe::AsyncTask async_task(context_, callback);
      probe::UserCallback probe(context_, "requestAnimationFrame",
                                AtomicString(), true);
      if (callback->use_legacy_time_base_)
        callback->handleEvent(high_res_now_ms_legacy);
      else
        callback->handleEvent(high_res_now_ms);
    }
  }

  callbacks_to_invoke_.clear();
}

DEFINE_TRACE(FrameRequestCallbackCollection) {
  visitor->Trace(callbacks_);
  visitor->Trace(callbacks_to_invoke_);
  visitor->Trace(context_);
}

}  // namespace blink
