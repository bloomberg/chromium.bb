// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentManager_h
#define PaymentManager_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "components/payments/content/payment_app.mojom-blink.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class PaymentAppManifest;
class PaymentInstruments;
class ScriptPromiseResolver;
class ScriptState;
class ServiceWorkerRegistration;

class MODULES_EXPORT PaymentManager final
    : public GarbageCollectedFinalized<PaymentManager>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(PaymentManager);

 public:
  static PaymentManager* Create(ServiceWorkerRegistration*);

  ScriptPromise setManifest(ScriptState*, const PaymentAppManifest&);
  ScriptPromise getManifest(ScriptState*);

  PaymentInstruments* instruments();

  DECLARE_TRACE();

 private:
  explicit PaymentManager(ServiceWorkerRegistration*);

  void OnSetManifest(ScriptPromiseResolver*,
                     payments::mojom::blink::PaymentAppManifestError);
  void OnGetManifest(ScriptPromiseResolver*,
                     payments::mojom::blink::PaymentAppManifestPtr,
                     payments::mojom::blink::PaymentAppManifestError);
  void OnServiceConnectionError();

  Member<ServiceWorkerRegistration> registration_;
  Member<PaymentInstruments> instruments_;
  payments::mojom::blink::PaymentManagerPtr manager_;
};

}  // namespace blink

#endif  // PaymentManager_h
