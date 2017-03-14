// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/RespondWithObserver.h"

#include <v8.h>

#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/ExecutionContext.h"
#include "modules/serviceworkers/WaitUntilObserver.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponse.h"

namespace blink {

class RespondWithObserver::ThenFunction final : public ScriptFunction {
 public:
  enum ResolveType {
    Fulfilled,
    Rejected,
  };

  static v8::Local<v8::Function> createFunction(ScriptState* scriptState,
                                                RespondWithObserver* observer,
                                                ResolveType type) {
    ThenFunction* self = new ThenFunction(scriptState, observer, type);
    return self->bindToV8Function();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_observer);
    ScriptFunction::trace(visitor);
  }

 private:
  ThenFunction(ScriptState* scriptState,
               RespondWithObserver* observer,
               ResolveType type)
      : ScriptFunction(scriptState),
        m_observer(observer),
        m_resolveType(type) {}

  ScriptValue call(ScriptValue value) override {
    ASSERT(m_observer);
    ASSERT(m_resolveType == Fulfilled || m_resolveType == Rejected);
    if (m_resolveType == Rejected) {
      m_observer->responseWasRejected(
          WebServiceWorkerResponseErrorPromiseRejected);
      value =
          ScriptPromise::reject(value.getScriptState(), value).getScriptValue();
    } else {
      m_observer->responseWasFulfilled(value);
    }
    m_observer = nullptr;
    return value;
  }

  Member<RespondWithObserver> m_observer;
  ResolveType m_resolveType;
};

void RespondWithObserver::contextDestroyed(ExecutionContext*) {
  if (m_observer) {
    DCHECK_EQ(Pending, m_state);
    m_observer.clear();
  }
  m_state = Done;
}

void RespondWithObserver::willDispatchEvent() {
  m_eventDispatchTime = WTF::currentTime();
}

void RespondWithObserver::didDispatchEvent(DispatchEventResult dispatchResult) {
  ASSERT(getExecutionContext());
  if (m_state != Initial)
    return;

  if (dispatchResult != DispatchEventResult::NotCanceled) {
    m_observer->incrementPendingActivity();
    responseWasRejected(WebServiceWorkerResponseErrorDefaultPrevented);
    return;
  }

  onNoResponse();
  m_state = Done;
  m_observer.clear();
}

void RespondWithObserver::respondWith(ScriptState* scriptState,
                                      ScriptPromise scriptPromise,
                                      ExceptionState& exceptionState) {
  if (m_state != Initial) {
    exceptionState.throwDOMException(
        InvalidStateError, "The event has already been responded to.");
    return;
  }

  m_state = Pending;
  m_observer->incrementPendingActivity();
  scriptPromise.then(
      ThenFunction::createFunction(scriptState, this, ThenFunction::Fulfilled),
      ThenFunction::createFunction(scriptState, this, ThenFunction::Rejected));
}

void RespondWithObserver::responseWasRejected(
    WebServiceWorkerResponseError error) {
  onResponseRejected(error);
  m_state = Done;
  m_observer->decrementPendingActivity();
  m_observer.clear();
}

void RespondWithObserver::responseWasFulfilled(const ScriptValue& value) {
  onResponseFulfilled(value);
  m_state = Done;
  m_observer->decrementPendingActivity();
  m_observer.clear();
}

RespondWithObserver::RespondWithObserver(ExecutionContext* context,
                                         int eventID,
                                         WaitUntilObserver* observer)
    : ContextLifecycleObserver(context),
      m_eventID(eventID),
      m_state(Initial),
      m_observer(observer) {}

DEFINE_TRACE(RespondWithObserver) {
  visitor->trace(m_observer);
  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink
