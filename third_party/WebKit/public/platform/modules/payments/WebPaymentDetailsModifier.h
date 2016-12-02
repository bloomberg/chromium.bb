// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPaymentDetailsModifier_h
#define WebPaymentDetailsModifier_h

#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/payments/WebPaymentItem.h"

namespace blink {

// https://w3c.github.io/browser-payment-api/#paymentdetailsmodifier-dictionary
struct WebPaymentDetailsModifier {
  WebVector<WebString> supportedMethods;
  WebPaymentItem total;
  WebVector<WebPaymentItem> additionalDisplayItems;
  WebString stringifiedData;
};

}  // namespace blink

#endif  // WebPaymentDetailsModifier_h
