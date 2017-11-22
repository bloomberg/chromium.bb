// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8IntersectionObserverDelegate.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/v8_intersection_observer_callback.h"
#include "core/dom/ExecutionContext.h"
#include "core/intersection_observer/IntersectionObserver.h"
#include "platform/bindings/V8PrivateProperty.h"
#include "platform/wtf/Assertions.h"

namespace blink {

V8IntersectionObserverDelegate::V8IntersectionObserverDelegate(
    V8IntersectionObserverCallback* callback,
    ScriptState* script_state)
    : ContextClient(ExecutionContext::From(script_state)),
      callback_(callback) {}

V8IntersectionObserverDelegate::~V8IntersectionObserverDelegate() {}

void V8IntersectionObserverDelegate::Deliver(
    const HeapVector<Member<IntersectionObserverEntry>>& entries,
    IntersectionObserver& observer) {
  callback_->InvokeAndReportException(&observer, entries, &observer);
}

ExecutionContext* V8IntersectionObserverDelegate::GetExecutionContext() const {
  return ContextClient::GetExecutionContext();
}

void V8IntersectionObserverDelegate::Trace(blink::Visitor* visitor) {
  visitor->Trace(callback_);
  IntersectionObserverDelegate::Trace(visitor);
  ContextClient::Trace(visitor);
}

void V8IntersectionObserverDelegate::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(callback_);
  IntersectionObserverDelegate::TraceWrappers(visitor);
}

}  // namespace blink
