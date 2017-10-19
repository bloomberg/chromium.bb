// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_sync/ServiceWorkerRegistrationSync.h"

#include "modules/background_sync/SyncManager.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"

namespace blink {

ServiceWorkerRegistrationSync::ServiceWorkerRegistrationSync(
    ServiceWorkerRegistration* registration)
    : registration_(registration) {}

ServiceWorkerRegistrationSync::~ServiceWorkerRegistrationSync() {}

const char* ServiceWorkerRegistrationSync::SupplementName() {
  return "ServiceWorkerRegistrationSync";
}

ServiceWorkerRegistrationSync& ServiceWorkerRegistrationSync::From(
    ServiceWorkerRegistration& registration) {
  ServiceWorkerRegistrationSync* supplement =
      static_cast<ServiceWorkerRegistrationSync*>(
          Supplement<ServiceWorkerRegistration>::From(registration,
                                                      SupplementName()));
  if (!supplement) {
    supplement = new ServiceWorkerRegistrationSync(&registration);
    ProvideTo(registration, SupplementName(), supplement);
  }
  return *supplement;
}

SyncManager* ServiceWorkerRegistrationSync::sync(
    ServiceWorkerRegistration& registration) {
  return ServiceWorkerRegistrationSync::From(registration).sync();
}

SyncManager* ServiceWorkerRegistrationSync::sync() {
  if (!sync_manager_)
    sync_manager_ = SyncManager::Create(registration_);
  return sync_manager_.Get();
}

void ServiceWorkerRegistrationSync::Trace(blink::Visitor* visitor) {
  visitor->Trace(registration_);
  visitor->Trace(sync_manager_);
  Supplement<ServiceWorkerRegistration>::Trace(visitor);
}

}  // namespace blink
