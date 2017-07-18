// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentAppServiceWorkerGlobalScope_h
#define PaymentAppServiceWorkerGlobalScope_h

#include "core/events/EventTarget.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class PaymentAppServiceWorkerGlobalScope {
  STATIC_ONLY(PaymentAppServiceWorkerGlobalScope);

 public:
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(abortpayment);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(canmakepayment);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(paymentrequest);
};

}  // namespace blink

#endif  // PaymentAppServiceWorkerGlobalScope_h
