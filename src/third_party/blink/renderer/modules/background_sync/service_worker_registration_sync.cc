// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_sync/service_worker_registration_sync.h"

#include "third_party/blink/renderer/modules/background_sync/periodic_sync_manager.h"
#include "third_party/blink/renderer/modules/background_sync/sync_manager.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"

namespace blink {

ServiceWorkerRegistrationSync::ServiceWorkerRegistrationSync(
    ServiceWorkerRegistration* registration)
    : registration_(registration) {}

ServiceWorkerRegistrationSync::~ServiceWorkerRegistrationSync() = default;

const char ServiceWorkerRegistrationSync::kSupplementName[] =
    "ServiceWorkerRegistrationSync";

ServiceWorkerRegistrationSync& ServiceWorkerRegistrationSync::From(
    ServiceWorkerRegistration& registration) {
  ServiceWorkerRegistrationSync* supplement =
      Supplement<ServiceWorkerRegistration>::From<
          ServiceWorkerRegistrationSync>(registration);
  if (!supplement) {
    supplement =
        MakeGarbageCollected<ServiceWorkerRegistrationSync>(&registration);
    ProvideTo(registration, supplement);
  }
  return *supplement;
}

SyncManager* ServiceWorkerRegistrationSync::sync(
    ServiceWorkerRegistration& registration) {
  return ServiceWorkerRegistrationSync::From(registration).sync();
}

SyncManager* ServiceWorkerRegistrationSync::sync() {
  if (!sync_manager_) {
    ExecutionContext* execution_context = registration_->GetExecutionContext();
    sync_manager_ = SyncManager::Create(
        registration_,
        execution_context->GetTaskRunner(TaskType::kInternalIPC));
  }
  return sync_manager_.Get();
}

PeriodicSyncManager* ServiceWorkerRegistrationSync::periodicSync(
    ServiceWorkerRegistration& registration) {
  return ServiceWorkerRegistrationSync::From(registration).periodicSync();
}

PeriodicSyncManager* ServiceWorkerRegistrationSync::periodicSync() {
  if (!periodic_sync_manager_) {
    ExecutionContext* execution_context = registration_->GetExecutionContext();
    periodic_sync_manager_ = PeriodicSyncManager::Create(
        registration_,
        execution_context->GetTaskRunner(TaskType::kInternalIPC));
  }
  return periodic_sync_manager_.Get();
}

void ServiceWorkerRegistrationSync::Trace(blink::Visitor* visitor) {
  visitor->Trace(registration_);
  visitor->Trace(sync_manager_);
  visitor->Trace(periodic_sync_manager_);
  Supplement<ServiceWorkerRegistration>::Trace(visitor);
}

}  // namespace blink
