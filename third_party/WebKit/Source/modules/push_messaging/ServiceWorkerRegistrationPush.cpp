// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/push_messaging/ServiceWorkerRegistrationPush.h"

#include "modules/push_messaging/PushManager.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"

namespace blink {

ServiceWorkerRegistrationPush::ServiceWorkerRegistrationPush(
    ServiceWorkerRegistration* registration)
    : registration_(registration) {}

ServiceWorkerRegistrationPush::~ServiceWorkerRegistrationPush() = default;

const char ServiceWorkerRegistrationPush::kSupplementName[] =
    "ServiceWorkerRegistrationPush";

ServiceWorkerRegistrationPush& ServiceWorkerRegistrationPush::From(
    ServiceWorkerRegistration& registration) {
  ServiceWorkerRegistrationPush* supplement =
      Supplement<ServiceWorkerRegistration>::From<
          ServiceWorkerRegistrationPush>(registration);
  if (!supplement) {
    supplement = new ServiceWorkerRegistrationPush(&registration);
    ProvideTo(registration, supplement);
  }
  return *supplement;
}

PushManager* ServiceWorkerRegistrationPush::pushManager(
    ServiceWorkerRegistration& registration) {
  return ServiceWorkerRegistrationPush::From(registration).pushManager();
}

PushManager* ServiceWorkerRegistrationPush::pushManager() {
  if (!push_manager_)
    push_manager_ = PushManager::Create(registration_);
  return push_manager_.Get();
}

void ServiceWorkerRegistrationPush::Trace(blink::Visitor* visitor) {
  visitor->Trace(registration_);
  visitor->Trace(push_manager_);
  Supplement<ServiceWorkerRegistration>::Trace(visitor);
}

}  // namespace blink
