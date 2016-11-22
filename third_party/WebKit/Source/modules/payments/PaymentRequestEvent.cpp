// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequestEvent.h"

#include "wtf/text/AtomicString.h"

namespace blink {

PaymentRequestEvent::~PaymentRequestEvent() {}

const AtomicString& PaymentRequestEvent::interfaceName() const {
  return EventNames::PaymentRequestEvent;
}

void PaymentRequestEvent::data(PaymentAppRequestData& data) const {
  NOTIMPLEMENTED();
}

void PaymentRequestEvent::respondWith(ScriptPromise) {
  NOTIMPLEMENTED();
}

DEFINE_TRACE(PaymentRequestEvent) {
  ExtendableEvent::trace(visitor);
}

PaymentRequestEvent::PaymentRequestEvent(const AtomicString& type,
                                         const PaymentAppRequestData& data,
                                         WaitUntilObserver* observer)
    : ExtendableEvent(type, ExtendableEventInit(), observer) {}

}  // namespace blink
