// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPaymentRequestEventData_h
#define WebPaymentRequestEventData_h

#include "public/platform/WebString.h"
#include "public/platform/modules/payments/WebCanMakePaymentEventData.h"

namespace blink {

struct WebPaymentRequestEventData : public WebCanMakePaymentEventData {
  WebString payment_request_id;
  WebString instrument_key;
  WebPaymentItem total;
};

}  // namespace blink

#endif  // WebPaymentRequestEventData_h
