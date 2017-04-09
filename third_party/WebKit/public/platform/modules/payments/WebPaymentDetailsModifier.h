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
  WebVector<WebString> supported_methods;
  WebPaymentItem total;
  WebVector<WebPaymentItem> additional_display_items;
  WebString stringified_data;
};

}  // namespace blink

#endif  // WebPaymentDetailsModifier_h
