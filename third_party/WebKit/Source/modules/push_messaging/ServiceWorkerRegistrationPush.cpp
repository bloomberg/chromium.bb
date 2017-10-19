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

ServiceWorkerRegistrationPush::~ServiceWorkerRegistrationPush() {}

const char* ServiceWorkerRegistrationPush::SupplementName() {
  return "ServiceWorkerRegistrationPush";
}

ServiceWorkerRegistrationPush& ServiceWorkerRegistrationPush::From(
    ServiceWorkerRegistration& registration) {
  ServiceWorkerRegistrationPush* supplement =
      static_cast<ServiceWorkerRegistrationPush*>(
          Supplement<ServiceWorkerRegistration>::From(registration,
                                                      SupplementName()));
  if (!supplement) {
    supplement = new ServiceWorkerRegistrationPush(&registration);
    ProvideTo(registration, SupplementName(), supplement);
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
