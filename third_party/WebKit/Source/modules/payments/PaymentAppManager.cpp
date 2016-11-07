// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentAppManager.h"

#include "modules/payments/PaymentAppManifest.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"

namespace blink {

PaymentAppManager* PaymentAppManager::create(
    ServiceWorkerRegistration* registration) {
  return new PaymentAppManager(registration);
}

ScriptPromise PaymentAppManager::getManifest() {
  NOTIMPLEMENTED();
  return ScriptPromise();
}

ScriptPromise PaymentAppManager::setManifest(
    const PaymentAppManifest& manifest) {
  NOTIMPLEMENTED();
  return ScriptPromise();
}

DEFINE_TRACE(PaymentAppManager) {
  visitor->trace(m_registration);
}

PaymentAppManager::PaymentAppManager(ServiceWorkerRegistration* registration)
    : m_registration(registration) {
  DCHECK(registration);
}

}  // namespace blink
