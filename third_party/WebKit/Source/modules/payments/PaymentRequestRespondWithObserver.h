// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentRequestRespondWithObserver_h
#define PaymentRequestRespondWithObserver_h

#include "modules/ModulesExport.h"
#include "modules/serviceworkers/RespondWithObserver.h"
#include "third_party/WebKit/common/service_worker/service_worker_error_type.mojom-blink.h"

namespace blink {

class ExecutionContext;
class ScriptValue;
class WaitUntilObserver;

// Implementation for PaymentRequestEvent.respondWith(), which is used by the
// payment handler to provide a payment response when the payment successfully
// completes.
class MODULES_EXPORT PaymentRequestRespondWithObserver final
    : public RespondWithObserver {
 public:
  ~PaymentRequestRespondWithObserver() override = default;

  static PaymentRequestRespondWithObserver* Create(ExecutionContext*,
                                                   int event_id,
                                                   WaitUntilObserver*);

  void OnResponseRejected(mojom::ServiceWorkerResponseError) override;
  void OnResponseFulfilled(const ScriptValue&) override;
  void OnNoResponse() override;

  virtual void Trace(blink::Visitor*);

 private:
  PaymentRequestRespondWithObserver(ExecutionContext*,
                                    int event_id,
                                    WaitUntilObserver*);
};

}  // namespace blink

#endif  // PaymentRequestRespondWithObserver_h
