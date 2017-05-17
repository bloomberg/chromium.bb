// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentManager_h
#define PaymentManager_h

#include "bindings/core/v8/ScriptPromise.h"
#include "components/payments/mojom/payment_app.mojom-blink.h"
#include "modules/ModulesExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class PaymentInstruments;
class ServiceWorkerRegistration;

class MODULES_EXPORT PaymentManager final
    : public GarbageCollectedFinalized<PaymentManager>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(PaymentManager);

 public:
  static PaymentManager* Create(ServiceWorkerRegistration*);

  PaymentInstruments* instruments();

  DECLARE_TRACE();

 private:
  explicit PaymentManager(ServiceWorkerRegistration*);

  void OnServiceConnectionError();

  Member<ServiceWorkerRegistration> registration_;
  payments::mojom::blink::PaymentManagerPtr manager_;
  Member<PaymentInstruments> instruments_;
};

}  // namespace blink

#endif  // PaymentManager_h
