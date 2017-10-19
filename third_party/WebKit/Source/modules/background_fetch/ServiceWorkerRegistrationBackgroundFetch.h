// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerRegistrationBackgroundFetch_h
#define ServiceWorkerRegistrationBackgroundFetch_h

#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/Supplementable.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Handle.h"

namespace blink {

class BackgroundFetchManager;

class ServiceWorkerRegistrationBackgroundFetch final
    : public GarbageCollectedFinalized<
          ServiceWorkerRegistrationBackgroundFetch>,
      public Supplement<ServiceWorkerRegistration> {
  USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerRegistrationBackgroundFetch);
  WTF_MAKE_NONCOPYABLE(ServiceWorkerRegistrationBackgroundFetch);

 public:
  virtual ~ServiceWorkerRegistrationBackgroundFetch();

  static ServiceWorkerRegistrationBackgroundFetch& From(
      ServiceWorkerRegistration&);

  static BackgroundFetchManager* backgroundFetch(ServiceWorkerRegistration&);
  BackgroundFetchManager* backgroundFetch();

  virtual void Trace(blink::Visitor*);

 private:
  explicit ServiceWorkerRegistrationBackgroundFetch(ServiceWorkerRegistration*);
  static const char* SupplementName();

  Member<ServiceWorkerRegistration> registration_;
  Member<BackgroundFetchManager> background_fetch_manager_;
};

}  // namespace blink

#endif  // ServiceWorkerRegistrationBackgroundFetch_h
