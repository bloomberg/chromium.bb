// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_SYNC_SERVICE_WORKER_REGISTRATION_SYNC_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_SYNC_SERVICE_WORKER_REGISTRATION_SYNC_H_

#include "base/macros.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class SyncManager;
class ServiceWorkerRegistration;

class ServiceWorkerRegistrationSync final
    : public GarbageCollectedFinalized<ServiceWorkerRegistrationSync>,
      public Supplement<ServiceWorkerRegistration> {
  USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerRegistrationSync);

 public:
  static const char kSupplementName[];

  explicit ServiceWorkerRegistrationSync(ServiceWorkerRegistration*);
  virtual ~ServiceWorkerRegistrationSync();
  static ServiceWorkerRegistrationSync& From(ServiceWorkerRegistration&);

  static SyncManager* sync(ServiceWorkerRegistration&);
  SyncManager* sync();

  void Trace(blink::Visitor*) override;

 private:
  Member<ServiceWorkerRegistration> registration_;
  Member<SyncManager> sync_manager_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegistrationSync);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_SYNC_SERVICE_WORKER_REGISTRATION_SYNC_H_
