// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentEventDataConversion_h
#define PaymentEventDataConversion_h

#include "modules/payments/CanMakePaymentEventInit.h"
#include "modules/payments/PaymentRequestEventInit.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class ScriptState;
struct WebCanMakePaymentEventData;
struct WebPaymentRequestEventData;

class MODULES_EXPORT PaymentEventDataConversion {
  STATIC_ONLY(PaymentEventDataConversion);

 public:
  static CanMakePaymentEventInit ToCanMakePaymentEventInit(
      ScriptState*,
      const WebCanMakePaymentEventData&);
  static PaymentRequestEventInit ToPaymentRequestEventInit(
      ScriptState*,
      const WebPaymentRequestEventData&);
};

}  // namespace blink

#endif  // PaymentEventDataConversion_h
