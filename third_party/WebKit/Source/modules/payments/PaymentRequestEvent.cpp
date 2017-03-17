// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequestEvent.h"

#include "modules/serviceworkers/RespondWithObserver.h"
#include "wtf/text/AtomicString.h"

namespace blink {

PaymentRequestEvent* PaymentRequestEvent::create(
    const AtomicString& type,
    const PaymentAppRequest& appRequest,
    RespondWithObserver* respondWithObserver,
    WaitUntilObserver* waitUntilObserver) {
  return new PaymentRequestEvent(type, appRequest, respondWithObserver,
                                 waitUntilObserver);
}

PaymentRequestEvent::~PaymentRequestEvent() {}

const AtomicString& PaymentRequestEvent::interfaceName() const {
  return EventNames::PaymentRequestEvent;
}

void PaymentRequestEvent::appRequest(PaymentAppRequest& appRequest) const {
  appRequest = m_appRequest;
}

void PaymentRequestEvent::respondWith(ScriptState* scriptState,
                                      ScriptPromise scriptPromise,
                                      ExceptionState& exceptionState) {
  stopImmediatePropagation();
  if (m_observer) {
    m_observer->respondWith(scriptState, scriptPromise, exceptionState);
  }
}

DEFINE_TRACE(PaymentRequestEvent) {
  visitor->trace(m_appRequest);
  visitor->trace(m_observer);
  ExtendableEvent::trace(visitor);
}

PaymentRequestEvent::PaymentRequestEvent(
    const AtomicString& type,
    const PaymentAppRequest& appRequest,
    RespondWithObserver* respondWithObserver,
    WaitUntilObserver* waitUntilObserver)
    : ExtendableEvent(type, ExtendableEventInit(), waitUntilObserver),
      m_appRequest(appRequest),
      m_observer(respondWithObserver) {}

}  // namespace blink
