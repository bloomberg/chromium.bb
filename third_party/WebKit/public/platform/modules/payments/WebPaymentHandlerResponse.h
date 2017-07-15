// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPaymentHandlerResponse_h
#define WebPaymentHandlerResponse_h

#include "public/platform/WebString.h"

namespace blink {

// https://w3c.github.io/payment-handler/#paymenthandlerresponse-dictionary
struct WebPaymentHandlerResponse {
  WebString method_name;
  WebString stringified_details;
};

}  // namespace blink

#endif  // WebPaymentHandlerResponse_h
