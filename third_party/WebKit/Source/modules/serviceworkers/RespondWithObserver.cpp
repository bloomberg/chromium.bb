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

class RespondWithObserver::ThenFunction final : public ScriptFunction {
 public:
  enum ResolveType {
    kFulfilled,
    kRejected,
  };

  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state,
                                                RespondWithObserver* observer,
                                                ResolveType type) {
    ThenFunction* self = new ThenFunction(script_state, observer, type);
    return self->BindToV8Function();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(observer_);
    ScriptFunction::Trace(visitor);
  }

 private:
  ThenFunction(ScriptState* script_state,
               RespondWithObserver* observer,
               ResolveType type)
      : ScriptFunction(script_state),
        observer_(observer),
        resolve_type_(type) {}

  ScriptValue Call(ScriptValue value) override {
    DCHECK(observer_);
    ASSERT(resolve_type_ == kFulfilled || resolve_type_ == kRejected);
    if (resolve_type_ == kRejected) {
      observer_->ResponseWasRejected(
          kWebServiceWorkerResponseErrorPromiseRejected);
      value =
          ScriptPromise::Reject(value.GetScriptState(), value).GetScriptValue();
    } else {
      observer_->ResponseWasFulfilled(value);
    }
    observer_ = nullptr;
    return value;
  }

  Member<RespondWithObserver> observer_;
  ResolveType resolve_type_;
};

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

  if (dispatch_result != DispatchEventResult::kNotCanceled) {
    observer_->IncrementPendingActivity();
    ResponseWasRejected(kWebServiceWorkerResponseErrorDefaultPrevented);
    return;
  }

  OnNoResponse();
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
  observer_->IncrementPendingActivity();
  script_promise.Then(ThenFunction::CreateFunction(script_state, this,
                                                   ThenFunction::kFulfilled),
                      ThenFunction::CreateFunction(script_state, this,
                                                   ThenFunction::kRejected));
}

void RespondWithObserver::ResponseWasRejected(
    WebServiceWorkerResponseError error) {
  OnResponseRejected(error);
  state_ = kDone;
  observer_->DecrementPendingActivity();
  observer_.Clear();
}

void RespondWithObserver::ResponseWasFulfilled(const ScriptValue& value) {
  OnResponseFulfilled(value);
  state_ = kDone;
  observer_->DecrementPendingActivity();
  observer_.Clear();
}

RespondWithObserver::RespondWithObserver(ExecutionContext* context,
                                         int event_id,
                                         WaitUntilObserver* observer)
    : ContextLifecycleObserver(context),
      event_id_(event_id),
      state_(kInitial),
      observer_(observer) {}

DEFINE_TRACE(RespondWithObserver) {
  visitor->Trace(observer_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
