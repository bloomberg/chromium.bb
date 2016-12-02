// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPaymentCurrencyAmount_h
#define WebPaymentCurrencyAmount_h

#include "public/platform/WebString.h"

namespace blink {

// https://w3c.github.io/browser-payment-api/#paymentcurrencyamount-dictionary
struct WebPaymentCurrencyAmount {
  WebString currency;
  WebString value;
  WebString currencySystem;
};

}  // namespace blink

#endif  // WebPaymentCurrencyAmount_h
