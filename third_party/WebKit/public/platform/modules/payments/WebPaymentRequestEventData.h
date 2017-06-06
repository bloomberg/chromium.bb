// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPaymentRequestEventData_h
#define WebPaymentRequestEventData_h

#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/payments/WebPaymentDetailsModifier.h"
#include "public/platform/modules/payments/WebPaymentItem.h"
#include "public/platform/modules/payments/WebPaymentMethodData.h"

namespace blink {

struct WebPaymentRequestEventData {
  WebString top_level_origin;
  WebString payment_request_origin;
  WebString payment_request_id;
  WebVector<WebPaymentMethodData> method_data;
  WebPaymentItem total;
  WebVector<WebPaymentDetailsModifier> modifiers;
  WebString instrument_key;
};

}  // namespace blink

#endif  // WebPaymentRequestEventData_h
