// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8IntersectionObserverDelegate.h"

#include "bindings/core/v8/IntersectionObserverCallback.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/ExecutionContext.h"
#include "core/intersection_observer/IntersectionObserver.h"
#include "platform/bindings/V8PrivateProperty.h"
#include "platform/wtf/Assertions.h"

namespace blink {

V8IntersectionObserverDelegate::V8IntersectionObserverDelegate(
    IntersectionObserverCallback* callback,
    ScriptState* script_state)
    : callback_(callback), script_state_(script_state) {}

V8IntersectionObserverDelegate::~V8IntersectionObserverDelegate() {}

void V8IntersectionObserverDelegate::Deliver(
    const HeapVector<Member<IntersectionObserverEntry>>& entries,
    IntersectionObserver& observer) {
  callback_->call(&observer, entries, &observer);
}

DEFINE_TRACE(V8IntersectionObserverDelegate) {
  visitor->Trace(callback_);
  IntersectionObserverDelegate::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(V8IntersectionObserverDelegate) {
  visitor->TraceWrappers(callback_);
  IntersectionObserverDelegate::TraceWrappers(visitor);
}

}  // namespace blink
