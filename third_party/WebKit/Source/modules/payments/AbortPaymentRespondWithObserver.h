// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AbortPaymentRespondWithObserver_h
#define AbortPaymentRespondWithObserver_h

#include "modules/ModulesExport.h"
#include "modules/serviceworkers/RespondWithObserver.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponseError.h"

namespace blink {

class ExecutionContext;
class ScriptValue;
class WaitUntilObserver;

// Implementation for AbortPaymentEvent.respondWith(), which is used by the
// payment handler to indicate whether it was able to abort the payment.
class MODULES_EXPORT AbortPaymentRespondWithObserver final
    : public RespondWithObserver {
 public:
  AbortPaymentRespondWithObserver(ExecutionContext*,
                                  int event_id,
                                  WaitUntilObserver*);
  ~AbortPaymentRespondWithObserver() override = default;

  void OnResponseRejected(WebServiceWorkerResponseError) override;
  void OnResponseFulfilled(const ScriptValue&) override;
  void OnNoResponse() override;

  DECLARE_VIRTUAL_TRACE();
};

}  // namespace blink

#endif  // AbortPaymentRespondWithObserver_h
