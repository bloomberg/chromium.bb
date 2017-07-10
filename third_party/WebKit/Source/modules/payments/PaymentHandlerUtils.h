// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentHandlerUtils_h
#define PaymentHandlerUtils_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponseError.h"

namespace blink {

class ExecutionContext;

class PaymentHandlerUtils {
  STATIC_ONLY(PaymentHandlerUtils);

 public:
  static void ReportResponseError(ExecutionContext*,
                                  const String& event_name_prefix,
                                  WebServiceWorkerResponseError);
};

}  // namespace blink

#endif  // PaymentHandlerUtils_h
