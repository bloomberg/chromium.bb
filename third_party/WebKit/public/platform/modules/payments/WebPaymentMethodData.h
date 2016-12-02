// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPaymentMethodData_h
#define WebPaymentMethodData_h

#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/payments/WebPaymentDetailsModifier.h"
#include "public/platform/modules/payments/WebPaymentItem.h"
#include "public/platform/modules/payments/WebPaymentMethodData.h"

namespace blink {

// https://w3c.github.io/browser-payment-api/#paymentmethoddata-dictionary
struct WebPaymentMethodData {
  WebVector<WebString> supportedMethods;
  WebString stringifiedData;
};

}  // namespace blink

#endif  // WebPaymentMethodData_h
