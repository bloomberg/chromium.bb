// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_app_service_worker_registration.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/modules/payments/payment_manager.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

PaymentAppServiceWorkerRegistration::~PaymentAppServiceWorkerRegistration() =
    default;

// static
PaymentAppServiceWorkerRegistration& PaymentAppServiceWorkerRegistration::From(
    ServiceWorkerRegistration& registration) {
  PaymentAppServiceWorkerRegistration* supplement =
      Supplement<ServiceWorkerRegistration>::From<
          PaymentAppServiceWorkerRegistration>(registration);

  if (!supplement) {
    supplement = MakeGarbageCollected<PaymentAppServiceWorkerRegistration>(
        &registration);
    ProvideTo(registration, supplement);
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
    payment_manager_ = MakeGarbageCollected<PaymentManager>(registration_);
  }
  return payment_manager_.Get();
}

void PaymentAppServiceWorkerRegistration::Trace(Visitor* visitor) {
  visitor->Trace(registration_);
  visitor->Trace(payment_manager_);
  Supplement<ServiceWorkerRegistration>::Trace(visitor);
}

PaymentAppServiceWorkerRegistration::PaymentAppServiceWorkerRegistration(
    ServiceWorkerRegistration* registration)
    : registration_(registration) {}

// static
const char PaymentAppServiceWorkerRegistration::kSupplementName[] =
    "PaymentAppServiceWorkerRegistration";

}  // namespace blink
