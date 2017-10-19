// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/ServiceWorkerRegistrationBackgroundFetch.h"

#include "modules/background_fetch/BackgroundFetchManager.h"

namespace blink {

ServiceWorkerRegistrationBackgroundFetch::
    ServiceWorkerRegistrationBackgroundFetch(
        ServiceWorkerRegistration* registration)
    : registration_(registration) {}

ServiceWorkerRegistrationBackgroundFetch::
    ~ServiceWorkerRegistrationBackgroundFetch() = default;

const char* ServiceWorkerRegistrationBackgroundFetch::SupplementName() {
  return "ServiceWorkerRegistrationBackgroundFetch";
}

ServiceWorkerRegistrationBackgroundFetch&
ServiceWorkerRegistrationBackgroundFetch::From(
    ServiceWorkerRegistration& registration) {
  ServiceWorkerRegistrationBackgroundFetch* supplement =
      static_cast<ServiceWorkerRegistrationBackgroundFetch*>(
          Supplement<ServiceWorkerRegistration>::From(registration,
                                                      SupplementName()));

  if (!supplement) {
    supplement = new ServiceWorkerRegistrationBackgroundFetch(&registration);
    ProvideTo(registration, SupplementName(), supplement);
  }

  return *supplement;
}

BackgroundFetchManager*
ServiceWorkerRegistrationBackgroundFetch::backgroundFetch(
    ServiceWorkerRegistration& registration) {
  return ServiceWorkerRegistrationBackgroundFetch::From(registration)
      .backgroundFetch();
}

BackgroundFetchManager*
ServiceWorkerRegistrationBackgroundFetch::backgroundFetch() {
  if (!background_fetch_manager_)
    background_fetch_manager_ = BackgroundFetchManager::Create(registration_);

  return background_fetch_manager_.Get();
}

void ServiceWorkerRegistrationBackgroundFetch::Trace(blink::Visitor* visitor) {
  visitor->Trace(registration_);
  visitor->Trace(background_fetch_manager_);
  Supplement<ServiceWorkerRegistration>::Trace(visitor);
}

}  // namespace blink
