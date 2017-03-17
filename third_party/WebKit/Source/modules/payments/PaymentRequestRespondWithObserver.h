// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentRequestRespondWithObserver_h
#define PaymentRequestRespondWithObserver_h

#include "modules/ModulesExport.h"
#include "modules/serviceworkers/RespondWithObserver.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponseError.h"

namespace blink {

class ExecutionContext;
class ScriptValue;
class WaitUntilObserver;

// This class observes the service worker's handling of a PaymentRequestEvent
// and notifies the client.
class MODULES_EXPORT PaymentRequestRespondWithObserver final
    : public RespondWithObserver {
 public:
  virtual ~PaymentRequestRespondWithObserver();

  static PaymentRequestRespondWithObserver* create(ExecutionContext*,
                                                   int eventID,
                                                   WaitUntilObserver*);

  void onResponseRejected(WebServiceWorkerResponseError) override;
  void onResponseFulfilled(const ScriptValue&) override;
  void onNoResponse() override;

  DECLARE_VIRTUAL_TRACE();

 private:
  PaymentRequestRespondWithObserver(ExecutionContext*,
                                    int eventID,
                                    WaitUntilObserver*);
};

}  // namespace blink

#endif  // PaymentRequestRespondWithObserver_h
