// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerRegistrationSync_h
#define ServiceWorkerRegistrationSync_h

#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class SyncManager;
class ServiceWorkerRegistration;

class ServiceWorkerRegistrationSync final
    : public GarbageCollectedFinalized<ServiceWorkerRegistrationSync>,
      public Supplement<ServiceWorkerRegistration> {
  USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerRegistrationSync);
  WTF_MAKE_NONCOPYABLE(ServiceWorkerRegistrationSync);

 public:
  virtual ~ServiceWorkerRegistrationSync();
  static ServiceWorkerRegistrationSync& From(ServiceWorkerRegistration&);

  static SyncManager* sync(ServiceWorkerRegistration&);
  SyncManager* sync();

  virtual void Trace(blink::Visitor*);

 private:
  explicit ServiceWorkerRegistrationSync(ServiceWorkerRegistration*);
  static const char* SupplementName();

  Member<ServiceWorkerRegistration> registration_;
  Member<SyncManager> sync_manager_;
};

}  // namespace blink

#endif  // ServiceWorkerRegistrationSync_h
