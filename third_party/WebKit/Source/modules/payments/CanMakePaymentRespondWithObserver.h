// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanMakePaymentRespondWithObserver_h
#define CanMakePaymentRespondWithObserver_h

#include "modules/ModulesExport.h"
#include "modules/serviceworkers/RespondWithObserver.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponseError.h"

namespace blink {

class ExecutionContext;
class ScriptValue;
class WaitUntilObserver;

// Implementation for CanMakePaymentEvent.respondWith(), which is used by the
// payment handler to indicate whether it can respond to a payment request.
class MODULES_EXPORT CanMakePaymentRespondWithObserver final
    : public RespondWithObserver {
 public:
  CanMakePaymentRespondWithObserver(ExecutionContext*,
                                    int event_id,
                                    WaitUntilObserver*);
  ~CanMakePaymentRespondWithObserver() override = default;

  void OnResponseRejected(WebServiceWorkerResponseError) override;
  void OnResponseFulfilled(const ScriptValue&) override;
  void OnNoResponse() override;

  virtual void Trace(blink::Visitor*);
};

}  // namespace blink

#endif  // CanMakePaymentRespondWithObserver_h
