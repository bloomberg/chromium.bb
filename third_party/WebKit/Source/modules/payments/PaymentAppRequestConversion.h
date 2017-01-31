// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentAppRequestConversion_h
#define PaymentAppRequestConversion_h

#include "modules/payments/PaymentAppRequest.h"
#include "wtf/Allocator.h"

namespace blink {

class ScriptState;
struct WebPaymentAppRequest;

class MODULES_EXPORT PaymentAppRequestConversion {
  STATIC_ONLY(PaymentAppRequestConversion);

 public:
  static PaymentAppRequest toPaymentAppRequest(ScriptState*,
                                               const WebPaymentAppRequest&);
};

}  // namespace blink

#endif  // PaymentAppRequestConversion_h
