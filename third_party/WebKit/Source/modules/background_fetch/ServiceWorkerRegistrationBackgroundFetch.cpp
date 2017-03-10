// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/ServiceWorkerRegistrationBackgroundFetch.h"

#include "modules/background_fetch/BackgroundFetchManager.h"

namespace blink {

ServiceWorkerRegistrationBackgroundFetch::
    ServiceWorkerRegistrationBackgroundFetch(
        ServiceWorkerRegistration* registration)
    : m_registration(registration) {}

ServiceWorkerRegistrationBackgroundFetch::
    ~ServiceWorkerRegistrationBackgroundFetch() = default;

const char* ServiceWorkerRegistrationBackgroundFetch::supplementName() {
  return "ServiceWorkerRegistrationBackgroundFetch";
}

ServiceWorkerRegistrationBackgroundFetch&
ServiceWorkerRegistrationBackgroundFetch::from(
    ServiceWorkerRegistration& registration) {
  ServiceWorkerRegistrationBackgroundFetch* supplement =
      static_cast<ServiceWorkerRegistrationBackgroundFetch*>(
          Supplement<ServiceWorkerRegistration>::from(registration,
                                                      supplementName()));

  if (!supplement) {
    supplement = new ServiceWorkerRegistrationBackgroundFetch(&registration);
    provideTo(registration, supplementName(), supplement);
  }

  return *supplement;
}

BackgroundFetchManager*
ServiceWorkerRegistrationBackgroundFetch::backgroundFetch(
    ServiceWorkerRegistration& registration) {
  return ServiceWorkerRegistrationBackgroundFetch::from(registration)
      .backgroundFetch();
}

BackgroundFetchManager*
ServiceWorkerRegistrationBackgroundFetch::backgroundFetch() {
  if (!m_backgroundFetchManager)
    m_backgroundFetchManager = BackgroundFetchManager::create(m_registration);

  return m_backgroundFetchManager.get();
}

DEFINE_TRACE(ServiceWorkerRegistrationBackgroundFetch) {
  visitor->trace(m_registration);
  visitor->trace(m_backgroundFetchManager);
  Supplement<ServiceWorkerRegistration>::trace(visitor);
}

}  // namespace blink
