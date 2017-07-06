// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCanMakePaymentEventData_h
#define WebCanMakePaymentEventData_h

#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/payments/WebPaymentDetailsModifier.h"
#include "public/platform/modules/payments/WebPaymentItem.h"
#include "public/platform/modules/payments/WebPaymentMethodData.h"

namespace blink {

struct WebCanMakePaymentEventData {
  WebString top_level_origin;
  WebString payment_request_origin;
  WebVector<WebPaymentMethodData> method_data;
  WebVector<WebPaymentDetailsModifier> modifiers;
};

}  // namespace blink

#endif  // WebCanMakePaymentEventData_h
