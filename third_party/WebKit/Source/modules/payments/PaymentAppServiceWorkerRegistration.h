// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentAppServiceWorkerRegistration_h
#define PaymentAppServiceWorkerRegistration_h

#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class PaymentManager;
class ScriptState;
class ServiceWorkerRegistration;

class PaymentAppServiceWorkerRegistration final
    : public GarbageCollectedFinalized<PaymentAppServiceWorkerRegistration>,
      public Supplement<ServiceWorkerRegistration> {
  USING_GARBAGE_COLLECTED_MIXIN(PaymentAppServiceWorkerRegistration);
  WTF_MAKE_NONCOPYABLE(PaymentAppServiceWorkerRegistration);

 public:
  virtual ~PaymentAppServiceWorkerRegistration();
  static PaymentAppServiceWorkerRegistration& From(ServiceWorkerRegistration&);

  static PaymentManager* paymentManager(ScriptState*,
                                        ServiceWorkerRegistration&);
  PaymentManager* paymentManager(ScriptState*);

  virtual void Trace(blink::Visitor*);

 private:
  explicit PaymentAppServiceWorkerRegistration(ServiceWorkerRegistration*);
  static const char* SupplementName();

  Member<ServiceWorkerRegistration> registration_;
  Member<PaymentManager> payment_manager_;
};

}  // namespace blink

#endif  // PaymentAppServiceWorkerRegistration_h
