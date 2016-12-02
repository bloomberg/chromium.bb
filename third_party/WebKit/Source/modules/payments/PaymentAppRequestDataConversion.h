// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentAppRequestDataConversion_h
#define PaymentAppRequestDataConversion_h

#include "modules/payments/PaymentAppRequestData.h"
#include "wtf/Allocator.h"

namespace blink {

class ScriptState;
struct WebPaymentAppRequestData;

class MODULES_EXPORT PaymentAppRequestDataConversion {
  STATIC_ONLY(PaymentAppRequestDataConversion);

 public:
  static PaymentAppRequestData toPaymentAppRequestData(
      ScriptState*,
      const WebPaymentAppRequestData&);
};

}  // namespace blink

#endif  // PaymentAppRequestDataConversion_h
