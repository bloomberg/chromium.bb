// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPaymentAppRequest_h
#define WebPaymentAppRequest_h

#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/payments/WebPaymentDetailsModifier.h"
#include "public/platform/modules/payments/WebPaymentItem.h"
#include "public/platform/modules/payments/WebPaymentMethodData.h"

namespace blink {

// https://w3c.github.io/webpayments-payment-apps-api/#idl-def-paymentapprequest
struct WebPaymentAppRequest {
  WebString origin;
  WebVector<WebPaymentMethodData> methodData;
  WebPaymentItem total;
  WebVector<WebPaymentDetailsModifier> modifiers;
  WebString optionId;
};

}  // namespace blink

#endif  // WebPaymentAppRequest_h
