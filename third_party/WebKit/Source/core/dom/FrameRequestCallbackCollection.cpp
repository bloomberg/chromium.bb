// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/FrameRequestCallbackCollection.h"

#include "core/inspector/InspectorTraceEvents.h"
#include "core/probe/CoreProbes.h"

namespace blink {

FrameRequestCallbackCollection::FrameRequestCallbackCollection(
    ExecutionContext* context)
    : context_(context) {}

FrameRequestCallbackCollection::CallbackId
FrameRequestCallbackCollection::RegisterCallback(FrameCallback* callback) {
  FrameRequestCallbackCollection::CallbackId id = ++next_callback_id_;
  callback->SetIsCancelled(false);
  callback->SetId(id);
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
    if (callbacks_[i]->Id() == id) {
      probe::AsyncTaskCanceledBreakable(context_, "cancelAnimationFrame",
                                        callbacks_[i]);
      callbacks_.EraseAt(i);
      TRACE_EVENT_INSTANT1("devtools.timeline", "CancelAnimationFrame",
                           TRACE_EVENT_SCOPE_THREAD, "data",
                           InspectorAnimationFrameEvent::Data(context_, id));
      return;
    }
  }
  for (const auto& callback : callbacks_to_invoke_) {
    if (callback->Id() == id) {
      probe::AsyncTaskCanceledBreakable(context_, "cancelAnimationFrame",
                                        callback);
      TRACE_EVENT_INSTANT1("devtools.timeline", "CancelAnimationFrame",
                           TRACE_EVENT_SCOPE_THREAD, "data",
                           InspectorAnimationFrameEvent::Data(context_, id));
      callback->SetIsCancelled(true);
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
  swap(callbacks_to_invoke_, callbacks_);

  for (const auto& callback : callbacks_to_invoke_) {
    if (!callback->IsCancelled()) {
      TRACE_EVENT1(
          "devtools.timeline", "FireAnimationFrame", "data",
          InspectorAnimationFrameEvent::Data(context_, callback->Id()));
      probe::AsyncTask async_task(context_, callback);
      probe::UserCallback probe(context_, "requestAnimationFrame",
                                AtomicString(), true);
      if (callback->GetUseLegacyTimeBase())
        callback->Invoke(high_res_now_ms_legacy);
      else
        callback->Invoke(high_res_now_ms);
    }
  }

  callbacks_to_invoke_.clear();
}

DEFINE_TRACE(FrameRequestCallbackCollection) {
  visitor->Trace(callbacks_);
  visitor->Trace(callbacks_to_invoke_);
  visitor->Trace(context_);
}

DEFINE_TRACE_WRAPPERS(FrameRequestCallbackCollection) {
  for (const auto& callback : callbacks_)
    visitor->TraceWrappers(callback);
  for (const auto& callback_to_invoke : callbacks_to_invoke_)
    visitor->TraceWrappers(callback_to_invoke);
}

FrameRequestCallbackCollection::V8FrameCallback::V8FrameCallback(
    V8FrameRequestCallback* callback)
    : callback_(callback) {}

DEFINE_TRACE(FrameRequestCallbackCollection::V8FrameCallback) {
  visitor->Trace(callback_);
  FrameRequestCallbackCollection::FrameCallback::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(FrameRequestCallbackCollection::V8FrameCallback) {
  visitor->TraceWrappers(callback_);
  FrameRequestCallbackCollection::FrameCallback::TraceWrappers(visitor);
}

void FrameRequestCallbackCollection::V8FrameCallback::Invoke(
    double highResTime) {
  callback_->call(nullptr, highResTime);
}

}  // namespace blink
