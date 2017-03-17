// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPaymentAppResponse_h
#define WebPaymentAppResponse_h

#include "public/platform/WebString.h"

namespace blink {

// https://w3c.github.io/webpayments-payment-apps-api/#idl-def-paymentappresponse
struct WebPaymentAppResponse {
  WebString methodName;
  WebString stringifiedDetails;
};

}  // namespace blink

#endif  // WebPaymentAppResponse_h
