// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequestEvent.h"

#include "wtf/text/AtomicString.h"

namespace blink {

PaymentRequestEvent* PaymentRequestEvent::create(
    const AtomicString& type,
    const PaymentAppRequestData& data,
    WaitUntilObserver* observer) {
  return new PaymentRequestEvent(type, data, observer);
}

PaymentRequestEvent::~PaymentRequestEvent() {}

const AtomicString& PaymentRequestEvent::interfaceName() const {
  return EventNames::PaymentRequestEvent;
}

void PaymentRequestEvent::data(PaymentAppRequestData& data) const {
  data = m_data;
}

void PaymentRequestEvent::respondWith(ScriptPromise) {
  NOTIMPLEMENTED();
}

DEFINE_TRACE(PaymentRequestEvent) {
  visitor->trace(m_data);
  ExtendableEvent::trace(visitor);
}

PaymentRequestEvent::PaymentRequestEvent(const AtomicString& type,
                                         const PaymentAppRequestData& data,
                                         WaitUntilObserver* observer)
    : ExtendableEvent(type, ExtendableEventInit(), observer), m_data(data) {}

}  // namespace blink
