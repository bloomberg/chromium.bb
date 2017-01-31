// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequestEvent.h"

#include "wtf/text/AtomicString.h"

namespace blink {

PaymentRequestEvent* PaymentRequestEvent::create(
    const AtomicString& type,
    const PaymentAppRequest& appRequest,
    WaitUntilObserver* observer) {
  return new PaymentRequestEvent(type, appRequest, observer);
}

PaymentRequestEvent::~PaymentRequestEvent() {}

const AtomicString& PaymentRequestEvent::interfaceName() const {
  return EventNames::PaymentRequestEvent;
}

void PaymentRequestEvent::appRequest(PaymentAppRequest& appRequest) const {
  appRequest = m_appRequest;
}

void PaymentRequestEvent::respondWith(ScriptPromise) {
  NOTIMPLEMENTED();
}

DEFINE_TRACE(PaymentRequestEvent) {
  visitor->trace(m_appRequest);
  ExtendableEvent::trace(visitor);
}

PaymentRequestEvent::PaymentRequestEvent(const AtomicString& type,
                                         const PaymentAppRequest& appRequest,
                                         WaitUntilObserver* observer)
    : ExtendableEvent(type, ExtendableEventInit(), observer),
      m_appRequest(appRequest) {}

}  // namespace blink
