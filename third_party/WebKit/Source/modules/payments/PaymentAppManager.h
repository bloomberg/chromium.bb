// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentAppManager_h
#define PaymentAppManager_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "components/payments/content/payment_app.mojom-blink.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class PaymentAppManifest;
class ScriptPromiseResolver;
class ScriptState;
class ServiceWorkerRegistration;

class MODULES_EXPORT PaymentAppManager final
    : public GarbageCollectedFinalized<PaymentAppManager>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(PaymentAppManager);

 public:
  static PaymentAppManager* create(ServiceWorkerRegistration*);

  ScriptPromise setManifest(ScriptState*, const PaymentAppManifest&);
  ScriptPromise getManifest(ScriptState*);

  DECLARE_TRACE();

 private:
  explicit PaymentAppManager(ServiceWorkerRegistration*);

  void onSetManifest(ScriptPromiseResolver*,
                     payments::mojom::blink::PaymentAppManifestError);
  void onGetManifest(ScriptPromiseResolver*,
                     payments::mojom::blink::PaymentAppManifestPtr,
                     payments::mojom::blink::PaymentAppManifestError);
  void onServiceConnectionError();

  Member<ServiceWorkerRegistration> m_registration;
  payments::mojom::blink::PaymentAppManagerPtr m_manager;
};

}  // namespace blink

#endif  // PaymentAppManager_h
