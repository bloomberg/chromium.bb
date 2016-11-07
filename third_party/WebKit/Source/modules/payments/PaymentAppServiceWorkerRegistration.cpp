// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentAppServiceWorkerRegistration.h"

#include "modules/payments/PaymentAppManager.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"

namespace blink {

PaymentAppServiceWorkerRegistration::~PaymentAppServiceWorkerRegistration() {}

// static
PaymentAppServiceWorkerRegistration& PaymentAppServiceWorkerRegistration::from(
    ServiceWorkerRegistration& registration) {
  PaymentAppServiceWorkerRegistration* supplement =
      static_cast<PaymentAppServiceWorkerRegistration*>(
          Supplement<ServiceWorkerRegistration>::from(registration,
                                                      supplementName()));

  if (!supplement) {
    supplement = new PaymentAppServiceWorkerRegistration(&registration);
    provideTo(registration, supplementName(), supplement);
  }

  return *supplement;
}

// static
PaymentAppManager* PaymentAppServiceWorkerRegistration::paymentAppManager(
    ServiceWorkerRegistration& registration) {
  return PaymentAppServiceWorkerRegistration::from(registration)
      .paymentAppManager();
}

PaymentAppManager* PaymentAppServiceWorkerRegistration::paymentAppManager() {
  if (!m_paymentAppManager)
    m_paymentAppManager = PaymentAppManager::create(m_registration);
  return m_paymentAppManager.get();
}

DEFINE_TRACE(PaymentAppServiceWorkerRegistration) {
  visitor->trace(m_registration);
  visitor->trace(m_paymentAppManager);
  Supplement<ServiceWorkerRegistration>::trace(visitor);
}

PaymentAppServiceWorkerRegistration::PaymentAppServiceWorkerRegistration(
    ServiceWorkerRegistration* registration)
    : m_registration(registration) {}

// static
const char* PaymentAppServiceWorkerRegistration::supplementName() {
  return "PaymentAppServiceWorkerRegistration";
}

}  // namespace blink
