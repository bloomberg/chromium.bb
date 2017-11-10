// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/events/EventTargetImpl.h"

namespace blink {

EventTargetImpl* EventTargetImpl::Create(ScriptState* script_state) {
  return new EventTargetImpl(script_state);
}

const AtomicString& EventTargetImpl::InterfaceName() const {
  return EventTargetNames::EventTargetImpl;
}

ExecutionContext* EventTargetImpl::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

void EventTargetImpl::Trace(blink::Visitor* visitor) {
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

EventTargetImpl::EventTargetImpl(ScriptState* script_state)
    : ContextLifecycleObserver(ExecutionContext::From(script_state)) {}

}  // namespace blink
