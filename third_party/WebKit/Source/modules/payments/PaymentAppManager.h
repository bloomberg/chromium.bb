// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentAppManager_h
#define PaymentAppManager_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class PaymentAppManifest;
class ServiceWorkerRegistration;

class MODULES_EXPORT PaymentAppManager final
    : public GarbageCollected<PaymentAppManager>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(PaymentAppManager);

 public:
  static PaymentAppManager* create(ServiceWorkerRegistration*);

  ScriptPromise setManifest(const PaymentAppManifest&);
  ScriptPromise getManifest();

  DECLARE_TRACE();

 private:
  explicit PaymentAppManager(ServiceWorkerRegistration*);

  Member<ServiceWorkerRegistration> m_registration;
};

}  // namespace blink

#endif  // PaymentAppManager_h
