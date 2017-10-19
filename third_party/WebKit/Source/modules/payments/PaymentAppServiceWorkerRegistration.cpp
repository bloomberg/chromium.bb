// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentAppServiceWorkerRegistration.h"

#include "core/dom/Document.h"
#include "modules/payments/PaymentManager.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

PaymentAppServiceWorkerRegistration::~PaymentAppServiceWorkerRegistration() {}

// static
PaymentAppServiceWorkerRegistration& PaymentAppServiceWorkerRegistration::From(
    ServiceWorkerRegistration& registration) {
  PaymentAppServiceWorkerRegistration* supplement =
      static_cast<PaymentAppServiceWorkerRegistration*>(
          Supplement<ServiceWorkerRegistration>::From(registration,
                                                      SupplementName()));

  if (!supplement) {
    supplement = new PaymentAppServiceWorkerRegistration(&registration);
    ProvideTo(registration, SupplementName(), supplement);
  }

  return *supplement;
}

// static
PaymentManager* PaymentAppServiceWorkerRegistration::paymentManager(
    ScriptState* script_state,
    ServiceWorkerRegistration& registration) {
  return PaymentAppServiceWorkerRegistration::From(registration)
      .paymentManager(script_state);
}

PaymentManager* PaymentAppServiceWorkerRegistration::paymentManager(
    ScriptState* script_state) {
  if (!payment_manager_) {
    payment_manager_ = PaymentManager::Create(registration_);
  }
  return payment_manager_.Get();
}

void PaymentAppServiceWorkerRegistration::Trace(blink::Visitor* visitor) {
  visitor->Trace(registration_);
  visitor->Trace(payment_manager_);
  Supplement<ServiceWorkerRegistration>::Trace(visitor);
}

PaymentAppServiceWorkerRegistration::PaymentAppServiceWorkerRegistration(
    ServiceWorkerRegistration* registration)
    : registration_(registration) {}

// static
const char* PaymentAppServiceWorkerRegistration::SupplementName() {
  return "PaymentAppServiceWorkerRegistration";
}

}  // namespace blink
