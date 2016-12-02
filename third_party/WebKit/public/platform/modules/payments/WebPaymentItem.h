// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPaymentItem_h
#define WebPaymentItem_h

#include "public/platform/WebString.h"
#include "public/platform/modules/payments/WebPaymentCurrencyAmount.h"

namespace blink {

// https://w3c.github.io/browser-payment-api/#paymentitem-dictionary
struct WebPaymentItem {
  WebString label;
  WebPaymentCurrencyAmount amount;
  bool pending;
};

}  // namespace blink

#endif  // WebPaymentItem_h
