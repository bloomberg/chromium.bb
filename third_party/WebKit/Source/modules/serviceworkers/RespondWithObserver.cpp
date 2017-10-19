// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/RespondWithObserver.h"

#include <v8.h>

#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/ExecutionContext.h"
#include "modules/serviceworkers/WaitUntilObserver.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponse.h"

namespace blink {

void RespondWithObserver::ContextDestroyed(ExecutionContext*) {
  if (observer_) {
    DCHECK_EQ(kPending, state_);
    observer_.Clear();
  }
  state_ = kDone;
}

void RespondWithObserver::WillDispatchEvent() {
  event_dispatch_time_ = WTF::CurrentTime();
}

void RespondWithObserver::DidDispatchEvent(
    DispatchEventResult dispatch_result) {
  DCHECK(GetExecutionContext());
  if (state_ != kInitial)
    return;

  if (dispatch_result == DispatchEventResult::kNotCanceled) {
    OnNoResponse();
  } else {
    OnResponseRejected(kWebServiceWorkerResponseErrorDefaultPrevented);
  }

  state_ = kDone;
  observer_.Clear();
}

void RespondWithObserver::RespondWith(ScriptState* script_state,
                                      ScriptPromise script_promise,
                                      ExceptionState& exception_state) {
  if (state_ != kInitial) {
    exception_state.ThrowDOMException(
        kInvalidStateError, "The event has already been responded to.");
    return;
  }

  state_ = kPending;
  observer_->WaitUntil(
      script_state, script_promise, exception_state,
      WTF::Bind(&RespondWithObserver::ResponseWasFulfilled,
                WrapPersistent(this)),
      WTF::Bind(&RespondWithObserver::ResponseWasRejected, WrapPersistent(this),
                kWebServiceWorkerResponseErrorPromiseRejected));
}

void RespondWithObserver::ResponseWasRejected(
    WebServiceWorkerResponseError error,
    const ScriptValue& value) {
  OnResponseRejected(error);
  state_ = kDone;
  observer_.Clear();
}

void RespondWithObserver::ResponseWasFulfilled(const ScriptValue& value) {
  OnResponseFulfilled(value);
  state_ = kDone;
  observer_.Clear();
}

RespondWithObserver::RespondWithObserver(ExecutionContext* context,
                                         int event_id,
                                         WaitUntilObserver* observer)
    : ContextLifecycleObserver(context),
      event_id_(event_id),
      state_(kInitial),
      observer_(observer) {}

void RespondWithObserver::Trace(blink::Visitor* visitor) {
  visitor->Trace(observer_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
